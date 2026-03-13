#pragma once

#include "IScheduler.hpp"
#include "FifoScheduler.hpp"
#include "PriorityScheduler.hpp"
#include "RoundRobinScheduler.hpp"

#include <memory>
#include <string>
#include <stdexcept>

// -----------------------------------------------------------------------
// SchedulerFactory — creates the right IScheduler from a config string
//
// This is the Factory Pattern:
//   - Caller asks for "priority" → gets a PriorityScheduler
//   - Caller never calls `new PriorityScheduler` directly
//   - Adding a new scheduler = add one case here, nowhere else
//
// Why a namespace with a free function instead of a class?
//   A factory with no state and one method doesn't need to be a class.
//   A free function in a namespace is simpler and equally readable.
//
// Usage:
//   auto scheduler = SchedulerFactory::create(cfg.scheduler_type);
//   // scheduler is std::unique_ptr<IScheduler>
//   // caller doesn't know (or care) which concrete type it got
// -----------------------------------------------------------------------
namespace SchedulerFactory {

inline std::unique_ptr<IScheduler> create(const std::string& type) {
    if (type == "fifo")         return std::make_unique<FifoScheduler>();
    if (type == "priority")     return std::make_unique<PriorityScheduler>();
    if (type == "round_robin")  return std::make_unique<RoundRobinScheduler>();

    throw std::runtime_error("SchedulerFactory: unknown type: " + type);
}

} // namespace SchedulerFactory