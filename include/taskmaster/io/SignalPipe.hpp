#pragma once

#include <string>

#include <signal.h>

namespace taskmaster {

struct SignalEvents {
    bool reloadRequested = false;
    bool childChanged = false;
    bool shutdownRequested = false;
};

/*
 * Converts asynchronous POSIX signals into bytes readable by poll().  Signal
 * handlers only call write(); all real work remains in the main thread.
 */
class SignalPipe {
public:
    SignalPipe();
    ~SignalPipe();

    SignalPipe(const SignalPipe &) = delete;
    SignalPipe &operator=(const SignalPipe &) = delete;

    bool install(std::string &error);
    int readFd() const;
    bool consume(SignalEvents &events, std::string &error);

private:
    bool setNonBlocking(int descriptor, std::string &error);
    bool installHandler(int signal, std::string &error);
    void restoreHandlers();

    int descriptors_[2];
    bool installed_;
    unsigned int installedCount_;
    struct sigaction *oldActions_;
};

} // namespace taskmaster

