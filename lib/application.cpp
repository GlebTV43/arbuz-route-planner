#include "application.h"
#include "utils.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

ConsoleApplication::ConsoleApplication(Requester requester, std::istream& input, std::ostream& output)
    : requester_(std::move(requester)), parser_(), input_(input), output_(output) {}

int ConsoleApplication::Run(const std::vector<std::string>& args) {
    if (!args.empty()) {
        ConsoleCommand command = parser_.ParseArgs(args);
        Execute(command);
        return command.GetType() == ConsoleCommand::Type::kInvalid ? 1 : 0;
    }

    PrintHelp();
    return RunInteractive();
}

int ConsoleApplication::RunInteractive() {
    std::string line;
    while (true) {
        output_ << "\n> ";
        if (!std::getline(input_, line)) {
            return 0;
        }
        ConsoleCommand command = parser_.ParseLine(line);
        if (!Execute(command)) {
            return 0;
        }
    }
}

bool ConsoleApplication::Execute(const ConsoleCommand& command) {
    switch (command.GetType()) {
        case ConsoleCommand::Type::kEmpty:
            return true;
        case ConsoleCommand::Type::kHelp:
            PrintHelp();
            return true;
        case ConsoleCommand::Type::kExit:
            output_ << "Завершение работы.\n";
            return false;
        case ConsoleCommand::Type::kCities:
            PrintCities();
            return true;
        case ConsoleCommand::Type::kClearCache:
            requester_.ClearCache();
            output_ << "Кэш очищен.\n";
            return true;
        case ConsoleCommand::Type::kUpdateCities:
            try {
                std::size_t count = requester_.UpdateCities();
                output_ << "Файл cities.json обновлен. Городов: " << count << "\n";
            } catch (const std::exception& error) {
                output_ << "Ошибка обновления городов: " << error.what() << "\n";
            }
            return true;
        case ConsoleCommand::Type::kRoute:
            if (command.Query()) {
                PrintRoute(*command.Query());
            }
            return true;
        case ConsoleCommand::Type::kInvalid:
            output_ << "Ошибка: " << command.Error() << "\n";
            return true;
    }
    return true;
}

void ConsoleApplication::PrintHelp() const {
    output_ << "Поиск маршрутов Яндекс Расписаний\n";
    output_ << "Команды:\n";
    output_ << "  YYYY-MM-DD                 маршрут Санкт-Петербург -> Саратов\n";
    output_ << "  back YYYY-MM-DD            обратный маршрут Саратов -> Санкт-Петербург\n";
    output_ << "  route откуда; куда; дата   произвольный маршрут\n";
    output_ << "  route откуда -> куда дата  произвольный маршрут без пробелов в городах\n";
    output_ << "  cities                     вывести список городов\n";
    output_ << "  update cities              обновить data/cities.json из API\n";
    output_ << "  clear                      очистить кэш\n";
    output_ << "  help                       показать помощь\n";
    output_ << "  exit                       выйти\n";
}

void ConsoleApplication::PrintCities() const {
    auto cities = requester_.GetCities();
    output_ << "Доступные города: " << cities.size() << "\n";
    for (const auto& city : cities) {
        output_ << "  " << city << "\n";
    }
}

void ConsoleApplication::PrintRoute(const RouteQuery& query) const {
    try {
        std::string from = requester_.GetCanonicalCity(query.from);
        std::string to = requester_.GetCanonicalCity(query.to);

        output_ << "Маршрут: " << from << " -> " << to << "\n";
        output_ << "Дата: " << query.date << "\n";

        auto result = requester_.GetRoutes(from, to, query.date);
        if (!result) {
            output_ << "Маршруты не найдены или сервис временно недоступен.\n";
            return;
        }

        RoutePrinter printer(output_);
        printer.PrintSchedule(*result);
    } catch (const std::out_of_range& error) {
        std::string message = error.what();
        output_ << "Ошибка: " << message << "\n";

        std::size_t sep = message.find(':');
        if (sep != std::string::npos && sep + 1 < message.size()) {
            PrintCityError(Trim(message.substr(sep + 1)));
        }
    } catch (const std::exception& error) {
        output_ << "Ошибка: " << error.what() << "\n";
    }
}

void ConsoleApplication::PrintCityError(const std::string& city) const {
    output_ << "Город не найден: " << city << "\n";
    auto nearest = requester_.FindNearestCities(city);
    if (!nearest.empty()) {
        output_ << "Похожие города:\n";
        for (const auto& value : nearest) {
            output_ << "  " << value << "\n";
        }
    }
}
