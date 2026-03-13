#pragma once

#include "Worker.hpp"
#include "TaskBroker.hpp"
#include "IObserver.hpp"

#include <vector>
#include <memory>
#include <chrono>
#include <iostream>

// -----------------------------------------------------------------------
// WorkerPool — updated for Milestone 4
//
// What changed:
//   - addObserver(IObserver*): registers an observer on ALL workers
//     Must be called before workers start receiving tasks.
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
    }

    ~WorkerPool() {
        if (!shutdown_called_) shutdown(2000);
    }

    WorkerPool(const WorkerPool&)            = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;
    WorkerPool(WorkerPool&&)                 = delete;
    WorkerPool& operator=(WorkerPool&&)      = delete;

    // Register an observer on every worker in the pool
    void addObserver(IObserver* obs) {
        for (auto& w : workers_) {
            w->addObserver(obs);
        }
    }

    void shutdown(int timeout_ms = 5000) {
        if (shutdown_called_) return;
        shutdown_called_ = true;

        broker_.shutdown();

        auto deadline = std::chrono::steady_clock::now()
                      + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            if (activeCount() == 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        workers_.clear();
    }

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