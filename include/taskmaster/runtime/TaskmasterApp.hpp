#pragma once

#include <memory>
#include <string>

#include "taskmaster/diff/DiffBuilder.hpp"
#include "taskmaster/diff/DiffExecutor.hpp"
#include "taskmaster/io/SignalPipe.hpp"
#include "taskmaster/cli/cli.hpp"

namespace taskmaster {

class TaskmasterApp {
public:
    explicit TaskmasterApp(const std::string &configPath);

    int run();

private:
    bool initialize();
    void beginShutdown(const std::string &reason);

    void eventLoop();

    bool reloadConfiguration();
    void executeCommand(const CmdArgs &args);

    bool allChildrenStopped() const;
    int nextPollTimeoutMs() const;

    void printStatus();

    ConfigParser parser_;
    SignalPipe signalPipe_;
    std::string configPath_;
    DiffBuilder diffBuilder_;
    std::unique_ptr<Logger> logger_;
    TaskmasterConfig configuration_;
    std::unique_ptr<ControlShell> shell_;
    std::unique_ptr<DiffExecutor> executor_;
    std::unique_ptr<ProcessSupervisor> supervisor_;

    bool shuttingDown_;

};

}
