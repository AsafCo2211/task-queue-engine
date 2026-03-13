#pragma once

#include "TaskBroker.hpp"
#include "Task.hpp"
#include "IObserver.hpp"
#include "TaskResult.hpp"

#include <thread>
#include <atomic>
#include <vector>
#include <stdexcept>

// -----------------------------------------------------------------------
// Worker — updated for Milestone 4 (Observer pattern)
//
// What changed:
//   - Holds a list of IObserver* (observers_)
//   - After every task (success or fail), calls notifyAll(result)
//   - Removed direct std::cout — Dashboard now owns the screen
//     (Workers still report errors to std::cerr as a fallback)
//
// Observer registration:
//   addObserver(IObserver*) — call before starting work
//   Observers are non-owning raw pointers: Worker doesn't manage
//   their lifetime. Caller must ensure they outlive the Worker.
// -----------------------------------------------------------------------
class Worker {
public:
    Worker(int id, TaskBroker& broker)
        : id_(id), broker_(broker)
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

    // Register an observer — must be called before tasks start flowing
    void addObserver(IObserver* obs) {
        observers_.push_back(obs);
    }

    int  id()     const { return id_; }
    bool isBusy() const { return busy_.load(); }

private:
    void run() {
        while (true) {
            std::optional<Task> maybe_task = broker_.dequeue();
            if (!maybe_task.has_value()) break;

            Task& task = maybe_task.value();
            busy_.store(true);

            // Record when we started executing (for latency measurement)
            auto started = std::chrono::steady_clock::now();
            task.status  = TaskStatus::RUNNING;

            TaskResult result;
            result.task_id    = task.id;
            result.task_name  = task.name;
            result.worker_id  = id_;
            result.started_at = started;

            try {
                task.payload();
                task.status   = TaskStatus::DONE;
                result.success = true;
            }
            catch (const std::exception& e) {
                task.status      = TaskStatus::FAILED;
                result.success   = false;
                result.error_msg = e.what();
            }
            catch (...) {
                task.status      = TaskStatus::FAILED;
                result.success   = false;
                result.error_msg = "unknown exception";
            }

            result.finished_at = std::chrono::steady_clock::now();

            // Notify all registered observers — this is the Observer pattern
            // Worker doesn't know who's listening or how many there are
            notifyAll(result);

            busy_.store(false);
        }
    }

    // ------------------------------------------------------------------
    // notifyAll — the Observer pattern in action
    //
    // Iterates over all registered observers and calls onTaskComplete().
    // Adding a new observer (Logger, AlertSystem) = zero changes here.
    // ------------------------------------------------------------------
    void notifyAll(const TaskResult& result) {
        for (auto* obs : observers_) {
            obs->onTaskComplete(result);
        }
    }

    int               id_;
    TaskBroker&       broker_;
    std::thread       thread_;
    std::atomic<bool> busy_  {false};

    // Non-owning pointers — observers outlive workers
    std::vector<IObserver*> observers_;
};