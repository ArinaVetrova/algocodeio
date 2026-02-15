#pragma once

#include <iostream>
#include <set>
#include <shared_mutex>
#include <unordered_map>

using namespace std;

template <typename Key, typename Val>
class TLfuCacheMtx {
private:
    using Timestamp = uint64_t;
    struct Frequency
    {
        uint64_t Freq = 0;
        Timestamp Tm = 0;

        bool operator<(const Frequency& other) const
        {
            if (Freq != other.Freq)
            {
                return Freq < other.Freq;
            }
            return Tm < other.Tm;
        }
    };

    size_t SizeLimit = 0;
    Timestamp globalTm = 0;

    unordered_map<Key, Val> Data;
    unordered_map<Key, Frequency> FreqMap;
    set<pair<Frequency, Key>> QueueToEvict;

    mutable std::shared_mutex mtx;

    void ResetKeyUsage(const Key& key) {
        auto it = FreqMap.find(key);
        if (it == FreqMap.end()){
            return;
        }
        QueueToEvict.erase({it->second, key});
        FreqMap.erase(it);
    }

    void UpdateKeyUsage(const Key& key) {
        auto it = FreqMap.find(key);
        if (it != FreqMap.end()) {
            QueueToEvict.erase({it->second, key});
            it->second.Freq++;
            it->second.Tm = ++globalTm;
            QueueToEvict.insert({it->second, key});
        }
        else {
            Frequency freq{1, ++globalTm};
            FreqMap[key] = freq;
            QueueToEvict.insert({freq, key});
        }
    }

public:
    TLfuCacheMtx(size_t sizeLimit) : SizeLimit(sizeLimit) {
        Data.reserve(sizeLimit);
    }

    bool Erase(const Key& key) {
        // exclusively lock Cache containers during erase operation
        std::unique_lock<std::shared_mutex> lock(mtx);

        auto it = Data.find(key);
        if (it == Data.end()) {
            std::cerr << "Erase: key not found" << std::endl;
            return false;
        }

        ResetKeyUsage(key);
        Data.erase(it);
        return true;
    }

    void Set(const Key& key, Val&& value) {
        if (SizeLimit == 0)
        {
            std::cerr << "Set: size limit for cache set to 0" << std::endl;
            return;
        }

        // exclusively lock Cache containers during add/update operation
        std::unique_lock<std::shared_mutex> lock(mtx);
        if (!Data.count(key) && Data.size() == SizeLimit) {
            if (!QueueToEvict.empty())
            {
                auto [_, keyToErase] = *QueueToEvict.begin();
                // change usage of Erase here to ResetKeyUsage + Data.erase
                // to avoid recurcive mtx lock (Erase also using same mtx)
                ResetKeyUsage(keyToErase);
                Data.erase(keyToErase);
            }
        }

        UpdateKeyUsage(key);

        Data[key] = std::move(value);
    }

    // unique lock Cache containers due to UpdateKeyUsage changes common containers
    Val* TryGet(const Key& key) {
        std::unique_lock<std::shared_mutex> lock(mtx);
        if (!Data.count(key)) {
            std::cerr << "Get: key not found" << std::endl;
            return nullptr;
        }

        UpdateKeyUsage(key);

        auto it = Data.find(key);
        return &it->second;
    }

    bool Exist(const Key& key) const {
        std::shared_lock<std::shared_mutex> lock(mtx);
        return Data.count(key);
    }
};
