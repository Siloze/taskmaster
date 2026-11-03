#pragma once

#include <functional>
#include <string>
#include "command.hpp"

namespace taskmaster {
class ControlShell {
public:
    using CommandHandler = std::function<void(const CmdArgs &)>;
    using EofHandler = std::function<void()>;

    ControlShell(CommandHandler commandHandler, EofHandler eofHandler);
    ~ControlShell();

    ControlShell(const ControlShell &) = delete;
    ControlShell &operator=(const ControlShell &) = delete;

    void initialize();
    void readChar();
private:

    static void onLine(char *line);
    static int completeSuggestion(int count, int key);
    static ControlShell *active_;
    std::string suggestionSuffix() const;
    void clearSuggestion();
    void showSuggestion();

    CommandMap commandMap_;
    CommandParser commandParser_;
    CommandHandler commandHandler_;
    EofHandler eofHandler_;
    bool installed_;
    bool suggestionVisible_;
};
}
