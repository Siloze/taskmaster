#include "taskmaster/diff/DiffExecutor.hpp"

namespace taskmaster {
namespace {

// Action vers string quoi
const char *actionName(ActionKind kind) {
    switch (kind) {
    case ActionKind::Add: return "add";
    case ActionKind::Update: return "update";
    case ActionKind::Remove: return "remove";
    case ActionKind::Start: return "start";
    case ActionKind::Stop: return "stop";
    case ActionKind::Restart: return "restart";
    }
    return "unknown";
}

}

DiffExecutor::DiffExecutor(ProcessSupervisor &supervisor, Logger &logger)
    : supervisor_(supervisor), logger_(logger) {}

// Pour appliquer et loguer chaque action d'un diff sur le superviseur (donné dans le constructeur)
bool DiffExecutor::apply(const Diff &diff, std::vector<std::string> &errors) {
    errors.clear();
    const std::string validationError = diff.validate();
    if (!validationError.empty()) {
        errors.push_back(validationError);
        logger_.error("diff", "rejected diff: " + validationError);
        return false;
    }

    bool success = true;
    for (const Action &action : diff.actions()) {
        logger_.info("diff", "applying " + std::string(actionName(action.kind))
                     + " for '" + action.programName + "'");
        std::string error;
        bool applied = false;
        switch (action.kind) {
        case ActionKind::Add:
            applied = supervisor_.add(*action.configuration, error);
            break;
        case ActionKind::Update:
            applied = supervisor_.update(*action.configuration, action.updateEffect, error);
            break;
        case ActionKind::Remove:
            applied = supervisor_.remove(action.programName, error);
            break;
        case ActionKind::Start:
            applied = supervisor_.start(action.programName, error);
            break;
        case ActionKind::Stop:
            applied = supervisor_.stop(action.programName, error);
            break;
        case ActionKind::Restart:
            applied = supervisor_.restart(action.programName, error);
            break;
        }
        if (!applied) {
            success = false;
            errors.push_back(error);
            logger_.error("diff", error);
        }
    }
    return success;
}

} // namespace taskmaster
