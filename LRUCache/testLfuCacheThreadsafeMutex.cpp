#include "TLfuCacheThreadsafeMutex.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

// deterministic LFU eviction, single-threaded
TEST(TLfuCacheMtx, EvictDeterministic) {
    TLfuCacheMtx<char,int> cache(2);

    cache.Set('a', 1);
    cache.Set('b', 2);

    cache.TryGet('a');      // freq a = 1
    cache.TryGet('b');      // freq b = 1
    cache.TryGet('b');      // freq b = 2

    cache.Set('c', 3);      // expect 'a' evicted

    EXPECT_EQ(cache.TryGet('a'), nullptr);
    EXPECT_EQ(*cache.TryGet('b'), 2);
    EXPECT_EQ(*cache.TryGet('c'), 3);

    cache.Set('b', 4);
    cache.Set('d', 5);      // expect 'c' evicted

    EXPECT_EQ(*cache.TryGet('b'), 4);
    EXPECT_EQ(cache.TryGet('c'), nullptr);
    EXPECT_EQ(*cache.TryGet('d'), 5);
}

// multithreaded access test
void ThreadSet(TLfuCacheMtx<char,int>& cache, char key, int value) {
    cache.Set(key, std::move(value));
}

void ThreadGet(TLfuCacheMtx<char,int>& cache, char key) {
    cache.TryGet(key);
}

TEST(TLfuCacheMtx, ThreadSafeAccess) {
    TLfuCacheMtx<char,int> cache(5);
    std::vector<std::thread> threads;

    // parallel writes
    threads.emplace_back(ThreadSet, std::ref(cache), 'a', 1);
    threads.emplace_back(ThreadSet, std::ref(cache), 'b', 2);
    threads.emplace_back(ThreadSet, std::ref(cache), 'c', 3);
    for(auto& t: threads){
        t.join();
    }
    threads.clear();

    // parallel reads
    threads.emplace_back(ThreadGet, std::ref(cache), 'a');
    threads.emplace_back(ThreadGet, std::ref(cache), 'b');
    threads.emplace_back(ThreadGet, std::ref(cache), 'c');
    for(auto& t: threads){
        t.join();
    }
    threads.clear();

    // parallel erase
    threads.emplace_back([&]{ cache.Erase('b'); });
    threads.emplace_back([&]{ cache.Erase('x'); });
    for(auto& t: threads){
        t.join();
    }

    EXPECT_FALSE(cache.Exist('b'));
    EXPECT_TRUE(cache.Exist('a'));
    EXPECT_TRUE(cache.Exist('c'));
}

// multithread deterministic eviction test
TEST(TLfuCacheMtx, ThreadSafeEvictDeterministic) {
    TLfuCacheMtx<char,int> cache(2);

    // add 'a' and 'b' concurrently
    std::thread t1([&]{ cache.Set('a', 1); });
    std::thread t2([&]{ cache.Set('b', 2); });
    t1.join(); t2.join();

    // touch 'a' twice and 'b' once in parallel
    std::vector<std::thread> touchThreads;
    touchThreads.emplace_back([&]{ cache.TryGet('a'); });
    touchThreads.emplace_back([&]{ cache.TryGet('b'); });
    touchThreads.emplace_back([&]{ cache.TryGet('a'); });
    for(auto& t : touchThreads) t.join();

    // now 'a' freq=3, 'b' freq=2

    // insert 'c', should evict 'b' (freq lower)
    cache.Set('c', 3);

    auto* valA = cache.TryGet('a');
    auto* valB = cache.TryGet('b');
    auto* valC = cache.TryGet('c');

    EXPECT_EQ(valA ? *valA : -1, 1);
    EXPECT_EQ(valB, nullptr); // b evicted
    EXPECT_EQ(valC ? *valC : -1, 3);

    // update 'c' once, add 'd', should evict 'c' (freq 1 < 'a'=3)
    cache.TryGet('c'); // freq c = 2
    cache.Set('d', 4);

    EXPECT_EQ(*cache.TryGet('a'), 1);
    EXPECT_EQ(cache.TryGet('c'), nullptr); // c evicted
    EXPECT_EQ(*cache.TryGet('d'), 4);
}
