#ifndef TASKMASTER_PROCESS_SUPERVISOR_HPP
#define TASKMASTER_PROCESS_SUPERVISOR_HPP

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <sys/types.h>

#include "taskmaster/diff/Diff.hpp"
#include "taskmaster/io/Logger.hpp"

namespace taskmaster {

enum class ProcessState {
    Starting,
    Running,
    Stopping,
    Exited,
    Fatal
};

struct ProcessStatus {
    std::string programName;
    unsigned int slot;
    pid_t pid;
    ProcessState state;
    unsigned int failedStarts;
};

struct ProcessSlot {
    pid_t pid = -1;
    ProcessState state = ProcessState::Exited;
    unsigned int failedStarts = 0;
    bool restartRequested = false;
    std::chrono::steady_clock::time_point startedAt;
    std::chrono::steady_clock::time_point stopDeadline;
};

struct ProgramRuntime {
    ProgramConfig configuration;
    std::vector<ProcessSlot> slots;
    bool wantsRunning = false;
    bool removalPending = false;
};


class ProcessSupervisor {
public:
    explicit ProcessSupervisor(Logger &logger);

    bool add(const ProgramConfig &configuration, std::string &error);
    bool remove(const std::string &programName, std::string &error);


    bool start(const std::string &programName, std::string &error);
    bool stop(const std::string &programName, std::string &error);

    bool restart(const std::string &programName, std::string &error);
    bool update(const ProgramConfig &configuration, UpdateEffect effect, std::string &error);

    void tick();
    std::vector<ProcessStatus> status() const;

    std::optional<std::chrono::steady_clock::time_point> nextDeadline() const;

private:

    bool launch(ProgramRuntime &program, ProcessSlot &slot, std::string &error);
    void requestStop(ProgramRuntime &program, ProcessSlot &slot);
    void reap(ProgramRuntime &program, ProcessSlot &slot);
    void handleExit(ProgramRuntime &program, ProcessSlot &slot, int waitStatus);
    void maintain(ProgramRuntime &program);
    void trimStoppedExtraSlots(ProgramRuntime &program);
    bool restartWithConfiguration(const ProgramConfig &configuration, std::string &error);
    void pruneRemovedPrograms();

    std::map<std::string, ProgramRuntime> programs_;
    Logger &logger_;
};

const char *toString(ProcessState state);

}

#endif
