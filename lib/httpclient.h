#pragma once

#include <optional>
#include <string>
#include <unordered_map>

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual std::optional<std::string> Get(const std::string& url,
                                           const std::unordered_map<std::string, std::string>& params) const = 0;
};

class CprHttpClient final : public IHttpClient {
public:
    std::optional<std::string> Get(const std::string& url,
                                   const std::unordered_map<std::string, std::string>& params) const override;
};
