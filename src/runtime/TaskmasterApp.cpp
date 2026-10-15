#include "taskmaster/runtime/TaskmasterApp.hpp"

#include <cerrno>
#include <climits>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <unistd.h>

namespace taskmaster {
namespace {

std::vector<std::string> splitWords(const std::string &line) {
    std::istringstream stream(line);
    std::vector<std::string> words;
    std::string word;
    while (stream >> word)
        words.push_back(word);
    return words;
}

void printPrompt() {
    std::cout << "taskmaster> " << std::flush;
}

} // namespace

TaskmasterApp::TaskmasterApp(const std::string &configPath)
    : configPath_(configPath), shuttingDown_(false) {}

int TaskmasterApp::run() {
    if (!initialize())
        return 1;
    logger_->info("main", "taskmaster started");
    printPrompt();
    eventLoop();
    logger_->info("main", "taskmaster stopped");
    return 0;
}

bool TaskmasterApp::initialize() {

    std::string error;
    if (!parser_.parseFile(configPath_, configuration_, error)) {
        std::cerr << error << '\n';
        return false;
    }
    logger_.reset(new Logger(configuration_.logFile));
    if (!logger_->isOpen()) {
        std::cerr << "cannot open log file '" << configuration_.logFile << "'\n";
        return false;
    }
    if (!signalPipe_.install(error)) {
        logger_->error("main", error);
        std::cerr << error << '\n';
        return false;
    }
    supervisor_.reset(new ProcessSupervisor(*logger_));
    executor_.reset(new DiffExecutor(*supervisor_, *logger_));

    const Diff initialDiff = diffBuilder_.build(JobMap(), configuration_.programs);
    std::vector<std::string> errors;
    if (!executor_->apply(initialDiff, errors)) {
        logger_->error("main", "cannot apply initial configuration");
        std::cerr << "cannot apply initial configuration\n";
        return false;
    }
    logger_->info("config", "loaded '" + configPath_ + "'");
    return true;
}

void TaskmasterApp::eventLoop() {
    while (!(shuttingDown_ && allChildrenStopped())) {
        struct pollfd descriptors[2] = {};
        descriptors[0].fd = STDIN_FILENO;
        descriptors[0].events = POLLIN;
        descriptors[1].fd = signalPipe_.readFd();
        descriptors[1].events = POLLIN;

        // poll sleeps without consuming CPU until input, a signal, or the
        // next start/stop deadline requires supervision work.
        const int result = poll(descriptors, 2, nextPollTimeoutMs());
        if (result < 0) {
            if (errno == EINTR)
                continue;
            logger_->error("main", "poll failed");
            beginShutdown("poll failure");
            continue;
        }
        if (descriptors[1].revents & POLLIN) {
            SignalEvents events;
            std::string error;
            if (!signalPipe_.consume(events, error)) {
                logger_->error("main", error);
                beginShutdown("signal pipe failure");
            } else {
                // SignalPipe only reports signals. All non-trivial work is
                // deliberately performed here, in the normal main thread.
                if (events.shutdownRequested)
                    beginShutdown("termination signal");
                if (events.reloadRequested && !shuttingDown_)
                    reloadConfiguration();
                if (events.childChanged)
                    supervisor_->tick();
            }
        }
        if (!shuttingDown_ && (descriptors[0].revents & (POLLIN | POLLHUP)))
            consumeStandardInput();

        /* Handles timer expiry even when poll woke for another event. */
        supervisor_->tick();
    }
}

bool TaskmasterApp::reloadConfiguration() {
    // Parse into a temporary object: a malformed file never corrupts the
    // currently active configuration.
    TaskmasterConfig candidate;
    std::string error;
    if (!parser_.parseFile(configPath_, candidate, error)) {
        logger_->error("config", "reload rejected: " + error);
        return false;
    }
    if (candidate.logFile != configuration_.logFile) {
        logger_->warning("config", "logfile changes require a restart; keeping current logfile");
        candidate.logFile = configuration_.logFile;
    }
    const Diff diff = diffBuilder_.build(configuration_.programs, candidate.programs);
    std::vector<std::string> errors;
    if (!executor_->apply(diff, errors)) {
        logger_->error("config", "reload could not be fully applied");
        return false;
    }
    configuration_ = candidate;
    logger_->info("config", "configuration reloaded");
    return true;
}

void TaskmasterApp::beginShutdown(const std::string &reason) {
    if (shuttingDown_)
        return;
    shuttingDown_ = true;
    logger_->info("main", "shutdown requested: " + reason);

    Diff stopAll;
    for (const std::pair<const std::string, ProgramConfig> &entry : configuration_.programs)
        stopAll.add(Action{ActionKind::Stop, entry.first, std::nullopt});
    std::vector<std::string> errors;
    executor_->apply(stopAll, errors);
}

bool TaskmasterApp::allChildrenStopped() const {
    for (const ProcessStatus &process : supervisor_->status()) {
        if (process.pid > 0)
            return false;
    }
    return true;
}

int TaskmasterApp::nextPollTimeoutMs() const {
    const std::optional<std::chrono::steady_clock::time_point> deadline = supervisor_->nextDeadline();
    if (!deadline.has_value())
        return -1;
    // ProcessSupervisor owns deadlines; the app only converts the closest one
    // into poll's millisecond timeout.
    const std::chrono::milliseconds delay = std::chrono::duration_cast<std::chrono::milliseconds>(
        *deadline - std::chrono::steady_clock::now());
    if (delay.count() <= 0)
        return 0;
    if (delay.count() > INT_MAX)
        return INT_MAX;
    return static_cast<int>(delay.count());
}

void TaskmasterApp::consumeStandardInput() {
    char buffer[512];
    const ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (count == 0) {
        beginShutdown("standard input closed");
        return;
    }
    if (count < 0) {
        if (errno != EINTR && errno != EAGAIN)
            logger_->error("cli", "cannot read standard input");
        return;
    }
    // read() may contain several commands or only part of one command.
    inputBuffer_.append(buffer, static_cast<std::size_t>(count));
    for (;;) {
        const std::size_t newline = inputBuffer_.find('\n');
        if (newline == std::string::npos)
            break;
        std::string line = inputBuffer_.substr(0, newline);
        inputBuffer_.erase(0, newline + 1);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        executeCommand(line);
        if (shuttingDown_)
            break;
        printPrompt();
    }
}

void TaskmasterApp::executeCommand(const std::string &line) {
    const std::vector<std::string> words = splitWords(line);
    if (words.empty())
        return;
    if (words[0] == "help") {
        std::cout << "commands: status, start NAME, stop NAME, restart NAME, reload, quit\n";
        return;
    }
    if (words[0] == "status") {
        printStatus();
        return;
    }
    if (words[0] == "reload") {
        std::cout << (reloadConfiguration() ? "configuration reloaded\n" : "reload failed; see log\n");
        return;
    }
    if (words[0] == "quit" || words[0] == "exit") {
        beginShutdown("CLI request");
        return;
    }
    if (words.size() != 2 || (words[0] != "start" && words[0] != "stop" && words[0] != "restart")) {
        std::cout << "unknown command; type help\n";
        return;
    }

    ActionKind kind = ActionKind::Start;
    if (words[0] == "stop")
        kind = ActionKind::Stop;
    else if (words[0] == "restart")
        kind = ActionKind::Restart;
    Diff diff;
    diff.add(Action{kind, words[1], std::nullopt});
    std::vector<std::string> errors;
    if (!executor_->apply(diff, errors))
        std::cout << errors.front() << '\n';
}

void TaskmasterApp::printStatus() {
    supervisor_->tick();
    for (const ProcessStatus &process : supervisor_->status()) {
        std::cout << process.programName << '[' << process.slot << "] pid=" << process.pid
                  << " state=" << toString(process.state)
                  << " failed_starts=" << process.failedStarts << '\n';
    }
}

} // namespace taskmaster
