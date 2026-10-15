#pragma once

#include <optional>
#include <string>
#include <vector>

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
    const std::vector<Action> &actions() const;
    std::string validate() const;

private:
    std::vector<Action> actions_;
};

