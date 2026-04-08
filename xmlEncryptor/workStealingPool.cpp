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
                    std::unique_lock lock(idleMtx);
                    idleCv.wait(lock, [&] {
                        if (token.stop_requested()) return true;
                        if (!workers[i]->tasks.empty()) return true;
                        // check if there is any task to steal
                        for (size_t j = 0; j < workers.size(); ++j) {
                            if (j == i) continue;
                            // try_to_lock because if mutex already locked - other thread is doing
                            // the job
                            std::unique_lock otherLock(workers[j]->mtx, std::try_to_lock);
                            if (otherLock && !workers[j]->tasks.empty()) return true;
                        }
                        return false;
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

    idleCv.notify_all();
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
    idleCv.notify_one();
}

void WorkStealingPool::SubmitToWorker(size_t workerIdx, std::function<void()> task) {
    activeTasks.fetch_add(1);
    std::lock_guard lock(workers[workerIdx]->mtx);
    workers[workerIdx]->tasks.push_back([this, task = std::move(task)]() mutable {
        task();
        if (activeTasks.fetch_sub(1) == 1)
        {
            std::lock_guard lock(allDoneMtx);
            allDoneCv.notify_all();
        }
    });
    idleCv.notify_one();
}

void WorkStealingPool::WaitAllTasksDone() {
    std::unique_lock lock(allDoneMtx);
    allDoneCv.wait(lock, [&] { return activeTasks.load() == 0; });
}
