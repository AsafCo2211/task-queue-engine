#include "Config.hpp"
#include "Task.hpp"
#include "TaskBroker.hpp"
#include "WorkerPool.hpp"
#include "SchedulerFactory.hpp"

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

// -----------------------------------------------------------------------
// Milestone 3 — Strategy Pattern: Schedulers
//
// What changed from M2:
//   - TaskBroker receives a scheduler from SchedulerFactory
//   - Scheduler type comes from config (zero hardcoding)
//   - Demo submits tasks with varying priorities so the difference
//     between FIFO and Priority is visible in the output
// -----------------------------------------------------------------------

static std::atomic<int> next_task_id {1};

// Tasks now have meaningful priorities: 1 (low), 5 (medium), 10 (high)
// so PriorityScheduler output is clearly different from FIFO
Task makeTask(const std::string& producer_name, int local_id, int priority) {
    Task t;
    t.id       = next_task_id.fetch_add(1);
    t.name     = producer_name + "-task-" + std::to_string(local_id)
                 + "[p=" + std::to_string(priority) + "]";
    t.priority = priority;
    t.payload  = []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    };
    return t;
}

int producerThread(const std::string& name, TaskBroker& broker, int count) {
    int submitted = 0;
    for (int i = 1; i <= count; ++i) {
        // Cycle through priorities 1, 5, 10 so we have a mix
        int priority = (i % 3 == 0) ? 10 : (i % 3 == 1) ? 1 : 5;
        if (broker.enqueue(makeTask(name, i, priority))) {
            ++submitted;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return submitted;
}

int main() {
    std::cout << "=== Distributed Task Queue Engine — Milestone 3 ===\n\n";

    // ------------------------------------------------------------------
    // Step 1: Load config
    // ------------------------------------------------------------------
    Config cfg;
    try {
        cfg = Config::load("config/config.json");
        cfg.applyEnvOverrides();
    } catch (const std::exception& e) {
        std::cerr << "Config error: " << e.what() << " — using defaults.\n";
    }
    cfg.print();

    // ------------------------------------------------------------------
    // Step 2: Create scheduler from config — this is the Factory Pattern
    //
    // main.cpp doesn't know if it's FIFO, Priority, or RoundRobin.
    // It just asks the factory and gets back a unique_ptr<IScheduler>.
    // Change config.json → different behaviour, zero code changes.
    // ------------------------------------------------------------------
    auto scheduler = SchedulerFactory::create(cfg.scheduler_type);

    // ------------------------------------------------------------------
    // Step 3: Hand scheduler to broker — this is the Strategy Pattern
    //
    // Broker stores unique_ptr<IScheduler>.
    // dequeue() calls scheduler_->next(queue_) — no if/else, no switch.
    // ------------------------------------------------------------------
    TaskBroker broker(cfg.queue_capacity, std::move(scheduler));
    std::cout << "\nScheduler: " << broker.schedulerName() << "\n\n";

    WorkerPool pool(broker, cfg.num_workers);

    // ------------------------------------------------------------------
    // Step 4: Submit tasks with mixed priorities
    // With FIFO      → tasks execute in submission order
    // With Priority  → high-priority [p=10] tasks jump the queue
    // With RoundRobin → tasks rotate evenly
    // ------------------------------------------------------------------
    std::cout << "Submitting tasks (mixed priorities: 1, 5, 10)...\n\n";
    const int TASKS_PER_PRODUCER = 6;

    std::vector<std::thread> producers;
    std::vector<int> counts(3, 0);
    producers.emplace_back([&](){ counts[0] = producerThread("Producer-A", broker, TASKS_PER_PRODUCER); });
    producers.emplace_back([&](){ counts[1] = producerThread("Producer-B", broker, TASKS_PER_PRODUCER); });
    producers.emplace_back([&](){ counts[2] = producerThread("Producer-C", broker, TASKS_PER_PRODUCER); });

    for (auto& p : producers) p.join();
    std::cout << "\nAll producers done. Total: "
              << counts[0] + counts[1] + counts[2] << " tasks\n";

    std::cout << "\nRequesting graceful shutdown...\n";
    pool.shutdown(cfg.shutdown_timeout_ms);

    std::cout << "\n=== All done. Milestone 3 complete. ===\n";
    return 0;
}