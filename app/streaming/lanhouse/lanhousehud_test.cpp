// Teste das funções puras do HUD, sem Qt/SDL. Rodar:
//   clang++ -std=c++17 -o /tmp/lanhousehud_test lanhousehud_test.cpp lanhousehud.cpp && /tmp/lanhousehud_test
#include "lanhousehud.h"

#include <cstdio>
#include <cstring>

using namespace LanhouseHud;

static int g_Failures = 0;

static void expectEq(const char* name, const char* got, const char* want)
{
    if (strcmp(got, want) != 0) {
        printf("FAIL %s\n   got:  %s\n   want: %s\n", name, got, want);
        g_Failures++;
    }
    else {
        printf("ok   %s\n", name);
    }
}

static void expectInt(const char* name, int got, int want)
{
    if (got != want) { printf("FAIL %s got %d want %d\n", name, got, want); g_Failures++; }
    else printf("ok   %s\n", name);
}

int main()
{
    char buf[128];

    formatMinutes({true, false, 87.9}, buf, sizeof(buf)); expectEq("minutos arredonda pra baixo", buf, "87min");
    formatMinutes({true, false, 0.4}, buf, sizeof(buf)); expectEq("menos de 1 minuto vira 0min", buf, "0min");
    formatMinutes({true, false, -12}, buf, sizeof(buf)); expectEq("negativo vira 0min", buf, "0min");
    formatMinutes({true, true, 0}, buf, sizeof(buf)); expectEq("ilimitado", buf, "\xE2\x88\x9E");
    formatMinutes({false, false, 0}, buf, sizeof(buf)); expectEq("desconhecido", buf, "\xE2\x80\x94");

    expectInt("nivel 87 = normal", minutesLevel({true, false, 87}), 0);
    expectInt("nivel 30 = normal", minutesLevel({true, false, 30}), 0);
    expectInt("nivel 29.9 = aviso", minutesLevel({true, false, 29.9}), 1);
    expectInt("nivel 10 = aviso", minutesLevel({true, false, 10}), 1);
    expectInt("nivel 9.9 = critico", minutesLevel({true, false, 9.9}), 2);
    expectInt("nivel ilimitado = normal", minutesLevel({true, true, 0}), 0);
    expectInt("nivel desconhecido = normal", minutesLevel({false, false, 0}), 0);

    shortCodecName("HEVC 10-bit HDR", buf, sizeof(buf)); expectEq("codec HEVC HDR", buf, "HEVC");
    shortCodecName("HEVC 4:4:4", buf, sizeof(buf)); expectEq("codec HEVC 444", buf, "HEVC");
    shortCodecName("H.264", buf, sizeof(buf)); expectEq("codec H.264", buf, "H.264");
    shortCodecName("h264", buf, sizeof(buf)); expectEq("codec h264 minusculo", buf, "H.264");
    shortCodecName("AV1 10-bit SDR", buf, sizeof(buf)); expectEq("codec AV1", buf, "AV1");
    shortCodecName("", buf, sizeof(buf)); expectEq("codec vazio", buf, "\xE2\x80\x94");

    formatHudLine(buf, sizeof(buf), 1920, 1080, 59.6, 12, 25.34, "HEVC 10-bit HDR", {true, false, 87.2});
    expectEq("HUD completo", buf, "1920x1080 \xC2\xB7 60fps \xC2\xB7 12ms \xC2\xB7 25.3Mbps \xC2\xB7 HEVC \xC2\xB7 87min");

    formatHudLine(buf, sizeof(buf), 0, 0, 0, -1, -1, "", {false, false, 0});
    expectEq("HUD sem nada medido ainda",
             buf,
             "\xE2\x80\x94 \xC2\xB7 \xE2\x80\x94 \xC2\xB7 \xE2\x80\x94 \xC2\xB7 \xE2\x80\x94 \xC2\xB7 \xE2\x80\x94 \xC2\xB7 \xE2\x80\x94");

    setMinutes(true, true, 0);
    Minutes m = getMinutes();
    expectInt("estado compartilhado guarda ilimitado", m.known && m.unlimited, 1);

    Quality q0 = getQuality();
    expectInt("qualidade começa como não medida", q0.rttMs < 0 && q0.mbps < 0 && q0.fps < 0, 1);
    setQuality(12, 25.3, 59.9);
    Quality q1 = getQuality();
    expectInt("qualidade guarda rtt", q1.rttMs, 12);
    expectInt("qualidade guarda mbps e fps", q1.mbps == 25.3 && q1.fps == 59.9, 1);

    printf(g_Failures == 0 ? "\nTODOS OS TESTES PASSARAM\n" : "\n%d FALHA(S)\n", g_Failures);
    return g_Failures == 0 ? 0 : 1;
}
