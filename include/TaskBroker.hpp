#pragma once

#include "Task.hpp"
#include "IScheduler.hpp"
#include "FifoScheduler.hpp"

#include <deque>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <memory>

// -----------------------------------------------------------------------
// TaskBroker — the heart of the system (updated for Milestone 3)
//
// What changed from M1/M2:
//   - Internal queue_ is now std::deque<Task> (was std::queue)
//     Reason: IScheduler::next() needs random access to reorder tasks.
//             std::queue only exposes front/back — not enough.
//
//   - Holds a std::unique_ptr<IScheduler>
//     Reason: Strategy pattern — broker delegates the "who goes next"
//             decision to the scheduler without knowing its type.
//
//   - dequeue() now calls scheduler_->next(queue_) instead of queue_.front()
//     This single line change is the entire Strategy pattern in action.
//
// Everything else (mutex, condition_variable, backpressure, shutdown)
// is unchanged — the scheduler plugs in without touching any of that.
// -----------------------------------------------------------------------
class TaskBroker {
public:
    // Default scheduler is FIFO — safe, predictable, zero config needed
    explicit TaskBroker(std::size_t capacity = 1000,
                        std::unique_ptr<IScheduler> scheduler =
                            std::make_unique<FifoScheduler>())
        : capacity_(capacity)
        , shutdown_(false)
        , scheduler_(std::move(scheduler))
    {}

    // ------------------------------------------------------------------
    // enqueue — called by Producers (unchanged from M1)
    // ------------------------------------------------------------------
    bool enqueue(Task task) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (shutdown_ || queue_.size() >= capacity_) {
                return false;
            }
            queue_.push_back(std::move(task));
        }
        cv_.notify_one();
        return true;
    }

    // ------------------------------------------------------------------
    // dequeue — now delegates ordering to the scheduler
    //
    // Before M3:  task = queue_.front(); queue_.pop();
    // After  M3:  task = scheduler_->next(queue_);
    //
    // That's it. One line. The entire Strategy pattern.
    // ------------------------------------------------------------------
    std::optional<Task> dequeue() {
        std::unique_lock<std::mutex> lock(mutex_);

        cv_.wait(lock, [this] {
            return !queue_.empty() || shutdown_;
        });

        if (queue_.empty()) {
            return std::nullopt;
        }

        // Delegate the selection to whichever scheduler is plugged in
        Task task = scheduler_->next(queue_);
        return task;
    }

    // ------------------------------------------------------------------
    // shutdown — unchanged from M1
    // ------------------------------------------------------------------
    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
    }

    // ------------------------------------------------------------------
    // setScheduler — swap algorithm at runtime (no restart needed)
    // ------------------------------------------------------------------
    void setScheduler(std::unique_ptr<IScheduler> s) {
        std::unique_lock<std::mutex> lock(mutex_);
        scheduler_ = std::move(s);
    }

    const char* schedulerName() const {
        return scheduler_->name();
    }

    // ------------------------------------------------------------------
    // Accessors
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

    std::deque<Task>                queue_;      // deque: random access for schedulers
    std::unique_ptr<IScheduler>     scheduler_;  // Strategy: pluggable algorithm

    mutable std::mutex              mutex_;
    std::condition_variable         cv_;
};