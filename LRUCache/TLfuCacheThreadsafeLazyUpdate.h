#pragma once

#include <unordered_map>
#include <list>
#include <iostream>
#include <thread>
#include <shared_mutex>

using namespace std;

template <typename Key, typename Val>
class TLfuCacheThreadsafeLazyUpdate {
private:
    using Frequency = int;

    struct Node {
        Val value;
        int freq;
        uint64_t readCounter;
        std::list<Key>::iterator it;
    };
    unordered_map<Key, Node> KeyMap;
    unordered_map<Frequency, std::list<Key>> FreqBuckets;
    //unordered_map<Key, uint64_t> ReadCounter; // counter for TryGet operations

    size_t SizeLimit = 0;
    Frequency MinFreq = 0;

    uint64_t LazyUpdateThreshold = 1; // update freq every LazyUpdateThreshold TryGet operations

    // added shared_mtx
    mutable std::shared_mutex mtx;

    void UpdateFrequency(const Key& key) {
        auto it = KeyMap.find(key);
        if (it == KeyMap.end()) return;

        Node& node = it->second;
        uint64_t oldFreq = node.freq;
        uint64_t newFreq = oldFreq + 1;

        // Remove key from old frequency bucket
        auto& oldBucket = FreqBuckets[oldFreq];
        oldBucket.erase(node.it);

        // Update frequency in node
        node.freq = newFreq;

        // Add to new frequency bucket (front = most recently used within this frequency)
        auto& newBucket = FreqBuckets[newFreq];
        newBucket.push_front(key);
        node.it = newBucket.begin();

        // If old bucket is empty and it was the minimum frequency, increment MinFreq
        if (oldFreq == MinFreq && oldBucket.empty()) {
            FreqBuckets.erase(oldFreq);
            // Find new minimum frequency among remaining buckets
            updateMinFreq();
        }
    }

    void EvictLFU() {
        // Get bucket with minimum frequency
        auto& minBucket = FreqBuckets[MinFreq];

        // Evict the least recently used key (back of the list)
        Key keyToErase = minBucket.back();
        minBucket.pop_back();

        KeyMap.erase(keyToErase);

        // Update MinFreq: find new minimum frequency in remaining buckets
        updateMinFreq();
    }

    void updateMinFreq()
    {
        if (FreqBuckets.empty()) {
            MinFreq = 0; // Cache is now empty
        } else {
            MinFreq = std::numeric_limits<Frequency>::max();
            for (const auto& pair : FreqBuckets) {
                if (pair.first < MinFreq) {
                    MinFreq = pair.first;
                }
            }
        }
    }

public:
    TLfuCacheThreadsafeLazyUpdate(size_t sizeLimit, size_t lazyUpdateThreshold) : SizeLimit(sizeLimit), LazyUpdateThreshold(lazyUpdateThreshold) {
        KeyMap.reserve(sizeLimit);
    }

    bool Erase(const Key& key) {
        std::unique_lock<std::shared_mutex> lock(mtx);

        auto itKeyMap = KeyMap.find(key);
        if (itKeyMap == KeyMap.end()) {
            std::cerr << "Erase: key not found" << std::endl;
            return false;
        }

        Node& node = itKeyMap->second;
        const auto& itFreqBuckets = FreqBuckets.find(node.freq);
        
        if (itFreqBuckets != FreqBuckets.end())
        {
            std::list<Key>& list = itFreqBuckets->second;
            list.erase(node.it);
        }
        KeyMap.erase(itKeyMap);
        return true;
    }

    void Set(const Key& key, Val&& value) {
        if (SizeLimit == 0)
        {
            std::cerr << "Set: size limit for cache set to 0" << std::endl;
            return;
        }

        std::unique_lock<std::shared_mutex> lock(mtx);
        if (auto it = KeyMap.find(key); it != KeyMap.end()) {
            it->second.value = std::move(value);
            UpdateFrequency(key);
            it->second.readCounter++;
            return;
        }

        if (KeyMap.size() >= SizeLimit) {
            EvictLFU();
        }

        // Insert new element with frequency 1
        auto& bucket = FreqBuckets[1];
        bucket.push_front(key);  // Front = most recently added within frequency 1

        // Create node with reference to this key in bucket
        KeyMap[key] = {std::move(value), 1, 0, bucket.begin()};
        MinFreq = 1;  // New element has frequency 1 — this is the new minimum
    }

    Val* TryGet(const Key& key) {
        std::shared_lock<std::shared_mutex> shLock(mtx);
        auto itMap = KeyMap.find(key);
        if (itMap == KeyMap.end()) {
            std::cerr << "Get: key not found" << std::endl;
            return nullptr;
        }

        itMap->second.readCounter++;

        // If readCount threshrold met update frequency for this key under unique lock
        if (itMap->second.readCounter >= LazyUpdateThreshold) {
            shLock.unlock();

            std::unique_lock<std::shared_mutex> uniqueLock(mtx);
            // Check again the key exists after shLock was unlocked 
            auto itAfter = KeyMap.find(key);
            if (itAfter != KeyMap.end() && itAfter->second.readCounter >= LazyUpdateThreshold) {
                UpdateFrequency(key);
                itAfter->second.readCounter = 0;
            }
        }
        return &itMap->second.value;
    }

    bool Exist(const Key& key) const {
        std::shared_lock<std::shared_mutex> shLock(mtx);
        return KeyMap.count(key);
    }
};
