#pragma once

#include "Task.hpp"
#include <chrono>
#include <string>

// -----------------------------------------------------------------------
// TaskResult — what a Worker reports after completing (or failing) a task
//
// Separating "result" from "task" is intentional:
//   - Task is the input (what to do)
//   - TaskResult is the output (what happened)
//
// This keeps the Observer interface clean — observers only get what they
// need (outcome + timing), not the full mutable Task object.
// -----------------------------------------------------------------------
struct TaskResult {
    int         task_id     {0};
    std::string task_name   {};
    int         worker_id   {0};
    bool        success     {true};
    std::string error_msg   {};   // populated if success == false

    // Timing — used by Monitor to compute latency
    std::chrono::steady_clock::time_point started_at;
    std::chrono::steady_clock::time_point finished_at;

    // Convenience: latency in milliseconds
    double latencyMs() const {
        return std::chrono::duration<double, std::milli>(
            finished_at - started_at).count();
    }
};