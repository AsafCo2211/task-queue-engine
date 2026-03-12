#pragma once

#include <functional>
#include <string>
#include <chrono>

// -----------------------------------------------------------------------
// TaskStatus — lifecycle of a single task
//
// PENDING   : created, sitting in the queue, not yet picked up
// RUNNING   : a Worker has dequeued it and is executing right now
// DONE      : completed successfully
// FAILED    : the payload threw an exception (or worker crashed)
// -----------------------------------------------------------------------
enum class TaskStatus {
    PENDING,
    RUNNING,
    DONE,
    FAILED
};

// -----------------------------------------------------------------------
// Task — the fundamental unit of work in the system
//
// Design decisions:
//
// 1. payload is std::function<void()>
//    - Any callable (lambda, functor, free function) fits here.
//    - We don't template Task itself because that would make every
//      container (queue, vector) need to know the type at compile time.
//      std::function gives us type erasure — the broker stores Task
//      objects without caring what's inside.
//
// 2. priority is an int (higher = more urgent)
//    - Used by PriorityScheduler in Milestone 3.
//    - FIFO scheduler ignores it entirely.
//
// 3. created_at uses steady_clock (not system_clock)
//    - steady_clock never goes backwards (no DST, no NTP jumps).
//    - Ideal for measuring durations / latency.
//
// 4. The struct is move-only in practice (payload holds a closure).
//    We keep copy available but prefer Task&& in enqueue().
// -----------------------------------------------------------------------
struct Task {
    // --- identity ---
    int         id          {0};
    std::string name        {};          // human-readable label (e.g. "render-42")
    int         priority    {0};         // higher = more urgent

    // --- the actual work ---
    std::function<void()> payload;       // what the Worker will call

    // --- lifecycle ---
    TaskStatus  status      {TaskStatus::PENDING};

    // --- timing (filled in by Monitor in Milestone 4) ---
    std::chrono::steady_clock::time_point created_at {
        std::chrono::steady_clock::now()
    };
};