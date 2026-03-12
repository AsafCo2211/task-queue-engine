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
│  [Producer A] ─────┐                ┌──▶ [Worker 1]             │
│  [Producer B] ─────┼──▶ [Broker] ───┼──▶ [Worker 2]             │
│  [Producer C] ─────┘                └──▶ [Worker 3]             │
│                                                                 │
│                     [Monitor / CLI Dashboard]                   │
└─────────────────────────────────────────────────────────────────┘
```

## Design Patterns

| Pattern          | Where used                          |
|------------------|-------------------------------------|
| Producer/Consumer | TaskBroker + WorkerPool            |
| Strategy         | IScheduler (FIFO / Priority / RR)   |
| Observer         | Worker → Monitor                    |
| Circuit Breaker  | Per-worker failure protection       |

## Build

```bash
mkdir build && cd build
cmake ..
make
./TaskQueue
```

## Docker

```bash
docker-compose up
```

## Project Status

| Milestone | Description                        | Status  |
|-----------|------------------------------------|---------|
| 0         | Project setup & scaffold           | ✅ Done |
| 1         | Core structures + Broker           | 🔜 Next |
| 2         | Config + WorkerPool                | ⬜      |
| 3         | Strategy: Schedulers               | ⬜      |
| 4         | Observer: Monitor + CLI Dashboard  | ⬜      |
| 5         | Circuit Breaker                    | ⬜      |
| 6         | Docker + Health Checks             | ⬜      |
| 7         | Tests + CI + Documentation         | ⬜      |