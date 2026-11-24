#pragma once

#include <fstream>
#include <string>

namespace taskmaster {

enum class LogLevel {
    Info,
    Warning,
    Error
};

class Logger {
public:
    explicit Logger(const std::string &path);

    bool isOpen() const;
    bool info(const std::string &component, const std::string &message);
    bool error(const std::string &component, const std::string &message);
    bool warning(const std::string &component, const std::string &message);

private:
    bool write(LogLevel level, const std::string &component, const std::string &message);

    std::ofstream stream_;
};

}

