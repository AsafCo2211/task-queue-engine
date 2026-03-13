#include <gtest/gtest.h>
#include "FifoScheduler.hpp"
#include "PriorityScheduler.hpp"
#include "RoundRobinScheduler.hpp"
#include "SchedulerFactory.hpp"

#include <deque>

// -----------------------------------------------------------------------
// Helper — builds a Task with a specific id and priority
// -----------------------------------------------------------------------
static Task makeTask(int id, int priority = 0) {
    Task t;
    t.id       = id;
    t.priority = priority;
    t.name     = "task-" + std::to_string(id);
    t.payload  = [](){};
    return t;
}

static std::deque<Task> makeTasks(std::vector<std::pair<int,int>> id_prio) {
    std::deque<Task> q;
    for (auto [id, prio] : id_prio) {
        q.push_back(makeTask(id, prio));
    }
    return q;
}

// ═══════════════════════════════════════════════════════════════════════
// FifoScheduler
// ═══════════════════════════════════════════════════════════════════════

TEST(FifoSchedulerTest, ReturnsFirstTask) {
    FifoScheduler s;
    auto q = makeTasks({{1,0},{2,0},{3,0}});

    auto t = s.next(q);
    EXPECT_EQ(t.id, 1);     // first in → first out
    EXPECT_EQ(q.size(), 2u);
}

TEST(FifoSchedulerTest, MaintainsInsertionOrder) {
    FifoScheduler s;
    auto q = makeTasks({{10,0},{20,0},{30,0}});

    EXPECT_EQ(s.next(q).id, 10);
    EXPECT_EQ(s.next(q).id, 20);
    EXPECT_EQ(s.next(q).id, 30);
}

TEST(FifoSchedulerTest, IgnoresPriority) {
    FifoScheduler s;
    // High priority task submitted AFTER low priority → still goes second
    auto q = makeTasks({{1, 0}, {2, 99}});

    EXPECT_EQ(s.next(q).id, 1);   // order, not priority
    EXPECT_EQ(s.next(q).id, 2);
}

// ═══════════════════════════════════════════════════════════════════════
// PriorityScheduler
// ═══════════════════════════════════════════════════════════════════════

TEST(PrioritySchedulerTest, ReturnsHighestPriority) {
    PriorityScheduler s;
    auto q = makeTasks({{1,1},{2,9},{3,3}});

    auto t = s.next(q);
    EXPECT_EQ(t.id, 2);      // highest priority = id 2 (prio 9)
    EXPECT_EQ(q.size(), 2u);
}

TEST(PrioritySchedulerTest, CorrectOrderAcrossMultipleCalls) {
    PriorityScheduler s;
    auto q = makeTasks({{1,5},{2,1},{3,9},{4,3}});

    EXPECT_EQ(s.next(q).id, 3);   // prio 9
    EXPECT_EQ(s.next(q).id, 1);   // prio 5
    EXPECT_EQ(s.next(q).id, 4);   // prio 3
    EXPECT_EQ(s.next(q).id, 2);   // prio 1
}

TEST(PrioritySchedulerTest, HandlesTiedPriorities) {
    PriorityScheduler s;
    // Both have same priority — should not crash, one of them returned
    auto q = makeTasks({{1,5},{2,5}});
    auto t = s.next(q);
    EXPECT_TRUE(t.id == 1 || t.id == 2);
    EXPECT_EQ(q.size(), 1u);
}

// ═══════════════════════════════════════════════════════════════════════
// RoundRobinScheduler
// ═══════════════════════════════════════════════════════════════════════

TEST(RoundRobinSchedulerTest, CyclesThroughTasks) {
    RoundRobinScheduler s;
    auto q = makeTasks({{1,0},{2,0},{3,0}});

    // counter starts at 0 → picks index 0, then 1, then 2
    int first  = s.next(q).id;   // index 0 of [1,2,3] = 1
    int second = s.next(q).id;   // index 1 of [2,3]   = 3
    int third  = s.next(q).id;   // index 0 of [2]     = 2

    // All three unique tasks were returned
    EXPECT_NE(first, second);
    EXPECT_NE(second, third);
    EXPECT_NE(first, third);
}

TEST(RoundRobinSchedulerTest, RemovesTaskFromQueue) {
    RoundRobinScheduler s;
    auto q = makeTasks({{1,0},{2,0}});
    s.next(q);
    EXPECT_EQ(q.size(), 1u);
}

// ═══════════════════════════════════════════════════════════════════════
// SchedulerFactory
// ═══════════════════════════════════════════════════════════════════════

TEST(SchedulerFactoryTest, CreatesFifo) {
    auto s = SchedulerFactory::create("fifo");
    EXPECT_STREQ(s->name(), "FIFO");
}

TEST(SchedulerFactoryTest, CreatesPriority) {
    auto s = SchedulerFactory::create("priority");
    EXPECT_STREQ(s->name(), "Priority");
}

TEST(SchedulerFactoryTest, CreatesRoundRobin) {
    auto s = SchedulerFactory::create("round_robin");
    EXPECT_STREQ(s->name(), "RoundRobin");
}

TEST(SchedulerFactoryTest, ThrowsOnUnknownType) {
    EXPECT_THROW(SchedulerFactory::create("unknown"), std::runtime_error);
}