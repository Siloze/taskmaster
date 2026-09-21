#ifndef TASKMASTER_COMMAND_HPP
#define TASKMASTER_COMMAND_HPP

#include "event.hpp"

#include <string>
#include <unordered_map>
#include <vector>

/* CLI -> args
 * https://en.cppreference.com/cpp/container/unordered_map
 */

namespace taskmaster::gchinaul {

    class CommandMap {
        public:
            CommandMap();

            bool find(const std::string& name, CmdType& out) const;
            std::vector<std::string> names() const;

        private:
            std::unordered_map<std::string, CmdType> commands_;
    };

    //to do (besoin de comprendre la suite)
}

#endif