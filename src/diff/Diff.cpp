#include "taskmaster/diff/Diff.hpp"

namespace taskmaster {


// Si une action est valide
std::string Action::validate() const {
    if (programName.empty())
        return "an action has no program name";

    if (kind == ActionKind::Add || kind == ActionKind::Update) {
        if (!configuration.has_value())
            return "add/update of '" + programName + "' requires a configuration";

        if (configuration->name != programName)
            return "action name and configuration name differ for '" + programName + "'";

        return configuration->validate();
    }

    if (configuration.has_value())
        return "manual action for '" + programName + "' must not carry a configuration";
    return "";
}

void Diff::add(const Action &action) {
    actions_.push_back(action);
}

const std::vector<Action> &Diff::actions() const {
    return actions_;
}

// Si un diff est valide quoi
std::string Diff::validate() const {
    for (const Action &action : actions_) {
        const std::string error = action.validate();
        if (!error.empty())
            return error;
    }
    return "";
}

}
