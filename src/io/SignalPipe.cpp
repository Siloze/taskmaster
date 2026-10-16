#include "taskmaster/io/SignalPipe.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>


//SignalPipe pour convertir les signaux en bytes qui apres seront lu par poll() dans le thread principal au moins il sait qu'un signal a eu lieu et il peut faire le travail qui va avec (reload, shutdown, etc)
namespace taskmaster {
namespace {

const int handledSignals[] = {SIGHUP, SIGCHLD, SIGINT, SIGTERM};
const unsigned int handledSignalCount = sizeof(handledSignals) / sizeof(handledSignals[0]);
int signalWriteFd = -1;

extern "C" void forwardSignal(int signal) {
    const int savedErrno = errno;
    const unsigned char event = static_cast<unsigned char>(signal);
    if (signalWriteFd != -1)
        (void)write(signalWriteFd, &event, sizeof(event));
    errno = savedErrno;
}

int signalIndex(int signal) {
    for (unsigned int index = 0; index < handledSignalCount; ++index) {
        if (handledSignals[index] == signal)
            return static_cast<int>(index);
    }
    return -1;
}

}

SignalPipe::SignalPipe() : descriptors_{-1, -1}, installed_(false), installedCount_(0),
                           oldActions_(new struct sigaction[handledSignalCount]) {}

SignalPipe::~SignalPipe() {
    restoreHandlers();
    if (descriptors_[0] != -1)
        close(descriptors_[0]);
    if (descriptors_[1] != -1)
        close(descriptors_[1]);
    delete[] oldActions_;
}

bool SignalPipe::setNonBlocking(int descriptor, std::string &error) {
    const int flags = fcntl(descriptor, F_GETFL);
    if (flags == -1 || fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) == -1) {
        error = "cannot set signal pipe to non-blocking mode: " + std::string(std::strerror(errno));
        return false;
    }
    return true;
}

bool SignalPipe::installHandler(int signal, std::string &error) {
    const int index = signalIndex(signal);
    struct sigaction action = {};
    action.sa_handler = forwardSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(signal, &action, &oldActions_[index]) == -1) {
        error = "cannot install signal handler: " + std::string(std::strerror(errno));
        return false;
    }
    ++installedCount_;
    return true;
}

bool SignalPipe::install(std::string &error) {
    if (installed_) {
        error = "signal pipe is already installed";
        return false;
    }
    if (pipe(descriptors_) == -1) {
        error = "cannot create signal pipe: " + std::string(std::strerror(errno));
        return false;
    }
    if (!setNonBlocking(descriptors_[0], error) || !setNonBlocking(descriptors_[1], error))
        return false;
    signalWriteFd = descriptors_[1];
    for (unsigned int index = 0; index < handledSignalCount; ++index) {
        if (!installHandler(handledSignals[index], error)) {
            restoreHandlers();
            return false;
        }
    }
    installed_ = true;
    return true;
}

int SignalPipe::readFd() const {
    return descriptors_[0];
}

bool SignalPipe::consume(SignalEvents &events, std::string &error) {
    unsigned char buffer[64];
    for (;;) {
        const ssize_t count = read(descriptors_[0], buffer, sizeof(buffer));
        if (count > 0) {
            for (ssize_t index = 0; index < count; ++index) {
                if (buffer[index] == SIGHUP)
                    events.reloadRequested = true;
                else if (buffer[index] == SIGCHLD)
                    events.childChanged = true;
                else if (buffer[index] == SIGINT || buffer[index] == SIGTERM)
                    events.shutdownRequested = true;
            }
            continue;
        }
        if (count == -1 && errno == EINTR)
            continue;
        if (count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return true;
        if (count == 0)
            return true;
        error = "cannot read signal pipe: " + std::string(std::strerror(errno));
        return false;
    }
}

void SignalPipe::restoreHandlers() {
    if (installedCount_ == 0)
        return;
    for (unsigned int index = 0; index < installedCount_; ++index)
        sigaction(handledSignals[index], &oldActions_[index], NULL);
    installedCount_ = 0;
    installed_ = false;
    signalWriteFd = -1;
}

} // namespace taskmaster
