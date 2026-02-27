#include "TLfuCacheThreadsafeLazyUpdate.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

// deterministic LFU eviction, single-threaded
TEST(TLfuCacheThreadsafeLazyUpdate, EvictDeterministic) {
    TLfuCacheThreadsafeLazyUpdate<char,int> cache(2,1);

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
void ThreadSet(TLfuCacheThreadsafeLazyUpdate<char,int>& cache, char key, int value) {
    cache.Set(key, std::move(value));
}

void ThreadGet(TLfuCacheThreadsafeLazyUpdate<char,int>& cache, char key) {
    cache.TryGet(key);
}

TEST(TLfuCacheThreadsafeLazyUpdate, ThreadSafeAccess) {
    TLfuCacheThreadsafeLazyUpdate<char,int> cache(5,1);
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
TEST(TLfuCacheThreadsafeLazyUpdate, ThreadSafeEvictDeterministic) {
    TLfuCacheThreadsafeLazyUpdate<char,int> cache(2,1);

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

// Singlethreaded test with lazy update threshold = 10
TEST(TLfuCacheThreadsafeLazyUpdate, LazyUpdateThreshold10_SingleThread) {
    TLfuCacheThreadsafeLazyUpdate<char, int> cache(3, 10);

    // Step 1: Insert keys in order 'a' -> 'b' -> 'c'
    cache.Set('a', 100);
    cache.Set('b', 200);
    cache.Set('c', 300);

    // Step 2: Read 'a' 9 times (counter=9 < threshold)
    for (int i = 0; i < 9; ++i) {
        cache.TryGet('a');
    }

    // Step 3: Read 'b' 12 times (counter reaches 10 ->triggers frequency update to 2)
    for (int i = 0; i < 12; ++i) {
        cache.TryGet('b');
    }

    // Step 4: Read 'c' 5 times (counter=5 < threshold)
    for (int i = 0; i < 5; ++i) {
        cache.TryGet('c');
    }

    // Step 5: Insert 'd' ->cache full ->evict LFU
    cache.Set('d', 400);

    // Expected eviction: among freq=1 keys ('a' and 'c'), LRU is 'c' (last in list)
    EXPECT_FALSE(cache.Exist('a'));  // 'a' is LRU in freq=1 -> evicted
    EXPECT_TRUE(cache.Exist('b'));  // 'b' has higher freq=2 -> safe
    EXPECT_TRUE(cache.Exist('c')); // 'c' freq=1, used recently -> safe
    EXPECT_TRUE(cache.Exist('d'));  // new key inserted

    // Verify 'b' frequency was updated
    auto* valB = cache.TryGet('b');
    EXPECT_NE(valB, nullptr);
    EXPECT_EQ(*valB, 200);
}

// Multithreaded test with lazy update threshold = 10
TEST(TLfuCacheThreadsafeLazyUpdate, LazyUpdateThreshold10_MultiThread) {
    TLfuCacheThreadsafeLazyUpdate<char, int> cache(2, 10);

    cache.Set('a', 0);
    // Step 1: Concurrent reads on 'a' 15 accesses
    std::vector<std::thread> readThreads;
    for (int i = 0; i < 15; ++i) {
        readThreads.emplace_back([&] {
            cache.TryGet('a');
        });
    }
    for (auto& t : readThreads) {
        t.join();
    }

    // After 15 reads: counter reaches 10 ->freq updated to 2
    auto* valX = cache.TryGet('a');
    EXPECT_NE(valX, nullptr);
    EXPECT_EQ(*valX, 0);

    // Step 2: Insert 'b' and 'c' -> evict LFU
    cache.Set('b', 1);  // freq=1
    cache.Set('c', 2);  // cache full

    // Expected: 'b' evicted (freq=1), 'a' remains (freq≥2)
    EXPECT_TRUE(cache.Exist('a'));
    EXPECT_FALSE(cache.Exist('b'));
    EXPECT_TRUE(cache.Exist('c'));

    // Step 3: Read 'c' 8 times (counter=8 < 10)
    for (int i = 0; i < 8; ++i) {
        cache.TryGet('c');
    }

    // Insert 'd' ->cache full again
    cache.Set('d', 3);

    // Now 'c' is candidate for eviction (freq=1, counter=8)
    EXPECT_TRUE(cache.Exist('a'));
    EXPECT_FALSE(cache.Exist('b'));
    EXPECT_TRUE(cache.Exist('d'));
}

// Edge case: frequent updates reset counter, preventing frequency rise
TEST(TLfuCacheThreadsafeLazyUpdate, LazyUpdateThreshold10_FrequentUpdates) {
    TLfuCacheThreadsafeLazyUpdate<char, int> cache(2, 10);

    // Step 1: Frequent updates reset counter
    for (int i = 0; i < 3; ++i) {
        cache.Set('a', 100 + i);  // freq increments each time
        for (int j = 0; j < 7; ++j) {  // read 7 times
            cache.TryGet('a');
        }
        // counter=7 each time, never reaches 10
    }
    // freq(a) = 4 (1 initial + 3 updates)

    // Step 2: Insert 'c' ->cache full ->evict LFU 'b'
    cache.Set('b', 200);
    cache.Set('c', 300);

    // 'a' never updated frequency ->may be evicted
    EXPECT_TRUE(cache.Exist('a'));
    EXPECT_FALSE(cache.Exist('b'));
    EXPECT_TRUE(cache.Exist('c'));

    // Step 3: Reinsert 'a' and read 10 times
    cache.Set('a', 300);
    for (int i = 0; i < 10; ++i) {
        cache.TryGet('b');  // counter reaches 10 ->freq updated to 2
    }

    cache.Set('d', 400);  // insert another key

    EXPECT_TRUE(cache.Exist('a'));  // now protected by freq=2
    EXPECT_FALSE(cache.Exist('c'));
    EXPECT_TRUE(cache.Exist('d'));
}

// slow stress test, uncomment if ready to wait
// TEST(TLfuCacheThreadsafeLazyUpdate, StressRaceTest) {
//     TLfuCacheThreadsafeLazyUpdate<int, int> cache(10, 5);

//     const int threadCount = 16;
//     const int operations = 1000;

//     std::vector<std::thread> threads;

//     for (int t = 0; t < threadCount; ++t) {
//         threads.emplace_back([&]() {
//             for (int i = 0; i < operations; ++i) {
//                 int key = rand() % 20;

//                 switch (rand() % 4) {
//                     case 0:
//                         cache.Set(key, std::move(i));
//                         break;
//                     case 1:
//                         cache.TryGet(key);
//                         break;
//                     case 2:
//                         cache.Erase(key);
//                         break;
//                     case 3:
//                         cache.Exist(key);
//                         break;
//                 }
//             }
//         });
//     }

//     for (auto& t : threads)
//         t.join();

//     SUCCEED(); 
// }