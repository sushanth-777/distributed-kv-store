#pragma once

#include <string>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

class KVStore {
public:
    KVStore() = default;
    ~KVStore() = default;

    // Store a key→value pair
    void put(const std::string& key, const std::string& value);

    // Retrieve the value for a key, or nullopt if missing
    std::optional<std::string> get(const std::string& key);

    // Delete a key (no-op if it doesn’t exist)
    void del(const std::string& key);

private:
    std::unordered_map<std::string, std::string> store;
    mutable std::shared_mutex mtx;
};
