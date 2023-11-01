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
        : bucket_count_(bucket_count)
        , maps_vector_(bucket_count)
        , mutex_vector_(bucket_count )
    {};

    Access operator[](const Key& key)
    {
        uint64_t tmp = static_cast<uint64_t>(key) % bucket_count_;
        return {std::lock_guard<std::mutex>(mutex_vector_[tmp]), maps_vector_[tmp][key]};
    }

    std::map<Key, Value> BuildOrdinaryMap()
    {
        std::map<Key, Value> res;
        for (size_t i = 0; i < bucket_count_; ++i) {
            std::lock_guard<std::mutex> guard(mutex_vector_[i]);
            res.insert(maps_vector_[i].begin(), maps_vector_[i].end());
        }
        return res;
    }

    void erase(const Key& key) {
        uint64_t tmp = static_cast<uint64_t>(key) % bucket_count_;
        std::lock_guard<std::mutex> g(mutex_vector_[tmp]);
        maps_vector_[tmp].erase(key);
    }

private:
    size_t bucket_count_;
    std::vector<std::map<Key, Value>> maps_vector_;
    std::vector<std::mutex> mutex_vector_;
};
