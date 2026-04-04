#include "workStealingPool.h"

#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

// Attempts to pop a task from the worker's own queue (LIFO)
std::function<void()> WorkStealingPool::tryPopOwn(size_t idx) {
    std::lock_guard lock(workers[idx]->mtx);

    if (workers[idx]->tasks.empty()) return nullptr;

    // Pop from back for LIFO behavior (better cache locality for own tasks)
    auto task = std::move(workers[idx]->tasks.back());
    workers[idx]->tasks.pop_back();
    return task;
}

// Attempts to steal a task from other worker queues (FIFO)
std::function<void()> WorkStealingPool::trySteal(size_t ownIdx) {
    // Iterate through all other workers to find work to steal
    for (size_t i = 0; i < workers.size(); ++i) {
        if (i == ownIdx) continue;  // Skip own queue

        std::lock_guard lock(workers[i]->mtx);

        if (workers[i]->tasks.empty()) continue;

        // Pop from front for FIFO behavior when stealing
        auto task = std::move(workers[i]->tasks.front());
        workers[i]->tasks.pop_front();
        return task;
    }
    return nullptr;
}

// Constructor - creates and starts all worker threads
WorkStealingPool::WorkStealingPool(size_t numThreads) {
    workers.reserve(numThreads);

    for (size_t i = 0; i < numThreads; ++i) {
        workers.push_back(std::make_unique<WorkerQueue>());
        auto& worker = *workers[i];

        // Create worker thread with stop_token for cooperative cancellation
        worker.thr = std::jthread([this, i](std::stop_token token) {
            // Continue running until stop is requested
            while (!token.stop_requested()) {
                // Try to get work: first from own queue, then steal from others
                auto task = tryPopOwn(i);

                if (!task) task = trySteal(i);

                if (task) {
                    task();
                } else {
                    // No work available - wait for notification
                    std::unique_lock lock(workers[i]->mtx);
                    workers[i]->cv.wait(lock, [&] {
                        // Wake up if: tasks available or stop requested
                        return !workers[i]->tasks.empty() || token.stop_requested();
                    });
                }
            }
        });
    }
}

WorkStealingPool::~WorkStealingPool() {
    // request_stop for each thread + notify to wake up sleeping threads
    for (auto& worker : workers) {
        worker->thr.request_stop();
    }
    for (auto& worker : workers) {
        worker->cv.notify_all();
    }
}

void WorkStealingPool::Submit(std::function<void()> task) {
    activeTasks.fetch_add(1);
    size_t idx = submitIdx.fetch_add(1) % workers.size();
    {
        std::lock_guard lock(workers[idx]->mtx);
        workers[idx]->tasks.push_back([this, task = std::move(task)]() mutable {
            task();
            if (activeTasks.fetch_sub(1) == 1) {
                allDoneCv.notify_all();  // it was last task — wake up WaitAlltasksDone
            }
        });
    }
    for (auto& w : workers) w->cv.notify_one();
}

void WorkStealingPool::WaitAllTasksDone() {
    std::unique_lock lock(allDoneMtx);
    allDoneCv.wait(lock, [&] { return activeTasks.load() == 0; });
}
