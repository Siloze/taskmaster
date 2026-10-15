#ifndef TASKMASTER_CLI_HPP
#define TASKMASTER_CLI_HPP

#include "command.hpp"

/* https://en.cppreference.com/cpp/header/functional */

#include <functional>
#include <string>

namespace taskmaster::gchinaul {

    class ControlShell {

        public:

            using CommandHandler = std::function<void(const CmdArgs&)>;

            ControlShell(
                const CommandParser& parser,
                CommandHandler handler
            );

            void run();

        private:

            const CommandParser& parser_;
            CommandHandler handler_;
    };
}

/* to do cli.cpp  bientôt */

#endif