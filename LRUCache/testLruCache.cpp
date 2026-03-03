#include "TLruCache.h"

#include <gtest/gtest.h>

// Check basic operations in SizeLimit: Set, Exist, TryGEt, Erase
TEST(TLruCache, Basic)
{
    TLruCache<char,int> cache(2);

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
TEST(TLruCache, Evict)
{
    TLruCache<char,int> cache(2);

    cache.Set('a', 1);
    cache.Set('b', 2);

    EXPECT_TRUE(cache.Exist('a'));
    EXPECT_TRUE(cache.Exist('b'));

    cache.Set('c', 3);

    EXPECT_NO_THROW({
        auto valA = cache.TryGet('a');
        auto valB = cache.TryGet('b');
        auto valC = cache.TryGet('c');
        EXPECT_EQ(valA, nullptr);
        EXPECT_EQ(*valB, 2);
        EXPECT_EQ(*valC, 3);
    });

    cache.Erase('a');
    EXPECT_FALSE(cache.Exist('a'));

    EXPECT_NO_THROW({
        EXPECT_EQ(cache.TryGet('a'), nullptr);
        EXPECT_EQ(*cache.TryGet('b'), 2);
    });
}


// Check value changed by the key
TEST(TLruCache, ChangeValue)
{
    TLruCache<char,int> cache(2);

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
    
    cache.Set('a', 3);

    EXPECT_NO_THROW({
        EXPECT_EQ(*cache.TryGet('a'), 3);
        EXPECT_EQ(*cache.TryGet('b'), 2);
    });
}

// Check TryGet updates usage and protect from eviction
TEST(TLruCache, TryGetUpdatesUsage)
{
    TLruCache<char,int> cache(2);

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
    
    cache.TryGet('a');
    cache.Set('c', 3);

    EXPECT_TRUE(cache.Exist('a'));
    EXPECT_FALSE(cache.Exist('b'));
    EXPECT_TRUE(cache.Exist('c'));
}
