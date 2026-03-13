#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <stdexcept>
#include <fstream>
#include <iostream>

// -----------------------------------------------------------------------
// Config — loads and validates config/config.json
//
// Design decisions:
//
// 1. Plain struct with static load() factory method.
//    - No inheritance, no virtual methods — Config is a value type.
//    - load() throws on bad input so the caller can decide how to handle it.
//
// 2. All fields have sensible defaults.
//    - If a key is missing from JSON, we fall back to the default.
//    - Only clearly invalid values (e.g. 0 workers) throw an exception.
//
// 3. We use nlohmann::json's value() method:
//    json.value("key", default) → returns default if key is absent.
//    This avoids KeyNotFound exceptions for optional config keys.
//
// Usage:
//   Config cfg = Config::load("config/config.json");
//   std::cout << cfg.num_workers;
// -----------------------------------------------------------------------
struct Config {
    // --- worker pool ---
    int         num_workers          {4};
    std::size_t queue_capacity       {1000};
    int         task_timeout_ms      {5000};
    int         shutdown_timeout_ms  {10000};

    // --- scheduler ---
    std::string scheduler_type       {"fifo"};

    // --- circuit breaker ---
    int         cb_failure_threshold {5};
    int         cb_recovery_timeout_s{30};

    // --- monitor ---
    int         monitor_refresh_ms   {1000};
    int         monitor_window_s     {30};

    // --- health check ---
    int         health_interval_s    {5};
    std::string health_status_file   {"/tmp/health"};

    // ------------------------------------------------------------------
    // load — reads a JSON file and returns a validated Config
    //
    // Throws std::runtime_error if:
    //   - file cannot be opened
    //   - JSON is malformed
    //   - values fail validation (e.g. num_workers < 1)
    // ------------------------------------------------------------------
    static Config load(const std::string& path) {
        // Open the file
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Config: cannot open file: " + path);
        }

        // Parse JSON — throws nlohmann::json::parse_error on bad JSON
        nlohmann::json j;
        try {
            file >> j;
        } catch (const nlohmann::json::parse_error& e) {
            throw std::runtime_error(
                std::string("Config: JSON parse error: ") + e.what());
        }

        // Fill struct from JSON (with defaults for missing keys)
        Config cfg;
        cfg.num_workers           = j.value("num_workers",           cfg.num_workers);
        cfg.queue_capacity        = j.value("queue_capacity",         cfg.queue_capacity);
        cfg.task_timeout_ms       = j.value("task_timeout_ms",        cfg.task_timeout_ms);
        cfg.shutdown_timeout_ms   = j.value("shutdown_timeout_ms",    cfg.shutdown_timeout_ms);
        cfg.scheduler_type        = j.value("scheduler_type",         cfg.scheduler_type);

        // Nested objects — use contains() to guard before descending
        if (j.contains("circuit_breaker")) {
            auto& cb = j["circuit_breaker"];
            cfg.cb_failure_threshold  = cb.value("failure_threshold",  cfg.cb_failure_threshold);
            cfg.cb_recovery_timeout_s = cb.value("recovery_timeout_s", cfg.cb_recovery_timeout_s);
        }
        if (j.contains("monitor")) {
            auto& m = j["monitor"];
            cfg.monitor_refresh_ms = m.value("refresh_interval_ms", cfg.monitor_refresh_ms);
            cfg.monitor_window_s   = m.value("window_size_s",        cfg.monitor_window_s);
        }
        if (j.contains("health_check")) {
            auto& h = j["health_check"];
            cfg.health_interval_s   = h.value("interval_s",   cfg.health_interval_s);
            cfg.health_status_file  = h.value("status_file",  cfg.health_status_file);
        }

        // Validate — throw early with a clear message rather than
        // letting the engine crash mysteriously later
        cfg.validate();
        return cfg;
    }

    // ------------------------------------------------------------------
    // ENV overrides — Docker sets these to tune behaviour without rebuild
    //
    // Called after load() so JSON values are the base and ENV wins.
    // (Used in Milestone 6 when we add docker-compose.)
    // ------------------------------------------------------------------
    void applyEnvOverrides() {
        if (const char* v = std::getenv("NUM_WORKERS"))     num_workers    = std::stoi(v);
        if (const char* v = std::getenv("SCHEDULER_TYPE"))  scheduler_type = v;
        if (const char* v = std::getenv("QUEUE_CAPACITY"))  queue_capacity = std::stoull(v);
        if (const char* v = std::getenv("TASK_TIMEOUT_MS")) task_timeout_ms= std::stoi(v);
        validate();  // re-validate after overrides
    }

    // ------------------------------------------------------------------
    // print — human-readable summary for startup logging
    // ------------------------------------------------------------------
    void print() const {
        std::cout << "--- Config ---\n"
                  << "  num_workers:         " << num_workers          << "\n"
                  << "  queue_capacity:      " << queue_capacity       << "\n"
                  << "  scheduler_type:      " << scheduler_type       << "\n"
                  << "  task_timeout_ms:     " << task_timeout_ms      << "\n"
                  << "  shutdown_timeout_ms: " << shutdown_timeout_ms  << "\n"
                  << "  cb_failure_threshold:" << cb_failure_threshold << "\n"
                  << "--------------\n";
    }

private:
    void validate() const {
        if (num_workers < 1)
            throw std::runtime_error("Config: num_workers must be >= 1");
        if (queue_capacity < 1)
            throw std::runtime_error("Config: queue_capacity must be >= 1");
        if (task_timeout_ms < 0)
            throw std::runtime_error("Config: task_timeout_ms must be >= 0");
        if (scheduler_type != "fifo" &&
            scheduler_type != "priority" &&
            scheduler_type != "round_robin") {
            throw std::runtime_error(
                "Config: unknown scheduler_type: " + scheduler_type);
        }
    }
};