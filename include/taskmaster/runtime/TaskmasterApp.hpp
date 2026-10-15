#pragma once

#include <memory>
#include <string>

#include "taskmaster/diff/DiffBuilder.hpp"
#include "taskmaster/diff/DiffExecutor.hpp"
#include "taskmaster/io/SignalPipe.hpp"

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
    void executeCommand(const std::string &line);

    bool allChildrenStopped() const;
    int nextPollTimeoutMs() const;
    void consumeStandardInput();

    void printStatus();

    std::string configPath_;
    std::string inputBuffer_;
    TaskmasterConfig configuration_;
    ConfigParser parser_;
    DiffBuilder diffBuilder_;
    SignalPipe signalPipe_;
    std::unique_ptr<Logger> logger_;
    std::unique_ptr<ProcessSupervisor> supervisor_;
    std::unique_ptr<DiffExecutor> executor_;
    bool shuttingDown_;
};

}
