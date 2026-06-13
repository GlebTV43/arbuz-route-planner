#pragma once

#include <optional>
#include <string>
#include <vector>

struct RouteQuery {
    std::string from;
    std::string to;
    std::string date;
};

class ConsoleCommand {
public:
    enum class Type {
        kEmpty,
        kHelp,
        kExit,
        kCities,
        kClearCache,
        kRoute,
        kInvalid
    };

    static ConsoleCommand Empty();
    static ConsoleCommand Help();
    static ConsoleCommand Exit();
    static ConsoleCommand Cities();
    static ConsoleCommand ClearCache();
    static ConsoleCommand Route(RouteQuery query);
    static ConsoleCommand Invalid(std::string error);

    Type GetType() const;
    const std::optional<RouteQuery>& Query() const;
    const std::string& Error() const;

private:
    ConsoleCommand(Type type, std::optional<RouteQuery> query = std::nullopt, std::string error = "");

    Type type_;
    std::optional<RouteQuery> query_;
    std::string error_;
};

class CommandParser {
public:
    CommandParser(std::string default_from = "Санкт-Петербург", std::string default_to = "Саратов");

    ConsoleCommand ParseLine(const std::string& line) const;
    ConsoleCommand ParseArgs(const std::vector<std::string>& args) const;

private:
    ConsoleCommand ParseRouteLine(const std::string& line) const;
    ConsoleCommand ParseRouteBySemicolon(const std::string& line) const;
    ConsoleCommand ParseDateOnly(const std::string& line) const;
    std::vector<std::string> SplitBySemicolon(const std::string& line) const;

    std::string default_from_;
    std::string default_to_;
};
