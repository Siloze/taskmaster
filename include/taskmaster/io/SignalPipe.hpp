#pragma once

#include <string>
#include <signal.h>

namespace taskmaster {

struct SignalEvents {
    bool childChanged = false;
    bool reloadRequested = false;
    bool shutdownRequested = false;
};

class SignalPipe {
    public:
        SignalPipe();
        ~SignalPipe();

        SignalPipe(const SignalPipe &) = delete;
        SignalPipe &operator=(const SignalPipe &) = delete;

        int readFd() const;
        bool install(std::string &error);
        bool consume(SignalEvents &events, std::string &error);

    private:
        void restoreHandlers();
        bool installHandler(int signal, std::string &error);
        bool setNonBlocking(int descriptor, std::string &error);

        int descriptors_[2];
        bool installed_;
        unsigned int installedCount_;
        struct sigaction *oldActions_;
    };
}

