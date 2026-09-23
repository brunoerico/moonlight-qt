#include "overlaymanager.h"
#include "path.h"
#include "../lanhouse/lanhousehud.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using namespace Overlay;

OverlayManager::OverlayManager() :
    m_Renderer(nullptr),
    m_FontData(Path::readDataFile("ModeSeven.ttf")),
    m_BarlowSemiBoldData(Path::readDataFile("Barlow-SemiBold.ttf")),
    m_BarlowBoldData(Path::readDataFile("Barlow-Bold.ttf"))
{
    memset(m_Overlays, 0, sizeof(m_Overlays));
    memset(m_LanhouseFonts, 0, sizeof(m_LanhouseFonts));

    m_Overlays[OverlayType::OverlayDebug].color = {0xD0, 0xD0, 0x00, 0xFF};
    m_Overlays[OverlayType::OverlayDebug].fontSize = 20;

    m_Overlays[OverlayType::OverlayStatusUpdate].color = {0xCC, 0x00, 0x00, 0xFF};
    m_Overlays[OverlayType::OverlayStatusUpdate].fontSize = 36;

    m_Overlays[OverlayType::OverlayMinimalStats].color = {0xFF, 0xFF, 0xFF, 0xFF};
    m_Overlays[OverlayType::OverlayMinimalStats].fontSize = 28;
    // Começa DESLIGADO em toda sessão (o cliente liga no menu do jogo se
    // quiser ver) e nunca é guardado - ver OverlayMenu em overlaymanager.h.
    m_Overlays[OverlayType::OverlayMinimalStats].enabled = false;

    // O painel do menu tem as próprias fontes (m_LanhouseFonts); a cor e o
    // tamanho daqui não são usados, só existem pra a entrada não ficar zerada.
    m_Overlays[OverlayType::OverlayMenu].color = {0xFC, 0xFA, 0xF7, 0xFF};
    m_Overlays[OverlayType::OverlayMenu].fontSize = 22;
    m_Overlays[OverlayType::OverlayMenu].enabled = false;

    // While TTF will usually not be initialized here, it is valid for that not to
    // be the case, since Session destruction is deferred and could overlap with
    // the lifetime of a new Session object.
    //SDL_assert(TTF_WasInit() == 0);

    if (TTF_Init() != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "TTF_Init() failed: %s",
                    TTF_GetError());
        return;
    }
}

OverlayManager::~OverlayManager()
{
    for (int i = 0; i < OverlayType::OverlayMax; i++) {
        if (m_Overlays[i].surface != nullptr) {
            SDL_FreeSurface(m_Overlays[i].surface);
        }
        if (m_Overlays[i].font != nullptr) {
            TTF_CloseFont(m_Overlays[i].font);
        }
    }

    for (int i = 0; i < 4; i++) {
        if (m_LanhouseFonts[i] != nullptr) {
            TTF_CloseFont(m_LanhouseFonts[i]);
        }
    }

    TTF_Quit();

    // For similar reasons to the comment in the constructor, this will usually,
    // but not always, deinitialize TTF. In the cases where Session objects overlap
    // in lifetime, there may be an additional reference on TTF for the new Session
    // that means it will not be cleaned up here.
    //SDL_assert(TTF_WasInit() == 0);
}

bool OverlayManager::isOverlayEnabled(OverlayType type)
{
    return m_Overlays[type].enabled;
}

char* OverlayManager::getOverlayText(OverlayType type)
{
    return m_Overlays[type].text;
}

void OverlayManager::updateOverlayText(OverlayType type, const char* text)
{
    SDL_utf8strlcpy(m_Overlays[type].text, text, sizeof(m_Overlays[0].text));
    setOverlayTextUpdated(type);
}

int OverlayManager::getOverlayMaxTextLength()
{
    return sizeof(m_Overlays[0].text);
}

int OverlayManager::getOverlayFontSize(OverlayType type)
{
    return m_Overlays[type].fontSize;
}

SDL_Surface* OverlayManager::getUpdatedOverlaySurface(OverlayType type)
{
    // If a new surface is available, return it. If not, return nullptr.
    // Caller must free the surface on success.
    return (SDL_Surface*)SDL_AtomicSetPtr((void**)&m_Overlays[type].surface, nullptr);
}

void OverlayManager::setOverlayTextUpdated(OverlayType type)
{
    // Only update the overlay state if it's enabled. If it's not enabled,
    // the renderer has already been notified by setOverlayState().
    if (m_Overlays[type].enabled) {
        notifyOverlayUpdated(type);
    }
}

void OverlayManager::setOverlayState(OverlayType type, bool enabled)
{
    bool stateChanged = m_Overlays[type].enabled != enabled;

    m_Overlays[type].enabled = enabled;

    if (stateChanged) {
        if (!enabled) {
            // Set the text to empty string on disable
            m_Overlays[type].text[0] = 0;
        }

        notifyOverlayUpdated(type);
    }
}

SDL_Color OverlayManager::getOverlayColor(OverlayType type)
{
    return m_Overlays[type].color;
}

void OverlayManager::setOverlayRenderer(IOverlayRenderer* renderer)
{
    m_Renderer = renderer;
}

void OverlayManager::notifyOverlayUpdated(OverlayType type)
{
    if (m_Renderer == nullptr) {
        return;
    }

    // Construct the required font to render the overlay. O menu da LanHouse
    // desenha com as próprias fontes (ver RenderLanhouseMenu); o HUD usa Barlow
    // quando ela está embutida, ModeSeven (a do Moonlight original) senão.
    if (type != OverlayType::OverlayMenu && m_Overlays[type].font == nullptr) {
        const QByteArray& fontData =
            (type == OverlayType::OverlayMinimalStats && !m_BarlowSemiBoldData.isEmpty()) ?
                m_BarlowSemiBoldData : m_FontData;

        if (fontData.isEmpty()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL overlay font failed to load");
            return;
        }

        // fontData must stay around until the font is closed
        m_Overlays[type].font = TTF_OpenFontRW(SDL_RWFromConstMem(fontData.constData(), fontData.size()),
                                               1,
                                               m_Overlays[type].fontSize);
        if (m_Overlays[type].font == nullptr) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "TTF_OpenFont() failed: %s",
                        TTF_GetError());

            // Can't proceed without a font
            return;
        }
    }

    // Exchange the old surface with the new one
    SDL_Surface* newSurface = nullptr;
    if (m_Overlays[type].enabled) {
        if (type == OverlayType::OverlayMinimalStats) {
            newSurface = RenderLanhouseHud(m_Overlays[type].font, m_Overlays[type].text);
        }
        else if (type == OverlayType::OverlayMenu) {
            newSurface = RenderLanhouseMenu(m_Overlays[type].text);
        }
        else {
            // The _Wrapped variant is required for line breaks to work
            newSurface = RenderTextOutlinedWrapped(m_Overlays[type].font,
                                                   m_Overlays[type].text,
                                                   m_Overlays[type].color,
                                                   {0, 0, 0, 255},
                                                   4,
                                                   1024);
        }
    }
    SDL_Surface* oldSurface = (SDL_Surface*)SDL_AtomicSetPtr((void**)&m_Overlays[type].surface, newSurface);

    // Notify the renderer
    m_Renderer->notifyOverlayUpdated(type);

    // Free the old surface
    if (oldSurface != nullptr) {
        SDL_FreeSurface(oldSurface);
    }
}

SDL_Surface* OverlayManager::RenderTextOutlinedWrapped(TTF_Font* font, const char* text, SDL_Color textColor, SDL_Color outlineColor, int outlineWidth, int wrapWidth) {
    if (text == nullptr || text[0] == '\0') {
        return nullptr;
    }

    int oldOutline = TTF_GetFontOutline(font);
    TTF_SetFontOutline(font, outlineWidth);

    // Verify that the string won't require wrapping (which could cause the outline and the text
    // to diverge due to different wrapping positions).
    //
    // FIXME: We do this rather than just disabling wrapping entirely (wrapWidth = 0) because we
    // need further testing to ensure that all renderers can handle non-NPOT overlay textures.
    for (const QString& line : QString(text).split('\n')) {
        int extent, count;
        if (TTF_MeasureUTF8(font, line.toUtf8(), wrapWidth, &extent, &count) == 0 && count < line.size()) {
            // If it requires wrapping, render it without the outline
            TTF_SetFontOutline(font, oldOutline);
            return TTF_RenderUTF8_Blended_Wrapped(font, text, textColor, wrapWidth);
        }
    }

    // Draw text twice, but outline is a bit bigger
    auto outlineSurface = TTF_RenderUTF8_Blended_Wrapped(font, text, outlineColor, wrapWidth);
    TTF_SetFontOutline(font, 0);
    auto textSurface = TTF_RenderUTF8_Blended_Wrapped(font, text, textColor, wrapWidth);
    TTF_SetFontOutline(font, oldOutline);

    if (outlineSurface == nullptr || textSurface == nullptr) {
        SDL_FreeSurface(outlineSurface);
        SDL_FreeSurface(textSurface);
        return nullptr;
    }

    // Merge the texts
    SDL_Rect dst = { outlineWidth, outlineWidth, textSurface->w, textSurface->h };
    SDL_BlitSurface(textSurface, nullptr, outlineSurface, &dst);

    SDL_FreeSurface(textSurface);
    return outlineSurface;
}

namespace {

// Retângulo preenchido com cantos arredondados, sobre a superfície RGBA do
// painel (SDL_FillRect por linha - sem depender de SDL2_gfx).
void fillRoundRect(SDL_Surface* surface, int x, int y, int w, int h, int radius, SDL_Color color)
{
    Uint32 mapped = SDL_MapRGBA(surface->format, color.r, color.g, color.b, color.a);
    radius = std::min(radius, std::min(w, h) / 2);
    for (int row = 0; row < h; row++) {
        int inset = 0;
        if (row < radius) {
            double dy = radius - row - 0.5;
            inset = (int)std::ceil(radius - std::sqrt((double)radius * radius - dy * dy));
        }
        else if (row >= h - radius) {
            double dy = row - (h - radius) + 0.5;
            inset = (int)std::ceil(radius - std::sqrt((double)radius * radius - dy * dy));
        }
        SDL_Rect line = { x + inset, y + row, w - 2 * inset, 1 };
        SDL_FillRect(surface, &line, mapped);
    }
}

void blitAt(SDL_Surface* dst, SDL_Surface* src, int x, int y)
{
    if (src == nullptr) {
        return;
    }
    SDL_Rect rect = { x, y, src->w, src->h };
    SDL_BlitSurface(src, nullptr, dst, &rect);
}

}

bool OverlayManager::ensureLanhouseFonts()
{
    if (m_LanhouseFonts[0] != nullptr) {
        return true;
    }
    if (m_BarlowBoldData.isEmpty() || m_BarlowSemiBoldData.isEmpty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Barlow overlay fonts failed to load");
        return false;
    }

    struct { const QByteArray* data; int size; } specs[4] = {
        { &m_BarlowBoldData, 44 },     // título "MENU"
        { &m_BarlowBoldData, 27 },     // título de cada linha
        { &m_BarlowSemiBoldData, 19 }, // detalhe da linha
        { &m_BarlowSemiBoldData, 18 }, // texto pequeno (marca, aviso, atalhos, selo)
    };
    for (int i = 0; i < 4; i++) {
        m_LanhouseFonts[i] = TTF_OpenFontRW(SDL_RWFromConstMem(specs[i].data->constData(), specs[i].data->size()),
                                            1, specs[i].size);
        if (m_LanhouseFonts[i] == nullptr) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "TTF_OpenFont(Barlow) failed: %s", TTF_GetError());
            for (int j = 0; j < i; j++) {
                TTF_CloseFont(m_LanhouseFonts[j]);
                m_LanhouseFonts[j] = nullptr;
            }
            m_LanhouseFonts[i] = nullptr;
            return false;
        }
    }
    return true;
}

// HUD: texto contornado, sem caixa. O último campo (minutos restantes) é
// desenhado à parte pra poder mudar de cor (âmbar/vermelho) sem colorir o
// resto da linha.
SDL_Surface* OverlayManager::RenderLanhouseHud(TTF_Font* font, const char* text)
{
    if (font == nullptr || text == nullptr || text[0] == '\0') {
        return nullptr;
    }

    const char* separator = " \xC2\xB7 ";
    const char* lastSeparator = nullptr;
    for (const char* p = strstr(text, separator); p != nullptr; p = strstr(p + 1, separator)) {
        lastSeparator = p;
    }

    std::string prefix = lastSeparator ? std::string(text, lastSeparator + strlen(separator)) : std::string(text);
    std::string minutes = lastSeparator ? std::string(lastSeparator + strlen(separator)) : std::string();

    SDL_Color white = {0xFF, 0xFF, 0xFF, 0xFF};
    SDL_Color black = {0, 0, 0, 0xFF};
    SDL_Color minutesColor = white;
    switch (LanhouseHud::minutesLevel(LanhouseHud::getMinutes())) {
    case 1: minutesColor = {0xFF, 0xC2, 0x4B, 0xFF}; break; // âmbar: abaixo de 30 min
    case 2: minutesColor = {0xFF, 0x6A, 0x5E, 0xFF}; break; // vermelho: abaixo de 10 min
    default: break;
    }

    const int outline = 3;
    SDL_Surface* head = RenderTextOutlinedWrapped(font, prefix.c_str(), white, black, outline, 4096);
    if (head == nullptr) {
        return nullptr;
    }
    if (minutes.empty()) {
        return head;
    }
    SDL_Surface* tail = RenderTextOutlinedWrapped(font, minutes.c_str(), minutesColor, black, outline, 4096);
    if (tail == nullptr) {
        return head;
    }

    // Cada superfície carrega o contorno dos dois lados; sobrepõe 2x o contorno
    // pra o trecho de minutos ficar colado no separador, sem vão.
    int overlap = 2 * outline;
    SDL_Surface* merged = SDL_CreateRGBSurfaceWithFormat(0, head->w + tail->w - overlap,
                                                         std::max(head->h, tail->h), 32, SDL_PIXELFORMAT_RGBA32);
    if (merged != nullptr) {
        SDL_SetSurfaceBlendMode(head, SDL_BLENDMODE_BLEND);
        SDL_SetSurfaceBlendMode(tail, SDL_BLENDMODE_BLEND);
        blitAt(merged, head, 0, 0);
        blitAt(merged, tail, head->w - overlap, 0);
    }
    SDL_FreeSurface(head);
    SDL_FreeSurface(tail);
    return merged;
}

// Menu do jogo: painel escuro com dois itens (Telemetria e Sair do jogo), o
// selecionado em vermelho, aviso de que o jogo continua rodando e os atalhos
// do dispositivo em uso. `model` = "selecionado|telemetriaLigada|controle"
// (controle: 0 teclado, 1 PlayStation, 2 Xbox).
SDL_Surface* OverlayManager::RenderLanhouseMenu(const char* model)
{
    int selected = 0, telemetryOn = 0, controller = 0;
    if (model == nullptr || sscanf(model, "%d|%d|%d", &selected, &telemetryOn, &controller) < 2) {
        return nullptr;
    }
    if (!ensureLanhouseFonts()) {
        return nullptr;
    }

    TTF_Font* titleFont = m_LanhouseFonts[0];
    TTF_Font* rowFont = m_LanhouseFonts[1];
    TTF_Font* detailFont = m_LanhouseFonts[2];
    TTF_Font* smallFont = m_LanhouseFonts[3];

    const SDL_Color paper = {0xFC, 0xFA, 0xF7, 0xFF};
    const SDL_Color muted = {0xCF, 0xC7, 0xC2, 0xFF};
    const SDL_Color ink = {0x1B, 0x16, 0x14, 0xFF};
    const SDL_Color red = {0xD6, 0x29, 0x1F, 0xFF};
    const SDL_Color panelBg = {0x12, 0x0E, 0x0C, 0xEE};

    const int panelW = 580;
    const int padX = 36;
    const int padTop = 34;
    const int padBottom = 28;
    const int innerW = panelW - 2 * padX;
    const int rowPadX = 24;
    const int rowPadY = 18;
    const int rowGap = 10;

    struct Row {
        const char* title;
        const char* detail;
        const char* badge; // nullptr = sem selo
    } rows[2] = {
        { "Telemetria", "Mostra resolução, fps, latência, taxa de dados, codec e tempo restante.",
          telemetryOn ? "LIGADA" : "DESLIGADA" },
        { "Sair do jogo", "Encerra a sessão e volta ao catálogo.", nullptr },
    };

    // Atalhos do dispositivo em uso (Barlow não tem setas, então texto puro).
    const char* hints = controller == 1 ? "D-pad navega    Cross confirma    Circle fecha"
                      : controller == 2 ? "D-pad navega    A confirma    B fecha"
                                        : "Setas navegam    Enter confirma    Esc fecha";
    const char* warning = "O jogo continua rodando enquanto o menu está aberto.";

    // 1) Renderiza todo o texto primeiro pra medir a altura do painel.
    SDL_Surface* title = TTF_RenderUTF8_Blended(titleFont, "MENU", paper);
    SDL_Surface* brand = TTF_RenderUTF8_Blended(smallFont, "LanHouse Cloud Gaming", muted);
    SDL_Surface* rowTitle[2] = {};
    SDL_Surface* rowDetail[2] = {};
    SDL_Surface* rowBadge[2] = {};
    int rowHeight[2] = {};
    int badgeW[2] = {};

    for (int i = 0; i < 2; i++) {
        rowTitle[i] = TTF_RenderUTF8_Blended(rowFont, rows[i].title, paper);
        if (rows[i].badge != nullptr) {
            rowBadge[i] = TTF_RenderUTF8_Blended(smallFont, rows[i].badge, (i == 0 && telemetryOn) ? ink : paper);
            badgeW[i] = rowBadge[i] ? rowBadge[i]->w + 40 : 0;
        }
        int detailWrap = innerW - 2 * rowPadX - (badgeW[i] ? badgeW[i] + 18 : 0);
        rowDetail[i] = TTF_RenderUTF8_Blended_Wrapped(detailFont, rows[i].detail, muted, detailWrap);
        rowHeight[i] = 2 * rowPadY + (rowTitle[i] ? rowTitle[i]->h : 0) + 4 + (rowDetail[i] ? rowDetail[i]->h : 0);
    }
    SDL_Surface* warn = TTF_RenderUTF8_Blended_Wrapped(smallFont, warning, muted, innerW);
    SDL_Surface* hint = TTF_RenderUTF8_Blended_Wrapped(smallFont, hints, paper, innerW);

    int headerH = title ? title->h : 0;
    int panelH = padTop + headerH + 26 + rowHeight[0] + rowGap + rowHeight[1] + 22 +
                 (warn ? warn->h : 0) + 20 + 1 + 16 + (hint ? hint->h : 0) + padBottom;

    SDL_Surface* panel = SDL_CreateRGBSurfaceWithFormat(0, panelW, panelH, 32, SDL_PIXELFORMAT_RGBA32);
    if (panel != nullptr) {
        SDL_SetSurfaceBlendMode(panel, SDL_BLENDMODE_BLEND);
        SDL_FillRect(panel, nullptr, SDL_MapRGBA(panel->format, panelBg.r, panelBg.g, panelBg.b, panelBg.a));
        // Fio claro na borda esquerda, separando o painel do jogo.
        SDL_Rect edge = { 0, 0, 1, panelH };
        SDL_FillRect(panel, &edge, SDL_MapRGBA(panel->format, 0x3C, 0x38, 0x36, 0xFF));

        int y = padTop;
        blitAt(panel, title, padX, y);
        if (title != nullptr) {
            // Ponto vermelho depois do título (mesmo detalhe do menu da TV).
            int dot = 14;
            fillRoundRect(panel, padX + title->w + 8, y + title->h - dot - 10, dot, dot, dot / 2, red);
        }
        if (brand != nullptr) {
            blitAt(panel, brand, panelW - padX - brand->w, y + headerH - brand->h - 6);
        }
        y += headerH + 26;

        for (int i = 0; i < 2; i++) {
            bool isSelected = (selected == i);
            SDL_Color rowBg = isSelected ? red : SDL_Color{0, 0, 0, 0};
            if (isSelected) {
                fillRoundRect(panel, padX, y, innerW, rowHeight[i], 14, red);
            }
            int textX = padX + rowPadX;
            blitAt(panel, rowTitle[i], textX, y + rowPadY);
            blitAt(panel, rowDetail[i], textX, y + rowPadY + (rowTitle[i] ? rowTitle[i]->h : 0) + 4);

            if (rowBadge[i] != nullptr) {
                int pillW = badgeW[i];
                int pillH = rowBadge[i]->h + 14;
                int pillX = padX + innerW - rowPadX - pillW;
                int pillY = y + (rowHeight[i] - pillH) / 2;
                bool on = (i == 0 && telemetryOn);
                SDL_Color border = on ? paper : SDL_Color{0xFC, 0xFA, 0xF7, 0x8C};
                fillRoundRect(panel, pillX, pillY, pillW, pillH, pillH / 2, border);
                // Miolo: branco quando ligado; quando desligado, a cor da linha por dentro do contorno.
                SDL_Color inner = on ? paper : (isSelected ? red : SDL_Color{panelBg.r, panelBg.g, panelBg.b, 0xFF});
                if (!on && !isSelected) {
                    inner = SDL_Color{panelBg.r, panelBg.g, panelBg.b, 0xFF};
                }
                fillRoundRect(panel, pillX + 2, pillY + 2, pillW - 4, pillH - 4, (pillH - 4) / 2, inner);
                blitAt(panel, rowBadge[i], pillX + (pillW - rowBadge[i]->w) / 2, pillY + (pillH - rowBadge[i]->h) / 2);
            }
            (void)rowBg;
            y += rowHeight[i] + rowGap;
        }
        y += 22 - rowGap;

        blitAt(panel, warn, padX + 8, y);
        y += (warn ? warn->h : 0) + 20;

        SDL_Rect rule = { padX, y, innerW, 1 };
        SDL_FillRect(panel, &rule, SDL_MapRGBA(panel->format, 0x3C, 0x38, 0x36, 0xFF));
        y += 1 + 16;
        blitAt(panel, hint, padX + 8, y);
    }

    SDL_FreeSurface(title);
    SDL_FreeSurface(brand);
    SDL_FreeSurface(warn);
    SDL_FreeSurface(hint);
    for (int i = 0; i < 2; i++) {
        SDL_FreeSurface(rowTitle[i]);
        SDL_FreeSurface(rowDetail[i]);
        SDL_FreeSurface(rowBadge[i]);
    }
    return panel;
}
