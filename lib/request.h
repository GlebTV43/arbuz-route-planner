#pragma once

#include "cachecontrol.h"
#include "citycatalog.h"
#include "httpclient.h"
#include "models.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using json = nlohmann::json;

class Requester {
public:
    explicit Requester(const std::string& apikey);
    Requester(std::string apikey, std::shared_ptr<IHttpClient> http_client,
              std::filesystem::path cache_dir = std::filesystem::path(PROJECT_DATA_PATH) / "cache");

    std::optional<json> GetRoute(const std::string& from, const std::string& to, const std::string& date) const;
    std::optional<ScheduleResult> GetRoutes(const std::string& from, const std::string& to, const std::string& date) const;
    std::vector<std::string> GetCities() const;
    std::string GetCanonicalCity(const std::string& city) const;
    std::vector<std::string> FindNearestCities(const std::string& city) const;
    void ClearCache();

private:
    std::string ResolveApiKey(std::string apikey) const;
    std::optional<json> Fetch(const std::string& from_code, const std::string& to_code, const std::string& date) const;
    std::string CacheKey(const std::string& from_code, const std::string& to_code, const std::string& date) const;

    std::string apikey_;
    CityCatalog cities_;
    std::shared_ptr<IHttpClient> http_client_;
    mutable CacheControl cache_;
    RouteParser parser_;
};
