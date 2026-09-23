#include "lanhousecommands.h"
#include "lanhousehud.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

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

    // Detached de propósito: a leitura bloqueia até o wrapper mandar algo ou
    // fechar o pipe, e o processo não deve esperar por isso pra encerrar.
#ifdef _WIN32
    // No Windows o Moonlight é um app "windowed" (sem console): lê o handle de
    // entrada herdado direto, em vez de depender do std::cin do runtime C.
    std::thread([]() {
        HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        if (input == nullptr || input == INVALID_HANDLE_VALUE) {
            return;
        }

        std::string pending;
        char buffer[256];
        DWORD bytesRead = 0;
        while (ReadFile(input, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
            pending.append(buffer, bytesRead);

            size_t newline;
            while ((newline = pending.find('\n')) != std::string::npos) {
                std::string line = pending.substr(0, newline);
                pending.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                handleLine(line);
            }
        }
    }).detach();
#else
    std::thread([]() {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            handleLine(line);
        }
    }).detach();
#endif
}

}
