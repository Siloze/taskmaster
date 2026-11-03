#include "taskmaster/diff/DiffBuilder.hpp"

namespace taskmaster {
namespace {

//Regarde les diff et redemarre le process si il y en a un d'importznt
bool restartRequired(const ProgramConfig &oldConfig, const ProgramConfig &newConfig) {
    return oldConfig.argv != newConfig.argv
        || oldConfig.environment != newConfig.environment
            || oldConfig.workingDirectory != newConfig.workingDirectory
                || oldConfig.fileUmask != newConfig.fileUmask
                    || oldConfig.stdoutPath != newConfig.stdoutPath
                    || oldConfig.stderrPath != newConfig.stderrPath;
}

//Regarde si la conf est egale ou pas (comme son nom l'indique hein, en foret de if deg)
bool configsEqual(const ProgramConfig &oldConfig, const ProgramConfig &newConfig) {
    return oldConfig.argv == newConfig.argv
        && oldConfig.numProcesses == newConfig.numProcesses
            && oldConfig.autostart == newConfig.autostart
                && oldConfig.autorestart == newConfig.autorestart
                    && oldConfig.expectedExitCodes == newConfig.expectedExitCodes
                        && oldConfig.startSeconds == newConfig.startSeconds
                            && oldConfig.startRetries == newConfig.startRetries
                                && oldConfig.stopSignal == newConfig.stopSignal
                                    && oldConfig.stopGraceSeconds == newConfig.stopGraceSeconds
                                        && oldConfig.stdoutPath == newConfig.stdoutPath
                                            && oldConfig.stderrPath == newConfig.stderrPath
                                                && oldConfig.environment == newConfig.environment
                                                    && oldConfig.workingDirectory == newConfig.workingDirectory
                                                        && oldConfig.fileUmask == newConfig.fileUmask;
}

//Quand il faut update un program, on regarde dit si c'est des changements qui necessitent un restart des process, un changemnt du nombre de process, ou juste un update de la conf
UpdateEffect updateEffect(const ProgramConfig &oldConfig, const ProgramConfig &newConfig) {
    if (restartRequired(oldConfig, newConfig))
        return UpdateEffect::RestartProcess;
    
    if (oldConfig.numProcesses != newConfig.numProcesses)
        return UpdateEffect::ScaleProcesses;

    return UpdateEffect::ApplyOnly;
}

}

// Compare les deux JobMaps et retourne un Diff qui contient les actions a effectuer pour que mettrz a jour la old config
Diff DiffBuilder::build(const JobMap &oldJobs, const JobMap &newJobs) const {
    Diff diff;
    for (const std::pair<const std::string, ProgramConfig> &entry : oldJobs) {
        if (newJobs.find(entry.first) == newJobs.end())
            diff.add(Action{ActionKind::Remove, entry.first, std::nullopt});
    }
    for (const std::pair<const std::string, ProgramConfig> &entry : newJobs) {
        const JobMap::const_iterator old = oldJobs.find(entry.first);
        if (old == oldJobs.end()) {
            diff.add(Action{ActionKind::Add, entry.first, entry.second});
        } else if (!configsEqual(old->second, entry.second)) {
            diff.add(Action{ActionKind::Update, entry.first, entry.second,
                            updateEffect(old->second, entry.second)});
        }
    }
    return diff;
}

}
