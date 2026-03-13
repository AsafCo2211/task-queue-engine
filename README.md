# Distributed Task Queue Engine

> C++20 · Docker · CLI Dashboard · Design Patterns · Microservices Architecture

A production-inspired task queue engine built from scratch to demonstrate
core software architecture skills: concurrency, design patterns, observability,
and operational readiness.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                       TaskQueueEngine                           │
│                                                                 │
│  [Producer A] ─────┐                ┌──▶ [Worker 1]            │
│  [Producer B] ─────┼──▶ [Broker] ───┼──▶ [Worker 2]            │
│  [Producer C] ─────┘                └──▶ [Worker 3]            │
│                                                                 │
│              [Config]   [Monitor / CLI Dashboard]               │
└─────────────────────────────────────────────────────────────────┘
```

### Data Flow — Task Lifecycle

```
1. Producer::submit(task)
       ↓
2. Broker::enqueue(task)   ← pushes to queue, wakes sleeping worker
       ↓
3. Scheduler::next()       ← decides which task goes first (M3)
       ↓
4. Worker::execute(task)   ← runs on dedicated thread
       ↓
5. Monitor::onComplete()   ← worker reports result (M4)
       ↓
6. CLI Dashboard           ← live stats refresh (M4)
```

---

## Design Patterns

| Pattern           | Where used                         | Milestone |
|-------------------|------------------------------------|-----------|
| Producer/Consumer | TaskBroker + WorkerPool            | M1        |
| Strategy          | IScheduler (FIFO / Priority / RR)  | M3        |
| Observer          | Worker → Monitor                   | M4        |
| Circuit Breaker   | Per-worker failure protection      | M5        |

---

## Component Responsibilities

| Component    | Responsibility                                              |
|--------------|-------------------------------------------------------------|
| Task         | Unit of work: id, priority, payload (std::function), status |
| TaskBroker   | Thread-safe queue with backpressure and graceful shutdown   |
| Worker       | Background thread: dequeue → execute → report              |
| WorkerPool   | RAII lifecycle management for N workers                     |
| Config       | Loads config.json, validates, supports ENV overrides        |
| IScheduler   | Strategy interface: FIFO / Priority / RoundRobin (M3)       |
| Monitor      | Collects metrics via Observer pattern (M4)                  |
| CLIDashboard | Live terminal UI refreshing every second (M4)               |
| CircuitBreaker | CLOSED/OPEN/HALF_OPEN state machine per worker (M5)       |
| HealthChecker | Writes UP/DEGRADED/DOWN to /tmp/health for Docker (M6)    |

---

## Key Design Decisions

**`std::function<void()>` as Task payload**
Type erasure — the broker stores tasks without knowing what's inside.
Any callable (lambda, functor, free function) fits in the same queue.

**`mutex` + `condition_variable` (not lock-free)**
Workers sleep instead of busy-waiting. `notify_one()` wakes exactly
one worker when work arrives. `notify_all()` wakes everyone on shutdown.
Lock-free queues are complex to implement correctly — over-engineering
for a portfolio project.

**`unique_ptr<Worker>` in WorkerPool**
Worker is non-movable (holds a reference + atomic). Storing pointers
gives stable addresses — the Worker never moves in memory while its
thread is alive.

**Config-driven, ENV-overridable**
No hardcoded values anywhere. JSON is the base; ENV variables override
for Docker deployments without recompile. Follows 12-Factor App methodology.

**RAII everywhere**
Worker starts its thread in the constructor, joins in the destructor.
WorkerPool's destructor calls shutdown() as a safety net.
Resources cannot leak — scope-based lifetime management.

---

## Build

```bash
mkdir build && cd build
cmake ..        # downloads nlohmann/json via FetchContent
make
cd ..
./build/TaskQueue
```

## Configuration

All parameters come from `config/config.json`:

```json
{
    "num_workers": 4,
    "queue_capacity": 1000,
    "scheduler_type": "priority",
    "task_timeout_ms": 5000,
    "shutdown_timeout_ms": 10000
}
```

Override with ENV variables (Docker):
```bash
NUM_WORKERS=8 SCHEDULER_TYPE=fifo ./build/TaskQueue
```

## Docker

```bash
docker-compose up   # coming in Milestone 6
```

---

## Project Status

| Milestone | Description                        | Status      |
|-----------|------------------------------------|-------------|
| 0         | Project setup & scaffold           | ✅ Done     |
| 1         | Core structures + Broker           | ✅ Done     |
| 2         | Config + WorkerPool                | ✅ Done     |
| 3         | Strategy: Schedulers               | 🔜 Next     |
| 4         | Observer: Monitor + CLI Dashboard  | ⬜          |
| 5         | Circuit Breaker                    | ⬜          |
| 6         | Docker + Health Checks             | ⬜          |
| 7         | Tests + CI + Documentation         | ⬜          |

---

## Interview Talking Points

> *"I built a distributed task queue engine in C++20 from scratch.
> The design is based on four patterns: Producer/Consumer for decoupling,
> Strategy for swappable scheduling algorithms, Observer for metrics
> collection without changing workers, and Circuit Breaker to prevent
> cascading failures. The system starts with a single docker-compose
> command and displays a live CLI dashboard. It's covered by 30 unit
> tests with CI on GitHub Actions."*

| Interview Question | This project answers it because... |
|--------------------|-------------------------------------|
| "Explain Producer/Consumer" | Implemented from scratch with CV, mutex, shutdown |
| "What is RAII?" | Worker and WorkerPool both demonstrate it |
| "mutex vs condition_variable?" | mutex protects data. CV lets threads wait for events |
| "What is backpressure?" | enqueue() returns false when queue is full |
| "What is graceful shutdown?" | WorkerPool waits for in-flight tasks before joining |
| "Why unique_ptr here?" | Non-movable types need stable addresses |