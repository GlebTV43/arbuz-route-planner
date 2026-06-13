#include <gtest/gtest.h>

#include "cachecontrol.h"
#include "citycatalog.h"
#include "commands.h"
#include "httpclient.h"
#include "models.h"
#include "request.h"
#include "utils.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

namespace {

json DirectSegment() {
    return {
        {"thread", {
            {"title", "Саратов — Москва"},
            {"number", "009Ж"},
            {"transport_type", "train"},
            {"vehicle", "Фирменный поезд"},
            {"carrier", {{"title", "РЖД"}}}
        }},
        {"from", {{"title", "Саратов-1-Пасс."}, {"code", "s9600001"}}},
        {"to", {{"title", "Москва Павелецкая"}, {"code", "s2000005"}}},
        {"departure", "2026-06-11T10:15:00+03:00"},
        {"arrival", "2026-06-11T20:45:00+03:00"},
        {"duration", 37800},
        {"has_transfers", false}
    };
}

json TransferSegment() {
    return {
        {"has_transfers", true},
        {"departure", "2026-06-11T08:00:00+03:00"},
        {"arrival", "2026-06-11T15:30:00+03:00"},
        {"details", json::array({
            {
                {"thread", {{"title", "Саратов — Ртищево"}, {"transport_type", "train"}}},
                {"from", {{"title", "Саратов-1-Пасс."}}},
                {"to", {{"title", "Ртищево-1"}}},
                {"departure", "2026-06-11T08:00:00+03:00"},
                {"arrival", "2026-06-11T10:00:00+03:00"},
                {"duration", 7200}
            },
            {
                {"type", "transfer"},
                {"transfer_point", {{"title", "Ртищево-1"}}},
                {"transfer_from", {{"title", "Ртищево-1"}}},
                {"transfer_to", {{"title", "Ртищево-1"}}},
                {"duration", 1800}
            },
            {
                {"thread", {{"title", "Ртищево — Москва"}, {"transport_type", "train"}}},
                {"from", {{"title", "Ртищево-1"}}},
                {"to", {{"title", "Москва Павелецкая"}}},
                {"departure", "2026-06-11T10:30:00+03:00"},
                {"arrival", "2026-06-11T15:30:00+03:00"},
                {"duration", 18000}
            }
        })}
    };
}

json ScheduleJson() {
    return {{"segments", json::array({DirectSegment(), TransferSegment()})}};
}

fs::path MakeTempDir(const std::string& name) {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path dir = fs::temp_directory_path() / (name + "_" + std::to_string(stamp));
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

class FakeHttpClient final : public IHttpClient {
public:
    explicit FakeHttpClient(std::optional<std::string> body) : body_(std::move(body)) {}

    std::optional<std::string> Get(const std::string& url,
                                   const std::unordered_map<std::string, std::string>& params) const override {
        ++calls_;
        last_url_ = url;
        last_params_ = params;
        return body_;
    }

    int Calls() const { return calls_; }
    const std::string& LastUrl() const { return last_url_; }
    const std::unordered_map<std::string, std::string>& LastParams() const { return last_params_; }

private:
    std::optional<std::string> body_;
    mutable int calls_ = 0;
    mutable std::string last_url_;
    mutable std::unordered_map<std::string, std::string> last_params_;
};

}  // namespace

TEST(CommandParserTest, ParsesSemicolonRouteWithCitySpaces) {
    CommandParser parser;
    auto command = parser.ParseLine("route Нижний Новгород; Санкт-Петербург; 11.06.2026");

    ASSERT_EQ(command.GetType(), ConsoleCommand::Type::kRoute);
    ASSERT_TRUE(command.Query().has_value());
    EXPECT_EQ(command.Query()->from, "Нижний Новгород");
    EXPECT_EQ(command.Query()->to, "Санкт-Петербург");
    EXPECT_EQ(command.Query()->date, "2026-06-11");
}

TEST(CommandParserTest, ParsesArrowRouteAndServiceCommands) {
    CommandParser parser("Санкт-Петербург", "Саратов");

    auto route = parser.ParseLine("route Саратов -> Москва 11.06.26");
    ASSERT_EQ(route.GetType(), ConsoleCommand::Type::kRoute);
    ASSERT_TRUE(route.Query().has_value());
    EXPECT_EQ(route.Query()->from, "Саратов");
    EXPECT_EQ(route.Query()->to, "Москва");
    EXPECT_EQ(route.Query()->date, "2026-06-11");

    auto back = parser.ParseLine("back 11.06.2026");
    ASSERT_EQ(back.GetType(), ConsoleCommand::Type::kRoute);
    EXPECT_EQ(back.Query()->from, "Саратов");
    EXPECT_EQ(back.Query()->to, "Санкт-Петербург");

    EXPECT_EQ(parser.ParseLine("cities").GetType(), ConsoleCommand::Type::kCities);
    EXPECT_EQ(parser.ParseLine("clear cache").GetType(), ConsoleCommand::Type::kClearCache);
    EXPECT_EQ(parser.ParseLine("help").GetType(), ConsoleCommand::Type::kHelp);
    EXPECT_EQ(parser.ParseLine("exit").GetType(), ConsoleCommand::Type::kExit);
}

TEST(CommandParserTest, RejectsBrokenInput) {
    CommandParser parser;

    EXPECT_EQ(parser.ParseLine("route Саратов Москва 11.06.2026").GetType(), ConsoleCommand::Type::kInvalid);
    EXPECT_EQ(parser.ParseLine("route Саратов; Москва; wrong-date").GetType(), ConsoleCommand::Type::kInvalid);
    EXPECT_EQ(parser.ParseLine("что-то непонятное").GetType(), ConsoleCommand::Type::kInvalid);
}

TEST(DateAndTextTest, NormalizesUserInput) {
    EXPECT_EQ(Trim("  Саратов \n"), "Саратов");
    EXPECT_EQ(Normalize("  МОСКВА  "), "москва");
    EXPECT_EQ(Normalize("Ёжик"), "ёжик");

    EXPECT_EQ(NormalizeDate("11.06.2026"), "2026-06-11");
    EXPECT_EQ(NormalizeDate("2026-06-11"), "2026-06-11");
    EXPECT_EQ(NormalizeDate("11.06.26"), "2026-06-11");
    EXPECT_EQ(NormalizeDate("29.02.2024"), "2024-02-29");
    EXPECT_EQ(NormalizeDate("29.02.2023"), "");
    EXPECT_EQ(NormalizeDate("31.04.2026"), "");
    EXPECT_EQ(NormalizeDate("20260611"), "");
}

TEST(CityCatalogTest, FindsCanonicalNamesAndNearestCities) {
    CityCatalog catalog;

    EXPECT_EQ(catalog.CanonicalName("саратов"), "Саратов");
    EXPECT_EQ(catalog.CanonicalName("САРАТОВ"), "Саратов");
    EXPECT_EQ(catalog.CanonicalName("  Масква  "), "Москва");
    EXPECT_EQ(catalog.CodeByName("саратов"), catalog.CodeByName("Саратов"));
    EXPECT_TRUE(catalog.Contains("москва"));

    auto nearest = catalog.Nearest("Масква");
    ASSERT_FALSE(nearest.empty());
    EXPECT_EQ(nearest.front(), "Москва");
}

TEST(CityCatalogTest, RejectsUnknownCity) {
    CityCatalog catalog;

    EXPECT_FALSE(catalog.TryCanonicalName("zzzzzzzz").has_value());
    EXPECT_THROW(catalog.CodeByName("zzzzzzzz"), std::out_of_range);
}

TEST(RouteParserTest, ParsesDirectRouteIntoObjects) {
    RouteParser parser;
    auto route = parser.ParseRoute(DirectSegment());

    ASSERT_TRUE(route.has_value());
    EXPECT_FALSE(route->HasTransfers());
    EXPECT_EQ(route->Duration(), 37800);
    ASSERT_EQ(route->Parts().size(), 1);
    ASSERT_TRUE(std::holds_alternative<TravelLeg>(route->Parts()[0]));

    const auto& leg = std::get<TravelLeg>(route->Parts()[0]);
    EXPECT_EQ(leg.From().Title(), "Саратов-1-Пасс.");
    EXPECT_EQ(leg.To().Title(), "Москва Павелецкая");
    EXPECT_EQ(leg.Thread().Title(), "Саратов — Москва");
    EXPECT_EQ(leg.Thread().Number(), "009Ж");
    EXPECT_EQ(leg.Thread().TransportType(), "train");
    EXPECT_EQ(leg.Thread().Carrier(), "РЖД");
}

TEST(RouteParserTest, ParsesTransferRouteAndSumsDuration) {
    RouteParser parser;
    auto route = parser.ParseRoute(TransferSegment());

    ASSERT_TRUE(route.has_value());
    EXPECT_TRUE(route->HasTransfers());
    EXPECT_EQ(route->Duration(), 27000);
    ASSERT_EQ(route->Parts().size(), 3);
    EXPECT_TRUE(std::holds_alternative<TravelLeg>(route->Parts()[0]));
    EXPECT_TRUE(std::holds_alternative<TransferLeg>(route->Parts()[1]));
    EXPECT_TRUE(std::holds_alternative<TravelLeg>(route->Parts()[2]));

    const auto& transfer = std::get<TransferLeg>(route->Parts()[1]);
    EXPECT_EQ(transfer.Point().Title(), "Ртищево-1");
    EXPECT_EQ(transfer.Duration(), 1800);
}

TEST(RouteParserTest, ParsesWholeScheduleAndSkipsBrokenSegments) {
    RouteParser parser;
    json data = {{"segments", json::array({DirectSegment(), json::object(), TransferSegment()})}};

    auto result = parser.ParseSchedule(data);

    EXPECT_EQ(result.Size(), 2);
    EXPECT_FALSE(result.Empty());
}

TEST(RoutePrinterTest, PrintsSimpleTextWithoutEmoji) {
    RouteParser parser;
    auto route = parser.ParseRoute(DirectSegment());
    ASSERT_TRUE(route.has_value());

    std::stringstream out;
    RoutePrinter printer(out);
    printer.PrintRoute(*route, 0);
    std::string text = out.str();

    EXPECT_NE(text.find("Маршрут 1: прямой"), std::string::npos);
    EXPECT_NE(text.find("Общее время: 10 ч. 30 мин."), std::string::npos);
    EXPECT_NE(text.find("поезд: Саратов — Москва"), std::string::npos);
    EXPECT_NE(text.find("Перевозчик: РЖД"), std::string::npos);
    EXPECT_EQ(text.find("🚆"), std::string::npos);
    EXPECT_EQ(text.find("🚌"), std::string::npos);
    EXPECT_EQ(text.find("✈"), std::string::npos);
}

TEST(TimeAndDurationTest, FormatsApiTimeAndDurationForPrinter) {
    auto parsed = ParseISO8601("2026-06-11T10:15:00+03:00");
    EXPECT_EQ(parsed.date, "11.06");
    EXPECT_EQ(parsed.time, "10:15");
    EXPECT_EQ(parsed.tz, "GMT+03");

    EXPECT_EQ(FormatDuration(0), "0 мин.");
    EXPECT_EQ(FormatDuration(-100), "0 мин.");
    EXPECT_EQ(FormatDuration(60), "1 мин.");
    EXPECT_EQ(FormatDuration(3661), "1 ч. 1 мин.");
}

TEST(CacheControlTest, StoresReadsExpiresAndClearsJson) {
    fs::path dir = MakeTempDir("lab7_cache_lifecycle");

    CacheControl cache(dir, 15, 10);
    cache.Put("route.json", {{"status", "ok"}});

    EXPECT_TRUE(cache.Has("route.json"));
    EXPECT_EQ(cache.Get("route.json")["status"], "ok");

    fs::last_write_time(dir / "route.json", fs::file_time_type::clock::now() - std::chrono::hours(1));
    EXPECT_FALSE(cache.Has("route.json"));

    cache.Clear();
    EXPECT_FALSE(fs::exists(dir / "route.json"));
    fs::remove_all(dir);
}

TEST(CacheControlTest, TrimsOldFilesToLimit) {
    fs::path dir = MakeTempDir("lab7_cache_trim");

    CacheControl cache(dir, 15, 2);
    cache.Put("first.json", {{"value", 1}});
    fs::last_write_time(dir / "first.json", fs::file_time_type::clock::now() - std::chrono::minutes(3));
    cache.Put("second.json", {{"value", 2}});
    fs::last_write_time(dir / "second.json", fs::file_time_type::clock::now() - std::chrono::minutes(2));
    cache.Put("third.json", {{"value", 3}});

    int files = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            ++files;
        }
    }
    EXPECT_EQ(files, 2);
    EXPECT_FALSE(fs::exists(dir / "first.json"));
    fs::remove_all(dir);
}

TEST(RequesterTest, UsesInjectedHttpClientAndCachesResult) {
    fs::path dir = MakeTempDir("lab7_request_cache");
    auto fake = std::make_shared<FakeHttpClient>(ScheduleJson().dump());
    Requester requester("TEST_KEY", fake, dir);

    auto first = requester.GetRoutes("саратов", "Масква", "11.06.2026");
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->Size(), 2);
    ASSERT_EQ(fake->Calls(), 1);
    EXPECT_NE(fake->LastUrl().find("api.rasp.yandex.net"), std::string::npos);
    EXPECT_EQ(fake->LastParams().at("apikey"), "TEST_KEY");
    EXPECT_EQ(fake->LastParams().at("date"), "2026-06-11");
    EXPECT_EQ(fake->LastParams().at("format"), "json");
    EXPECT_EQ(fake->LastParams().at("transfers"), "true");

    auto second = requester.GetRoutes("САРАТОВ", "Москва", "2026-06-11");
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->Size(), 2);
    EXPECT_EQ(fake->Calls(), 1);

    fs::remove_all(dir);
}

TEST(RequesterTest, InvalidDateDoesNotCallHttpClient) {
    fs::path dir = MakeTempDir("lab7_request_bad_date");
    auto fake = std::make_shared<FakeHttpClient>(ScheduleJson().dump());
    Requester requester("TEST_KEY", fake, dir);

    auto result = requester.GetRoute("Москва", "Саратов", "bad-date");

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(fake->Calls(), 0);
    fs::remove_all(dir);
}

TEST(RequesterTest, BadJsonFromHttpClientReturnsNullopt) {
    fs::path dir = MakeTempDir("lab7_request_bad_json");
    auto fake = std::make_shared<FakeHttpClient>(std::string{"{ broken json"});
    Requester requester("TEST_KEY", fake, dir);

    auto result = requester.GetRoute("Москва", "Саратов", "2026-06-11");

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(fake->Calls(), 1);
    fs::remove_all(dir);
}

TEST(RequesterTest, ReadsRouteFromPreparedCacheWithoutHttpCall) {
    fs::path dir = MakeTempDir("lab7_request_prepared_cache");
    CityCatalog catalog;
    std::string filename = catalog.CodeByName("Москва") + "_" + catalog.CodeByName("Саратов") + "_2026-06-11.json";

    CacheControl cache(dir);
    cache.Put(filename, ScheduleJson());

    auto fake = std::make_shared<FakeHttpClient>(std::nullopt);
    Requester requester("TEST_KEY", fake, dir);

    auto result = requester.GetRoute("москва", "саратов", "11.06.2026");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)["segments"].size(), 2);
    EXPECT_EQ(fake->Calls(), 0);
    fs::remove_all(dir);
}
