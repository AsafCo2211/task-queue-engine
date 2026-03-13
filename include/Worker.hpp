#pragma once

#include "TaskBroker.hpp"
#include "Task.hpp"
#include "IObserver.hpp"
#include "TaskResult.hpp"
#include "CircuitBreaker.hpp"

#include <thread>
#include <atomic>
#include <vector>
#include <stdexcept>
#include <iostream>

// -----------------------------------------------------------------------
// Worker — updated for Milestone 5 (Circuit Breaker)
//
// What changed from M4:
//   - Each Worker owns a CircuitBreaker instance
//   - Before executing a task: cb_.allowRequest() is checked
//     - false → task is re-queued (returned to broker), Worker sleeps
//     - true  → execute normally
//   - After execution: recordSuccess() or recordFailure() called
//   - CircuitBreaker state is embedded in TaskResult for Dashboard
// -----------------------------------------------------------------------
class Worker {
public:
    Worker(int id, TaskBroker& broker,
           int cb_threshold = 3, int cb_recovery_s = 5)
        : id_(id)
        , broker_(broker)
        , cb_(cb_threshold, cb_recovery_s)
    {
        thread_ = std::thread(&Worker::run, this);
    }

    ~Worker() {
        if (thread_.joinable()) thread_.join();
    }

    Worker(const Worker&)            = delete;
    Worker& operator=(const Worker&) = delete;
    Worker(Worker&&)                 = delete;
    Worker& operator=(Worker&&)      = delete;

    void addObserver(IObserver* obs) { observers_.push_back(obs); }

    int         id()           const { return id_; }
    bool        isBusy()       const { return busy_.load(); }
    std::string circuitState() const { return cb_.stateName(); }
    int         failureCount() const { return cb_.failureCount(); }

private:
    void run() {
        while (true) {
            std::optional<Task> maybe_task = broker_.dequeue();
            if (!maybe_task.has_value()) break;

            Task task = std::move(maybe_task.value());
            busy_.store(true);

            // ----------------------------------------------------------
            // Circuit Breaker check — BEFORE executing the task
            //
            // If circuit is OPEN: don't even try.
            // Re-enqueue the task so another worker (or this one later)
            // can attempt it after the cooldown.
            // ----------------------------------------------------------
            if (!cb_.allowRequest()) {
                // Put the task back — we're in cooldown
                broker_.enqueue(std::move(task));
                busy_.store(false);

                // Sleep a bit so we don't spin-loop while OPEN
                double wait = cb_.secondsUntilRetry();
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(static_cast<int>(wait * 200)));
                continue;
            }

            // ----------------------------------------------------------
            // Execute the task
            // ----------------------------------------------------------
            auto started   = std::chrono::steady_clock::now();
            task.status    = TaskStatus::RUNNING;

            TaskResult result;
            result.task_id    = task.id;
            result.task_name  = task.name;
            result.worker_id  = id_;
            result.started_at = started;

            try {
                task.payload();
                task.status    = TaskStatus::DONE;
                result.success = true;
                cb_.recordSuccess();              // ← tell the circuit: OK
            }
            catch (const std::exception& e) {
                task.status      = TaskStatus::FAILED;
                result.success   = false;
                result.error_msg = e.what();
                cb_.recordFailure();              // ← tell the circuit: FAIL
            }
            catch (...) {
                task.status      = TaskStatus::FAILED;
                result.success   = false;
                result.error_msg = "unknown exception";
                cb_.recordFailure();
            }

            result.finished_at = std::chrono::steady_clock::now();
            notifyAll(result);
            busy_.store(false);
        }
    }

    void notifyAll(const TaskResult& result) {
        for (auto* obs : observers_) obs->onTaskComplete(result);
    }

    int               id_;
    TaskBroker&       broker_;
    std::thread       thread_;
    std::atomic<bool> busy_  {false};

    CircuitBreaker          cb_;
    std::vector<IObserver*> observers_;
};