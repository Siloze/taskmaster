#ifndef TASKMASTER_EVENT_HPP
#define TASKMASTER_EVENT_HPP

#include <string>

/* CLI -> args */

namespace taskmaster::gchinaul {

    enum class CmdType {
        Status,
        Start,
        Stop,
        Restart,
        Reload,
        ShutDown,
        Help
    };

    enum class EventType {
        None,
        ChildExit,
        Reload,
        Shutdown
    };

    struct CmdArgs {
        CmdType command = CmdType::Help;
        std::string program;
    };

    struct SigEvent {
        EventType type = EventType::none;
        int sig = 0;
    };
}

#endif