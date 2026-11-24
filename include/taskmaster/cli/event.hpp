#pragma once

#include <string>

namespace taskmaster {

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
        EventType type = EventType::None;
        int sig = 0;
    };
}