#pragma once

#include <cstddef>

// LanHouse - conteúdo do HUD de telemetria e estado do "tempo restante".
//
// Mesmo formato nas três telas (TV, navegador, este app desktop):
//
//   1920x1080 · 60fps · 12ms · 25.3Mbps · HEVC · 87min
//
// sem rótulos, na ordem fixa; "—" no campo que ainda não existe. Tudo aqui é
// função pura (ou estado simples) só com a biblioteca padrão, pra dar pra
// testar sem Qt/SDL - ver lanhousehud_test.cpp.
namespace LanhouseHud {

struct Minutes {
    // false até o app wrapper mandar o primeiro valor (ou se rodar sem
    // wrapper, ex.: lanhouse-connect direto no terminal): o HUD mostra "—".
    bool known = false;
    bool unlimited = false;
    double minutes = 0;
};

// Escrito pelo canal de comandos do wrapper (lanhousecommands.cpp), lido pelo
// decoder de vídeo em outra thread - por isso com mutex.
void setMinutes(bool known, bool unlimited, double minutes);
Minutes getMinutes();

// "87min" (arredonda pra baixo: nunca prometer mais tempo do que resta),
// "∞" no plano ilimitado, "—" enquanto não se sabe.
void formatMinutes(const Minutes& m, char* out, size_t len);

// 0 = normal, 1 = aviso (abaixo de 30 min, mesmo limite do dashboard),
// 2 = crítico (abaixo de 10 min).
int minutesLevel(const Minutes& m);

// "HEVC 10-bit HDR" -> "HEVC", "H.264" -> "H.264": nome curto igual nas 3 telas.
void shortCodecName(const char* raw, char* out, size_t len);

// rttMs < 0 e mbps < 0 significam "ainda não medido" (viram "—").
void formatHudLine(char* out, size_t len, int width, int height, double fps,
                   int rttMs, double mbps, const char* codec, const Minutes& minutes);

}
