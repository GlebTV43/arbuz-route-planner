#include "cachecontrol.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <vector>

CacheControl::CacheControl(const fs::path& cache_dir, int max_age_min, std::size_t max_items)
    : cache_dir_(cache_dir), max_age_min_(max_age_min), max_items_(max_items) {
    if (!fs::exists(cache_dir_)) {
        fs::create_directories(cache_dir_);
    }
    ClearExpired(max_age_min_);
    TrimToLimit();
}

void CacheControl::Put(const std::string& key, const json& data) {
    fs::create_directories(cache_dir_);

    fs::path target = PathFor(key);
    fs::path temp = target;
    temp += ".tmp";

    {
        std::ofstream file(temp);
        if (!file.is_open()) {
            throw std::runtime_error("Не удалось записать кэш: " + temp.string());
        }
        file << data.dump(2);
    }

    std::error_code error;
    fs::rename(temp, target, error);
    if (error) {
        fs::remove(target, error);
        error.clear();
        fs::rename(temp, target, error);
    }
    if (error) {
        throw std::runtime_error("Не удалось обновить кэш: " + target.string());
    }

    TrimToLimit();
}

json CacheControl::Get(const std::string& key) const {
    std::ifstream file(PathFor(key));
    if (!file.is_open()) {
        throw std::runtime_error("Запись кэша не найдена: " + key);
    }
    return json::parse(file);
}

bool CacheControl::Has(const std::string& key, int max_age_min) const {
    fs::path path = PathFor(key);
    if (!fs::exists(path) || !fs::is_regular_file(path)) {
        return false;
    }

    auto now = fs::file_time_type::clock::now();
    auto changed = fs::last_write_time(path);
    auto age = std::chrono::duration_cast<std::chrono::minutes>(now - changed);
    return age.count() <= max_age_min;
}

bool CacheControl::Has(const std::string& key) const {
    return Has(key, max_age_min_);
}

void CacheControl::Clear() {
    if (!fs::exists(cache_dir_)) {
        return;
    }

    for (const auto& entry : fs::directory_iterator(cache_dir_)) {
        if (entry.is_regular_file()) {
            fs::remove(entry.path());
        }
    }
}

fs::path CacheControl::PathFor(const std::string& key) const {
    return cache_dir_ / SafeKey(key);
}

std::string CacheControl::SafeKey(const std::string& key) const {
    std::string result;
    result.reserve(key.size());
    for (unsigned char c : key) {
        bool good = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (good || c == '.' || c == '_' || c == '-') {
            result.push_back(static_cast<char>(c));
        } else {
            result.push_back('_');
        }
    }
    return result.empty() ? "empty.json" : result;
}

void CacheControl::ClearExpired(int max_age_min) {
    if (!fs::exists(cache_dir_)) {
        return;
    }

    auto now = fs::file_time_type::clock::now();
    for (const auto& entry : fs::directory_iterator(cache_dir_)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto age = std::chrono::duration_cast<std::chrono::minutes>(now - fs::last_write_time(entry.path()));
        if (age.count() > max_age_min) {
            fs::remove(entry.path());
        }
    }
}

void CacheControl::TrimToLimit() {
    if (max_items_ == 0 || !fs::exists(cache_dir_)) {
        return;
    }

    std::vector<fs::directory_entry> entries;
    for (const auto& entry : fs::directory_iterator(cache_dir_)) {
        if (entry.is_regular_file()) {
            entries.push_back(entry);
        }
    }

    if (entries.size() <= max_items_) {
        return;
    }

    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
        return fs::last_write_time(left.path()) < fs::last_write_time(right.path());
    });

    for (std::size_t i = 0; i + max_items_ < entries.size(); ++i) {
        fs::remove(entries[i].path());
    }
}
