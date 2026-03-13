#pragma once

#include "IScheduler.hpp"

// -----------------------------------------------------------------------
// FifoScheduler — First In, First Out
//
// The simplest possible scheduler.
// Tasks are executed in the exact order they were submitted.
//
// Use when:
//   - All tasks are equally important
//   - Fairness matters (no task should "cut the line")
//   - Predictable ordering is needed for debugging
//
// Time complexity: O(1) — just take from the front
// -----------------------------------------------------------------------
class FifoScheduler : public IScheduler {
public:
    Task next(std::deque<Task>& queue) override {
        // Take the oldest task (front = first submitted)
        Task t = std::move(queue.front());
        queue.pop_front();
        return t;
    }

    const char* name() const override { return "FIFO"; }
};