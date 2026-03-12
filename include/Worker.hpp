#pragma once

#include "TaskBroker.hpp"
#include "Task.hpp"

#include <thread>
#include <atomic>
#include <iostream>
#include <stdexcept>

// -----------------------------------------------------------------------
// Worker — a single background thread that consumes tasks from the broker
//
// Lifecycle:
//   1. Constructor starts the thread immediately (RAII-style).
//   2. The thread loops: dequeue → mark RUNNING → execute → mark DONE/FAILED
//   3. When broker.dequeue() returns nullopt (shutdown), the loop exits.
//   4. Destructor joins the thread (waits for it to finish cleanly).
//
// Error handling:
//   If the task payload throws, we catch it, mark the task FAILED,
//   and print a message — the Worker continues to the next task.
//   (Circuit Breaker in Milestone 5 will count these failures.)
//
// Thread safety:
//   Each Worker owns its own thread. The only shared resource is the
//   TaskBroker (which is thread-safe internally).
// -----------------------------------------------------------------------
class Worker {
public:
    Worker(int id, TaskBroker& broker)
        : id_(id), broker_(broker)
    {
        // Start the worker thread immediately upon construction.
        // The thread runs the private run() method.
        thread_ = std::thread(&Worker::run, this);
    }

    // Destructor — wait for the thread to finish before destroying the object.
    // If we did NOT join, the destructor of std::thread would call
    // std::terminate() and crash the program.
    ~Worker() {
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    // Non-copyable: a thread cannot be copied.
    Worker(const Worker&)            = delete;
    Worker& operator=(const Worker&) = delete;

    // Movable: we need this to store Workers in std::vector (Milestone 2)
    Worker(Worker&&)            = delete;
    Worker& operator=(Worker&&) = delete;

    int  id()       const { return id_; }
    bool isBusy()   const { return busy_.load(); }

private:
    // ------------------------------------------------------------------
    // run — the main loop executed by the background thread
    //
    // Keeps pulling tasks from the broker until dequeue() returns nullopt
    // (which happens when broker.shutdown() is called AND queue is empty).
    // ------------------------------------------------------------------
    void run() {
        while (true) {
            // Block here until a task is available or shutdown is signalled.
            std::optional<Task> maybe_task = broker_.dequeue();

            // nullopt = broker shut down and queue is empty → exit loop
            if (!maybe_task.has_value()) {
                break;
            }

            Task& task = maybe_task.value();
            busy_.store(true);

            std::cout << "[Worker " << id_ << "] starting task "
                      << task.id << " (" << task.name << ")\n";

            task.status = TaskStatus::RUNNING;

            try {
                // Execute the actual work (the lambda stored in payload)
                task.payload();
                task.status = TaskStatus::DONE;

                std::cout << "[Worker " << id_ << "] finished task "
                          << task.id << " ✓\n";
            }
            catch (const std::exception& e) {
                task.status = TaskStatus::FAILED;
                std::cerr << "[Worker " << id_ << "] task " << task.id
                          << " FAILED: " << e.what() << "\n";
            }
            catch (...) {
                task.status = TaskStatus::FAILED;
                std::cerr << "[Worker " << id_ << "] task " << task.id
                          << " FAILED: unknown exception\n";
            }

            busy_.store(false);
        }

        std::cout << "[Worker " << id_ << "] exiting.\n";
    }

    int              id_;
    TaskBroker&      broker_;     // reference — broker outlives all workers
    std::thread      thread_;
    std::atomic<bool> busy_ {false};
};