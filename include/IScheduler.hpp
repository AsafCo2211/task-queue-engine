#pragma once

#include "Task.hpp"
#include <deque>

// -----------------------------------------------------------------------
// IScheduler — Strategy interface for task selection
//
// This is the heart of the Strategy pattern:
//   - One interface, multiple implementations
//   - TaskBroker holds a std::unique_ptr<IScheduler>
//   - Swapping the scheduler = zero changes to TaskBroker or Workers
//
// Why std::deque<Task>& and not std::queue<Task>&?
//   std::queue is a restricted adapter — it only exposes front/back/push/pop.
//   PriorityScheduler and RoundRobinScheduler need to iterate or reorder.
//   std::deque gives full random access while still being efficient.
//
// Contract:
//   - next() is called when a Worker is ready for a task
//   - It MUST remove the chosen task from the deque
//   - It MAY reorder the deque (RoundRobin, Priority)
//   - Caller guarantees deque is non-empty before calling next()
// -----------------------------------------------------------------------
class IScheduler {
public:
    virtual ~IScheduler() = default;

    // Pick the next task to execute and remove it from the queue.
    // Precondition: !queue.empty()
    virtual Task next(std::deque<Task>& queue) = 0;

    // Human-readable name for logging and Dashboard display
    virtual const char* name() const = 0;
};