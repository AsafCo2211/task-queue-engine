#include "Config.hpp"
#include "Task.hpp"
#include "TaskBroker.hpp"
#include "WorkerPool.hpp"

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

// -----------------------------------------------------------------------
// Milestone 2 — Config + WorkerPool
//
// What changed from M1:
//   - All magic numbers gone → come from config.json
//   - WorkerPool replaces the raw vector<unique_ptr<Worker>> in main
//   - Graceful shutdown with timeout replaces the manual poll loop
//   - ENV overrides ready for Docker (Milestone 6)
// -----------------------------------------------------------------------

static std::atomic<int> next_task_id {1};

Task makeTask(const std::string& producer_name, int local_id) {
    Task t;
    t.id       = next_task_id.fetch_add(1);
    t.name     = producer_name + "-task-" + std::to_string(local_id);
    t.priority = local_id % 3;
    t.payload  = []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50 + (rand() % 100)));
    };
    return t;
}

int producerThread(const std::string& name, TaskBroker& broker, int count) {
    int submitted = 0;
    for (int i = 1; i <= count; ++i) {
        if (broker.enqueue(makeTask(name, i))) {
            std::cout << "[" << name << "] submitted task " << i << "/" << count << "\n";
            ++submitted;
        } else {
            std::cout << "[" << name << "] queue full/shutdown — task " << i << " dropped\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return submitted;
}

int main() {
    std::cout << "=== Distributed Task Queue Engine — Milestone 2 ===\n\n";

    // ------------------------------------------------------------------
    // Step 1: Load config
    // Path is relative to where you run the binary from.
    // Run from the repo root: ./build/TaskQueue
    // ------------------------------------------------------------------
    Config cfg;
    try {
        cfg = Config::load("config/config.json");
        cfg.applyEnvOverrides();   // Docker ENV wins over JSON
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << "\n";
        std::cerr << "Using defaults.\n";
    }
    cfg.print();

    // ------------------------------------------------------------------
    // Step 2: Create broker and worker pool — driven entirely by config
    // ------------------------------------------------------------------
    TaskBroker broker(cfg.queue_capacity);
    WorkerPool pool(broker, cfg.num_workers);

    // ------------------------------------------------------------------
    // Step 3: Launch producers (same as M1 — 3 producers, 5 tasks each)
    // ------------------------------------------------------------------
    std::cout << "\nLaunching producers...\n\n";
    const int TASKS_PER_PRODUCER = 5;

    std::vector<std::thread> producers;
    std::vector<int> counts(3, 0);
    producers.emplace_back([&](){ counts[0] = producerThread("Producer-A", broker, TASKS_PER_PRODUCER); });
    producers.emplace_back([&](){ counts[1] = producerThread("Producer-B", broker, TASKS_PER_PRODUCER); });
    producers.emplace_back([&](){ counts[2] = producerThread("Producer-C", broker, TASKS_PER_PRODUCER); });

    for (auto& p : producers) p.join();
    std::cout << "\nAll producers done. Total submitted: "
              << counts[0] + counts[1] + counts[2] << "\n";

    // ------------------------------------------------------------------
    // Step 4: Graceful shutdown via WorkerPool
    //
    // WorkerPool::shutdown() replaces the manual poll loop from M1.
    // timeout comes from config so ops can tune it without recompile.
    // ------------------------------------------------------------------
    std::cout << "\nRequesting graceful shutdown...\n";
    pool.shutdown(cfg.shutdown_timeout_ms);

    std::cout << "\n=== All done. Milestone 2 complete. ===\n";
    return 0;
}