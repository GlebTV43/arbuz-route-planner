#include "../lib/application.h"

#include <iostream>
#include <string>
#include <vector>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    try {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }

        Requester requester("");
        ConsoleApplication app(std::move(requester), std::cin, std::cout);
        return app.Run(args);
    } catch (const std::exception& error) {
        std::cerr << "Ошибка запуска: " << error.what() << "\n";
        return 1;
    }
}
