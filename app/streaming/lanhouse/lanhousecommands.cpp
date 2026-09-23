#include "lanhousecommands.h"
#include "lanhousehud.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

namespace LanhouseCommands {

namespace {

void handleLine(const std::string& line)
{
    const char* text = line.c_str();
    if (strncmp(text, "minutes ", 8) != 0) {
        return;
    }
    const char* arg = text + 8;
    while (*arg == ' ') arg++;

    if (strncmp(arg, "unlimited", 9) == 0) {
        LanhouseHud::setMinutes(true, true, 0);
    }
    else if (strncmp(arg, "unknown", 7) == 0) {
        LanhouseHud::setMinutes(false, false, 0);
    }
    else {
        char* end = nullptr;
        double value = strtod(arg, &end);
        if (end != arg && value >= 0) {
            LanhouseHud::setMinutes(true, false, value);
        }
    }
}

}

void startStdinReader()
{
    static std::atomic<bool> started(false);
    if (started.exchange(true)) {
        return;
    }

    // Detached de propósito: getline bloqueia até o wrapper mandar algo ou
    // fechar o pipe, e o processo não deve esperar por isso pra encerrar.
    std::thread([]() {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            handleLine(line);
        }
    }).detach();
}

}
