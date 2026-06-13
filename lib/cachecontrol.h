#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;

class CacheControl {
public:
    explicit CacheControl(const fs::path& cache_dir, int max_age_min = 15, std::size_t max_items = 100);

    void Put(const std::string& key, const json& data);
    json Get(const std::string& key) const;
    bool Has(const std::string& key, int max_age_min) const;
    bool Has(const std::string& key) const;
    void Clear();

private:
    fs::path PathFor(const std::string& key) const;
    std::string SafeKey(const std::string& key) const;
    void ClearExpired(int max_age_min);
    void TrimToLimit();

    fs::path cache_dir_;
    int max_age_min_ = 15;
    std::size_t max_items_ = 100;
};
