#include "taskmaster/io/Logger.hpp"

#include <ctime>
#include <iomanip>

//Simple logger qui ecris les actions dans le fichiers de log quoi

namespace taskmaster {
namespace {

const char *levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Info: return "INFO";
    case LogLevel::Warning: return "WARN";
    case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

}

Logger::Logger(const std::string &path) : stream_(path.c_str(), std::ios::out | std::ios::app) {}

bool Logger::isOpen() const {
    return stream_.is_open();
}

bool Logger::info(const std::string &component, const std::string &message) {
    return write(LogLevel::Info, component, message);
}

bool Logger::warning(const std::string &component, const std::string &message) {
    return write(LogLevel::Warning, component, message);
}

bool Logger::error(const std::string &component, const std::string &message) {
    return write(LogLevel::Error, component, message);
}

bool Logger::write(LogLevel level, const std::string &component, const std::string &message) {
    if (!stream_.is_open())
        return false;
    const std::time_t now = std::time(NULL);
    std::tm localTime = {};
    if (localtime_r(&now, &localTime) == NULL)
        return false;
    stream_ << std::put_time(&localTime, "%Y-%m-%dT%H:%M:%S")
            << " [" << levelName(level) << "] [" << component << "] "
            << message << '\n';
    stream_.flush();
    return stream_.good();
}

} // namespace taskmaster
