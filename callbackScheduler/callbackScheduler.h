#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <map>

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
