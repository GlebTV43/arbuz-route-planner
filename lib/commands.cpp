#include "commands.h"
#include "utils.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace {

bool StartsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::string Join(const std::vector<std::string>& parts, std::size_t begin, std::size_t end) {
    std::string result;
    for (std::size_t i = begin; i < end; ++i) {
        if (!result.empty()) {
            result += " ";
        }
        result += parts[i];
    }
    return result;
}

std::vector<std::string> SplitWords(const std::string& line) {
    std::stringstream input(line);
    std::vector<std::string> result;
    std::string word;
    while (input >> word) {
        result.push_back(word);
    }
    return result;
}

}  // namespace

ConsoleCommand::ConsoleCommand(Type type, std::optional<RouteQuery> query, std::string error)
    : type_(type), query_(std::move(query)), error_(std::move(error)) {}

ConsoleCommand ConsoleCommand::Empty() { return ConsoleCommand(Type::kEmpty); }
ConsoleCommand ConsoleCommand::Help() { return ConsoleCommand(Type::kHelp); }
ConsoleCommand ConsoleCommand::Exit() { return ConsoleCommand(Type::kExit); }
ConsoleCommand ConsoleCommand::Cities() { return ConsoleCommand(Type::kCities); }
ConsoleCommand ConsoleCommand::ClearCache() { return ConsoleCommand(Type::kClearCache); }
ConsoleCommand ConsoleCommand::Route(RouteQuery query) { return ConsoleCommand(Type::kRoute, std::move(query)); }
ConsoleCommand ConsoleCommand::Invalid(std::string error) { return ConsoleCommand(Type::kInvalid, std::nullopt, std::move(error)); }

ConsoleCommand::Type ConsoleCommand::GetType() const { return type_; }
const std::optional<RouteQuery>& ConsoleCommand::Query() const { return query_; }
const std::string& ConsoleCommand::Error() const { return error_; }

CommandParser::CommandParser(std::string default_from, std::string default_to)
    : default_from_(std::move(default_from)), default_to_(std::move(default_to)) {}

ConsoleCommand CommandParser::ParseLine(const std::string& raw_line) const {
    std::string line = Trim(raw_line);
    if (line.empty()) {
        return ConsoleCommand::Empty();
    }

    std::string normalized = Normalize(line);
    if (normalized == "help" || normalized == "h" || normalized == "помощь") {
        return ConsoleCommand::Help();
    }
    if (normalized == "exit" || normalized == "quit" || normalized == "q" || normalized == "выход") {
        return ConsoleCommand::Exit();
    }
    if (normalized == "cities" || normalized == "города") {
        return ConsoleCommand::Cities();
    }
    if (normalized == "clear" || normalized == "clear cache" || normalized == "cache clear") {
        return ConsoleCommand::ClearCache();
    }

    if (StartsWith(normalized, "route ") || StartsWith(normalized, "маршрут ")) {
        return ParseRouteLine(line);
    }

    if (line.find(';') != std::string::npos) {
        return ParseRouteBySemicolon(line);
    }

    if (StartsWith(normalized, "back ") || StartsWith(normalized, "назад ")) {
        std::size_t first_space = line.find(' ');
        std::string date = NormalizeDate(line.substr(first_space + 1));
        if (date.empty()) {
            return ConsoleCommand::Invalid("Неверная дата для обратного маршрута");
        }
        return ConsoleCommand::Route({default_to_, default_from_, date});
    }

    return ParseDateOnly(line);
}

ConsoleCommand CommandParser::ParseArgs(const std::vector<std::string>& args) const {
    if (args.empty()) {
        return ConsoleCommand::Empty();
    }
    return ParseLine(Join(args, 0, args.size()));
}

ConsoleCommand CommandParser::ParseRouteLine(const std::string& raw_line) const {
    std::string line = raw_line;
    std::size_t first_space = line.find(' ');
    if (first_space == std::string::npos) {
        return ConsoleCommand::Invalid("После route нужно указать маршрут и дату");
    }
    line = Trim(line.substr(first_space + 1));

    auto semicolon = ParseRouteBySemicolon(line);
    if (semicolon.GetType() != ConsoleCommand::Type::kInvalid) {
        return semicolon;
    }

    std::vector<std::string> words = SplitWords(line);
    if (words.size() < 3) {
        return ConsoleCommand::Invalid("Формат: route город_откуда; город_куда; дата");
    }

    std::string date = NormalizeDate(words.back());
    if (date.empty()) {
        return ConsoleCommand::Invalid("Дата должна быть в формате YYYY-MM-DD или DD.MM.YYYY");
    }

    auto arrow = std::find(words.begin(), words.end(), "->");
    if (arrow == words.end()) {
        return ConsoleCommand::Invalid("Для городов с пробелами используй формат: route откуда; куда; дата");
    }

    std::size_t arrow_index = static_cast<std::size_t>(arrow - words.begin());
    if (arrow_index == 0 || arrow_index + 1 >= words.size() - 1) {
        return ConsoleCommand::Invalid("Неверный формат маршрута");
    }

    std::string from = Join(words, 0, arrow_index);
    std::string to = Join(words, arrow_index + 1, words.size() - 1);
    return ConsoleCommand::Route({from, to, date});
}

ConsoleCommand CommandParser::ParseRouteBySemicolon(const std::string& line) const {
    auto parts = SplitBySemicolon(line);
    if (parts.size() == 1) {
        return ConsoleCommand::Invalid("");
    }
    if (parts.size() != 3) {
        return ConsoleCommand::Invalid("Формат через точку с запятой: откуда; куда; дата");
    }

    std::string from = parts[0];
    std::string to = parts[1];
    std::string date = NormalizeDate(parts[2]);
    if (from.empty() || to.empty() || date.empty()) {
        return ConsoleCommand::Invalid("Нужно указать два города и корректную дату");
    }
    return ConsoleCommand::Route({from, to, date});
}

ConsoleCommand CommandParser::ParseDateOnly(const std::string& line) const {
    std::string date = NormalizeDate(line);
    if (date.empty()) {
        return ConsoleCommand::Invalid("Не понял команду. Введи help для списка команд");
    }
    return ConsoleCommand::Route({default_from_, default_to_, date});
}

std::vector<std::string> CommandParser::SplitBySemicolon(const std::string& line) const {
    std::stringstream input(line);
    std::vector<std::string> result;
    std::string part;
    while (std::getline(input, part, ';')) {
        result.push_back(Trim(part));
    }
    return result;
}
