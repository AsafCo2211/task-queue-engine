#pragma once

#include "Monitor.hpp"
#include "TaskBroker.hpp"
#include "WorkerPool.hpp"

#include <thread>
#include <atomic>
#include <iostream>
#include <iomanip>
#include <sstream>

// -----------------------------------------------------------------------
// CLIDashboard — live terminal display, refreshes every second
//
// Runs on its own dedicated thread. This is the solution to the
// "garbled output" problem from Milestone 1:
//   - Workers no longer print to cout directly
//   - They call Monitor::onTaskComplete() (a simple data update)
//   - Dashboard OWNS the screen — it's the only thread that prints
//
// Rendering technique — ANSI escape codes:
//   \033[2J     — clear entire screen
//   \033[H      — move cursor to top-left (home)
//   \033[1m     — bold text
//   \033[0m     — reset formatting
//   \033[32m    — green text
//   \033[31m    — red text
//
// These codes work in any modern terminal (macOS Terminal, iTerm2,
// Linux terminals, Windows Terminal). No external library needed.
// -----------------------------------------------------------------------
class CLIDashboard {
public:
    CLIDashboard(Monitor& monitor,
                 TaskBroker& broker,
                 WorkerPool& pool,
                 int refresh_ms = 1000)
        : monitor_(monitor)
        , broker_(broker)
        , pool_(pool)
        , refresh_ms_(refresh_ms)
        , running_(false)
    {}

    ~CLIDashboard() {
        stop();
    }

    // Start the dashboard thread
    void start() {
        running_.store(true);
        thread_ = std::thread(&CLIDashboard::run, this);
    }

    // Signal the thread to stop and wait for it
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
        // Final render when stopped so last state is visible
        render();
    }

    void render() {
        auto s = monitor_.snapshot();
        s.queue_depth    = static_cast<int>(broker_.size());
        s.workers_active = pool_.activeCount();
        s.scheduler_name = broker_.schedulerName();

        // Build the output in a string first, then print once.
        // This prevents partial renders if the terminal is slow.
        std::ostringstream out;

        // Clear screen and move to top-left
        out << "\033[2J\033[3J\033[H";

        // ── Header ──────────────────────────────────────────────────
        out << "\033[1m"  // bold
            << "╔══════════════════════════════════════════════════╗\n"
            << "║   Distributed Task Queue Engine  —  Live         ║\n"
            << "╠══════════════════════════════════════════════════╣\n"
            << "\033[0m"; // reset bold

        // ── Scheduler ───────────────────────────────────────────────
        out << "║  Scheduler : \033[1m"
            << padRight(s.scheduler_name, 34)
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

        // ── Metrics ─────────────────────────────────────────────────
        out << "║  Throughput     : "
            << padRight(formatDouble(s.throughput_per_s, 1) + " tasks/sec", 30)
            << " ║\n";

        out << "║  Avg Latency    : "
            << padRight(formatDouble(s.avg_latency_ms, 1) + " ms", 30)
            << " ║\n";

        // Error rate in red if > 0
        std::string err_str = formatDouble(s.error_rate_pct, 1) + "%";
        out << "║  Error Rate     : ";
        if (s.error_rate_pct > 0.0) out << "\033[31m";  // red
        out << padRight(err_str, 30);
        if (s.error_rate_pct > 0.0) out << "\033[0m";
        out << " ║\n";

        out << "╠══════════════════════════════════════════════════╣\n";

        // ── Totals ──────────────────────────────────────────────────
        out << "║  \033[32mCompleted\033[0m      : "
            << padRight(std::to_string(s.total_completed), 30)
            << " ║\n";

        out << "║  \033[31mFailed\033[0m         : "
            << padRight(std::to_string(s.total_failed), 30)
            << " ║\n";

        out << "╚══════════════════════════════════════════════════╝\n";

        // Single write to stdout — no interleaving possible
        std::cout << out.str() << std::flush;
    }

    // ── Formatting helpers ───────────────────────────────────────────

    static std::string padRight(const std::string& s, int width) {
        if (static_cast<int>(s.size()) >= width) return s;
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
    std::atomic<bool> running_;
    std::thread       thread_;
};