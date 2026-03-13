#pragma once

#include "Worker.hpp"
#include "TaskBroker.hpp"
#include "IObserver.hpp"

#include <vector>
#include <memory>
#include <chrono>
#include <string>

// -----------------------------------------------------------------------
// WorkerPool — updated for Milestone 5
//
// What changed:
//   - Constructor accepts cb_threshold + cb_recovery_s from Config
//     and passes them to each Worker
//   - circuitStates() returns a summary for the Dashboard
// -----------------------------------------------------------------------
class WorkerPool {
public:
    WorkerPool(TaskBroker& broker, int num_workers,
               int cb_threshold = 3, int cb_recovery_s = 5)
        : broker_(broker), shutdown_called_(false)
    {
        workers_.reserve(num_workers);
        for (int i = 1; i <= num_workers; ++i) {
            workers_.push_back(std::make_unique<Worker>(
                i, broker_, cb_threshold, cb_recovery_s));
        }
    }

    ~WorkerPool() {
        if (!shutdown_called_) shutdown(2000);
    }

    WorkerPool(const WorkerPool&)            = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;
    WorkerPool(WorkerPool&&)                 = delete;
    WorkerPool& operator=(WorkerPool&&)      = delete;

    void addObserver(IObserver* obs) {
        for (auto& w : workers_) w->addObserver(obs);
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
        for (const auto& w : workers_)
            if (w->isBusy()) ++count;
        return count;
    }

    int totalCount() const {
        return static_cast<int>(workers_.size());
    }

    // Returns "W1:CLOSED W2:OPEN W3:CLOSED ..." for the Dashboard
    std::string circuitStates() const {
        std::string result;
        for (const auto& w : workers_) {
            result += "W" + std::to_string(w->id())
                    + ":" + w->circuitState() + " ";
        }
        return result;
    }

private:
    TaskBroker&                          broker_;
    std::vector<std::unique_ptr<Worker>> workers_;
    bool                                 shutdown_called_;
};