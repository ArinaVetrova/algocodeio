#pragma once

#include <unordered_map>
#include <list>
#include <iostream>
#include <memory>

using namespace std;

template <typename Key, typename Val>
class TLfuCache {
private:
    using Frequency = int;

    struct Node {
        shared_ptr<Val> value;
        uint64_t freq;
        std::list<Key>::iterator it;
    };
    unordered_map<Key, Node> KeyMap;
    unordered_map<Frequency, std::list<Key>> FreqBuckets;

    size_t SizeLimit = 0;
    Frequency MinFreq = 0;

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
            MinFreq = newFreq;
        }
    }

    void EvictLFU() {
        // Get bucket with minimum frequency
        auto& minBucket = FreqBuckets[MinFreq];

        // Evict the least recently used key (back of the list)
        Key keyToErase = minBucket.back();
        minBucket.pop_back();

        KeyMap.erase(keyToErase);

        if (minBucket.empty()) {
            FreqBuckets.erase(MinFreq);
        }
        // Update MinFreq: find new minimum frequency in remaining buckets
        UpdateMinFreq();
    }

    void UpdateMinFreq()
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
    TLfuCache(size_t sizeLimit) : SizeLimit(sizeLimit) {
        KeyMap.reserve(sizeLimit);
    }

    bool Erase(const Key& key) {
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
            if (list.empty()) {
                FreqBuckets.erase(node.freq);

                if (node.freq == MinFreq) {
                    UpdateMinFreq();
                }
            }
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

        if (auto it = KeyMap.find(key); it != KeyMap.end()) {
            it->second.value = std::make_shared<Val>(std::move(value));
            UpdateFrequency(key);
            return;
        }

        if (KeyMap.size() >= SizeLimit) {
            EvictLFU();
        }

        // Insert new element with frequency 1
        auto& bucket = FreqBuckets[1];
        bucket.push_front(key);  // Front = most recently added within frequency 1

        // Create node with reference to this key in bucket
        KeyMap[key] = {std::make_shared<Val>(std::move(value)), 1, bucket.begin()};
        MinFreq = 1;  // New element has frequency 1 — this is the new minimum
    }

    shared_ptr<Val> TryGet(const Key& key) {
        auto itMap = KeyMap.find(key);
        if (itMap == KeyMap.end()) {
            std::cerr << "Get: key not found" << std::endl;
            return nullptr;
        }

        UpdateFrequency(key);
        return itMap->second.value;
    }

    bool Exist(const Key& key) const {
        return KeyMap.count(key);
    }
};
