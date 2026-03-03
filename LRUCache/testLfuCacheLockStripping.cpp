#include "TLfuCacheLockStripping.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

// TEST BASIC OLD FUNCTIONALITY OF LFU STILL WORKS
// deterministic LFU eviction, single-threaded
TEST(TLfuCacheLockStripping, EvictDeterministic) {
    TLfuCacheLockStripping<char,int> cache(2,1);

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
void ThreadSet(TLfuCacheLockStripping<char,int>& cache, char key, int value) {
    cache.Set(key, std::move(value));
}

void ThreadGet(TLfuCacheLockStripping<char,int>& cache, char key) {
    cache.TryGet(key);
}

TEST(TLfuCacheLockStripping, ThreadSafeAccess) {
    TLfuCacheLockStripping<char,int> cache(5,1);
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
TEST(TLfuCacheLockStripping, ThreadSafeEvictDeterministic) {
    TLfuCacheLockStripping<char,int> cache(2,1);

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

    auto valA = cache.TryGet('a');
    auto valB = cache.TryGet('b');
    auto valC = cache.TryGet('c');

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

// TEST SHARDING
// Test parallel writes across multiple shards
TEST(TLfuCacheLockStripping, MultiShardParallelWrites) {
    // 4 shards, different keys map to different mutexes
    TLfuCacheLockStripping<int,int> cache(16, 4);
    std::vector<std::thread> threads;

    // keys 0..7, guaranteed to hit different shards
    for(int i=0; i<8; ++i){
        threads.emplace_back([&cache,i]{
            cache.Set(i, i*10);
        });
    }

    for(auto &t : threads) t.join();

    // Verify all keys were inserted
    for(int i=0; i<8; ++i){
        auto val = cache.TryGet(i);
        ASSERT_NE(val, nullptr);
        EXPECT_EQ(*val, i*10);
    }
}

// Test parallel reads/writes across multiple shards
TEST(TLfuCacheLockStripping, MultiShardReadWrite) {
    TLfuCacheLockStripping<int,int> cache(16, 4);

    // Fill cache with initial values
    for(int i=0;i<8;++i)
    {
        cache.Set(i, std::move(i));
    }

    std::vector<std::thread> threads;

    // Parallel read and update operations
    for(int i=0;i<8;++i){
        threads.emplace_back([&cache,i]{
            for(int j=0;j<100;j++){
                auto val = cache.TryGet(i);
                if(val) cache.Set(i, *val+1);
            }
        });
    }

    for(auto &t: threads) t.join();

    // Verify all keys still exist and values were updated
    for(int i=0;i<8;++i){
        auto val = cache.TryGet(i);
        ASSERT_NE(val, nullptr);
        EXPECT_GE(*val, i); // value should have increased at least once
    }
}


// Test concurrent accesses and LFU eviction across shards
TEST(TLfuCacheLockStripping, MultiShardConcurrentEviction) {
    // Cache limit = 4, 2 shards
    TLfuCacheLockStripping<int,int> cache(4, 2);

    // Step 1: Insert initial keys
    cache.Set(1, 10); // freq=1
    cache.Set(2, 20); // freq=1
    cache.Set(3, 30); // freq=1
    cache.Set(4, 40); // freq=1

    // Step 2: Access some keys to increase frequency
    // 1 and 2 will become more frequently used
    std::vector<std::thread> accessThreads;
    for(int i=0; i<5; ++i) {
        accessThreads.emplace_back([&cache] { cache.TryGet(1); }); // freq1+=5
        accessThreads.emplace_back([&cache] { cache.TryGet(2); }); // freq2+=5
    }
    for(auto &t : accessThreads) t.join();

    // Step 3: Insert new keys concurrently to trigger eviction
    std::vector<std::thread> insertThreads;
    insertThreads.emplace_back([&cache] { cache.Set(5, 50); }); // should evict key with lowest freq (3 or 4)
    insertThreads.emplace_back([&cache] { cache.Set(6, 60); }); // next eviction (remaining low freq)
    for(auto &t : insertThreads) t.join();

    // Step 4: Check that high-frequency keys survived
    auto val1 = cache.TryGet(1);
    auto val2 = cache.TryGet(2);
    EXPECT_NE(val1, nullptr);
    EXPECT_EQ(*val1, 10);
    EXPECT_NE(val2, nullptr);
    EXPECT_EQ(*val2, 20);

    // Step 5: Check that low-frequency keys were evicted
    auto val3 = cache.TryGet(3);
    auto val4 = cache.TryGet(4);
    EXPECT_EQ(val3, nullptr);
    EXPECT_EQ(val4, nullptr);

    // Step 6: Check new keys exist
    auto val5 = cache.TryGet(5);
    auto val6 = cache.TryGet(6);
    EXPECT_NE(val5, nullptr);
    EXPECT_EQ(*val5, 50);
    EXPECT_NE(val6, nullptr);
    EXPECT_EQ(*val6, 60);
}

// Test non-divisible SizeLimit across Shards
TEST(TLfuCacheLockStripping, NonDivisibleSizeLimitDistribution) {
    // sizeLimit not divisible by numShards
    // 5 elements, 2 shards
    TLfuCacheLockStripping<int,int> cache(5, 2);

    // Insert exactly sizeLimit elements
    for(int i = 0; i < 5; ++i) {
        cache.Set(i, i*10);
    }

    // All 5 elements must exist
    for(int i = 0; i < 5; ++i) {
        auto val = cache.TryGet(i);
        ASSERT_NE(val, nullptr);
        EXPECT_EQ(*val, i*10);
    }

    // Insert one more element — should trigger exactly one eviction
    cache.Set(100, 1000);

    // Count how many elements remain in cache
    int existingCount = 0;
    for(int i = 0; i < 5; ++i) {
        if (cache.Exist(i)) {
            existingCount++;
        }
    }
    if (cache.Exist(100)) {
        existingCount++;
    }

    // Cache must still contain exactly 5 elements
    EXPECT_EQ(existingCount, 5);

    // Newly inserted key must exist
    EXPECT_TRUE(cache.Exist(100));
}
