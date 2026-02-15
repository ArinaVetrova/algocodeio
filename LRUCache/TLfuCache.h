#pragma once

#include <unordered_map>
#include <set>
#include <iostream>

using namespace std;

template <typename Key, typename Val>
class TLfuCache {
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
    TLfuCache(size_t sizeLimit) : SizeLimit(sizeLimit) {
        Data.reserve(sizeLimit);
    }

    bool Erase(const Key& key) {
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

        if (!Data.count(key) && Data.size() == SizeLimit) {
            if (!QueueToEvict.empty())
            {
                auto [_, keyToErase] = *QueueToEvict.begin();
                Erase(keyToErase);
            }
        }

        UpdateKeyUsage(key);

        Data[key] = std::move(value);
    }

    Val* TryGet(const Key& key) {
        if (!Data.count(key)) {
            std::cerr << "Get: key not found" << std::endl;
            return nullptr;
        }

        UpdateKeyUsage(key);
        auto it = Data.find(key);
        return &it->second;
    }

    bool Exist(const Key& key) const {
        return Data.count(key);
    }
};
