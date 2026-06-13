#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

using json = nlohmann::json;

struct ParsedTime {
    std::string date;
    std::string time;
    std::string tz;
};

ParsedTime ParseISO8601(const std::string& s);
std::string FormatDuration(int total_seconds);
std::string Normalize(const std::string& s);
std::vector<std::string> FindNearestCity(const std::string& city, const std::vector<std::string>& cities);
std::string Trim(const std::string& s);
std::string NormalizeDate(const std::string& date);
