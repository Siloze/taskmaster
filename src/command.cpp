#include "command.hpp"

#include <algorithm>
#include <sstream>

namespace taskmaster::gchinaul {

    CommandMap::CommandMap()
        : commands_{
            {"status", CmdType::Status},
            {"start", CmdType::Start},
            {"stop", CmdType::Stop},
            {"restart", CmdType::Restart},
            {"reload", CmdType::Reload},
            {"shutdown", CmdType::ShutDown},
            {"help", CmdType::Help}
        }
    {
    }

    bool CommandMap::find(const std::string& name, CmdType& out) const
    {
        const auto it = commands_.find(name);
        if (it == commands_.end()) {
            return false;
        }

        out = it->second;
        return true;
    }

    std::vector<std::string> CommandMap::names() const
    {
        std::vector<std::string> result;

        for (const auto& entry : commands_) {
            result.push_back(entry.first);
        }

        std::sort(result.begin(), result.end());
        return result;
    }

    CommandParser::CommandParser(const CommandMap& map)
        : map_(map)
    {
    }

    std::vector<std::string> CommandParser::tokenize(
        const std::string& line
    )
    {
        std::vector<std::string> tokens;
        std::istringstream input(line);
        std::string token;

        while (input >> token) {
            tokens.push_back(token);
        }

        return tokens;
    }

    bool CommandParser::parse(
        const std::string& line,
        CmdArgs& out,
        std::string& error
    ) const
    {
        error.clear();

        const std::vector<std::string> tokens = tokenize(line);
        if (tokens.empty()) {
            error = "empty command";
            return false;
        }

        CmdType command;
        if (!map_.find(tokens[0], command)) {
            error = "unknown command: " + tokens[0];
            return false;
        }

        const std::size_t argumentCount = tokens.size() - 1;

        switch(command) {
            case CmdType::Status:
                if (argumentCount > 1) {
                    error = "usage: status [program]";
                    return false;
                }
                break;
            
            case CmdType::Start:
            case CmdType::Stop:
            case CmdType::Restart:
                if (argumentCount != 1) {
                    error = "usage: " + tokens[0] + " <program>";
                    return false;
                }
                break;

            case CmdType::Reload:
            case CmdType::ShutDown:
            case CmdType::Help:
                if (argumentCount != 0) {
                    error = "usage: " + tokens[0];
                    return false;
                }
                break;
        }

        CmdArgs parsed;
        parsed.command = command;

        if (argumentCount == 1) {
            parsed.program = tokens[1];
        }

        out = parsed;
        return true;
    }
}
