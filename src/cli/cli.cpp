#include "taskmaster/cli/cli.hpp"

#include <cstdlib>
#include <iostream>
#include <utility>
#include <sys/ioctl.h>
#include <unistd.h>
#include <readline/history.h>
#include <readline/readline.h>

namespace taskmaster {

ControlShell *ControlShell::active_ = nullptr;

ControlShell::ControlShell(CommandHandler commandHandler, EofHandler eofHandler)
    : commandParser_(commandMap_), commandHandler_(std::move(commandHandler)),
      eofHandler_(std::move(eofHandler)),
      installed_(false), suggestionVisible_(false) {}

ControlShell::~ControlShell() {
    if (installed_) {
        clearSuggestion();
        rl_callback_handler_remove();
        active_ = nullptr;
    }
}

void ControlShell::initialize() {
    active_ = this;
    // Quand pas sur mac, readline compile pas avec les signaux donc on desactive la gestion des signaux de readline
#if !defined(__APPLE__) || RL_READLINE_VERSION >= 0x0600
    rl_catch_signals = 0;
#endif
    // a chaque entrée on appel onLine
    rl_callback_handler_install("taskmaster> ", &ControlShell::onLine);

    // a chaque tab on appel l'auto completion
    rl_bind_key('\t', &ControlShell::completeSuggestion);
    installed_ = true;
}

// a chaque caractere ecris il est appelé par TaskmasterApp::eventLoop() (ligne 100)
void ControlShell::readChar() {
    clearSuggestion();
    rl_callback_read_char();
    showSuggestion();
}

void ControlShell::clearSuggestion() {
    if (suggestionVisible_) {
        std::cout << "\033[K" << std::flush;
        suggestionVisible_ = false;
    }
}

std::string ControlShell::suggestionSuffix() const {
    // Proposer uniquement si le curseur se trouve à la fin d'une ligne non vide.
    if (rl_end == 0 || rl_point != rl_end)
        return "";

    // Oj s'arrete au premier espace, pas encore mis dec ompletion sur les args
    const std::string input(rl_line_buffer);
    if (input.find_first_of(" \t\r\n") != std::string::npos)
        return "";

    for (const std::string &command : commandMap_.names()) {
        //on garde la cmd la plus longue qui correspond
        if (command.size() <= input.size() || command.compare(0, input.size(), input) != 0)
            continue;

        return command.substr(input.size());
    }
    return "";
}

int ControlShell::completeSuggestion(int count, int key) {
    (void)count;
    (void)key;
    if (active_ == nullptr)
        return 0;

    const std::string suffix = active_->suggestionSuffix();
    if (suffix.empty())
        return 0;

    //on ajt le text a la fin de la ligne
    rl_insert_text(suffix.c_str());
    rl_redisplay();
    return 0;
}

void ControlShell::showSuggestion() {

    // Compatibilité, merci stako-verflow
    if (!isatty(STDOUT_FILENO))
        return;
    const char *term = std::getenv("TERM");
    if (term == nullptr || std::string(term) == "dumb")
        return;

    const std::string suffix = suggestionSuffix();
    if (suffix.empty())
        return;

    // Verif la largeur du terminal etc
    struct winsize size = {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0 || size.ws_col == 0 || 12 + static_cast<std::size_t>(rl_end) + suffix.size() >= size.ws_col)
        return;

    // afficher le reste de la cmg en gris
    std::cout << "\033[90m" << suffix << "\033[0m\033[" << suffix.size()
              << "D" << std::flush;

    // Pr que clearsuggestion supp
    suggestionVisible_ = true;
}

void ControlShell::onLine(char *line) {
    if (line == nullptr) {
        active_->eofHandler_();
        return;
    }
    //la cest quoi du coup ?
    const std::string input(line);
    std::free(line);
    if (input.find_first_not_of(" \t\r\n") != std::string::npos)
        add_history(input.c_str());
    CmdArgs args;
    std::string error;
    //ah cool
    if (!active_->commandParser_.parse(input, args, error)) {
        if (error != "empty command")
            std::cout << error << '\n';
        return;
    }
    active_->commandHandler_(args);
}
}
