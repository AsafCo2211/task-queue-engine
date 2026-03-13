#include <gtest/gtest.h>
#include "WorkerPool.hpp"
#include "TaskBroker.hpp"
#include "IObserver.hpp"
#include "TaskResult.hpp"

#include <atomic>
#include <thread>
#include <chrono>

// -----------------------------------------------------------------------
// CountingObserver — test double that counts completed tasks
// -----------------------------------------------------------------------
class CountingObserver : public IObserver {
public:
    void onTaskComplete(const TaskResult& result) override {
        if (result.success) ++completed;
        else                ++failed;
    }
    std::atomic<int> completed {0};
    std::atomic<int> failed    {0};
};

static Task makeTask(int id, bool fail = false) {
    Task t;
    t.id      = id;
    t.name    = "task-" + std::to_string(id);
    t.payload = [fail]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (fail) throw std::runtime_error("forced failure");
    };
    return t;
}

// ═══════════════════════════════════════════════════════════════════════
// Basic spawn and shutdown
// ═══════════════════════════════════════════════════════════════════════

TEST(WorkerPoolTest, SpawnsCorrectNumberOfWorkers) {
    TaskBroker broker(100);
    WorkerPool pool(broker, 4);

    EXPECT_EQ(pool.totalCount(), 4);

    pool.shutdown(2000);
}

TEST(WorkerPoolTest, ActiveCountStartsAtZero) {
    TaskBroker broker(100);
    WorkerPool pool(broker, 3);

    // No tasks submitted — workers are sleeping
    EXPECT_EQ(pool.activeCount(), 0);

    pool.shutdown(2000);
}

// ═══════════════════════════════════════════════════════════════════════
// Task execution
// ═══════════════════════════════════════════════════════════════════════

TEST(WorkerPoolTest, ExecutesAllSubmittedTasks) {
    TaskBroker broker(100);
    WorkerPool pool(broker, 3);

    CountingObserver obs;
    pool.addObserver(&obs);

    // Submit 20 tasks
    for (int i = 1; i <= 20; ++i) {
        broker.enqueue(makeTask(i));
    }

    // Wait for all to complete
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::seconds(5);
    while (obs.completed + obs.failed < 20 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    pool.shutdown(2000);

    EXPECT_EQ(obs.completed.load(), 20);
    EXPECT_EQ(obs.failed.load(), 0);
}

TEST(WorkerPoolTest, CountsFailedTasksSeparately) {
    TaskBroker broker(100);
    WorkerPool pool(broker, 2, 10, 1);  // high threshold so CB doesn't open

    CountingObserver obs;
    pool.addObserver(&obs);

    // 5 normal + 5 failing
    for (int i = 1; i <= 5; ++i) broker.enqueue(makeTask(i, false));
    for (int i = 6; i <= 10; ++i) broker.enqueue(makeTask(i, true));

    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::seconds(5);
    while (obs.completed + obs.failed < 10 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    pool.shutdown(2000);

    EXPECT_EQ(obs.completed.load(), 5);
    EXPECT_EQ(obs.failed.load(),    5);
}

// ═══════════════════════════════════════════════════════════════════════
// Graceful shutdown
// ═══════════════════════════════════════════════════════════════════════

TEST(WorkerPoolTest, ShutdownIsIdempotent) {
    TaskBroker broker(100);
    WorkerPool pool(broker, 2);

    // Calling shutdown twice should not crash
    pool.shutdown(1000);
    pool.shutdown(1000);   // second call — no-op
}

TEST(WorkerPoolTest, DestructorCallsShutdownAutomatically) {
    TaskBroker broker(100);

    {
        WorkerPool pool(broker, 2);
        broker.enqueue(makeTask(1));
        // pool goes out of scope here → destructor → shutdown
    }

    // If we get here without hanging, destructor worked correctly
    SUCCEED();
}

// ═══════════════════════════════════════════════════════════════════════
// Observer registration
// ═══════════════════════════════════════════════════════════════════════

TEST(WorkerPoolTest, MultipleObserversAllReceiveEvents) {
    TaskBroker broker(100);
    WorkerPool pool(broker, 2, 10, 1);

    CountingObserver obs1, obs2;
    pool.addObserver(&obs1);
    pool.addObserver(&obs2);

    broker.enqueue(makeTask(1));
    broker.enqueue(makeTask(2));

    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::seconds(3);
    while (obs1.completed < 2 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    pool.shutdown(2000);

    // Both observers should have received the same events
    EXPECT_EQ(obs1.completed.load(), 2);
    EXPECT_EQ(obs2.completed.load(), 2);
}