#include "models.h"
#include "utils.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <type_traits>
#include <utility>

namespace {

std::string ReadString(const json& data, const std::string& key, const std::string& fallback = "") {
    if (!data.is_object() || !data.contains(key) || !data[key].is_string()) {
        return fallback;
    }
    return data[key].get<std::string>();
}

int ReadDuration(const json& data) {
    if (!data.is_object() || !data.contains("duration") || !data["duration"].is_number()) {
        return 0;
    }
    return static_cast<int>(data["duration"].get<double>());
}

Station ReadStation(const json& data, const std::string& key) {
    if (!data.is_object() || !data.contains(key)) {
        return Station();
    }
    return Station::FromJson(data[key]);
}

std::string TransportName(const std::string& type) {
    if (type == "plane") return "самолет";
    if (type == "train") return "поезд";
    if (type == "suburban") return "электричка";
    if (type == "bus") return "автобус";
    if (type == "water") return "водный транспорт";
    if (type == "helicopter") return "вертолет";
    return type.empty() ? "транспорт" : type;
}

std::string FormatDateTime(const std::string& iso) {
    auto value = ParseISO8601(iso);
    std::string result = value.date + " " + value.time;
    if (!value.tz.empty()) {
        result += " " + value.tz;
    }
    return result;
}

std::string NonEmpty(const std::string& value, const std::string& fallback) {
    return value.empty() ? fallback : value;
}

}  // namespace

Station::Station(std::string title, std::string code, std::string type, std::string transport_type)
    : title_(std::move(title)), code_(std::move(code)), type_(std::move(type)), transport_type_(std::move(transport_type)) {}

Station Station::FromJson(const json& data) {
    if (!data.is_object()) {
        return Station();
    }

    std::string title = ReadString(data, "title", "Неизвестная станция");
    if (title.empty()) {
        title = ReadString(data, "popular_title", "Неизвестная станция");
    }

    return Station(title, ReadString(data, "code"), ReadString(data, "type"), ReadString(data, "transport_type"));
}

const std::string& Station::Title() const { return title_; }
const std::string& Station::Code() const { return code_; }
const std::string& Station::Type() const { return type_; }
const std::string& Station::TransportType() const { return transport_type_; }

TransportThread::TransportThread(std::string title, std::string number, std::string transport_type,
                                 std::string vehicle, std::string carrier)
    : title_(std::move(title)), number_(std::move(number)), transport_type_(std::move(transport_type)),
      vehicle_(std::move(vehicle)), carrier_(std::move(carrier)) {}

TransportThread TransportThread::FromJson(const json& data) {
    if (!data.is_object()) {
        return TransportThread();
    }

    std::string carrier;
    if (data.contains("carrier") && data["carrier"].is_object()) {
        carrier = ReadString(data["carrier"], "title");
    }

    return TransportThread(ReadString(data, "title", "Рейс"), ReadString(data, "number"),
                           ReadString(data, "transport_type", "unknown"), ReadString(data, "vehicle"), carrier);
}

const std::string& TransportThread::Title() const { return title_; }
const std::string& TransportThread::Number() const { return number_; }
const std::string& TransportThread::TransportType() const { return transport_type_; }
const std::string& TransportThread::Vehicle() const { return vehicle_; }
const std::string& TransportThread::Carrier() const { return carrier_; }

TravelLeg::TravelLeg(Station from, Station to, TransportThread thread, std::string departure,
                     std::string arrival, int duration_seconds)
    : from_(std::move(from)), to_(std::move(to)), thread_(std::move(thread)), departure_(std::move(departure)),
      arrival_(std::move(arrival)), duration_seconds_(duration_seconds) {}

std::optional<TravelLeg> TravelLeg::FromJson(const json& data) {
    if (!data.is_object() || !data.contains("thread") || data["thread"].is_null()) {
        return std::nullopt;
    }

    return TravelLeg(ReadStation(data, "from"), ReadStation(data, "to"), TransportThread::FromJson(data["thread"]),
                     ReadString(data, "departure"), ReadString(data, "arrival"), ReadDuration(data));
}

const Station& TravelLeg::From() const { return from_; }
const Station& TravelLeg::To() const { return to_; }
const TransportThread& TravelLeg::Thread() const { return thread_; }
const std::string& TravelLeg::Departure() const { return departure_; }
const std::string& TravelLeg::Arrival() const { return arrival_; }
int TravelLeg::Duration() const { return duration_seconds_; }

TransferLeg::TransferLeg(Station point, Station from, Station to, int duration_seconds)
    : point_(std::move(point)), from_(std::move(from)), to_(std::move(to)), duration_seconds_(duration_seconds) {}

std::optional<TransferLeg> TransferLeg::FromJson(const json& data) {
    if (!data.is_object()) {
        return std::nullopt;
    }

    bool is_transfer = false;
    if (data.contains("is_transfer") && data["is_transfer"].is_boolean()) {
        is_transfer = data["is_transfer"].get<bool>();
    }
    if (ReadString(data, "type") == "transfer") {
        is_transfer = true;
    }
    if (!is_transfer) {
        return std::nullopt;
    }

    return TransferLeg(ReadStation(data, "transfer_point"), ReadStation(data, "transfer_from"),
                       ReadStation(data, "transfer_to"), ReadDuration(data));
}

const Station& TransferLeg::Point() const { return point_; }
const Station& TransferLeg::From() const { return from_; }
const Station& TransferLeg::To() const { return to_; }
int TransferLeg::Duration() const { return duration_seconds_; }

RouteInfo::RouteInfo(bool has_transfers, std::string departure, std::string arrival,
                     int duration_seconds, std::vector<RoutePart> parts)
    : has_transfers_(has_transfers), departure_(std::move(departure)), arrival_(std::move(arrival)),
      duration_seconds_(duration_seconds), parts_(std::move(parts)) {}

bool RouteInfo::HasTransfers() const { return has_transfers_; }
const std::string& RouteInfo::Departure() const { return departure_; }
const std::string& RouteInfo::Arrival() const { return arrival_; }
int RouteInfo::Duration() const { return duration_seconds_; }
const std::vector<RoutePart>& RouteInfo::Parts() const { return parts_; }
bool RouteInfo::Empty() const { return parts_.empty(); }

ScheduleResult::ScheduleResult(std::vector<RouteInfo> routes) : routes_(std::move(routes)) {}
const std::vector<RouteInfo>& ScheduleResult::Routes() const { return routes_; }
bool ScheduleResult::Empty() const { return routes_.empty(); }
std::size_t ScheduleResult::Size() const { return routes_.size(); }

ScheduleResult RouteParser::ParseSchedule(const json& data) const {
    if (!data.is_object() || !data.contains("segments") || !data["segments"].is_array()) {
        return ScheduleResult();
    }

    std::vector<RouteInfo> routes;
    for (const auto& segment : data["segments"]) {
        auto route = ParseRoute(segment);
        if (route && !route->Empty()) {
            routes.push_back(std::move(*route));
        }
    }
    return ScheduleResult(std::move(routes));
}

std::optional<RouteInfo> RouteParser::ParseRoute(const json& data) const {
    if (!data.is_object()) {
        return std::nullopt;
    }

    bool has_transfers = false;
    if (data.contains("has_transfers") && data["has_transfers"].is_boolean()) {
        has_transfers = data["has_transfers"].get<bool>();
    }

    std::vector<RoutePart> parts;
    if (has_transfers && data.contains("details") && data["details"].is_array()) {
        parts = ParseDetails(data["details"]);
    } else {
        auto leg = TravelLeg::FromJson(data);
        if (leg) {
            parts.emplace_back(std::move(*leg));
        }
    }

    int duration = ReadDuration(data);
    if (duration <= 0) {
        duration = SumDuration(parts);
    }

    return RouteInfo(has_transfers, ReadString(data, "departure"), ReadString(data, "arrival"), duration, std::move(parts));
}

std::vector<RoutePart> RouteParser::ParseDetails(const json& details) const {
    std::vector<RoutePart> parts;
    for (const auto& item : details) {
        if (auto leg = TravelLeg::FromJson(item)) {
            parts.emplace_back(std::move(*leg));
        } else if (auto transfer = TransferLeg::FromJson(item)) {
            parts.emplace_back(std::move(*transfer));
        }
    }
    return parts;
}

int RouteParser::SumDuration(const std::vector<RoutePart>& parts) const {
    int total = 0;
    for (const auto& part : parts) {
        total += std::visit([](const auto& value) { return value.Duration(); }, part);
    }
    return total;
}

RoutePrinter::RoutePrinter(std::ostream& out) : out_(out) {}

void RoutePrinter::PrintSchedule(const ScheduleResult& result) const {
    if (result.Empty()) {
        out_ << "Маршруты не найдены.\n";
        return;
    }

    for (std::size_t i = 0; i < result.Routes().size(); ++i) {
        PrintRoute(result.Routes()[i], static_cast<int>(i));
    }
}

void RoutePrinter::PrintRoute(const RouteInfo& route, int index) const {
    out_ << "\nМаршрут " << index + 1 << ": " << (route.HasTransfers() ? "с пересадкой" : "прямой") << "\n";
    out_ << std::string(58, '-') << "\n";
    out_ << "Общее время: " << FormatDuration(route.Duration()) << "\n";

    int leg_number = 1;
    for (const auto& part : route.Parts()) {
        std::visit([&](const auto& value) {
            using PartType = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<PartType, TravelLeg>) {
                PrintTravelLeg(value, leg_number++);
            } else {
                PrintTransferLeg(value);
            }
        }, part);
    }
}

void RoutePrinter::PrintTravelLeg(const TravelLeg& leg, int number) const {
    const auto& thread = leg.Thread();
    out_ << "\n" << number << ") " << TransportName(thread.TransportType()) << ": " << NonEmpty(thread.Title(), "Рейс") << "\n";
    if (!thread.Number().empty()) {
        out_ << "   Номер: " << thread.Number() << "\n";
    }
    if (!thread.Vehicle().empty()) {
        out_ << "   Транспорт: " << thread.Vehicle() << "\n";
    }
    if (!thread.Carrier().empty()) {
        out_ << "   Перевозчик: " << thread.Carrier() << "\n";
    }
    out_ << "   Отправление: " << FormatDateTime(leg.Departure()) << ", " << leg.From().Title() << "\n";
    out_ << "   Прибытие:   " << FormatDateTime(leg.Arrival()) << ", " << leg.To().Title() << "\n";
    out_ << "   В пути: " << FormatDuration(leg.Duration()) << "\n";
}

void RoutePrinter::PrintTransferLeg(const TransferLeg& leg) const {
    out_ << "\n   Пересадка";
    if (!leg.Point().Title().empty() && leg.Point().Title() != "Неизвестная станция") {
        out_ << ": " << leg.Point().Title();
    }
    out_ << "\n";
    out_ << "   Ожидание: " << FormatDuration(leg.Duration()) << "\n";
    if (leg.From().Title() != "Неизвестная станция" || leg.To().Title() != "Неизвестная станция") {
        out_ << "   Место: " << leg.From().Title() << " -> " << leg.To().Title() << "\n";
    }
}
