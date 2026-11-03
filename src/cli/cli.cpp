#include "taskmaster/cli/cli.hpp"

#include <iostream>
#include <utility>

namespace taskmaster {

    ControlShell::ControlShell(

        const CommandParser& parser,
        CommandHandler handler
    )
        : parser_(parser),
        handler_(std::move(handler))
    {
    }

    void ControlShell::run()
    {
        std::string line;

        while (true) {

            std::cout << "taskmaster> " << std::flush;
            if (!std::getline(std::cin, line)) {
                break;
            }

            if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
                continue;
            }

            CmdArgs args;
            std::string error;

            if (!parser_.parse(line, args, error)) {
                std::cerr << "Erreur :" << error << '\n';
                continue;
            }

            handler_(args);

            if (args.command == CmdType::ShutDown) {
                break;
            }
        }
    }
}