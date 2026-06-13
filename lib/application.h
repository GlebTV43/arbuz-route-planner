#pragma once

#include "commands.h"
#include "request.h"

#include <iosfwd>
#include <string>
#include <vector>

class ConsoleApplication {
public:
    ConsoleApplication(Requester requester, std::istream& input, std::ostream& output);

    int Run(const std::vector<std::string>& args);

private:
    int RunInteractive();
    bool Execute(const ConsoleCommand& command);
    void PrintHelp() const;
    void PrintCities() const;
    void PrintRoute(const RouteQuery& query) const;
    void PrintCityError(const std::string& city) const;

    Requester requester_;
    CommandParser parser_;
    std::istream& input_;
    std::ostream& output_;
};
