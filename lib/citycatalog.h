#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class CityCatalog {
public:
    explicit CityCatalog(std::filesystem::path filename = {});

    std::size_t UpdateFromStationsList(const nlohmann::json& stations_list);

    const std::string& CodeByName(const std::string& name) const;
    std::optional<std::string> TryCodeByName(const std::string& name) const;
    std::string CanonicalName(const std::string& name) const;
    std::optional<std::string> TryCanonicalName(const std::string& name) const;
    std::vector<std::string> Names() const;
    std::vector<std::string> Nearest(const std::string& name) const;
    bool Contains(const std::string& name) const;

private:
    void Load(const std::filesystem::path& filename);
    nlohmann::json ExtractCities(const nlohmann::json& stations_list) const;
    void SaveCities(const nlohmann::json& cities) const;

    std::filesystem::path filename_;
    std::unordered_map<std::string, std::string> cities_;
    std::unordered_map<std::string, std::string> normalized_names_;
};
