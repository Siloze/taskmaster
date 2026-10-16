#include "taskmaster/io/ConfigParser.hpp"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <set>
#include <signal.h>
#include <sstream>

namespace taskmaster {
namespace {

std::string trim(const std::string &value) {
    const std::string whitespace = " \t\r\n";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos)
        return "";
    const std::size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

std::string lower(std::string value) {
    for (char &character : value) {
        if (character >= 'A' && character <= 'Z')
            character = static_cast<char>(character - 'A' + 'a');
    }
    return value;
}

std::string stripComment(const std::string &line) {
    char quote = '\0';
    bool escaped = false;
    for (std::size_t index = 0; index < line.size(); ++index) {
        const char character = line[index];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (character == '\\' && quote != '\'') {
            escaped = true;
            continue;
        }
        if (character == '\'' || character == '"') {
            if (quote == '\0')
                quote = character;
            else if (quote == character)
                quote = '\0';
            continue;
        }
        if (character == '#' && quote == '\0')
            return line.substr(0, index);
    }
    return line;
}

std::size_t unquotedEquals(const std::string &line) {
    char quote = '\0';
    bool escaped = false;
    for (std::size_t index = 0; index < line.size(); ++index) {
        const char character = line[index];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (character == '\\' && quote != '\'') {
            escaped = true;
            continue;
        }
        if (character == '\'' || character == '"') {
            if (quote == '\0')
                quote = character;
            else if (quote == character)
                quote = '\0';
            continue;
        }
        if (character == '=' && quote == '\0')
            return index;
    }
    return std::string::npos;
}

bool parseText(const std::string &raw, std::string &value, std::string &error) {
    const std::string input = trim(raw);
    if (input.empty()) {
        value = "";
        return true;
    }
    if (input.front() != '\'' && input.front() != '"') {
        value = input;
        return true;
    }
    const char quote = input.front();
    if (input.size() < 2 || input.back() != quote) {
        error = "unterminated quoted value";
        return false;
    }
    value.clear();
    bool escaped = false;
    for (std::size_t index = 1; index + 1 < input.size(); ++index) {
        const char character = input[index];
        if (escaped) {
            value += character;
            escaped = false;
        } else if (character == '\\' && quote == '"') {
            escaped = true;
        } else {
            value += character;
        }
    }
    if (escaped) {
        error = "unfinished escape sequence";
        return false;
    }
    return true;
}

bool parseCommand(const std::string &raw, std::vector<std::string> &arguments, std::string &error) {
    const std::string input = trim(raw);
    std::string current;
    char quote = '\0';
    bool escaped = false;
    bool hasToken = false;
    arguments.clear();
    for (std::size_t index = 0; index < input.size(); ++index) {
        const char character = input[index];
        if (escaped) {
            current += character;
            hasToken = true;
            escaped = false;
            continue;
        }
        if (character == '\\' && quote != '\'') {
            escaped = true;
            hasToken = true;
            continue;
        }
        if (character == '\'' || character == '"') {
            if (quote == '\0')
                quote = character;
            else if (quote == character)
                quote = '\0';
            else
                current += character;
            hasToken = true;
            continue;
        }
        if ((character == ' ' || character == '\t') && quote == '\0') {
            if (hasToken) {
                arguments.push_back(current);
                current.clear();
                hasToken = false;
            }
            continue;
        }
        current += character;
        hasToken = true;
    }
    if (quote != '\0' || escaped) {
        error = "unterminated command quote or escape";
        return false;
    }
    if (hasToken)
        arguments.push_back(current);
    if (arguments.empty()) {
        error = "command cannot be empty";
        return false;
    }
    return true;
}

bool parseUnsigned(const std::string &raw, unsigned int base, unsigned int maximum,
                   unsigned int &value) {
    const std::string input = trim(raw);
    if (input.empty() || input.front() == '-')
        return false;
    char *end = NULL;
    errno = 0;
    const unsigned long parsed = std::strtoul(input.c_str(), &end, base);
    if (errno != 0 || end == input.c_str() || *end != '\0' || parsed > maximum)
        return false;
    value = static_cast<unsigned int>(parsed);
    return true;
}

bool parseBoolean(const std::string &raw, bool &value) {
    const std::string input = lower(trim(raw));
    if (input == "true" || input == "yes" || input == "1") {
        value = true;
        return true;
    }
    if (input == "false" || input == "no" || input == "0") {
        value = false;
        return true;
    }
    return false;
}

bool parseExitCodes(const std::string &raw, std::set<int> &codes) {
    std::istringstream stream(raw);
    std::string item;
    std::set<int> parsed;
    while (std::getline(stream, item, ',')) {
        unsigned int code = 0;
        if (!parseUnsigned(item, 10, 255, code))
            return false;
        parsed.insert(static_cast<int>(code));
    }
    if (parsed.empty())
        return false;
    codes = parsed;
    return true;
}

bool parseSignal(const std::string &raw, int &signal) {
    const std::string input = lower(trim(raw));
    unsigned int numeric = 0;
    if (parseUnsigned(input, 10, static_cast<unsigned int>(NSIG - 1), numeric)) {
        if (numeric == 0)
            return false;
        signal = static_cast<int>(numeric);
        return true;
    }
    static const std::map<std::string, int> signals = {
        {"hup", SIGHUP}, {"int", SIGINT}, {"quit", SIGQUIT}, {"kill", SIGKILL},
        {"term", SIGTERM}, {"usr1", SIGUSR1}, {"usr2", SIGUSR2}, {"chld", SIGCHLD}
    };
    std::string name = input;
    if (name.compare(0, 3, "sig") == 0)
        name.erase(0, 3);
    const std::map<std::string, int>::const_iterator found = signals.find(name);
    if (found == signals.end())
        return false;
    signal = found->second;
    return true;
}

std::string atLine(const std::string &path, unsigned long line, const std::string &message) {
    return path + ":" + std::to_string(line) + ": " + message;
}

bool setProgramField(ProgramConfig &program, const std::string &key, const std::string &raw, std::string &error) {
    if (key == "cmd" || key == "command")
        return parseCommand(raw, program.argv, error);
    if (key == "numprocs") {
        if (!parseUnsigned(raw, 10, UINT_MAX, program.numProcesses))
            error = "numprocs must be a positive integer";
        return error.empty() && program.numProcesses != 0;
    }
    if (key == "autostart") {
        if (!parseBoolean(raw, program.autostart))
            error = "autostart must be true or false";
        return error.empty();
    }
    if (key == "autorestart") {
        const std::string value = lower(trim(raw));
        if (value == "always")
            program.autorestart = AutoRestart::Always;
        else if (value == "never")
            program.autorestart = AutoRestart::Never;
        else if (value == "unexpected")
            program.autorestart = AutoRestart::Unexpected;
        else
            error = "autorestart must be always, never, or unexpected";
        return error.empty();
    }
    if (key == "exitcodes") {
        if (!parseExitCodes(raw, program.expectedExitCodes))
            error = "exitcodes must be a comma-separated list from 0 to 255";
        return error.empty();
    }
    if (key == "starttime") {
        if (!parseUnsigned(raw, 10, UINT_MAX, program.startSeconds))
            error = "starttime must be a non-negative integer";
        return error.empty();
    }
    if (key == "startretries") {
        if (!parseUnsigned(raw, 10, UINT_MAX, program.startRetries))
            error = "startretries must be a non-negative integer";
        return error.empty();
    }
    if (key == "stopsignal") {
        if (!parseSignal(raw, program.stopSignal))
            error = "stopsignal is invalid";
        return error.empty();
    }
    if (key == "stoptime") {
        if (!parseUnsigned(raw, 10, UINT_MAX, program.stopGraceSeconds))
            error = "stoptime must be a non-negative integer";
        return error.empty();
    }
    if (key == "umask") {
        if (!parseUnsigned(raw, 8, 0777, program.fileUmask))
            error = "umask must be an octal value from 000 to 777";
        return error.empty();
    }
    if (key == "workingdir") {
        std::string value;
        if (!parseText(raw, value, error))
            return false;
        if (value.empty())
            error = "workingdir cannot be empty";
        else
            program.workingDirectory = value;
        return error.empty();
    }
    if (key == "stdout" || key == "stderr") {
        std::string value;
        if (!parseText(raw, value, error))
            return false;
        std::optional<std::string> &target = key == "stdout" ? program.stdoutPath : program.stderrPath;
        if (lower(value) == "discard")
            target.reset();
        else if (value.empty())
            error = key + " cannot be empty; use discard instead";
        else
            target = value;
        return error.empty();
    }
    if (key.compare(0, 4, "env.") == 0) {
        const std::string name = key.substr(4);
        std::string value;
        if (name.empty() || name.find('=') != std::string::npos) {
            error = "environment variable name is invalid";
            return false;
        }
        if (!parseText(raw, value, error))
            return false;
        program.environment[name] = value;
        return true;
    }
    error = "unknown program option '" + key + "'";
    return false;
}

}

bool ConfigParser::parseFile(const std::string &path, TaskmasterConfig &configuration, std::string &error) const {
    std::ifstream input(path.c_str());
    if (!input.is_open()) {
        error = "cannot open configuration file '" + path + "'";
        return false;
    }

    TaskmasterConfig parsed;
    std::string currentProgram;
    std::map<std::string, std::set<std::string> > seenKeys;
    std::string line;
    unsigned long lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const std::string content = trim(stripComment(line));
        if (content.empty())
            continue;
        if (content.front() == '[') {
            if (content.back() != ']') {
                error = atLine(path, lineNumber, "unterminated section header");
                return false;
            }
            const std::string section = trim(content.substr(1, content.size() - 2));
            const std::string prefix = "program ";
            if (section.compare(0, prefix.size(), prefix) != 0
                || trim(section.substr(prefix.size())).empty()) {
                error = atLine(path, lineNumber, "expected [program NAME]");
                return false;
            }
            currentProgram = trim(section.substr(prefix.size()));
            if (parsed.programs.find(currentProgram) != parsed.programs.end()) {
                error = atLine(path, lineNumber, "duplicate program '" + currentProgram + "'");
                return false;
            }
            ProgramConfig program;
            program.name = currentProgram;
            parsed.programs.insert(std::make_pair(currentProgram, program));
            continue;
        }

        const std::size_t equals = unquotedEquals(content);
        if (equals == std::string::npos) {
            error = atLine(path, lineNumber, "expected key = value");
            return false;
        }
        const std::string rawKey = trim(content.substr(0, equals));
        std::string key = lower(rawKey);
        if (key.compare(0, 4, "env.") == 0)
            key = "env." + rawKey.substr(4);
        const std::string value = trim(content.substr(equals + 1));
        if (key.empty()) {
            error = atLine(path, lineNumber, "empty key");
            return false;
        }
        if (currentProgram.empty()) {
            if (key != "logfile") {
                error = atLine(path, lineNumber, "only logfile is allowed outside a program section");
                return false;
            }
            if (!parseText(value, parsed.logFile, error)) {
                error = atLine(path, lineNumber, error);
                return false;
            }
            if (parsed.logFile.empty()) {
                error = atLine(path, lineNumber, "logfile cannot be empty");
                return false;
            }
            continue;
        }

        if (!seenKeys[currentProgram].insert(key).second) {
            error = atLine(path, lineNumber, "duplicate option '" + key + "'");
            return false;
        }
        ProgramConfig &program = parsed.programs.find(currentProgram)->second;
        std::string fieldError;
        if (!setProgramField(program, key, value, fieldError)) {
            error = atLine(path, lineNumber, fieldError);
            return false;
        }
    }
    if (input.bad()) {
        error = "failed while reading configuration file '" + path + "'";
        return false;
    }
    for (const std::pair<const std::string, ProgramConfig> &entry : parsed.programs) {
        const std::string validationError = entry.second.validate();
        if (!validationError.empty()) {
            error = "configuration for '" + entry.first + "': " + validationError;
            return false;
        }
    }
    configuration = parsed;
    return true;
}

} 
