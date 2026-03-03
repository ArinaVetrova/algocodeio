#include "callbackScheduler.h"

#include <gtest/gtest.h>

TEST(CallbackSchedulerTest, Simple)
{
    std::mutex m;
    std::condition_variable cv;
    bool done = false;
    auto callback = [&] {
        std::unique_lock lock(m);
        done = true;
        cv.notify_one();
    };

    CallbackScheduler scheduler;
    auto when = std::chrono::system_clock::now() + 1s;
    scheduler.Schedule(std::move(callback), when);

    {
        std::unique_lock lock(m);
        cv.wait(lock, [&] { return done; });
    }
}

TEST(CallbackSchedulerTest, MultipleCallbacksInOrder)
{
    std::mutex m;
    std::condition_variable cv;
    std::vector<int> results;

    CallbackScheduler scheduler;

    auto now = std::chrono::system_clock::now();

    scheduler.Schedule([&]{
        std::unique_lock lock(m);
        results.push_back(1);
        cv.notify_one();
    }, now + 10ms);

    scheduler.Schedule([&]{
        std::unique_lock lock(m);
        results.push_back(2);
        cv.notify_one();
    }, now + 20ms);

    scheduler.Schedule([&]{
        std::unique_lock lock(m);
        results.push_back(3);
        cv.notify_one();
    }, now + 30ms);

    std::unique_lock lock(m);
    cv.wait_for(lock, 100ms, [&]{ return results.size() == 3; });

    EXPECT_EQ(results.size(), 3);
    EXPECT_EQ(results[0], 1);
    EXPECT_EQ(results[1], 2);
    EXPECT_EQ(results[2], 3);
}

TEST(CallbackSchedulerTest, EarlyCallbackAddedLater)
{
    std::mutex m;
    std::condition_variable cv;
    std::vector<int> results;

    CallbackScheduler scheduler;

    auto now = std::chrono::system_clock::now();

    scheduler.Schedule([&]{
        std::unique_lock lock(m);
        results.push_back(2);
        cv.notify_one();
    }, now + 50ms);

    std::this_thread::sleep_for(10ms);

    scheduler.Schedule([&]{
        std::unique_lock lock(m);
        results.push_back(1);
        cv.notify_one();
    }, now + 20ms);

    std::unique_lock lock(m);
    cv.wait_for(lock, 200ms, [&]{ return results.size() == 2; });

    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results[0], 1);
    EXPECT_EQ(results[1], 2);
}

TEST(CallbackSchedulerTest, CancelledCallbackIsNotExecuted)
{
    std::mutex m;
    std::condition_variable cv;
    bool called = false;

    CallbackScheduler scheduler;
    auto now = std::chrono::system_clock::now();

    auto handle = scheduler.Schedule([&] {
        std::unique_lock lock(m);
        called = true;
        cv.notify_one();
    }, now + 50ms);

    std::this_thread::sleep_for(10ms);

    scheduler.Cancel(handle);

    std::unique_lock lock(m);
    bool wasCalled = cv.wait_for(lock, 100ms, [&] { return called; });

    EXPECT_FALSE(wasCalled);
    EXPECT_FALSE(called);
}

TEST(CallbackSchedulerTest, MixedCancelledAndNormalCallbacks)
{
    std::mutex m;
    std::condition_variable cv;
    std::vector<int> results;

    CallbackScheduler scheduler;
    auto now = std::chrono::system_clock::now();

    auto h1 = scheduler.Schedule([&] {
        std::unique_lock lock(m);
        results.push_back(1);
        cv.notify_one();
    }, now + 10ms);

    auto h2 = scheduler.Schedule([&] {
        std::unique_lock lock(m);
        results.push_back(2);
        cv.notify_one();
    }, now + 20ms);

    auto h3 = scheduler.Schedule([&] {
        std::unique_lock lock(m);
        results.push_back(3);
        cv.notify_one();
    }, now + 30ms);

    std::this_thread::sleep_for(5ms);
    scheduler.Cancel(h2);

    std::unique_lock lock(m);
    cv.wait_for(lock, 200ms, [&] { return results.size() == 2; });

    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0], 1);
    EXPECT_EQ(results[1], 3);
}
