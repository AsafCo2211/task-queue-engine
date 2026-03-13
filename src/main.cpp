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
// Milestone 5 — Circuit Breaker
//
// Demo strategy:
//   Phase 1 (0-2s):  normal tasks — circuits CLOSED
//   Phase 2 (2-4s):  burst of failing tasks — circuits trip to OPEN
//   Phase 3 (4s+) :  normal tasks again — circuits recover to CLOSED
//
// Watch the Dashboard:
//   - "Circuits" row will show W1:OPEN W2:OPEN etc. in red
//   - After recovery_timeout (5s in config), HALF_OPEN → probe → CLOSED
// -----------------------------------------------------------------------

static std::atomic<int> next_task_id {1};

Task makeTask(const std::string& name, int local_id, bool fail) {
    Task t;
    t.id       = next_task_id.fetch_add(1);
    t.priority = local_id % 5;
    t.name     = name + "-" + std::to_string(local_id)
                 + (fail ? "[FAIL]" : "[OK]");
    t.payload  = [fail]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        if (fail) throw std::runtime_error("simulated failure");
    };
    return t;
}

int main() {
    // ------------------------------------------------------------------
    // Config — fail fast
    // ------------------------------------------------------------------
    Config cfg;
    try {
        cfg = Config::load("config/config.json");
        cfg.applyEnvOverrides();
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << "\n";
        return 1;
    }

    // ------------------------------------------------------------------
    // Wire up components
    // ------------------------------------------------------------------
    auto scheduler = SchedulerFactory::create(cfg.scheduler_type);
    TaskBroker broker(cfg.queue_capacity, std::move(scheduler));

    // Pass CB thresholds from config to each Worker via WorkerPool
    WorkerPool pool(broker, cfg.num_workers,
                    cfg.cb_failure_threshold,
                    cfg.cb_recovery_timeout_s);

    Monitor monitor(cfg.monitor_window_s);
    pool.addObserver(&monitor);

    CLIDashboard dashboard(monitor, broker, pool, cfg.monitor_refresh_ms);
    dashboard.start();

    // ------------------------------------------------------------------
    // Phase 1 — normal tasks (circuits stay CLOSED)
    // ------------------------------------------------------------------
    std::cout << "\033[2J\033[H";
    for (int i = 1; i <= 20; ++i) {
        broker.enqueue(makeTask("normal", i, false));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // ------------------------------------------------------------------
    // Phase 2 — burst of failures (circuits trip to OPEN)
    //
    // We send enough failures to exceed cb_failure_threshold (3 in config)
    // so the circuit on at least one Worker trips open.
    // ------------------------------------------------------------------
    for (int i = 1; i <= 15; ++i) {
        broker.enqueue(makeTask("fail-burst", i, true));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Wait long enough for circuit to open — visible in Dashboard
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // ------------------------------------------------------------------
    // Phase 3 — recovery: send normal tasks after cooldown
    // Workers in HALF_OPEN will probe with one task → succeed → CLOSED
    // ------------------------------------------------------------------
    for (int i = 1; i <= 20; ++i) {
        broker.enqueue(makeTask("recovery", i, false));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Wait for queue to drain + final dashboard render
    while (broker.size() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // ------------------------------------------------------------------
    // Graceful shutdown
    // ------------------------------------------------------------------
    dashboard.stop();
    pool.shutdown(cfg.shutdown_timeout_ms);

    auto snap = monitor.snapshot();
    std::cout << "\n✅ Run complete.\n"
              << "   Completed : " << snap.total_completed << "\n"
              << "   Failed    : " << snap.total_failed    << "\n"
              << "   Avg ms    : " << snap.avg_latency_ms  << "\n";

    return 0;
}