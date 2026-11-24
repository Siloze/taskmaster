#pragma once

#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <optional>

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
    bool restartRequested = false;
    unsigned int failedStarts = 0;
    ProcessState state = ProcessState::Exited;
    std::chrono::steady_clock::time_point startedAt;
    std::chrono::steady_clock::time_point stopDeadline;
};

struct ProgramRuntime {
    bool wantsRunning = false;
    ProgramConfig configuration;
    bool removalPending = false;
    std::vector<ProcessSlot> slots;
};


class ProcessSupervisor {
    public:
        explicit ProcessSupervisor(Logger &logger);

        void tick();

        bool remove(const std::string &programName, std::string &error);
        bool add(const ProgramConfig &configuration, std::string &error);


        bool stop(const std::string &programName, std::string &error);
        bool start(const std::string &programName, std::string &error);

        bool restart(const std::string &programName, std::string &error);
        bool update(const ProgramConfig &configuration, UpdateEffect effect, std::string &error);

        std::vector<ProcessStatus> status() const;

        std::optional<std::chrono::steady_clock::time_point> nextDeadline() const;


    private:

        void pruneRemovedPrograms();

        void maintain(ProgramRuntime &program);
        void trimStoppedExtraSlots(ProgramRuntime &program);
        void reap(ProgramRuntime &program, ProcessSlot &slot);
        void requestStop(ProgramRuntime &program, ProcessSlot &slot);
        bool launch(ProgramRuntime &program, ProcessSlot &slot, std::string &error);
        void handleExit(ProgramRuntime &program, ProcessSlot &slot, int waitStatus);

        bool restartWithConfiguration(const ProgramConfig &configuration, bool shouldRun, std::string &error);

        Logger &logger_;
        std::map<std::string, ProgramRuntime> programs_;
    };

    const char *toString(ProcessState state);

}
