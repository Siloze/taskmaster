#include <cassert>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include "taskmaster/io/ConfigParser.hpp"
#include "taskmaster/diff/DiffBuilder.hpp"
#include "taskmaster/diff/DiffExecutor.hpp"
#include "taskmaster/io/Logger.hpp"

int main() {
    taskmaster::ConfigParser parser;
    taskmaster::TaskmasterConfig fileConfig;
    std::string error;
    assert(parser.parseFile("tests/conf/taskmaster.conf", fileConfig, error));
    assert(fileConfig.programs.size() == 1);
    assert(fileConfig.programs.at("sleeper").argv == std::vector<std::string>({"/bin/sleep", "10"}));
    assert(fileConfig.programs.at("sleeper").expectedExitCodes.count(2) == 1);
    assert(fileConfig.programs.at("sleeper").environment.at("TASKMASTER_TEST_VERSION") == "1");

    const std::string logPath = fileConfig.logFile;
    unlink(logPath.c_str());
    taskmaster::Logger logger(logPath);
    assert(logger.isOpen());

    taskmaster::ProgramConfig sleeper = fileConfig.programs.at("sleeper");

    taskmaster::DiffBuilder builder;
    taskmaster::JobMap initialJobs;
    initialJobs.insert(std::make_pair("sleeper", sleeper));
    const taskmaster::Diff add = builder.build(taskmaster::JobMap(), initialJobs);
    assert(add.actions().size() == 1);
    assert(add.actions()[0].kind == taskmaster::ActionKind::Add);

    taskmaster::ProcessSupervisor supervisor(logger);
    taskmaster::DiffExecutor executor(supervisor, logger);

    std::vector<std::string> errors;

    assert(executor.apply(add, errors));
    assert(supervisor.status().size() == 1);
    assert(supervisor.status()[0].pid > 0);

    taskmaster::ProgramConfig scaledSleeper = sleeper;
    scaledSleeper.numProcesses = 2;
    taskmaster::JobMap scaledJobs;
    scaledJobs.insert(std::make_pair("sleeper", scaledSleeper));
    const taskmaster::Diff scale = builder.build(initialJobs, scaledJobs);
    assert(scale.actions().size() == 1);
    assert(scale.actions()[0].updateEffect == taskmaster::UpdateEffect::ScaleProcesses);
    assert(executor.apply(scale, errors));
    assert(supervisor.status().size() == 2);
    assert(supervisor.status()[1].pid > 0);

    taskmaster::ProgramConfig restartedSleeper = scaledSleeper;
    restartedSleeper.numProcesses = 1;
    restartedSleeper.environment["TASKMASTER_TEST_VERSION"] = "2";
    taskmaster::JobMap restartedJobs;
    restartedJobs.insert(std::make_pair("sleeper", restartedSleeper));
    const taskmaster::Diff restartUpdate = builder.build(scaledJobs, restartedJobs);
    assert(restartUpdate.actions().size() == 1);
    assert(restartUpdate.actions()[0].updateEffect == taskmaster::UpdateEffect::RestartProcess);
    assert(executor.apply(restartUpdate, errors));
    for (int attempt = 0; attempt < 100; ++attempt) {
        supervisor.tick();
        if (supervisor.status().size() == 1 && supervisor.status()[0].pid > 0)
            break;
        usleep(10000);
    }
    assert(supervisor.status().size() == 1);
    assert(supervisor.status()[0].pid > 0);

    taskmaster::Diff stop;
    stop.add(taskmaster::Action{taskmaster::ActionKind::Stop, "sleeper", std::nullopt});
    assert(executor.apply(stop, errors));

    for (int attempt = 0; attempt < 100; ++attempt) {
        supervisor.tick();
        if (supervisor.status()[0].pid == -1)
            break;
        usleep(10000);
    }
    assert(supervisor.status()[0].pid == -1);

    std::ifstream logFile(logPath.c_str());
    const std::string contents((std::istreambuf_iterator<char>(logFile)),
                               std::istreambuf_iterator<char>());
    assert(contents.find("[diff] applying add") != std::string::npos);
    assert(contents.find("started 'sleeper'") != std::string::npos);
    unlink(logPath.c_str());
    return 0;
}
