#include "TLfuCache.h"

#include <gtest/gtest.h>

// Check basic operations in SizeLimit: Set, Exist, TryGEt, Erase
TEST(TLfuCache, Basic)
{
    TLfuCache<char,int> cache(2);

    cache.Set('a', 1);
    cache.Set('b', 2);

    EXPECT_TRUE(cache.Exist('a'));
    EXPECT_TRUE(cache.Exist('b'));

    EXPECT_NO_THROW({
        auto* valA = cache.TryGet('a');
        auto* valB = cache.TryGet('b');
        EXPECT_EQ(*valA, 1);
        EXPECT_EQ(*valB, 2);
    });

    cache.Erase('a');
    EXPECT_FALSE(cache.Exist('a'));

    EXPECT_NO_THROW({
    auto* valA = cache.TryGet('a');
    auto* valB = cache.TryGet('b');
    EXPECT_EQ(valA, nullptr);
    EXPECT_EQ(*valB, 2);
    });
}

// Check first added element evicted then SizeLimit exceeded
TEST(TLfuCache, Evict)
{
    TLfuCache<char,int> cache(2);

    cache.Set('a', 1);
    cache.Set('b', 2);

    EXPECT_TRUE(cache.Exist('a'));
    EXPECT_TRUE(cache.Exist('b'));

    EXPECT_NO_THROW({
        auto valA = cache.TryGet('a');
        auto valB = cache.TryGet('b');
        EXPECT_EQ(*valA, 1);
        EXPECT_EQ(*valB, 2);
    });

    cache.Set('c', 3);

    // expect 'a' evicted, because we checked 'a' 2 times, 'b' 2 times, but a was touched ealier
    EXPECT_NO_THROW({
        auto valA = cache.TryGet('a');
        auto valB = cache.TryGet('b');
        auto valC = cache.TryGet('c');
        EXPECT_EQ(valA, nullptr);
        EXPECT_EQ(*valB, 2);
        EXPECT_EQ(*valC, 3);
    });

    cache.Set('b', 4);
    cache.Set('d', 5);

    // expect 'c' evicted, it's freq less than 'b'
    EXPECT_NO_THROW({
        EXPECT_EQ(*cache.TryGet('b'), 4);
        EXPECT_EQ(cache.TryGet('c'), nullptr);
        EXPECT_EQ(*cache.TryGet('d'), 5);
    });
}

