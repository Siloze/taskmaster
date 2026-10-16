#include "taskmaster/runtime/ProgramConfig.hpp"

namespace taskmaster {

// Si une config de programme (ce qui es tdans le ficheir de config) est valide
std::string ProgramConfig::validate() const {
    if (name.empty())
        return "program name cannot be empty";
    if (argv.empty() || argv.front().empty())
        return "program '" + name + "' has no command";
    if (numProcesses == 0)
        return "program '" + name + "' must have at least one process";
    if (stopSignal <= 0 || stopSignal >= NSIG)
        return "program '" + name + "' has an invalid stop signal";
    for (const std::pair<const std::string, std::string> &entry : environment) {
        if (entry.first.empty() || entry.first.find('=') != std::string::npos)
            return "program '" + name + "' has an invalid environment key";
    }
    return "";
}

}
