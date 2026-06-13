#include "request.h"
#include "utils.h"

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace {

constexpr const char* kSearchUrl = "https://api.rasp.yandex.net/v3.0/search/";
constexpr const char* kStationsListUrl = "https://api.rasp.yandex-net.ru/v3.0/stations_list/";
constexpr int kCacheLifetimeMinutes = 15;
constexpr std::size_t kCacheMaxItems = 120;

std::string ReadKeyFromFile() {
    std::filesystem::path key_file = std::filesystem::path(PROJECT_DATA_PATH) / ".." / "apikey.txt";
    std::ifstream file(key_file);
    if (!file.is_open()) {
        return "";
    }

    std::string key;
    std::getline(file, key);
    return Trim(key);
}

}  // namespace

Requester::Requester(const std::string& apikey)
    : Requester(apikey, std::make_shared<CprHttpClient>(), std::filesystem::path(PROJECT_DATA_PATH) / "cache") {}

Requester::Requester(std::string apikey, std::shared_ptr<IHttpClient> http_client, std::filesystem::path cache_dir,
                     std::filesystem::path cities_file)
    : apikey_(ResolveApiKey(std::move(apikey))), cities_(std::move(cities_file)), http_client_(std::move(http_client)),
      cache_(std::move(cache_dir), kCacheLifetimeMinutes, kCacheMaxItems) {
    if (!http_client_) {
        throw std::invalid_argument("HTTP-клиент не задан");
    }
    if (apikey_.empty()) {
        throw std::runtime_error("API-ключ не найден. Укажите ключ аргументом, через YANDEX_API_KEY или в apikey.txt");
    }
}

std::string Requester::ResolveApiKey(std::string apikey) const {
    apikey = Trim(apikey);
    if (!apikey.empty()) {
        return apikey;
    }

    if (const char* env_key = std::getenv("YANDEX_API_KEY")) {
        apikey = Trim(env_key);
    }
    if (!apikey.empty()) {
        return apikey;
    }

    return ReadKeyFromFile();
}

std::optional<json> Requester::GetRoute(const std::string& from, const std::string& to, const std::string& date) const {
    std::string normalized_date = NormalizeDate(date);
    if (normalized_date.empty()) {
        return std::nullopt;
    }

    std::string fixed_from = cities_.CanonicalName(from);
    std::string fixed_to = cities_.CanonicalName(to);
    const std::string& from_code = cities_.CodeByName(fixed_from);
    const std::string& to_code = cities_.CodeByName(fixed_to);
    std::string key = CacheKey(from_code, to_code, normalized_date);

    if (cache_.Has(key)) {
        try {
            return cache_.Get(key);
        } catch (...) {
            cache_.Clear();
        }
    }

    auto data = Fetch(from_code, to_code, normalized_date);
    if (data) {
        cache_.Put(key, *data);
    }
    return data;
}

std::optional<ScheduleResult> Requester::GetRoutes(const std::string& from, const std::string& to, const std::string& date) const {
    auto data = GetRoute(from, to, date);
    if (!data) {
        return std::nullopt;
    }
    ScheduleResult result = parser_.ParseSchedule(*data);
    if (result.Empty()) {
        return std::nullopt;
    }
    return result;
}

std::optional<json> Requester::Fetch(const std::string& from_code, const std::string& to_code, const std::string& date) const {
    std::unordered_map<std::string, std::string> params = {
        {"apikey", apikey_},
        {"from", from_code},
        {"to", to_code},
        {"date", date},
        {"format", "json"},
        {"limit", "50"},
        {"transfers", "true"}
    };

    auto body = http_client_->Get(kSearchUrl, params);
    if (!body) {
        return std::nullopt;
    }

    try {
        json data = json::parse(*body);
        if (!data.is_object() || !data.contains("segments") || !data["segments"].is_array()) {
            return std::nullopt;
        }
        return data;
    } catch (...) {
        return std::nullopt;
    }
}


std::optional<json> Requester::FetchStationsList() const {
    std::unordered_map<std::string, std::string> params = {
        {"apikey", apikey_},
        {"format", "json"},
        {"lang", "ru_RU"}
    };

    auto body = http_client_->Get(kStationsListUrl, params);
    if (!body) {
        return std::nullopt;
    }

    try {
        json data = json::parse(*body);
        if (!data.is_object() || !data.contains("countries") || !data["countries"].is_array()) {
            return std::nullopt;
        }
        return data;
    } catch (...) {
        return std::nullopt;
    }
}

std::string Requester::CacheKey(const std::string& from_code, const std::string& to_code, const std::string& date) const {
    return from_code + "_" + to_code + "_" + date + ".json";
}

std::vector<std::string> Requester::GetCities() const {
    return cities_.Names();
}

std::string Requester::GetCanonicalCity(const std::string& city) const {
    return cities_.CanonicalName(city);
}

std::vector<std::string> Requester::FindNearestCities(const std::string& city) const {
    return cities_.Nearest(city);
}

void Requester::ClearCache() {
    cache_.Clear();
}


std::size_t Requester::UpdateCities() {
    auto data = FetchStationsList();
    if (!data) {
        throw std::runtime_error("Не удалось получить список городов из API");
    }
    return cities_.UpdateFromStationsList(*data);
}
