#include <atomic>
#include <thread>
#include <chrono>
#include <functional>
#include <mutex>
#include <condition_variable>

#include <gtest/gtest.h>

using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
using namespace std::chrono_literals;

class CallbackScheduler
{
    struct Callback
    {
        std::function<void()> func;
        std::atomic<bool> cancelled{false};
    };

    std::multimap<TimePoint, std::shared_ptr<Callback>> callbacks;

    std::mutex mtx;
    std::condition_variable cv;
    std::thread workThread;

    std::atomic<bool> stop{false};

    void run()
    {
        while(!stop)
        {
            std::shared_ptr<Callback> cbPtr;
            TimePoint when;

            std::unique_lock lock(mtx);
            cv.wait(lock, [&] () { return stop.load() || !callbacks.empty();});
            if (stop.load())
            {
                return;
            }

            if (!callbacks.empty())
            {
                when = callbacks.begin()->first;
                cbPtr = callbacks.begin()->second;
                auto now = std::chrono::system_clock::now();

                if (when <= now)
                {
                    // Cancelled callbacks are "lazily" removed during processing:
                    // NOTE: they remain in the multimap until their scheduled time and still taking memory
                    callbacks.erase(callbacks.begin());
                    if (!cbPtr->cancelled.load())
                    {
                        lock.unlock();
                        try
                        {
                            cbPtr->func();
                        }
                        catch(const std::exception& e)
                        {
                            std::cerr <<  "Exception caught during callback execution: " << e.what() << std::endl;
                        }
                        catch(...)
                        {
                            std::cerr << "Exception caught during callback execution" << std::endl;
                        }
                        
                    }
                }
                else
                {
                    cv.wait_until(lock, when, [this, when]{ return stop.load() ||  (!callbacks.empty() && callbacks.begin()->first < when);});
                }
            }
        }
    }

public:
    CallbackScheduler()
    {
        workThread = std::thread(&CallbackScheduler::run, this);
    }

    ~CallbackScheduler()
    {
        stop.store(true);
        cv.notify_one();
        if (workThread.joinable())
        {
            workThread.join();
        }
    }

    std::shared_ptr<Callback> Schedule(std::function<void()> callback, TimePoint when)
    {
        auto cbPtr = std::make_shared<Callback>();
        cbPtr->func = std::move(callback);

        std::lock_guard<std::mutex> lock(mtx);
        callbacks.emplace(when, cbPtr);
        cv.notify_one();
        return cbPtr;
    }

    void Cancel(std::shared_ptr<Callback> cbPtr)
    {
        cbPtr->cancelled.store(true);
    }
};


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


int main()
{
    testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}