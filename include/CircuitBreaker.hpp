#pragma once

#include <chrono>
#include <string>
#include <mutex>

// -----------------------------------------------------------------------
// CircuitBreaker — protects a Worker from cascading failures
//
// State machine:
//
//   CLOSED → normal operation, counts consecutive failures
//   OPEN   → too many failures, rejects tasks immediately (fast fail)
//   HALF_OPEN → recovery probe: try one task, decide based on result
//
// Transitions:
//   CLOSED  → OPEN       when failure_count_ >= threshold_
//   OPEN    → HALF_OPEN  when recovery_timeout has elapsed
//   HALF_OPEN → CLOSED   when the probe task succeeds
//   HALF_OPEN → OPEN     when the probe task fails
//
// Thread safety:
//   All public methods are protected by mutex_ — safe to call from
//   the Worker thread and read from the Dashboard thread simultaneously.
// -----------------------------------------------------------------------

enum class CBState {
    CLOSED,     // healthy — passing tasks through
    OPEN,       // broken — rejecting all tasks
    HALF_OPEN   // probing — letting one task through to test
};

class CircuitBreaker {
public:
    explicit CircuitBreaker(int failure_threshold = 3,
                            int recovery_timeout_s = 5)
        : threshold_(failure_threshold)
        , recovery_timeout_(std::chrono::seconds(recovery_timeout_s))
        , state_(CBState::CLOSED)
        , failure_count_(0)
    {}

    // ------------------------------------------------------------------
    // allowRequest — should the Worker attempt this task?
    //
    // CLOSED:    yes, always
    // OPEN:      no — unless recovery timeout has passed (→ HALF_OPEN)
    // HALF_OPEN: yes, but only for the one probe task
    // ------------------------------------------------------------------
    bool allowRequest() {
        std::unique_lock lock(mutex_);

        if (state_ == CBState::CLOSED) {
            return true;
        }

        if (state_ == CBState::OPEN) {
            // Check if enough time has passed to try again
            auto now = std::chrono::steady_clock::now();
            if (now - last_failure_time_ >= recovery_timeout_) {
                // Transition to HALF_OPEN — let one task through as a probe
                state_ = CBState::HALF_OPEN;
                return true;
            }
            // Still in cooldown — reject
            return false;
        }

        // HALF_OPEN — allow the probe
        if (state_ == CBState::HALF_OPEN) {
            return true;
        }

        return false;
    }

    // ------------------------------------------------------------------
    // recordSuccess — task completed without throwing
    //
    // CLOSED:    reset failure counter (streak broken)
    // HALF_OPEN: probe succeeded → back to CLOSED (fully recovered)
    // OPEN:      shouldn't happen, but treat as recovery
    // ------------------------------------------------------------------
    void recordSuccess() {
        std::unique_lock lock(mutex_);

        failure_count_ = 0;
        state_ = CBState::CLOSED;
    }

    // ------------------------------------------------------------------
    // recordFailure — task threw an exception
    //
    // CLOSED:    increment counter. If threshold reached → OPEN
    // HALF_OPEN: probe failed → back to OPEN, reset cooldown timer
    // OPEN:      already open, just refresh the timer
    // ------------------------------------------------------------------
    void recordFailure() {
        std::unique_lock lock(mutex_);

        last_failure_time_ = std::chrono::steady_clock::now();
        ++failure_count_;

        if (state_ == CBState::HALF_OPEN) {
            // Probe failed — not recovered yet
            state_ = CBState::OPEN;
            return;
        }

        if (state_ == CBState::CLOSED && failure_count_ >= threshold_) {
            state_ = CBState::OPEN;
        }
    }

    // ------------------------------------------------------------------
    // Accessors — used by Dashboard to display circuit status
    // ------------------------------------------------------------------
    CBState state() const {
        std::unique_lock lock(mutex_);
        return state_;
    }

    int failureCount() const {
        std::unique_lock lock(mutex_);
        return failure_count_;
    }

    // Human-readable state name for the Dashboard
    std::string stateName() const {
        std::unique_lock lock(mutex_);
        switch (state_) {
            case CBState::CLOSED:    return "CLOSED";
            case CBState::OPEN:      return "OPEN";
            case CBState::HALF_OPEN: return "HALF_OPEN";
        }
        return "UNKNOWN";
    }

    // Seconds remaining in OPEN cooldown (0 if not OPEN)
    double secondsUntilRetry() const {
        std::unique_lock lock(mutex_);
        if (state_ != CBState::OPEN) return 0.0;

        auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - last_failure_time_).count();
        double total = std::chrono::duration<double>(recovery_timeout_).count();
        return std::max(0.0, total - elapsed);
    }

private:
    int                                           threshold_;
    std::chrono::seconds                          recovery_timeout_;

    mutable std::mutex                            mutex_;
    CBState                                       state_;
    int                                           failure_count_;
    std::chrono::steady_clock::time_point         last_failure_time_;
};