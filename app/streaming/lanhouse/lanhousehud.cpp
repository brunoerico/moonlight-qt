#include "lanhousehud.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

namespace {

std::mutex s_MinutesLock;
LanhouseHud::Minutes s_Minutes;

// Caracteres fora do ASCII escritos em UTF-8 pra o arquivo não depender da
// codificação do compilador (o SDL_ttf recebe UTF-8).
const char* const kDash = "\xE2\x80\x94";      // U+2014 "—"
const char* const kInfinity = "\xE2\x88\x9E";  // U+221E "∞"
const char* const kSeparator = " \xC2\xB7 ";   // " · "

std::string upper(const char* s)
{
    std::string out(s ? s : "");
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });
    return out;
}

}

namespace LanhouseHud {

void setMinutes(bool known, bool unlimited, double minutes)
{
    std::lock_guard<std::mutex> guard(s_MinutesLock);
    s_Minutes.known = known;
    s_Minutes.unlimited = unlimited;
    s_Minutes.minutes = minutes;
}

Minutes getMinutes()
{
    std::lock_guard<std::mutex> guard(s_MinutesLock);
    return s_Minutes;
}

void formatMinutes(const Minutes& m, char* out, size_t len)
{
    if (m.known && m.unlimited) {
        snprintf(out, len, "%s", kInfinity);
    }
    else if (!m.known || std::isnan(m.minutes)) {
        snprintf(out, len, "%s", kDash);
    }
    else {
        snprintf(out, len, "%dmin", (int)std::max(0.0, std::floor(m.minutes)));
    }
}

int minutesLevel(const Minutes& m)
{
    if (!m.known || m.unlimited || std::isnan(m.minutes)) {
        return 0;
    }
    if (m.minutes < 10) {
        return 2;
    }
    if (m.minutes < 30) {
        return 1;
    }
    return 0;
}

void shortCodecName(const char* raw, char* out, size_t len)
{
    std::string c = upper(raw);
    if (c.find("AV1") != std::string::npos) {
        snprintf(out, len, "AV1");
    }
    else if (c.find("HEVC") != std::string::npos || c.find("H.265") != std::string::npos ||
             c.find("H265") != std::string::npos) {
        snprintf(out, len, "HEVC");
    }
    else if (c.find("H.264") != std::string::npos || c.find("H264") != std::string::npos) {
        snprintf(out, len, "H.264");
    }
    else if (c.empty()) {
        snprintf(out, len, "%s", kDash);
    }
    else {
        snprintf(out, len, "%s", c.c_str());
    }
}

void formatHudLine(char* out, size_t len, int width, int height, double fps,
                   int rttMs, double mbps, const char* codec, const Minutes& minutes)
{
    char resolution[32];
    char fpsText[24];
    char rttText[24];
    char rateText[24];
    char codecText[24];
    char minutesText[24];

    if (width > 0 && height > 0) snprintf(resolution, sizeof(resolution), "%dx%d", width, height);
    else snprintf(resolution, sizeof(resolution), "%s", kDash);

    if (fps > 0) snprintf(fpsText, sizeof(fpsText), "%.0ffps", fps);
    else snprintf(fpsText, sizeof(fpsText), "%s", kDash);

    if (rttMs >= 0) snprintf(rttText, sizeof(rttText), "%dms", rttMs);
    else snprintf(rttText, sizeof(rttText), "%s", kDash);

    if (mbps >= 0) snprintf(rateText, sizeof(rateText), "%.1fMbps", mbps);
    else snprintf(rateText, sizeof(rateText), "%s", kDash);

    shortCodecName(codec, codecText, sizeof(codecText));
    formatMinutes(minutes, minutesText, sizeof(minutesText));

    snprintf(out, len, "%s%s%s%s%s%s%s%s%s%s%s",
             resolution, kSeparator, fpsText, kSeparator, rttText, kSeparator,
             rateText, kSeparator, codecText, kSeparator, minutesText);
}

}
