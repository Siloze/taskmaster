#ifndef TASKMASTER_CONFIG_CONFIG_PARSER_HPP
#define TASKMASTER_CONFIG_CONFIG_PARSER_HPP

#include <map>
#include <string>

#include "taskmaster/runtime/ProgramConfig.hpp"

namespace taskmaster {

using JobMap = std::map<std::string, ProgramConfig>;

struct TaskmasterConfig {
    std::string logFile = "/tmp/taskmaster.log";
    JobMap programs;
};

class ConfigParser {
public:
    bool parseFile(const std::string &path, TaskmasterConfig &configuration,
                   std::string &error) const;
};

}

#endif
