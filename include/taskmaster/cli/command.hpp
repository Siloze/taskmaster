#ifndef TASKMASTER_COMMAND_HPP
#define TASKMASTER_COMMAND_HPP

#include "event.hpp"

#include <string>
#include <unordered_map>
#include <vector>

/* CLI -> args
 * https://en.cppreference.com/cpp/container/unordered_map
 */

namespace taskmaster {

    class CommandMap {
        public:
            CommandMap();

            bool find(const std::string& name, CmdType& out) const;
            std::vector<std::string> names() const;

        private:
            std::unordered_map<std::string, CmdType> commands_;
    };

    /* CommandType::start -> "sleepy" */
    class CommandParser
    {
        public:
            explicit CommandParser(const CommandMap& map);

            bool parse(
                const std::string& line,
                CmdArgs& out,
                std::string& error
            ) const;

        private:
            const CommandMap& map_;

            static std::vector<std::string> tokenize(
                const std::string& line
            );
    };
}

#endif