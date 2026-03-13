#pragma once

#include "WorkerPool.hpp"
#include "TaskBroker.hpp"

#include <thread>
#include <atomic>
#include <fstream>
#include <string>
#include <chrono>
#include <iostream>

// -----------------------------------------------------------------------
// HealthChecker — writes system health status to a file periodically
//
// Docker reads this file via HEALTHCHECK CMD to decide if the container
// is healthy, degraded, or down.
//
// Status logic:
//   UP        — broker running, all circuits CLOSED
//   DEGRADED  — broker running, but some circuits OPEN (partial failure)
//   DOWN      — broker has shut down
//
// Why a file and not an HTTP endpoint?
//   An HTTP endpoint would require a web server — unnecessary complexity.
//   A file is simple, reliable, and Docker supports it natively:
//   HEALTHCHECK CMD cat /tmp/health | grep -q "UP" || exit 1
// -----------------------------------------------------------------------
class HealthChecker {
public:
    HealthChecker(TaskBroker& broker,
                  WorkerPool& pool,
                  const std::string& status_file = "/tmp/health",
                  int interval_s = 5)
        : broker_(broker)
        , pool_(pool)
        , status_file_(status_file)
        , interval_ms_(interval_s * 1000)
        , running_(false)
    {}

    ~HealthChecker() { stop(); }

    void start() {
        running_.store(true);
        thread_ = std::thread(&HealthChecker::run, this);
    }

    void stop() {
        running_.store(false);
        if (thread_.joinable()) thread_.join();
    }

    // Returns current status string — also used by Dashboard
    std::string currentStatus() const {
        if (broker_.isShutdown()) return "DOWN";

        // Check if any circuit is OPEN → DEGRADED
        std::string circuits = pool_.circuitStates();
        if (circuits.find("OPEN") != std::string::npos) return "DEGRADED";

        return "UP";
    }

private:
    void run() {
        // Write immediately on start so Docker sees status right away
        writeStatus();

        while (running_.load()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(interval_ms_));
            writeStatus();
        }
    }

    void writeStatus() {
        std::string status = currentStatus();

        std::ofstream file(status_file_, std::ios::trunc);
        if (!file.is_open()) {
            std::cerr << "[HealthChecker] cannot write to: "
                      << status_file_ << "\n";
            return;
        }

        file << status << "\n";
        file.flush();
    }

    TaskBroker&       broker_;
    WorkerPool&       pool_;
    std::string       status_file_;
    int               interval_ms_;
    std::atomic<bool> running_;
    std::thread       thread_;
};