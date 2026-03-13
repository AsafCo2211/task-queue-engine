#pragma once

#include "TaskResult.hpp"

// -----------------------------------------------------------------------
// IObserver — Observer pattern interface
//
// Any component that wants to know when a task completes implements this.
// Worker holds a list of IObserver* and calls onTaskComplete() after
// each task — without knowing who is listening or how many there are.
//
// Current implementors:
//   - Monitor (Milestone 4): collects metrics
//
// Future implementors (easy to add without touching Worker):
//   - CsvLogger:     writes results to a CSV file
//   - AlertSystem:   sends alert if error_rate > threshold
//   - Prometheus:    exposes metrics for scraping
// -----------------------------------------------------------------------
class IObserver {
public:
    virtual ~IObserver() = default;

    // Called by Worker immediately after a task finishes (success or fail).
    // Implementations must be thread-safe — multiple Workers call this
    // concurrently from different threads.
    virtual void onTaskComplete(const TaskResult& result) = 0;
};