#pragma once

#include <string>
#include <vector>
#include <optional>
#include "taskmaster/runtime/ProgramConfig.hpp"

namespace taskmaster {

    enum class ActionKind {
        Add,
        Update,
        Remove,
        Start,
        Stop,
        Restart
    };

    enum class UpdateEffect {
        ApplyOnly,
        ScaleProcesses,
        RestartProcess
    };

    struct Action {
        ActionKind kind;
        std::string programName;
        std::optional<ProgramConfig> configuration;
        UpdateEffect updateEffect = UpdateEffect::ApplyOnly;

        std::string validate() const;
    };

    class Diff {
    public:
        void add(const Action &action);

        std::string validate() const;
        const std::vector<Action> &actions() const;
    private:
        std::vector<Action> actions_;
    };

}
