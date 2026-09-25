#pragma once

#include <QString>

#include "SDL_compat.h"
#include <SDL_ttf.h>

namespace Overlay {

enum OverlayType {
    OverlayDebug,
    OverlayStatusUpdate,

    // HUD de telemetria da LanHouse, igual na TV, no navegador e aqui:
    //   1920x1080 · 60fps · 12ms · 25.3Mbps · HEVC · 87min
    // Texto contornado, sem caixa, canto superior esquerdo. Começa DESLIGADO em
    // toda sessão e só liga pelo menu do jogo (OverlayMenu) - ao contrário do
    // OverlayDebug (Ctrl+Shift+Alt+S), que continua sendo o overlay verboso do
    // Moonlight original.
    OverlayMinimalStats,

    // Menu do jogo da LanHouse (painel lateral, canto superior direito): o
    // texto do overlay é o modelo "selecionado|telemetriaLigada|controle"
    // (ver input/lanhousemenu.cpp) e o painel inteiro é desenhado aqui,
    // em SDL, pra ficar idêntico no macOS e no Windows.
    OverlayMenu,

    OverlayMax
};

class IOverlayRenderer
{
public:
    virtual ~IOverlayRenderer() = default;

    virtual void notifyOverlayUpdated(OverlayType type) = 0;
};

class OverlayManager
{
public:
    OverlayManager();
    ~OverlayManager();

    bool isOverlayEnabled(OverlayType type);
    char* getOverlayText(OverlayType type);
    void updateOverlayText(OverlayType type, const char* text);
    int getOverlayMaxTextLength();
    void setOverlayTextUpdated(OverlayType type);
    void setOverlayState(OverlayType type, bool enabled);
    SDL_Color getOverlayColor(OverlayType type);
    int getOverlayFontSize(OverlayType type);
    SDL_Surface* getUpdatedOverlaySurface(OverlayType type);

    void setOverlayRenderer(IOverlayRenderer* renderer);

private:
    void notifyOverlayUpdated(OverlayType type);
    SDL_Surface* RenderTextOutlinedWrapped(TTF_Font* font, const char* text, SDL_Color textColor, SDL_Color outlineColor, int outlineWidth, int wrapWidth);
    SDL_Surface* RenderLanhouseHud(TTF_Font* font, const char* text);
    SDL_Surface* RenderLanhouseMenu(const char* model);
    bool ensureLanhouseFonts();

    struct {
        bool enabled;
        int fontSize;
        SDL_Color color;
        char text[1024];

        TTF_Font* font;
        SDL_Surface* surface;
    } m_Overlays[OverlayMax];
    IOverlayRenderer* m_Renderer;
    QByteArray m_FontData;

    // Inter (a mesma fonte do site e da TV) pro HUD e o menu da LanHouse. Os
    // bytes precisam ficar vivos enquanto as fontes estiverem abertas.
    QByteArray m_InterSemiBoldData;
    QByteArray m_InterBoldData;
    // 0 título do menu, 1 título de linha, 2 detalhe, 3 pequeno
    TTF_Font* m_LanhouseFonts[4];
};

}
