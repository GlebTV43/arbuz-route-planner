#include "httpclient.h"

#include <cpr/cpr.h>

std::optional<std::string> CprHttpClient::Get(const std::string& url,
                                              const std::unordered_map<std::string, std::string>& params) const {
    cpr::Parameters request_params{};
    for (const auto& [key, value] : params) {
        request_params.Add({key, value});
    }

    auto response = cpr::Get(cpr::Url{url}, request_params);
    if (response.error.code != cpr::ErrorCode::OK || response.status_code != 200) {
        return std::nullopt;
    }
    return response.text;
}
