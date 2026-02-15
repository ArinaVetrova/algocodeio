#pragma once 

#include <unordered_map>
#include <set>
#include <iostream>

using namespace std;

// made the class template, added Key and Val typenames
template <typename Key, typename Val>
class TLruCache {
private:
    // used Timestamp alias, used uint64_t instead of signed int
    // (time should be always positive, signed int can overflow)
    using Timestamp = uint64_t;
    // size_t matches container size() type and avoids signed/unsigned bugs
    size_t SizeLimit = 0;

    Timestamp Tm = 0;

    // rewrite containers using template params and alias
    unordered_map<Key, Val> Data;
    unordered_map<Key, Timestamp> LastUsed;
    set<pair<Timestamp, Key>> Queue;

    // added helper method ResetKeyUsage
    void ResetKeyUsage(const Key& key) {
        auto it = LastUsed.find(key);
        if (it == LastUsed.end()){
            return;
        }
        Queue.erase({it->second, key});
        LastUsed.erase(key);
    }

    // added helper method UpdateKeyUsage
    void UpdateKeyUsage(const Key& key) {
        Queue.erase({LastUsed[key], key});
        LastUsed[key] = Tm++;
        Queue.insert({LastUsed[key], key});
    }

public:
    TLruCache(size_t sizeLimit) : SizeLimit(sizeLimit) {
        // reserve capacity for Data to avoid rehash
        Data.reserve(sizeLimit);

    }

    // changed exception throw to false return, added log for error case
    // used helper func to simplify
    bool Erase(const Key& key) {
        if (!Data.count(key)) {
            std::cerr << "Erase: key not found" << std::endl;
            return false;
        }

        ResetKeyUsage(key);
        Data.erase(key);
        return true;
    }

    // avoid dereferencing Queue.begin() when cache capacity is 0
    // use Val&&, baecause Val objects can be heavy
    void Set(const Key& key, Val&& value) {
        if (SizeLimit == 0)
        {
            std::cerr << "Set: size limit for cache set to 0" << std::endl;
            return;
        }

        // if there was a key found - it was also erased from the map --> delete unnecessary erase from map
        if (Data.count(key)) {
            ResetKeyUsage(key);
        }

        if (Queue.size() == SizeLimit) {
            auto [_, key] = *Queue.begin();
            Erase(key);
        }

        // before we inserted new {Tm, key} pair to the Queue, 
        // the previous pair stayed in the Queue --> now deleting previous pair before update
        UpdateKeyUsage(key);

        // use std::move in case Val objects are heavy
        Data[key] = std::move(value);
    }

    // user need to check returned value and use it in case it's not nullptr
    // return Val*: 
    // 1. to avoid copy of heavy Val; 
    // 2. to give signal outside that the key is not found - return nullptr instead of throwing an exception
    Val* TryGet(const Key& key) {
        if (!Data.count(key)) {
            std::cerr << "Get: key not found" << std::endl;
            return nullptr;
        }

        UpdateKeyUsage(key);
        return &Data[key];
    }

    // added const to func signature
    bool Exist(const Key& key) const {
        return Data.count(key);
    }
};
