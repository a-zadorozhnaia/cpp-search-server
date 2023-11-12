#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <vector>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count)
    {};

    Access operator[](const Key& key)
    {
        auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        return {std::lock_guard<std::mutex>(bucket.mutex), bucket.map[key]};
    }

    std::map<Key, Value> BuildOrdinaryMap()
    {
        std::map<Key, Value> res;
        for (auto& [mutex, map] : buckets_) {
            std::lock_guard<std::mutex> guard(mutex);
            res.insert(map.begin(), map.end());
        }
        return res;
    }

    void erase(const Key& key) {
        auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        std::lock_guard<std::mutex> g(bucket.mutex);
        bucket.map.erase(key);
    }

private:
    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> map;
    };
    std::vector<Bucket> buckets_;
};
