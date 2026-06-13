#include "citycatalog.h"
#include "utils.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

CityCatalog::CityCatalog(std::filesystem::path filename) {
    if (filename.empty()) {
        filename = std::filesystem::path(PROJECT_DATA_PATH) / "cities.json";
    }
    Load(filename);
}

void CityCatalog::Load(const std::filesystem::path& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Файл с городами не найден: " + filename.string());
    }

    json data;
    file >> data;
    if (!data.is_array()) {
        throw std::runtime_error("Файл с городами имеет неверный формат");
    }

    for (const auto& city : data) {
        if (!city.is_object() || !city.contains("name") || !city.contains("id")) {
            continue;
        }
        if (!city["name"].is_string() || !city["id"].is_string()) {
            continue;
        }
        std::string name = city["name"].get<std::string>();
        cities_[name] = city["id"].get<std::string>();
        normalized_names_.try_emplace(Normalize(name), name);
    }

    if (cities_.empty()) {
        throw std::runtime_error("Файл с городами пустой");
    }
}

const std::string& CityCatalog::CodeByName(const std::string& name) const {
    std::string canonical = CanonicalName(name);
    return cities_.at(canonical);
}

std::optional<std::string> CityCatalog::TryCodeByName(const std::string& name) const {
    auto canonical = TryCanonicalName(name);
    if (!canonical) {
        return std::nullopt;
    }
    return cities_.at(*canonical);
}

std::string CityCatalog::CanonicalName(const std::string& name) const {
    auto canonical = TryCanonicalName(name);
    if (!canonical) {
        throw std::out_of_range("Город не найден: " + name);
    }
    return *canonical;
}

std::optional<std::string> CityCatalog::TryCanonicalName(const std::string& name) const {
    auto exact = cities_.find(name);
    if (exact != cities_.end()) {
        return exact->first;
    }

    auto normalized = normalized_names_.find(Normalize(name));
    if (normalized != normalized_names_.end()) {
        return normalized->second;
    }

    auto nearest = Nearest(name);
    if (nearest.size() == 1) {
        return nearest.front();
    }

    return std::nullopt;
}

std::vector<std::string> CityCatalog::Names() const {
    std::vector<std::string> result;
    result.reserve(cities_.size());
    for (const auto& [name, code] : cities_) {
        result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> CityCatalog::Nearest(const std::string& name) const {
    return FindNearestCity(name, Names());
}

bool CityCatalog::Contains(const std::string& name) const {
    return TryCanonicalName(name).has_value();
}
