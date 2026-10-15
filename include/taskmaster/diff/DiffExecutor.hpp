#pragma once

#include <string>
#include <vector>

#include "taskmaster/diff/Diff.hpp"
#include "taskmaster/io/Logger.hpp"
#include "taskmaster/runtime/ProcessSupervisor.hpp"

namespace taskmaster {

class DiffExecutor {
public:
    DiffExecutor(ProcessSupervisor &supervisor, Logger &logger);

    bool apply(const Diff &diff, std::vector<std::string> &errors);

private:
    ProcessSupervisor &supervisor_;
    Logger &logger_;
};

}

