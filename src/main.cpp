#include "Config.hpp"
#include "Task.hpp"
#include "TaskBroker.hpp"
#include "WorkerPool.hpp"
#include "SchedulerFactory.hpp"
#include "Monitor.hpp"
#include "CLIDashboard.hpp"

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

// -----------------------------------------------------------------------
// Milestone 4 — Observer Pattern + CLI Dashboard
//
// What changed from M3:
//   - Monitor registered as Observer on all Workers
//   - CLIDashboard starts on its own thread, refreshes every second
//   - Workers no longer print directly — Dashboard owns the screen
//   - More tasks + longer run to make the Dashboard visible
// -----------------------------------------------------------------------

static std::atomic<int> next_task_id {1};

Task makeTask(const std::string& producer_name, int local_id, bool fail = false) {
    Task t;
    t.id       = next_task_id.fetch_add(1);
    t.priority = local_id % 5;
    t.name     = producer_name + "-task-" + std::to_string(local_id)
                 + "[p=" + std::to_string(t.priority) + "]";
    t.payload  = [fail]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20 + (rand() % 80)));
        if (fail) throw std::runtime_error("simulated failure");
    };
    return t;
}

void producerThread(const std::string& name, TaskBroker& broker, int count) {
    for (int i = 1; i <= count; ++i) {
        // ~10% of tasks will fail — so we see error rate > 0 in Dashboard
        bool fail = (rand() % 10 == 0);
        broker.enqueue(makeTask(name, i, fail));
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
}

int main() {
    // ------------------------------------------------------------------
    // Step 1: Load config — fail fast on bad config (fixed in M3 review)
    // ------------------------------------------------------------------
    Config cfg;
    try {
        cfg = Config::load("config/config.json");
        cfg.applyEnvOverrides();
    } catch (const std::exception& e) {
        std::cerr << "FATAL: config error: " << e.what() << "\n";
        return 1;
    }

    // ------------------------------------------------------------------
    // Step 2: Wire up all components
    // ------------------------------------------------------------------
    auto scheduler = SchedulerFactory::create(cfg.scheduler_type);
    TaskBroker broker(cfg.queue_capacity, std::move(scheduler));

    WorkerPool pool(broker, cfg.num_workers);

    // Monitor is the Observer — registered on every Worker in the pool
    Monitor monitor(cfg.monitor_window_s);
    pool.addObserver(&monitor);

    // Dashboard reads from Monitor + Broker + Pool every second
    CLIDashboard dashboard(monitor, broker, pool, cfg.monitor_refresh_ms);

    // ------------------------------------------------------------------
    // Step 3: Start Dashboard BEFORE producers — so it's visible
    // ------------------------------------------------------------------
    dashboard.start();

    // ------------------------------------------------------------------
    // Step 4: Run producers — more tasks, longer run = visible Dashboard
    // ------------------------------------------------------------------
    const int TASKS_PER_PRODUCER = 40;
    std::vector<std::thread> producers;
    producers.emplace_back([&](){ producerThread("Producer-A", broker, TASKS_PER_PRODUCER); });
    producers.emplace_back([&](){ producerThread("Producer-B", broker, TASKS_PER_PRODUCER); });
    producers.emplace_back([&](){ producerThread("Producer-C", broker, TASKS_PER_PRODUCER); });

    for (auto& p : producers) p.join();

    // Give workers time to drain the queue
    while (broker.size() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // ------------------------------------------------------------------
    // Step 5: Graceful shutdown
    // ------------------------------------------------------------------
    dashboard.stop();
    pool.shutdown(cfg.shutdown_timeout_ms);

    // Final summary below the dashboard
    auto snap = monitor.snapshot();
    std::cout << "\n✅ Run complete.\n"
              << "   Completed : " << snap.total_completed << "\n"
              << "   Failed    : " << snap.total_failed    << "\n"
              << "   Avg ms    : " << snap.avg_latency_ms  << "\n";

    return 0;
}