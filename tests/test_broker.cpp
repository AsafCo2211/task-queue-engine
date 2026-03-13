#include <gtest/gtest.h>
#include "TaskBroker.hpp"
#include "FifoScheduler.hpp"

#include <thread>
#include <vector>
#include <atomic>

// -----------------------------------------------------------------------
// Helper — builds a minimal valid Task
// -----------------------------------------------------------------------
static Task makeTask(int id) {
    Task t;
    t.id      = id;
    t.name    = "test-task-" + std::to_string(id);
    t.payload = [](){};   // no-op payload
    return t;
}

// ═══════════════════════════════════════════════════════════════════════
// Basic enqueue / dequeue
// ═══════════════════════════════════════════════════════════════════════

TEST(BrokerTest, EnqueueIncreasesSize) {
    TaskBroker broker(10);
    EXPECT_EQ(broker.size(), 0u);

    broker.enqueue(makeTask(1));
    EXPECT_EQ(broker.size(), 1u);

    broker.enqueue(makeTask(2));
    EXPECT_EQ(broker.size(), 2u);
}

TEST(BrokerTest, DequeueReturnsTasks) {
    TaskBroker broker(10);
    broker.enqueue(makeTask(42));

    auto result = broker.dequeue();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->id, 42);
}

TEST(BrokerTest, DequeueDecreasesSize) {
    TaskBroker broker(10);
    broker.enqueue(makeTask(1));
    broker.enqueue(makeTask(2));

    broker.dequeue();
    EXPECT_EQ(broker.size(), 1u);
}

// ═══════════════════════════════════════════════════════════════════════
// Backpressure — capacity enforcement
// ═══════════════════════════════════════════════════════════════════════

TEST(BrokerTest, RejectsWhenFull) {
    TaskBroker broker(3);   // capacity of 3

    EXPECT_TRUE(broker.enqueue(makeTask(1)));
    EXPECT_TRUE(broker.enqueue(makeTask(2)));
    EXPECT_TRUE(broker.enqueue(makeTask(3)));
    EXPECT_FALSE(broker.enqueue(makeTask(4)));  // ← should be rejected
    EXPECT_EQ(broker.size(), 3u);
}

TEST(BrokerTest, AcceptsAfterDequeue) {
    TaskBroker broker(2);

    broker.enqueue(makeTask(1));
    broker.enqueue(makeTask(2));
    EXPECT_FALSE(broker.enqueue(makeTask(3)));  // full

    broker.dequeue();                           // make room
    EXPECT_TRUE(broker.enqueue(makeTask(3)));   // now fits
}

// ═══════════════════════════════════════════════════════════════════════
// Shutdown behaviour
// ═══════════════════════════════════════════════════════════════════════

TEST(BrokerTest, RejectsEnqueueAfterShutdown) {
    TaskBroker broker(10);
    broker.shutdown();
    EXPECT_FALSE(broker.enqueue(makeTask(1)));
}

TEST(BrokerTest, DequeueReturnsNulloptAfterShutdownAndDrain) {
    TaskBroker broker(10);

    // Enqueue one task, then shut down
    broker.enqueue(makeTask(1));
    broker.shutdown();

    // First dequeue gets the task
    auto first = broker.dequeue();
    EXPECT_TRUE(first.has_value());

    // Second dequeue: queue empty + shutdown → nullopt
    auto second = broker.dequeue();
    EXPECT_FALSE(second.has_value());
}

TEST(BrokerTest, IsShutdownReflectsState) {
    TaskBroker broker(10);
    EXPECT_FALSE(broker.isShutdown());
    broker.shutdown();
    EXPECT_TRUE(broker.isShutdown());
}

// ═══════════════════════════════════════════════════════════════════════
// Thread safety — concurrent producers
// ═══════════════════════════════════════════════════════════════════════

TEST(BrokerTest, ConcurrentEnqueueIsSafe) {
    TaskBroker broker(10000);
    std::atomic<int> id_counter {1};
    std::vector<std::thread> producers;

    // 5 threads, 100 tasks each = 500 total
    for (int t = 0; t < 5; ++t) {
        producers.emplace_back([&]() {
            for (int i = 0; i < 100; ++i) {
                Task task = makeTask(id_counter.fetch_add(1));
                broker.enqueue(std::move(task));
            }
        });
    }
    for (auto& p : producers) p.join();

    EXPECT_EQ(broker.size(), 500u);
}