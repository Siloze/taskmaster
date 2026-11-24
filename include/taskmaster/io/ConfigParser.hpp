#pragma once

#include <map>
#include <string>
#include "taskmaster/runtime/ProgramConfig.hpp"

namespace taskmaster {

using JobMap = std::map<std::string, ProgramConfig>;

struct TaskmasterConfig {
    JobMap programs;
    std::string logFile = "/tmp/taskmaster.log";
};

class ConfigParser {
    public:
        bool parseFile(const std::string &path, TaskmasterConfig &configuration, std::string &error) const;
    };
}
