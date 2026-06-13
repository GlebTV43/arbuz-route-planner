#include "utils.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <vector>

ParsedTime ParseISO8601(const std::string& s) {
    if (s.size() < 16) {
        return {"??.??", "??:??", ""};
    }

    std::string month = s.substr(5, 2);
    std::string day = s.substr(8, 2);
    std::string hhmm = s.substr(11, 5);
    std::string tz;

    std::size_t plus = s.find('+', 16);
    std::size_t minus = s.find('-', 16);
    std::size_t pos = std::min(plus == std::string::npos ? s.size() : plus,
                               minus == std::string::npos ? s.size() : minus);
    if (pos < s.size() && pos + 3 <= s.size()) {
        tz = "GMT" + s.substr(pos, 3);
    }

    return {day + "." + month, hhmm, tz};
}

std::string FormatDuration(int total_seconds) {
    if (total_seconds <= 0) {
        return "0 мин.";
    }

    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;

    std::string result;
    if (hours > 0) {
        result += std::to_string(hours) + " ч. ";
    }
    result += std::to_string(minutes) + " мин.";
    return result;
}

std::string Trim(const std::string& s) {
    std::size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    std::size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

namespace {

char32_t LowerChar(char32_t value) {
    if (value >= U'A' && value <= U'Z') {
        return value + 32;
    }
    if (value >= U'А' && value <= U'Я') {
        return value + 32;
    }
    if (value == U'Ё') {
        return U'ё';
    }
    return value;
}

std::vector<char32_t> DecodeUtf8(const std::string& text, bool lower) {
    std::vector<char32_t> result;
    result.reserve(text.size());

    for (std::size_t i = 0; i < text.size();) {
        unsigned char first = static_cast<unsigned char>(text[i]);
        char32_t value = first;
        std::size_t step = 1;

        if ((first & 0x80) == 0) {
            value = first;
        } else if ((first & 0xE0) == 0xC0 && i + 1 < text.size()) {
            unsigned char second = static_cast<unsigned char>(text[i + 1]);
            if ((second & 0xC0) == 0x80) {
                value = ((first & 0x1F) << 6) | (second & 0x3F);
                step = 2;
            }
        } else if ((first & 0xF0) == 0xE0 && i + 2 < text.size()) {
            unsigned char second = static_cast<unsigned char>(text[i + 1]);
            unsigned char third = static_cast<unsigned char>(text[i + 2]);
            if ((second & 0xC0) == 0x80 && (third & 0xC0) == 0x80) {
                value = ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F);
                step = 3;
            }
        } else if ((first & 0xF8) == 0xF0 && i + 3 < text.size()) {
            unsigned char second = static_cast<unsigned char>(text[i + 1]);
            unsigned char third = static_cast<unsigned char>(text[i + 2]);
            unsigned char fourth = static_cast<unsigned char>(text[i + 3]);
            if ((second & 0xC0) == 0x80 && (third & 0xC0) == 0x80 && (fourth & 0xC0) == 0x80) {
                value = ((first & 0x07) << 18) | ((second & 0x3F) << 12) | ((third & 0x3F) << 6) |
                        (fourth & 0x3F);
                step = 4;
            }
        }

        result.push_back(lower ? LowerChar(value) : value);
        i += step;
    }

    return result;
}

std::string EncodeUtf8(const std::vector<char32_t>& text) {
    std::string result;
    result.reserve(text.size() * 2);

    for (char32_t value : text) {
        if (value <= 0x7F) {
            result.push_back(static_cast<char>(value));
        } else if (value <= 0x7FF) {
            result.push_back(static_cast<char>(0xC0 | (value >> 6)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3F)));
        } else if (value <= 0xFFFF) {
            result.push_back(static_cast<char>(0xE0 | (value >> 12)));
            result.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xF0 | (value >> 18)));
            result.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3F)));
        }
    }

    return result;
}

std::vector<char32_t> NormalizeToCodepoints(const std::string& text) {
    return DecodeUtf8(Trim(text), true);
}

int LevDistanceRaw(const std::vector<char32_t>& first, const std::vector<char32_t>& second, int limit) {
    int n = static_cast<int>(first.size());
    int m = static_cast<int>(second.size());

    if (n == 0) {
        return m;
    }
    if (m == 0) {
        return n;
    }
    if (limit >= 0 && std::abs(n - m) > limit) {
        return limit + 1;
    }

    std::vector<int> prev(m + 1);
    std::vector<int> curr(m + 1);
    for (int j = 0; j <= m; ++j) {
        prev[j] = j;
    }

    for (int i = 1; i <= n; ++i) {
        curr[0] = i;
        int row_best = curr[0];

        for (int j = 1; j <= m; ++j) {
            int cost = first[i - 1] == second[j - 1] ? 0 : 1;
            int insert_cost = curr[j - 1] + 1;
            int delete_cost = prev[j] + 1;
            int replace_cost = prev[j - 1] + cost;

            curr[j] = std::min({insert_cost, delete_cost, replace_cost});
            row_best = std::min(row_best, curr[j]);
        }

        if (limit >= 0 && row_best > limit) {
            return limit + 1;
        }

        std::swap(prev, curr);
    }

    return prev[m];
}

}  // namespace

std::string Normalize(const std::string& s) {
    return EncodeUtf8(NormalizeToCodepoints(s));
}

std::string NormalizeDate(const std::string& date) {
    std::string source = Trim(date);
    char sep = 0;
    for (char c : source) {
        if (c == '-' || c == '.' || c == '/') {
            sep = c;
            break;
        }
    }
    if (sep == 0) {
        return "";
    }

    std::vector<std::string> parts;
    std::stringstream input(source);
    std::string part;
    while (std::getline(input, part, sep)) {
        part = Trim(part);
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    if (parts.size() != 3) {
        return "";
    }

    int y = 0;
    int m = 0;
    int d = 0;
    try {
        int first = std::stoi(parts[0]);
        int second = std::stoi(parts[1]);
        int third = std::stoi(parts[2]);
        if (parts[0].size() == 4 || first > 31) {
            y = first;
            m = second;
            d = third;
        } else {
            d = first;
            m = second;
            y = third;
        }
        if (y < 100) {
            y += 2000;
        }
    } catch (...) {
        return "";
    }

    if (m < 1 || m > 12 || d < 1) {
        return "";
    }

    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool is_leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    if (is_leap) {
        days_in_month[1] = 29;
    }
    if (d > days_in_month[m - 1]) {
        return "";
    }

    char buffer[11];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", y, m, d);
    return std::string(buffer);
}

std::vector<std::string> FindNearestCity(const std::string& city, const std::vector<std::string>& cities) {
    auto normalized_city = NormalizeToCodepoints(city);
    int limit = normalized_city.size() > 8 ? 3 : 2;
    int best = limit + 1;
    std::vector<std::pair<std::string, int>> found;

    for (const auto& current : cities) {
        auto normalized_current = NormalizeToCodepoints(current);
        int distance = LevDistanceRaw(normalized_city, normalized_current, limit);
        if (distance <= limit) {
            found.push_back({current, distance});
            best = std::min(best, distance);
        }
    }

    std::vector<std::string> result;
    for (const auto& [current, distance] : found) {
        if (distance == best) {
            result.push_back(current);
        }
    }
    return result;
}
