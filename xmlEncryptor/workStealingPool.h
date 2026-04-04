#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class WorkStealingPool {
   public:
    // Constructor - creates a thread pool with specified number of threads
    explicit WorkStealingPool(size_t numThreads);

    // Destructor - shuts down the pool and waits for all threads to finish
    ~WorkStealingPool();

    // Submits a task to the pool for execution
    void Submit(std::function<void()> task);

    // Waits for all tasks in all threads done
    void WaitAllTasksDone();

   private:
    // WorkerQueue structure holds tasks, synchronization primitives, and thread for each worker
    struct WorkerQueue {
        std::deque<std::function<void()>> tasks;  // Queue of pending tasks (LIFO for own queue)
        std::mutex mtx;                           // Mutex for protecting tasks access
        std::condition_variable cv;               // Condition variable for task notification
        std::jthread thr;                         // Worker thread

        WorkerQueue() = default;
        // Disable copy and move because mutex and condition_variable are not movable
        WorkerQueue(const WorkerQueue&) = delete;
        WorkerQueue& operator=(const WorkerQueue&) = delete;
        WorkerQueue(WorkerQueue&&) = delete;
        WorkerQueue& operator=(WorkerQueue&&) = delete;
    };

    std::vector<std::unique_ptr<WorkerQueue>> workers;  // Collection of worker queues
    std::atomic<size_t> submitIdx{0};                   // Round-robin index for task distribution

    std::atomic<int> activeTasks = 0;
    std::mutex allDoneMtx;
    std::condition_variable allDoneCv;

    // Tries to pop a task from own queue (LIFO - work stealing optimization)
    std::function<void()> tryPopOwn(size_t idx);

    // Tries to steal a task from other worker's queue (FIFO - work stealing)
    std::function<void()> trySteal(size_t ownIdx);
};
