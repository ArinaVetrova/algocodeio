#pragma once

#include <unordered_map>
#include <list>
#include <vector>
#include <iostream>
#include <memory>
#include <thread>
#include <shared_mutex>


using namespace std;

template <typename Key, typename Val>
class TLfuCacheLockStripping {
private:
    using Frequency = int;

    struct Node {
        shared_ptr<Val> value;
        uint64_t freq;
        std::list<Key>::iterator it;
    };

    struct Shard {
        std::unordered_map<Key, Node> KeyMap;
        std::unordered_map<Frequency, std::list<Key>> FreqBuckets;
        Frequency MinFreq = 0;

        std::shared_mutex mtx;

        Shard(size_t sizeLimits)
        {
            KeyMap.reserve(sizeLimits);
        }
    };

    // amount of non-depended shardIdxs of data to make access to shared resources faster,
    // without blocking access to whole cache. This num can be changed in constuctor of TLfuCacheLockStripping
    size_t NumShards;
    // using unique_ptr because struct Shard contains non-copiable shared_mutex inside
    // so value containing Shard should be movable
    std::vector<std::unique_ptr<Shard>> Shards;

    size_t SizeLimit = 0;

    void UpdateFrequency(Shard& shard, const Key& key) {
        auto it = shard.KeyMap.find(key);
        if (it == shard.KeyMap.end()) return;

        Node& node = it->second;
        uint64_t oldFreq = node.freq;
        uint64_t newFreq = oldFreq + 1;

        // Remove key from old frequency bucket
        auto& oldBucket = shard.FreqBuckets[oldFreq];
        oldBucket.erase(node.it);

        // Update frequency in node
        node.freq = newFreq;

        // Add to new frequency bucket (front = most recently used within this frequency)
        auto& newBucket = shard.FreqBuckets[newFreq];
        newBucket.push_front(key);
        node.it = newBucket.begin();

        // If old bucket is empty and it was the minimum frequency, increment MinFreq
        if (oldFreq == shard.MinFreq && oldBucket.empty()) {
            shard.FreqBuckets.erase(oldFreq);
            shard.MinFreq = newFreq;
        }
    }

    void EvictLFU(Shard& shard) {
        // Get bucket with minimum frequency
        auto& minBucket = shard.FreqBuckets[shard.MinFreq];

        // Evict the least recently used key (back of the list)
        Key keyToErase = minBucket.back();
        minBucket.pop_back();

        shard.KeyMap.erase(keyToErase);

        if (minBucket.empty()) {
            shard.FreqBuckets.erase(shard.MinFreq);
        }
        // Update MinFreq: find new minimum frequency in remaining buckets
        UpdateMinFreq(shard);
    }

    void UpdateMinFreq(Shard& shard)
    {
        if (shard.FreqBuckets.empty()) {
            shard.MinFreq = 0; // Cache is now empty
        } else {
            shard.MinFreq = std::numeric_limits<Frequency>::max();
            for (const auto& pair : shard.FreqBuckets) {
                if (pair.first < shard.MinFreq) {
                    shard.MinFreq = pair.first;
                }
            }
        }
    }

    Shard& GetShard(Key key)
    {
        size_t shardIdx = std::hash<Key>{}(key) % NumShards;
        return *Shards[shardIdx];
    }

public:
    TLfuCacheLockStripping(size_t sizeLimit, size_t numShards): SizeLimit(sizeLimit), NumShards(numShards) {
        Shards.reserve(numShards);

        for (size_t i = 0; i < numShards; ++i)
        {
            // Total capacity may be smaller than requested because of integer division
            Shards.emplace_back(make_unique<Shard>(sizeLimit/numShards));
        }
    }

    bool Erase(const Key& key) {
        auto& shard = GetShard(key);

        std::unique_lock<std::shared_mutex> uLock(shard.mtx);

        auto itKeyMap = shard.KeyMap.find(key);
        if (itKeyMap == shard.KeyMap.end()) {
            std::cerr << "Erase: key not found" << std::endl;
            return false;
        }

        Node& node = itKeyMap->second;
        const auto& itFreqBuckets = shard.FreqBuckets.find(node.freq);
        if (itFreqBuckets != shard.FreqBuckets.end())
        {
            std::list<Key>& list = itFreqBuckets->second;
            list.erase(node.it);
            if (list.empty()) {
                shard.FreqBuckets.erase(node.freq);

                if (node.freq == shard.MinFreq) {
                    UpdateMinFreq(shard);
                }
            }
        }
        shard.KeyMap.erase(itKeyMap);
        return true;
    }

    void Set(const Key& key, Val&& value) {
        if (SizeLimit == 0)
        {
            std::cerr << "Set: size limit for cache set to 0" << std::endl;
            return;
        }

        auto& shard = GetShard(key);
        std::unique_lock<std::shared_mutex> uLock(shard.mtx);

        if (auto it = shard.KeyMap.find(key); it != shard.KeyMap.end()) {
            it->second.value = std::make_shared<Val>(std::move(value));
            UpdateFrequency(shard, key);
            return;
        }

        if (shard.KeyMap.size() >= SizeLimit) {
            EvictLFU(shard);
        }

        // Insert new element with frequency 1
        auto& bucket = shard.FreqBuckets[1];
        bucket.push_front(key);  // Front = most recently added within frequency 1

        // Create node with reference to this key in bucket
        shard.KeyMap[key] = {std::make_shared<Val>(std::move(value)), 1, bucket.begin()};
        shard.MinFreq = 1;  // New element has frequency 1 — this is the new minimum
    }

    shared_ptr<Val> TryGet(const Key& key) {
        auto& shard = GetShard(key);

        std::shared_lock<std::shared_mutex> shLock(shard.mtx);

        auto itMap = shard.KeyMap.find(key);
        if (itMap == shard.KeyMap.end()) {
            std::cerr << "Get: key not found" << std::endl;
            return nullptr;
        }

        // save value to return to avoid iterator invalidation or changes in the map between shLock.unlock and uniqueLock.lock
        auto val = itMap->second.value;

        {
            shLock.unlock();
            std::unique_lock<std::shared_mutex> uniqueLock(shard.mtx);
            // Check if the key still exists after unlock-lock is inside UpdateFrequency func
            UpdateFrequency(shard, key);
        }

        return val;
    }

    bool Exist(const Key& key) {
        auto& shard = GetShard(key);
        std::shared_lock<std::shared_mutex> shLock(shard.mtx);
        return shard.KeyMap.count(key);
    }
};
