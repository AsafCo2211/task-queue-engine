#pragma once

#include "Monitor.hpp"
#include "TaskBroker.hpp"
#include "WorkerPool.hpp"
#include "HealthChecker.hpp"

#include <thread>
#include <atomic>
#include <iostream>
#include <iomanip>
#include <sstream>

// -----------------------------------------------------------------------
// CLIDashboard — updated for Milestone 6
//
// What changed:
//   - Accepts optional HealthChecker* for health status row
//   - Shows UP (green) / DEGRADED (yellow) / DOWN (red)
// -----------------------------------------------------------------------
class CLIDashboard {
public:
    CLIDashboard(Monitor& monitor, TaskBroker& broker,
                 WorkerPool& pool, int refresh_ms = 1000,
                 HealthChecker* health = nullptr)
        : monitor_(monitor), broker_(broker)
        , pool_(pool), refresh_ms_(refresh_ms)
        , health_(health), running_(false)
    {}

    ~CLIDashboard() { stop(); }

    void start() {
        running_.store(true);
        thread_ = std::thread(&CLIDashboard::run, this);
    }

    void stop() {
        running_.store(false);
        if (thread_.joinable()) thread_.join();
    }

private:
    void run() {
        while (running_.load()) {
            render();
            monitor_.resetThroughputWindow();
            std::this_thread::sleep_for(
                std::chrono::milliseconds(refresh_ms_));
        }
        render();
    }

    void render() {
        auto s = monitor_.snapshot();
        s.queue_depth    = static_cast<int>(broker_.size());
        s.workers_active = pool_.activeCount();
        s.scheduler_name = broker_.schedulerName();

        std::string circuits  = pool_.circuitStates();
        std::string health_status = health_ ? health_->currentStatus() : "N/A";
        bool any_open = circuits.find("OPEN") != std::string::npos;

        std::ostringstream out;
        out << "\033[2J\033[3J\033[H";

        // ── Header ──────────────────────────────────────────────────
        out << "\033[1m"
            << "╔══════════════════════════════════════════════════╗\n"
            << "║   Distributed Task Queue Engine  —  Live         ║\n"
            << "╠══════════════════════════════════════════════════╣\n"
            << "\033[0m";

        // ── Health Status ────────────────────────────────────────────
        out << "║  Health     : ";
        if      (health_status == "UP")       out << "\033[32m";  // green
        else if (health_status == "DEGRADED") out << "\033[33m";  // yellow
        else                                  out << "\033[31m";  // red
        out << padRight(health_status, 35) << "\033[0m║\n";

        // ── Scheduler ───────────────────────────────────────────────
        out << "║  Scheduler  : \033[1m"
            << padRight(s.scheduler_name, 33)
            << "\033[0m  ║\n"
            << "╠══════════════════════════════════════════════════╣\n";

        // ── Queue & Workers ─────────────────────────────────────────
        out << "║  Queue Depth    : "
            << padRight(std::to_string(s.queue_depth) + " tasks waiting", 30)
            << " ║\n";
        out << "║  Workers Active : "
            << padRight(std::to_string(s.workers_active) + " / "
                + std::to_string(pool_.totalCount()), 30)
            << " ║\n";
        out << "╠══════════════════════════════════════════════════╣\n";

        // ── Circuit Breaker ──────────────────────────────────────────
        out << "║  Circuits : ";
        if (any_open) out << "\033[31m";
        out << padRight(circuits, 37);
        if (any_open) out << "\033[0m";
        out << "║\n";
        out << "╠══════════════════════════════════════════════════╣\n";

        // ── Metrics ─────────────────────────────────────────────────
        out << "║  Throughput     : "
            << padRight(formatDouble(s.throughput_per_s, 1) + " tasks/sec", 30)
            << " ║\n";
        out << "║  Avg Latency    : "
            << padRight(formatDouble(s.avg_latency_ms, 1) + " ms", 30)
            << " ║\n";

        std::string err_str = formatDouble(s.error_rate_pct, 1) + "%";
        out << "║  Error Rate     : ";
        if (s.error_rate_pct > 0.0) out << "\033[31m";
        out << padRight(err_str, 30);
        if (s.error_rate_pct > 0.0) out << "\033[0m";
        out << " ║\n";
        out << "╠══════════════════════════════════════════════════╣\n";

        // ── Totals ──────────────────────────────────────────────────
        out << "║  \033[32mCompleted\033[0m      : "
            << padRight(std::to_string(s.total_completed), 30) << " ║\n";
        out << "║  \033[31mFailed\033[0m         : "
            << padRight(std::to_string(s.total_failed), 30) << " ║\n";
        out << "╚══════════════════════════════════════════════════╝\n";

        std::cout << out.str() << std::flush;
    }

    static std::string padRight(const std::string& s, int width) {
        if (static_cast<int>(s.size()) >= width) return s.substr(0, width);
        return s + std::string(width - s.size(), ' ');
    }

    static std::string formatDouble(double v, int decimals) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(decimals) << v;
        return ss.str();
    }

    Monitor&          monitor_;
    TaskBroker&       broker_;
    WorkerPool&       pool_;
    int               refresh_ms_;
    HealthChecker*    health_;       // optional — nullptr if not used
    std::atomic<bool> running_;
    std::thread       thread_;
};