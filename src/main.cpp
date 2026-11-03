#include <iostream>

#include "../include/taskmaster/runtime/TaskmasterApp.hpp"

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <configuration-file>\n";
        return 64;
    }
    // main stays intentionally tiny: TaskmasterApp owns every resource and
    // performs orderly child shutdown before it returns.
    taskmaster::TaskmasterApp application(argv[1]);
    return application.run();
}
