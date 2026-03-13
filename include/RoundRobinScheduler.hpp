#pragma once

#include "IScheduler.hpp"

// -----------------------------------------------------------------------
// RoundRobinScheduler — Distribute Tasks Evenly
//
// Cycles through tasks in a rotating fashion using an index counter.
// Each call advances the index by 1 (wrapping around).
//
// Example with 4 tasks [A, B, C, D] and counter starting at 0:
//   Call 1: index=0 → picks A, removes it, counter becomes 1
//   Call 2: index=1%3=1 → picks C (was index 2, now index 1 after A removed)
//   ...
//
// Use when:
//   - You want to avoid starvation (no task waits forever)
//   - Even distribution across producers matters
//   - All tasks have similar duration
//
// Design note:
//   The counter_ persists across calls — it's the scheduler's state.
//   This is intentional: RoundRobin's whole point is remembering where
//   it left off. The counter wraps with modulo to stay in bounds.
//
// Time complexity: O(1)
// -----------------------------------------------------------------------
class RoundRobinScheduler : public IScheduler {
public:
    Task next(std::deque<Task>& queue) override {
        // Wrap counter to valid index range
        std::size_t index = counter_ % queue.size();
        counter_++;

        // Move out, then erase
        Task t = std::move(queue[index]);
        queue.erase(queue.begin() + static_cast<std::ptrdiff_t>(index));
        return t;
    }

    const char* name() const override { return "RoundRobin"; }

private:
    std::size_t counter_ {0};
};