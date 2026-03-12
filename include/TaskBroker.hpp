#pragma once

#include "Task.hpp"

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

// -----------------------------------------------------------------------
// TaskBroker — the heart of the system
//
// Responsibilities:
//   - Accept tasks from any number of Producer threads (enqueue)
//   - Hand tasks out to any number of Worker threads  (dequeue)
//   - Let Workers sleep when the queue is empty       (condition_variable)
//   - Signal all Workers to stop cleanly              (shutdown)
//
// Thread safety:
//   Every public method is safe to call from multiple threads
//   simultaneously. The mutex_ serialises all queue access.
//
// Capacity:
//   If capacity_ is reached, enqueue() returns false (backpressure).
//   The Producer is responsible for handling that signal.
//   (Milestone 5 expands this into Circuit Breaker logic.)
// -----------------------------------------------------------------------
class TaskBroker {
public:
    // capacity = max tasks allowed in the queue at once
    explicit TaskBroker(std::size_t capacity = 1000)
        : capacity_(capacity), shutdown_(false) {}

    // ------------------------------------------------------------------
    // enqueue — called by Producers
    //
    // Takes a Task by value (move-friendly).
    // Returns false if the queue is full (backpressure) or shutting down.
    // ------------------------------------------------------------------
    bool enqueue(Task task) {
        {
            std::unique_lock<std::mutex> lock(mutex_);

            // Reject new work during shutdown or when queue is full
            if (shutdown_ || queue_.size() >= capacity_) {
                return false;
            }

            queue_.push(std::move(task));
        }
        // Notify ONE sleeping Worker that there is work to do.
        // We release the lock BEFORE notifying — this is the canonical
        // pattern: avoids the notified thread immediately re-blocking
        // on the mutex we still hold.
        cv_.notify_one();
        return true;
    }

    // ------------------------------------------------------------------
    // dequeue — called by Workers
    //
    // Blocks (sleeps) until a task is available OR shutdown is requested.
    // Returns std::nullopt when the broker is shut down and queue is empty.
    //
    // std::optional<Task> lets us express "nothing left" without exceptions.
    // ------------------------------------------------------------------
    std::optional<Task> dequeue() {
        std::unique_lock<std::mutex> lock(mutex_);

        // Sleep until: queue has something  OR  we are shutting down.
        // The lambda is the "predicate" — cv.wait re-checks it every
        // time the thread is woken up (guards against spurious wakeups).
        cv_.wait(lock, [this] {
            return !queue_.empty() || shutdown_;
        });

        // Woken up because of shutdown AND queue is now empty → stop.
        if (queue_.empty()) {
            return std::nullopt;
        }

        // Move the front task out of the queue (avoids a copy of payload)
        Task task = std::move(queue_.front());
        queue_.pop();
        return task;
    }

    // ------------------------------------------------------------------
    // shutdown — signals all Workers to finish and exit
    //
    // After this call:
    //   - enqueue() will return false for any new tasks
    //   - dequeue() will drain remaining tasks, then return nullopt
    //
    // notify_all() wakes EVERY sleeping Worker so none remain stuck.
    // ------------------------------------------------------------------
    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
    }

    // ------------------------------------------------------------------
    // Accessors (safe to call from any thread)
    // ------------------------------------------------------------------
    std::size_t size() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool isShutdown() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return shutdown_;
    }

private:
    std::size_t                     capacity_;
    bool                            shutdown_;

    std::queue<Task>                queue_;
    mutable std::mutex              mutex_;    // mutable: const methods can lock
    std::condition_variable         cv_;
};