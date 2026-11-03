CXX := c++
CPPFLAGS := -Iinclude
CXXFLAGS := -std=c++17 -Wall -Werror -Wextra -Wpedantic

# macOS ships a libedit compatibility layer; use GNU Readline when available.
ifeq ($(shell uname -s),Darwin)
READLINE_PREFIX := $(shell brew --prefix readline 2>/dev/null)
ifneq ($(READLINE_PREFIX),)
CPPFLAGS += -I$(READLINE_PREFIX)/include
LDFLAGS += -L$(READLINE_PREFIX)/lib
endif
endif
LDLIBS += -lreadline

TARGET := taskmaster

SOURCES := \
    src/main.cpp \
    src/cli/cli.cpp \
    src/cli/command.cpp \
    src/diff/Diff.cpp \
    src/diff/DiffBuilder.cpp \
    src/diff/DiffExecutor.cpp \
    src/io/ConfigParser.cpp \
    src/io/Logger.cpp \
    src/io/SignalPipe.cpp \
    src/runtime/ProcessSupervisor.cpp \
    src/runtime/ProgramConfig.cpp \
    src/runtime/TaskmasterApp.cpp

HEADERS := $(shell find include -type f -name '*.hpp')

.PHONY: all clean re

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) $(LDFLAGS) -o $@ $(LDLIBS)

clean:
	rm -f $(TARGET)

re: clean all