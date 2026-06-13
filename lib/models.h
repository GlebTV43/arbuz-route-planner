#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

using json = nlohmann::json;

class Station {
public:
    Station() = default;
    Station(std::string title, std::string code, std::string type, std::string transport_type);

    static Station FromJson(const json& data);

    const std::string& Title() const;
    const std::string& Code() const;
    const std::string& Type() const;
    const std::string& TransportType() const;

private:
    std::string title_ = "Неизвестная станция";
    std::string code_;
    std::string type_;
    std::string transport_type_;
};

class TransportThread {
public:
    TransportThread() = default;
    TransportThread(std::string title, std::string number, std::string transport_type,
                    std::string vehicle, std::string carrier);

    static TransportThread FromJson(const json& data);

    const std::string& Title() const;
    const std::string& Number() const;
    const std::string& TransportType() const;
    const std::string& Vehicle() const;
    const std::string& Carrier() const;

private:
    std::string title_ = "Рейс";
    std::string number_;
    std::string transport_type_ = "unknown";
    std::string vehicle_;
    std::string carrier_;
};

class TravelLeg {
public:
    TravelLeg() = default;
    TravelLeg(Station from, Station to, TransportThread thread, std::string departure,
              std::string arrival, int duration_seconds);

    static std::optional<TravelLeg> FromJson(const json& data);

    const Station& From() const;
    const Station& To() const;
    const TransportThread& Thread() const;
    const std::string& Departure() const;
    const std::string& Arrival() const;
    int Duration() const;

private:
    Station from_;
    Station to_;
    TransportThread thread_;
    std::string departure_;
    std::string arrival_;
    int duration_seconds_ = 0;
};

class TransferLeg {
public:
    TransferLeg() = default;
    TransferLeg(Station point, Station from, Station to, int duration_seconds);

    static std::optional<TransferLeg> FromJson(const json& data);

    const Station& Point() const;
    const Station& From() const;
    const Station& To() const;
    int Duration() const;

private:
    Station point_;
    Station from_;
    Station to_;
    int duration_seconds_ = 0;
};

using RoutePart = std::variant<TravelLeg, TransferLeg>;

class RouteInfo {
public:
    RouteInfo() = default;
    RouteInfo(bool has_transfers, std::string departure, std::string arrival,
              int duration_seconds, std::vector<RoutePart> parts);

    bool HasTransfers() const;
    const std::string& Departure() const;
    const std::string& Arrival() const;
    int Duration() const;
    const std::vector<RoutePart>& Parts() const;
    bool Empty() const;

private:
    bool has_transfers_ = false;
    std::string departure_;
    std::string arrival_;
    int duration_seconds_ = 0;
    std::vector<RoutePart> parts_;
};

class ScheduleResult {
public:
    explicit ScheduleResult(std::vector<RouteInfo> routes = {});

    const std::vector<RouteInfo>& Routes() const;
    bool Empty() const;
    std::size_t Size() const;

private:
    std::vector<RouteInfo> routes_;
};

class RouteParser {
public:
    ScheduleResult ParseSchedule(const json& data) const;
    std::optional<RouteInfo> ParseRoute(const json& data) const;

private:
    std::vector<RoutePart> ParseDetails(const json& details) const;
    int SumDuration(const std::vector<RoutePart>& parts) const;
};

class RoutePrinter {
public:
    explicit RoutePrinter(std::ostream& out);

    void PrintRoute(const RouteInfo& route, int index) const;
    void PrintSchedule(const ScheduleResult& result) const;
    void PrintTravelLeg(const TravelLeg& leg, int number) const;
    void PrintTransferLeg(const TransferLeg& leg) const;

private:
    std::ostream& out_;
};
