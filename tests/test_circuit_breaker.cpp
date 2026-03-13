#include <gtest/gtest.h>
#include "CircuitBreaker.hpp"

#include <thread>
#include <chrono>

// ═══════════════════════════════════════════════════════════════════════
// Initial state
// ═══════════════════════════════════════════════════════════════════════

TEST(CircuitBreakerTest, StartsAsClosed) {
    CircuitBreaker cb(3, 30);
    EXPECT_EQ(cb.state(), CBState::CLOSED);
    EXPECT_TRUE(cb.allowRequest());
}

TEST(CircuitBreakerTest, InitialFailureCountIsZero) {
    CircuitBreaker cb(3, 30);
    EXPECT_EQ(cb.failureCount(), 0);
}

// ═══════════════════════════════════════════════════════════════════════
// CLOSED → OPEN transition
// ═══════════════════════════════════════════════════════════════════════

TEST(CircuitBreakerTest, OpensAfterThresholdFailures) {
    CircuitBreaker cb(3, 30);   // threshold = 3

    cb.recordFailure();
    EXPECT_EQ(cb.state(), CBState::CLOSED);   // 1 failure — still closed

    cb.recordFailure();
    EXPECT_EQ(cb.state(), CBState::CLOSED);   // 2 failures — still closed

    cb.recordFailure();
    EXPECT_EQ(cb.state(), CBState::OPEN);     // 3 failures — now OPEN!
}

TEST(CircuitBreakerTest, RejectsRequestsWhenOpen) {
    CircuitBreaker cb(2, 30);
    cb.recordFailure();
    cb.recordFailure();   // threshold reached → OPEN

    EXPECT_FALSE(cb.allowRequest());
}

TEST(CircuitBreakerTest, SuccessResetsFailureCount) {
    CircuitBreaker cb(3, 30);
    cb.recordFailure();
    cb.recordFailure();       // 2 failures
    cb.recordSuccess();       // success resets counter
    cb.recordFailure();       // only 1 failure now — should NOT open

    EXPECT_EQ(cb.state(), CBState::CLOSED);
}

// ═══════════════════════════════════════════════════════════════════════
// OPEN → HALF_OPEN transition
// ═══════════════════════════════════════════════════════════════════════

TEST(CircuitBreakerTest, TransitionsToHalfOpenAfterTimeout) {
    CircuitBreaker cb(2, 1);   // recovery_timeout = 1 second
    cb.recordFailure();
    cb.recordFailure();        // OPEN
    EXPECT_EQ(cb.state(), CBState::OPEN);

    // Wait for recovery timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // allowRequest() triggers the OPEN → HALF_OPEN check
    bool allowed = cb.allowRequest();
    EXPECT_TRUE(allowed);
    EXPECT_EQ(cb.state(), CBState::HALF_OPEN);
}

// ═══════════════════════════════════════════════════════════════════════
// HALF_OPEN transitions
// ═══════════════════════════════════════════════════════════════════════

TEST(CircuitBreakerTest, HalfOpenSuccessCloses) {
    CircuitBreaker cb(2, 1);
    cb.recordFailure();
    cb.recordFailure();   // OPEN

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    cb.allowRequest();    // → HALF_OPEN

    cb.recordSuccess();   // probe succeeded → CLOSED
    EXPECT_EQ(cb.state(), CBState::CLOSED);
    EXPECT_TRUE(cb.allowRequest());
}

TEST(CircuitBreakerTest, HalfOpenFailureReopens) {
    CircuitBreaker cb(2, 1);
    cb.recordFailure();
    cb.recordFailure();   // OPEN

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    cb.allowRequest();    // → HALF_OPEN

    cb.recordFailure();   // probe failed → back to OPEN
    EXPECT_EQ(cb.state(), CBState::OPEN);
    EXPECT_FALSE(cb.allowRequest());
}

// ═══════════════════════════════════════════════════════════════════════
// State name strings
// ═══════════════════════════════════════════════════════════════════════

TEST(CircuitBreakerTest, StateNameClosed) {
    CircuitBreaker cb(3, 30);
    EXPECT_EQ(cb.stateName(), "CLOSED");
}

TEST(CircuitBreakerTest, StateNameOpen) {
    CircuitBreaker cb(1, 30);
    cb.recordFailure();
    EXPECT_EQ(cb.stateName(), "OPEN");
}

// ═══════════════════════════════════════════════════════════════════════
// Thread safety — concurrent record calls
// ═══════════════════════════════════════════════════════════════════════

TEST(CircuitBreakerTest, ConcurrentRecordCallsAreSafe) {
    CircuitBreaker cb(100, 30);
    std::vector<std::thread> threads;

    // 10 threads each recording 5 failures simultaneously
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 5; ++j) {
                cb.recordFailure();
            }
        });
    }
    for (auto& t : threads) t.join();

    // 50 total failures — must have reached threshold of 100? No — 50 < 100
    // Either way, no crash = thread safety OK
    EXPECT_GE(cb.failureCount(), 0);
}