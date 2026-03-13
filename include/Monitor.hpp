#pragma once

#include "IObserver.hpp"
#include "TaskResult.hpp"

#include <shared_mutex>
#include <vector>
#include <deque>
#include <atomic>
#include <chrono>
#include <numeric>
#include <algorithm>

// -----------------------------------------------------------------------
// Monitor — Observer implementation that tracks live metrics
//
// Receives TaskResult events from Workers (via onTaskComplete) and
// computes running statistics for the CLI Dashboard.
//
// Thread safety — two-tier locking:
//   - results_mutex_: shared_mutex (readers/writers pattern)
//     Multiple Dashboard reads can happen simultaneously (shared_lock).
//     One Worker write locks exclusively (unique_lock).
//
// Metrics computed:
//   - total completed / failed
//   - average latency (ms) over the last window_size results
//   - error rate (%)
//   - throughput: tasks completed in the last ~1 second
// -----------------------------------------------------------------------
class Monitor : public IObserver {
public:
    explicit Monitor(int window_size = 100)
        : window_size_(window_size)
        , total_completed_(0)
        , total_failed_(0)
    {
        window_start_ = std::chrono::steady_clock::now();
    }

    // ------------------------------------------------------------------
    // onTaskComplete — called by Workers (from multiple threads)
    //
    // Uses unique_lock (exclusive write) because we modify results_.
    // ------------------------------------------------------------------
    void onTaskComplete(const TaskResult& result) override {
        std::unique_lock lock(results_mutex_);

        if (result.success) {
            ++total_completed_;
        } else {
            ++total_failed_;
        }

        // Keep a sliding window of recent results for latency calculation
        recent_results_.push_back(result);
        if (static_cast<int>(recent_results_.size()) > window_size_) {
            recent_results_.pop_front();
        }

        // Track completions per second window
        completions_this_window_++;
    }

    // ------------------------------------------------------------------
    // Snapshot — a point-in-time copy of all metrics
    // Safe to call from the Dashboard thread (shared_lock = multiple
    // readers allowed simultaneously, no writer blocked here).
    // ------------------------------------------------------------------
    struct Snapshot {
        long   total_completed {0};
        long   total_failed    {0};
        double avg_latency_ms  {0.0};
        double error_rate_pct  {0.0};
        double throughput_per_s{0.0};
        int    queue_depth     {0};   // filled in by main / Dashboard
        int    workers_active  {0};   // filled in by main / Dashboard
        std::string scheduler_name {};
    };

    Snapshot snapshot() const {
        std::shared_lock lock(results_mutex_);

        Snapshot s;
        s.total_completed = total_completed_;
        s.total_failed    = total_failed_;

        // Average latency over the sliding window
        if (!recent_results_.empty()) {
            double sum = 0.0;
            for (const auto& r : recent_results_) {
                sum += r.latencyMs();
            }
            s.avg_latency_ms = sum / static_cast<double>(recent_results_.size());
        }

        // Error rate
        long total = total_completed_ + total_failed_;
        if (total > 0) {
            s.error_rate_pct = 100.0 * static_cast<double>(total_failed_)
                                     / static_cast<double>(total);
        }

        // Throughput — reset the window counter each time we measure
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(
            now - window_start_).count();
        if (elapsed > 0.0) {
            s.throughput_per_s = static_cast<double>(completions_this_window_)
                                 / elapsed;
        }

        return s;
    }

    // Called by Dashboard after reading throughput to reset the window
    void resetThroughputWindow() {
        std::unique_lock lock(results_mutex_);
        completions_this_window_ = 0;
        window_start_ = std::chrono::steady_clock::now();
    }

private:
    int                        window_size_;
    mutable std::shared_mutex  results_mutex_;   // shared = multi-reader safe

    long                       total_completed_;
    long                       total_failed_;
    long                       completions_this_window_ {0};

    std::deque<TaskResult>     recent_results_;  // sliding window

    std::chrono::steady_clock::time_point window_start_;
};