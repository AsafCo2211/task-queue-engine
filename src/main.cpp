#include "Config.hpp"
#include "Task.hpp"
#include "TaskBroker.hpp"
#include "WorkerPool.hpp"
#include "SchedulerFactory.hpp"
#include "Monitor.hpp"
#include "CLIDashboard.hpp"
#include "HealthChecker.hpp"

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

// -----------------------------------------------------------------------
// Milestone 6 — Docker + Health Checks
//
// What changed from M5:
//   - HealthChecker writes UP/DEGRADED/DOWN to /tmp/health every 5s
//   - Dashboard shows health status row (green/yellow/red)
//   - Dockerfile + docker-compose.yml added in docker/
//   - ENV overrides active: NUM_WORKERS, SCHEDULER_TYPE etc.
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
    // Step 1: Load config — fail fast
    // ------------------------------------------------------------------
    Config cfg;
    try {
        cfg = Config::load("config/config.json");
        cfg.applyEnvOverrides();  // ENV wins — Docker uses this
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << "\n";
        return 1;
    }
    cfg.print();

    // ------------------------------------------------------------------
    // Step 2: Wire up all components
    // ------------------------------------------------------------------
    auto scheduler = SchedulerFactory::create(cfg.scheduler_type);
    TaskBroker broker(cfg.queue_capacity, std::move(scheduler));

    WorkerPool pool(broker, cfg.num_workers,
                    cfg.cb_failure_threshold,
                    cfg.cb_recovery_timeout_s);

    Monitor     monitor(cfg.monitor_window_s);
    pool.addObserver(&monitor);

    // HealthChecker: writes status file Docker reads for HEALTHCHECK
    HealthChecker health(broker, pool,
                         cfg.health_status_file,
                         cfg.health_interval_s);

    // Pass health to Dashboard so it shows the status row
    CLIDashboard dashboard(monitor, broker, pool,
                           cfg.monitor_refresh_ms, &health);

    // ------------------------------------------------------------------
    // Step 3: Start background threads
    // Order matters: health first, then dashboard
    // ------------------------------------------------------------------
    health.start();
    dashboard.start();

    // ------------------------------------------------------------------
    // Phase 1: Normal tasks — Health shows UP (green)
    // ------------------------------------------------------------------
    for (int i = 1; i <= 20; ++i) {
        broker.enqueue(makeTask("normal", i, false));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // ------------------------------------------------------------------
    // Phase 2: Failure burst — Health shows DEGRADED (yellow)
    // Circuits trip → HealthChecker sees OPEN → writes "DEGRADED"
    // ------------------------------------------------------------------
    for (int i = 1; i <= 15; ++i) {
        broker.enqueue(makeTask("fail-burst", i, true));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // ------------------------------------------------------------------
    // Phase 3: Recovery — circuits close → Health back to UP
    // ------------------------------------------------------------------
    for (int i = 1; i <= 20; ++i) {
        broker.enqueue(makeTask("recovery", i, false));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    while (broker.size() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // ------------------------------------------------------------------
    // Graceful shutdown
    // ------------------------------------------------------------------
    health.stop();
    dashboard.stop();
    pool.shutdown(cfg.shutdown_timeout_ms);

    auto snap = monitor.snapshot();
    std::cout << "\n✅ Run complete.\n"
              << "   Completed : " << snap.total_completed << "\n"
              << "   Failed    : " << snap.total_failed    << "\n"
              << "   Avg ms    : " << snap.avg_latency_ms  << "\n";

    return 0;
}