#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <signal.h>

namespace taskmaster {

enum class AutoRestart {
    Never,
    Always,
    Unexpected
};

struct ProgramConfig {
    std::string name;
    std::vector<std::string> argv;

    unsigned int fileUmask = 022;
    unsigned int numProcesses = 1;

    bool autostart = true;

    int stopSignal = SIGTERM;
    unsigned int startSeconds = 1;
    unsigned int startRetries = 3;
    unsigned int stopGraceSeconds = 10;
    std::set<int> expectedExitCodes = {0};

    AutoRestart autorestart = AutoRestart::Unexpected;

    std::optional<std::string> stdoutPath;
    std::optional<std::string> stderrPath;

    std::map<std::string, std::string> environment;
    std::optional<std::string> workingDirectory;

    std::string validate() const;
};

}

