#include "taskmaster/runtime/ProcessSupervisor.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

//gere les process (donc a une liste de programmes qui eux ont la config du programme, une liste de process, un pid, un etat, etc)

namespace taskmaster {
namespace {

bool isAlive(pid_t pid) {
    return pid > 0;
}

bool sendToProcessGroup(pid_t pid, int signal) {
    if (kill(-pid, signal) == 0)
        return true;
    if (errno != ESRCH)
        return false;
    return kill(pid, signal) == 0 || errno == ESRCH;
}

const char *effectName(UpdateEffect effect) {
    switch (effect) {
    case UpdateEffect::ApplyOnly: return "apply-only";
    case UpdateEffect::ScaleProcesses: return "scale-processes";
    case UpdateEffect::RestartProcess: return "restart-process";
    }
    return "unknown";
}

void redirectOutput(const std::optional<std::string> &path, int descriptor) {
    const char *target = path.has_value() ? path->c_str() : "/dev/null";
    const int file = open(target, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file < 0)
        _exit(126);
    if (dup2(file, descriptor) < 0)
        _exit(126);
    close(file);
}

} // namespace

const char *toString(ProcessState state) {
    switch (state) {
    case ProcessState::Starting: return "STARTING";
    case ProcessState::Running: return "RUNNING";
    case ProcessState::Stopping: return "STOPPING";
    case ProcessState::Exited: return "EXITED";
    case ProcessState::Fatal: return "FATAL";
    }
    return "UNKNOWN";
}

ProcessSupervisor::ProcessSupervisor(Logger &logger) : logger_(logger) {}

bool ProcessSupervisor::add(const ProgramConfig &configuration, std::string &error) {
    error = configuration.validate();
    if (!error.empty())
        return false;
    if (programs_.find(configuration.name) != programs_.end()) {
        error = "cannot add existing program '" + configuration.name + "'";
        return false;
    }

    ProgramRuntime &program = programs_[configuration.name];
    program.configuration = configuration;
    program.removalPending = false;
    program.wantsRunning = configuration.autostart;
    program.slots.resize(configuration.numProcesses);
    for (unsigned int index = 0; index < configuration.numProcesses; ++index)
        program.slots[index].restartRequested = true;
    logger_.info("process", "added program '" + configuration.name + "'");
    maintain(program);
    return true;
}

bool ProcessSupervisor::update(const ProgramConfig &configuration, UpdateEffect effect,
                               std::string &error) {
    error = configuration.validate();
    if (!error.empty())
        return false;
    const std::map<std::string, ProgramRuntime>::iterator found = programs_.find(configuration.name);
    if (found == programs_.end()) {
        error = "cannot update unknown program '" + configuration.name + "'";
        return false;
    }
    if (effect == UpdateEffect::RestartProcess)
        return restartWithConfiguration(configuration, error);

    ProgramRuntime &program = found->second;
    const std::size_t oldCount = program.slots.size();
    program.configuration = configuration;
    logger_.info("process", "updated program '" + configuration.name + "' ("
                 + effectName(effect) + ")");
    if (effect == UpdateEffect::ApplyOnly)
        return true;

    if (oldCount < configuration.numProcesses) {
        program.slots.resize(configuration.numProcesses);
        for (std::size_t index = oldCount; index < program.slots.size(); ++index)
            program.slots[index].restartRequested = true;
    }
    for (std::size_t index = configuration.numProcesses; index < program.slots.size(); ++index) {
        program.slots[index].restartRequested = false;
        requestStop(program, program.slots[index]);
    }
    maintain(program);
    trimStoppedExtraSlots(program);
    return true;
}

bool ProcessSupervisor::start(const std::string &programName, std::string &error) {
    const std::map<std::string, ProgramRuntime>::iterator found = programs_.find(programName);
    if (found == programs_.end()) {
        error = "cannot start unknown program '" + programName + "'";
        return false;
    }
    ProgramRuntime &program = found->second;
    program.removalPending = false;
    program.wantsRunning = true;
    logger_.info("process", "start requested for '" + programName + "'");
    for (std::size_t index = 0; index < program.configuration.numProcesses; ++index) {
        ProcessSlot &slot = program.slots[index];
        slot.restartRequested = true;
        if (slot.state == ProcessState::Fatal) {
            slot.state = ProcessState::Exited;
            slot.failedStarts = 0;
        }
    }
    maintain(program);
    return true;
}

bool ProcessSupervisor::stop(const std::string &programName, std::string &error) {
    const std::map<std::string, ProgramRuntime>::iterator found = programs_.find(programName);
    if (found == programs_.end()) {
        error = "cannot stop unknown program '" + programName + "'";
        return false;
    }
    ProgramRuntime &program = found->second;
    program.wantsRunning = false;
    logger_.info("process", "stop requested for '" + programName + "'");
    for (ProcessSlot &slot : program.slots) {
        slot.restartRequested = false;
        requestStop(program, slot);
    }
    return true;
}

bool ProcessSupervisor::restart(const std::string &programName, std::string &error) {
    const std::map<std::string, ProgramRuntime>::iterator found = programs_.find(programName);
    if (found == programs_.end()) {
        error = "cannot restart unknown program '" + programName + "'";
        return false;
    }
    const ProgramConfig configuration = found->second.configuration;
    logger_.info("process", "restart requested for '" + programName + "'");
    return restartWithConfiguration(configuration, error);
}

bool ProcessSupervisor::restartWithConfiguration(const ProgramConfig &configuration, std::string &error) {
    error = configuration.validate();
    if (!error.empty())
        return false;

    const std::map<std::string, ProgramRuntime>::iterator found = programs_.find(configuration.name);
    if (found == programs_.end())
        return add(configuration, error);

    ProgramRuntime &program = found->second;
    program.configuration = configuration;
    program.removalPending = false;
    program.wantsRunning = true;
    logger_.info("process", "restarting program '" + configuration.name + "'");
    if (program.slots.size() < configuration.numProcesses)
        program.slots.resize(configuration.numProcesses);

    for (std::size_t index = 0; index < program.slots.size(); ++index) {
        ProcessSlot &slot = program.slots[index];
        const bool shouldRestart = index < configuration.numProcesses;
        slot.restartRequested = shouldRestart;
        if (!isAlive(slot.pid) && shouldRestart) {
            slot.state = ProcessState::Exited;
            slot.failedStarts = 0;
        }
        requestStop(program, slot);
    }
    maintain(program);
    return true;
}

bool ProcessSupervisor::remove(const std::string &programName, std::string &error) {
    const std::map<std::string, ProgramRuntime>::iterator found = programs_.find(programName);
    if (found == programs_.end()) {
        error = "cannot remove unknown program '" + programName + "'";
        return false;
    }
    ProgramRuntime &program = found->second;
    program.wantsRunning = false;
    program.removalPending = true;
    logger_.info("process", "removal requested for '" + programName + "'");
    for (ProcessSlot &slot : program.slots) {
        slot.restartRequested = false;
        requestStop(program, slot);
    }
    pruneRemovedPrograms();
    return true;
}

bool ProcessSupervisor::launch(ProgramRuntime &program, ProcessSlot &slot, std::string &error) {
    const pid_t child = fork();
    if (child < 0) {
        error = "fork failed for '" + program.configuration.name + "': " + std::strerror(errno);
        logger_.error("process", error);
        return false;
    }
    if (child == 0) {
        setpgid(0, 0);
        umask(program.configuration.fileUmask);
        if (program.configuration.workingDirectory.has_value() && chdir(program.configuration.workingDirectory->c_str()) != 0)
            _exit(126);
        for (const std::pair<const std::string, std::string> &entry : program.configuration.environment) {
            if (setenv(entry.first.c_str(), entry.second.c_str(), 1) != 0)
                _exit(126);
        }
        redirectOutput(program.configuration.stdoutPath, STDOUT_FILENO);
        redirectOutput(program.configuration.stderrPath, STDERR_FILENO);
        std::vector<char *> arguments;
        arguments.reserve(program.configuration.argv.size() + 1);
        for (const std::string &argument : program.configuration.argv)
            arguments.push_back(const_cast<char *>(argument.c_str()));
        arguments.push_back(NULL);
        execvp(arguments[0], arguments.data());
        _exit(127);
    }

    slot.pid = child;
    slot.state = ProcessState::Starting;
    slot.startedAt = std::chrono::steady_clock::now();
    logger_.info("process", "started '" + program.configuration.name + "' pid="
                 + std::to_string(child));
    return true;
}

void ProcessSupervisor::requestStop(ProgramRuntime &program, ProcessSlot &slot) {
    if (!isAlive(slot.pid) || slot.state == ProcessState::Stopping)
        return;
    if (!sendToProcessGroup(slot.pid, program.configuration.stopSignal)) {
        logger_.error("process", "cannot stop '" + program.configuration.name + "' pid="
                      + std::to_string(slot.pid) + ": " + std::strerror(errno));
        return;
    }
    slot.state = ProcessState::Stopping;
    slot.stopDeadline = std::chrono::steady_clock::now()
                      + std::chrono::seconds(program.configuration.stopGraceSeconds);
    logger_.info("process", "sent stop signal to '" + program.configuration.name + "' pid="
                 + std::to_string(slot.pid));
}

void ProcessSupervisor::reap(ProgramRuntime &program, ProcessSlot &slot) {
    if (!isAlive(slot.pid))
        return;
    int waitStatus = 0;
    const pid_t result = waitpid(slot.pid, &waitStatus, WNOHANG);
    if (result == 0)
        return;
    if (result == slot.pid) {
        slot.pid = -1;
        handleExit(program, slot, waitStatus);
        return;
    }
    if (errno == ECHILD) {
        slot.pid = -1;
        slot.state = ProcessState::Fatal;
        logger_.error("process", "lost child state for '" + program.configuration.name + "'");
    } else {
        logger_.error("process", "waitpid failed for '" + program.configuration.name + "': "
                      + std::strerror(errno));
    }
}

void ProcessSupervisor::handleExit(ProgramRuntime &program, ProcessSlot &slot, int waitStatus) {
    const ProcessState stateBeforeExit = slot.state;
    const bool exitedNormally = WIFEXITED(waitStatus);
    const int exitCode = exitedNormally ? WEXITSTATUS(waitStatus) : 128 + WTERMSIG(waitStatus);
    const bool expected = exitedNormally
                       && program.configuration.expectedExitCodes.count(exitCode) != 0;
    const bool startupFailure = stateBeforeExit == ProcessState::Starting;

    slot.state = ProcessState::Exited;
    logger_.info("process", "'" + program.configuration.name + "' pid exited with status "
                 + std::to_string(exitCode));

    if (program.removalPending)
        return;
    if (stateBeforeExit == ProcessState::Stopping)
        return;
    slot.restartRequested = false;
    if (startupFailure) {
        ++slot.failedStarts;
        if (slot.failedStarts > program.configuration.startRetries) {
            slot.state = ProcessState::Fatal;
            logger_.error("process", "'" + program.configuration.name
                          + "' reached its start retry limit");
        } else {
            slot.restartRequested = true;
            logger_.warning("process", "'" + program.configuration.name
                            + "' failed to start; retry scheduled");
        }
        return;
    }

    if (program.configuration.autorestart == AutoRestart::Always
        || (program.configuration.autorestart == AutoRestart::Unexpected && !expected)) {
        slot.restartRequested = true;
        logger_.warning("process", "'" + program.configuration.name
                        + "' exited unexpectedly; restart scheduled");
    }
}

void ProcessSupervisor::maintain(ProgramRuntime &program) {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < program.slots.size(); ++index) {
        ProcessSlot &slot = program.slots[index];
        const bool desired = program.wantsRunning && !program.removalPending
                          && index < program.configuration.numProcesses;
        if (slot.state == ProcessState::Starting
            && now - slot.startedAt >= std::chrono::seconds(program.configuration.startSeconds)) {
            slot.state = ProcessState::Running;
            slot.failedStarts = 0;
            logger_.info("process", "'" + program.configuration.name + "' is running");
        }
        if (slot.state == ProcessState::Stopping && now >= slot.stopDeadline) {
            sendToProcessGroup(slot.pid, SIGKILL);
            logger_.warning("process", "force-killed '" + program.configuration.name + "' pid="
                            + std::to_string(slot.pid));
            slot.stopDeadline = std::chrono::steady_clock::time_point::max();
        }
        if (!desired && isAlive(slot.pid) && slot.state != ProcessState::Stopping)
            requestStop(program, slot);
        if (desired && slot.restartRequested && !isAlive(slot.pid)
            && slot.state == ProcessState::Exited) {
            std::string error;
            if (!launch(program, slot, error)) {
                slot.state = ProcessState::Fatal;
            }
            slot.restartRequested = false;
        }
    }
}

void ProcessSupervisor::trimStoppedExtraSlots(ProgramRuntime &program) {
    while (program.slots.size() > program.configuration.numProcesses
           && !isAlive(program.slots.back().pid)) {
        program.slots.pop_back();
    }
}

void ProcessSupervisor::pruneRemovedPrograms() {
    for (std::map<std::string, ProgramRuntime>::iterator it = programs_.begin(); it != programs_.end();) {
        bool hasChildren = false;
        for (const ProcessSlot &slot : it->second.slots)
            hasChildren = hasChildren || isAlive(slot.pid);
        if (it->second.removalPending && !hasChildren) {
            logger_.info("process", "removed program '" + it->first + "'");
            it = programs_.erase(it);
        } else {
            ++it;
        }
    }
}

void ProcessSupervisor::tick() {
    for (std::pair<const std::string, ProgramRuntime> &entry : programs_) {
        ProgramRuntime &program = entry.second;
        for (ProcessSlot &slot : program.slots)
            reap(program, slot);
        maintain(program);
        trimStoppedExtraSlots(program);
    }
    pruneRemovedPrograms();
}

std::vector<ProcessStatus> ProcessSupervisor::status() const {
    std::vector<ProcessStatus> result;
    for (const std::pair<const std::string, ProgramRuntime> &entry : programs_) {
        for (std::size_t index = 0; index < entry.second.slots.size(); ++index) {
            const ProcessSlot &slot = entry.second.slots[index];
            result.push_back(ProcessStatus{entry.first, static_cast<unsigned int>(index), slot.pid,
                                           slot.state, slot.failedStarts});
        }
    }
    return result;
}

std::optional<std::chrono::steady_clock::time_point> ProcessSupervisor::nextDeadline() const {
    std::optional<std::chrono::steady_clock::time_point> earliest;
    for (const std::pair<const std::string, ProgramRuntime> &entry : programs_) {
        const ProgramRuntime &program = entry.second;
        for (const ProcessSlot &slot : program.slots) {
            std::chrono::steady_clock::time_point deadline;
            if (slot.state == ProcessState::Starting) {
                deadline = slot.startedAt + std::chrono::seconds(program.configuration.startSeconds);
            } else if (slot.state == ProcessState::Stopping) {
                deadline = slot.stopDeadline;
            } else {
                continue;
            }
            if (!earliest.has_value() || deadline < *earliest)
                earliest = deadline;
        }
    }
    return earliest;
}

}
