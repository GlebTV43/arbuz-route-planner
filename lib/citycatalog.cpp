#include "citycatalog.h"
#include "utils.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

std::optional<std::string> JsonString(const json& object, const char* key) {
    if (!object.is_object() || !object.contains(key) || !object[key].is_string()) {
        return std::nullopt;
    }
    std::string value = Trim(object[key].get<std::string>());
    if (value.empty()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::string> YandexCode(const json& object) {
    if (!object.is_object() || !object.contains("codes") || !object["codes"].is_object()) {
        return std::nullopt;
    }
    return JsonString(object["codes"], "yandex_code");
}

}  // namespace

CityCatalog::CityCatalog(std::filesystem::path filename) {
    if (filename.empty()) {
        filename = std::filesystem::path(PROJECT_DATA_PATH) / "cities.json";
    }
    filename_ = std::move(filename);
    Load(filename_);
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

    cities_.clear();
    normalized_names_.clear();

    for (const auto& city : data) {
        auto name = JsonString(city, "name");
        auto code = JsonString(city, "id");
        if (!name || !code) {
            continue;
        }
        cities_[*name] = *code;
        normalized_names_.try_emplace(Normalize(*name), *name);
    }

    if (cities_.empty()) {
        throw std::runtime_error("Файл с городами пустой");
    }
}

std::size_t CityCatalog::UpdateFromStationsList(const json& stations_list) {
    json cities = ExtractCities(stations_list);
    if (cities.empty()) {
        throw std::runtime_error("В ответе API не найдено городов");
    }

    SaveCities(cities);
    Load(filename_);
    return cities.size();
}

json CityCatalog::ExtractCities(const json& stations_list) const {
    if (!stations_list.is_object() || !stations_list.contains("countries") || !stations_list["countries"].is_array()) {
        throw std::runtime_error("Ответ со списком станций имеет неверный формат");
    }

    json result = json::array();
    std::unordered_set<std::string> used_names;

    for (const auto& country : stations_list["countries"]) {
        if (!country.is_object() || !country.contains("regions") || !country["regions"].is_array()) {
            continue;
        }

        for (const auto& region : country["regions"]) {
            if (!region.is_object() || !region.contains("settlements") || !region["settlements"].is_array()) {
                continue;
            }

            for (const auto& settlement : region["settlements"]) {
                auto name = JsonString(settlement, "title");
                auto code = YandexCode(settlement);
                if (!name || !code || code->empty() || (*code)[0] != 'c') {
                    continue;
                }

                std::string normalized = Normalize(*name);
                if (!used_names.insert(normalized).second) {
                    continue;
                }

                result.push_back({{"id", *code}, {"name", *name}});
            }
        }
    }

    std::sort(result.begin(), result.end(), [](const json& left, const json& right) {
        return left.value("name", "") < right.value("name", "");
    });

    return result;
}

void CityCatalog::SaveCities(const json& cities) const {
    fs::create_directories(filename_.parent_path());

    fs::path temp = filename_;
    temp += ".tmp";

    {
        std::ofstream file(temp);
        if (!file.is_open()) {
            throw std::runtime_error("Не удалось записать файл городов: " + temp.string());
        }
        file << cities.dump(2);
    }

    std::error_code error;
    fs::rename(temp, filename_, error);
    if (error) {
        fs::remove(filename_, error);
        error.clear();
        fs::rename(temp, filename_, error);
    }
    if (error) {
        fs::remove(temp);
        throw std::runtime_error("Не удалось обновить файл городов: " + filename_.string());
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
