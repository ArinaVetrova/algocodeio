#pragma once 

#include <unordered_map>
#include <list>
#include <iostream>

using namespace std;

// made the class template, added Key and Val typenames
template <typename Key, typename Val>
class TLruCache {
private:
    // size_t matches container size() type and avoids signed/unsigned bugs

    size_t SizeLimit = 0;
    struct Node
    {
        Key key;
        Val val;
    };

    // rewrite containers using template params and alias
    // move to list + unordered_map to provide O(1) for cache operations
    std::list<Node> CacheList;
    std::unordered_map<Key, typename std::list<Node>::iterator> Data;

public:
    TLruCache(size_t sizeLimit) : SizeLimit(sizeLimit) {
        // reserve capacity for Data to avoid rehash
        Data.reserve(sizeLimit);

    }

    // changed exception throw to false return, added log for error case
    // used helper func to simplify
    bool Erase(const Key& key) {
        auto listIt = Data.find(key);
        if (listIt == Data.end()) {
            std::cerr << "Erase: key not found" << std::endl;
            return false;
        }

        CacheList.erase(listIt->second);
        Data.erase(key);
        return true;
    }

    // use Val&&, baecause Val objects can be heavy
    void Set(const Key& key, Val&& value) {
        if (SizeLimit == 0)
        {
            std::cerr << "Set: size limit for cache set to 0" << std::endl;
            return;
        }

        
        // if we already have this keey, just update it's val and pos
        if (auto it = Data.find(key); it != Data.end())
        {
            it->second->val = std::move(value);
            CacheList.splice(CacheList.begin(), CacheList, it->second);
            return;
        }

        if (Data.size() == SizeLimit) {
            const auto& lastNode = CacheList.back();
            Data.erase(lastNode.key);
            CacheList.pop_back();
        }

        // use std::move in case Val objects are heavy
        auto it = CacheList.insert(CacheList.begin(), {key, std::move(value)});
        Data[key] = it;
    }

    // user need to check returned value and use it in case it's not nullptr
    // return Val*: 
    // 1. to avoid copy of heavy Val; 
    // 2. to give signal outside that the key is not found - return nullptr instead of throwing an exception
    Val* TryGet(const Key& key) {
        auto itMap = Data.find(key);
        if (itMap == Data.end()) {
            std::cerr << "Get: key not found" << std::endl;
            return nullptr;
        }

        auto itList = itMap->second;
        Val* val = &(itList->val);
        CacheList.splice(CacheList.begin(), CacheList, itList);
        return val;
    }

    // added const to func signature
    bool Exist(const Key& key) const {
        return Data.count(key);
    }
};
