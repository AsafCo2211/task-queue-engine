#include "Task.hpp"
#include "TaskBroker.hpp"
#include "Worker.hpp"

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

// -----------------------------------------------------------------------
// Milestone 1 Demo — Producer/Consumer in action
//
// Setup:
//   - 1 shared TaskBroker (capacity: 100)
//   - 3 Worker threads  — started first, waiting for work
//   - 3 Producer threads — each submits 5 tasks (15 tasks total)
// -----------------------------------------------------------------------

static std::atomic<int> next_task_id {1};

Task makeTask(const std::string& producer_name, int local_id) {
    Task t;
    t.id       = next_task_id.fetch_add(1);
    t.name     = producer_name + "-task-" + std::to_string(local_id);
    t.priority = local_id % 3;
    t.payload  = [name = t.name]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50 + (rand() % 100)));
    };
    return t;
}

int producerThread(const std::string& name, TaskBroker& broker, int count) {
    int submitted = 0;
    for (int i = 1; i <= count; ++i) {
        Task t = makeTask(name, i);
        bool ok = broker.enqueue(std::move(t));
        if (ok) {
            std::cout << "[" << name << "] submitted task " << i << "/" << count << "\n";
            ++submitted;
        } else {
            std::cout << "[" << name << "] queue full — task " << i << " dropped\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return submitted;
}

int main() {
    std::cout << "=== Distributed Task Queue Engine — Milestone 1 ===\n\n";

    TaskBroker broker(100);

    std::cout << "Starting 3 workers...\n";
    std::vector<std::unique_ptr<Worker>> workers;
    workers.reserve(3);
    for (int i = 1; i <= 3; ++i) {
        workers.push_back(std::make_unique<Worker>(i, broker));
    }

    std::cout << "Launching 3 producers (5 tasks each = 15 total)...\n\n";
    std::vector<std::thread> producers;
    std::vector<int> submitted_counts(3, 0);

    producers.emplace_back([&]() { submitted_counts[0] = producerThread("Producer-A", broker, 5); });
    producers.emplace_back([&]() { submitted_counts[1] = producerThread("Producer-B", broker, 5); });
    producers.emplace_back([&]() { submitted_counts[2] = producerThread("Producer-C", broker, 5); });

    for (auto& p : producers) p.join();

    int total = submitted_counts[0] + submitted_counts[1] + submitted_counts[2];
    std::cout << "\nAll producers done. Total submitted: " << total << "\n";
    std::cout << "Waiting for workers to drain queue...\n";

    while (broker.size() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "\nShutting down broker...\n";
    broker.shutdown();
    workers.clear();  // ~Worker() joins each thread

    std::cout << "\n=== All done. Milestone 1 complete. ===\n";
    return 0;
}