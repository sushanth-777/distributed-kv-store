#include "kv_store.h"

void KVStore::put(const std::string& key, const std::string& value) {
    std::unique_lock lock(mtx);
    store[key] = value;
}

std::optional<std::string> KVStore::get(const std::string& key) {
    std::shared_lock lock(mtx);
    auto it = store.find(key);
    if (it != store.end()) {
        return it->second;
    }
    return std::nullopt;
}

void KVStore::del(const std::string& key) {
    std::unique_lock lock(mtx);
    store.erase(key);
}
