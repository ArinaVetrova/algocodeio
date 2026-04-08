#include "workStealingPool.h"

#include <gtest/gtest.h>
#include <atomic>
#include <vector>

TEST(WorkStealingPoolTest, SingleTaskExecuted) {
    WorkStealingPool pool(2);
    std::atomic<bool> executed = false;

    pool.Submit([&] { executed = true; });

    pool.WaitAllTasksDone();
    EXPECT_TRUE(executed);
}

TEST(WorkStealingPoolTest, AllTasksExecuted) {
    WorkStealingPool pool(4);
    constexpr int taskCount = 100;
    std::atomic<int> counter = 0;

    for (int i = 0; i < taskCount; ++i) {
        pool.Submit([&] { counter.fetch_add(1); });
    }

    pool.WaitAllTasksDone();
    EXPECT_EQ(counter.load(), taskCount);
}

TEST(WorkStealingPoolTest, TasksExecutedConcurrently) {
    WorkStealingPool pool(4);
    std::atomic<int> concurrentCount = 0;
    std::atomic<int> maxConcurrent = 0;

    constexpr int taskCount = 8;
    for (int i = 0; i < taskCount; ++i) {
        pool.Submit([&] {
            int current = concurrentCount.fetch_add(1) + 1;
            int expected = maxConcurrent.load();
            while (current > expected &&
                   !maxConcurrent.compare_exchange_weak(expected, current));
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            concurrentCount.fetch_sub(1);
        });
    }

    pool.WaitAllTasksDone();
    EXPECT_GT(maxConcurrent.load(), 1);
}

TEST(WorkStealingPoolTest, SingleThread) {
    WorkStealingPool pool(1);
    std::atomic<int> counter = 0;
    constexpr int taskCount = 10;

    for (int i = 0; i < taskCount; ++i) {
        pool.Submit([&] { counter.fetch_add(1); });
    }

    pool.WaitAllTasksDone();
    EXPECT_EQ(counter.load(), taskCount);
}

TEST(WorkStealingPoolTest, TasksWithDependencies) {
    WorkStealingPool pool(4);
    std::atomic<int> counter = 0;

    pool.Submit([&] {
        counter.fetch_add(1);
        pool.Submit([&] {
            counter.fetch_add(1);
            pool.Submit([&] {
                counter.fetch_add(1);
            });
        });
    });

    pool.WaitAllTasksDone();
    EXPECT_EQ(counter.load(), 3);
}

TEST(WorkStealingPoolTest, WorkStealingHappens) {
    WorkStealingPool pool(4);
    std::atomic<int> counter = 0;
    constexpr int taskCount = 40;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < taskCount; ++i) {
        pool.SubmitToWorker(0, [&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            counter.fetch_add(1);
        });
    }

    pool.WaitAllTasksDone();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    EXPECT_EQ(counter.load(), taskCount);
    // without stealing: 40 tasks * 10ms = 400ms with one thread
    // with stealing: 4 threads takes 40 tasks → ~100ms
    EXPECT_LT(elapsed, 400);
}

