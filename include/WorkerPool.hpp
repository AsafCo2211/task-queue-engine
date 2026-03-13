#pragma once

#include "Worker.hpp"
#include "TaskBroker.hpp"

#include <vector>
#include <memory>
#include <chrono>
#include <iostream>

// -----------------------------------------------------------------------
// WorkerPool — creates and manages N Worker threads
//
// Responsibilities:
//   - Spawn N workers on construction (RAII)
//   - Provide shutdown(timeout_ms) with a deadline
//   - Report how many workers are currently busy
//
// RAII contract:
//   Constructor  → threads start immediately
//   Destructor   → calls shutdown() if not already called
//                  (safety net — caller should shutdown explicitly)
//
// Shutdown sequence:
//   1. broker_.shutdown()       — signals all workers to stop accepting
//   2. Wait up to timeout_ms    — give in-flight tasks time to finish
//   3. workers_.clear()         — destructs each Worker → joins thread
//
// Why store unique_ptr<Worker>?
//   Worker has a reference member (broker_) and an atomic member (busy_).
//   Neither is movable, so Worker itself is not movable.
//   Storing pointers solves this — the pointer moves, the Worker stays put.
// -----------------------------------------------------------------------
class WorkerPool {
public:
    WorkerPool(TaskBroker& broker, int num_workers)
        : broker_(broker), shutdown_called_(false)
    {
        workers_.reserve(num_workers);
        for (int i = 1; i <= num_workers; ++i) {
            workers_.push_back(std::make_unique<Worker>(i, broker_));
        }
        std::cout << "[WorkerPool] spawned " << num_workers << " workers\n";
    }

    // Destructor is the safety net — if the caller forgot to call shutdown()
    // explicitly, we still clean up correctly.
    ~WorkerPool() {
        if (!shutdown_called_) {
            shutdown(2000);  // 2s grace period in destructor
        }
    }

    // Non-copyable, non-movable (owns threads)
    WorkerPool(const WorkerPool&)            = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;
    WorkerPool(WorkerPool&&)                 = delete;
    WorkerPool& operator=(WorkerPool&&)      = delete;

    // ------------------------------------------------------------------
    // shutdown — graceful stop with a timeout
    //
    // 1. Signals the broker to stop (workers will finish current task
    //    then exit their loop).
    // 2. Polls until all workers are idle OR timeout_ms expires.
    // 3. Destroys workers (joins threads).
    //
    // timeout_ms = how long to wait for in-flight tasks to complete.
    // If the deadline passes, we still join — workers will finish their
    // current task and then see the shutdown flag.
    // ------------------------------------------------------------------
    void shutdown(int timeout_ms = 5000) {
        if (shutdown_called_) return;
        shutdown_called_ = true;

        std::cout << "[WorkerPool] shutdown requested (timeout: "
                  << timeout_ms << "ms)\n";

        // Signal broker → workers will exit their dequeue() loop
        broker_.shutdown();

        // Poll until all workers are idle (no task running) or timeout
        auto deadline = std::chrono::steady_clock::now()
                      + std::chrono::milliseconds(timeout_ms);

        while (std::chrono::steady_clock::now() < deadline) {
            if (activeCount() == 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (activeCount() > 0) {
            std::cout << "[WorkerPool] timeout reached — "
                      << activeCount() << " worker(s) still active\n";
        }

        // Join all threads — blocks until each worker's run() returns
        workers_.clear();
        std::cout << "[WorkerPool] all workers stopped\n";
    }

    // ------------------------------------------------------------------
    // Accessors
    // ------------------------------------------------------------------

    // How many workers currently executing a task
    int activeCount() const {
        int count = 0;
        for (const auto& w : workers_) {
            if (w->isBusy()) ++count;
        }
        return count;
    }

    int totalCount() const {
        return static_cast<int>(workers_.size());
    }

private:
    TaskBroker&                          broker_;
    std::vector<std::unique_ptr<Worker>> workers_;
    bool                                 shutdown_called_;
};