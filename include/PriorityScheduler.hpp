#pragma once

#include "IScheduler.hpp"
#include <algorithm>

// -----------------------------------------------------------------------
// PriorityScheduler — Highest Priority First
//
// Scans the entire queue for the task with the highest priority value.
// Higher priority int = more urgent = goes first.
//
// Use when:
//   - Some tasks are more important than others (e.g. user-facing vs batch)
//   - SLA requirements — critical tasks must not wait behind low-priority ones
//
// Design note:
//   We use std::max_element on the deque rather than maintaining a
//   std::priority_queue internally. Why?
//
//   - The deque is shared with the broker — we don't own it
//   - New tasks arrive via enqueue() at any time
//   - A separate priority_queue would need to stay in sync = complexity
//   - For portfolio-scale queue sizes, O(n) scan is perfectly fine
//
// Time complexity: O(n) per selection — acceptable for moderate queue sizes
// -----------------------------------------------------------------------
class PriorityScheduler : public IScheduler {
public:
    Task next(std::deque<Task>& queue) override {
        // Find the element with the highest priority
        auto it = std::max_element(
            queue.begin(), queue.end(),
            [](const Task& a, const Task& b) {
                return a.priority < b.priority;  // max = highest priority
            }
        );

        // Move the task out before erasing (erase invalidates the object)
        Task t = std::move(*it);
        queue.erase(it);
        return t;
    }

    const char* name() const override { return "Priority"; }
};