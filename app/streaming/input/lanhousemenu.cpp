#include "input.h"
#include "streaming/session.h"

#include <Limelight.h>
#include "SDL_compat.h"

#include <cstdio>

// LanHouse - menu do jogo do app desktop. Mesmos dois itens da TV e do
// navegador: "Telemetria" (liga/desliga o HUD) e "Sair do jogo". O desenho do
// painel fica em OverlayManager::RenderLanhouseMenu; aqui só a entrada e o
// estado. Tudo roda na thread principal (o loop de eventos do Session), exceto
// o callback do timer, que só posta um evento.

#define LANHOUSE_MENU_ITEM_TELEMETRY 0
#define LANHOUSE_MENU_ITEM_EXIT 1
#define LANHOUSE_MENU_ITEM_COUNT 2

// Igual à TV e ao navegador: segurar por 3s pra não abrir sem querer.
#define LANHOUSE_COMBO_HOLD_MS 3000

// L1+R1+Start (Xbox: LB+RB+Menu; PlayStation: L1+R1+Options).
#define LANHOUSE_COMBO_BUTTONS (LB_FLAG | RB_FLAG | PLAY_FLAG)

Uint32 SdlInputHandler::lanhouseComboTimerCallback(Uint32 /* interval */, void* /* param */)
{
    // Thread do timer do SDL: só SDL_PushEvent, que é thread-safe.
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_USEREVENT;
    event.user.code = SDL_CODE_LANHOUSE_MENU_COMBO;
    SDL_PushEvent(&event);
    return 0; // não repete
}

void SdlInputHandler::lanhouseUpdateComboHold(GamepadState* state)
{
    if (m_LanhouseMenuOpen) {
        return;
    }

    if (state->buttons == LANHOUSE_COMBO_BUTTONS) {
        // O jogo não deve ver o atalho: bloqueia os três botões até soltarem.
        state->lanhouseMask |= LANHOUSE_COMBO_BUTTONS;

#if SDL_VERSION_ATLEAST(2, 0, 12)
        switch (SDL_GameControllerGetType(state->controller)) {
        case SDL_CONTROLLER_TYPE_PS3:
        case SDL_CONTROLLER_TYPE_PS4:
        case SDL_CONTROLLER_TYPE_PS5:
            m_LanhouseComboController = 1;
            break;
        default:
            m_LanhouseComboController = 2;
            break;
        }
#else
        m_LanhouseComboController = 2;
#endif

        if (m_LanhouseComboTimer == 0) {
            m_LanhouseComboTimer = SDL_AddTimer(LANHOUSE_COMBO_HOLD_MS,
                                                SdlInputHandler::lanhouseComboTimerCallback, this);
        }
    }
    else if (m_LanhouseComboTimer != 0) {
        // Soltou algum botão antes dos 3s.
        SDL_RemoveTimer(m_LanhouseComboTimer);
        m_LanhouseComboTimer = 0;
    }
}

void SdlInputHandler::lanhouseOpenMenuFromGamepad()
{
    m_LanhouseComboTimer = 0;

    // O evento pode chegar depois de o jogador já ter soltado o atalho.
    bool stillHeld = false;
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        if (m_GamepadState[i].controller != nullptr &&
                m_GamepadState[i].buttons == LANHOUSE_COMBO_BUTTONS) {
            stillHeld = true;
            break;
        }
    }
    if (stillHeld) {
        lanhouseOpenMenu(m_LanhouseComboController);
    }
}

void SdlInputHandler::lanhouseOpenMenu(int controllerKind)
{
    if (m_LanhouseMenuOpen) {
        return;
    }

    m_LanhouseMenuOpen = true;
    m_LanhouseMenuSelected = LANHOUSE_MENU_ITEM_TELEMETRY;
    m_LanhouseMenuController = controllerKind;

    // O jogo continua rodando por trás, mas sem nenhuma tecla/botão preso.
    raiseAllKeys();
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        if (m_GamepadState[i].controller != nullptr) {
            sendGamepadState(&m_GamepadState[i]); // com o menu aberto, manda tudo solto
        }
    }

    Overlay::OverlayManager& overlays = Session::get()->getOverlayManager();
    char model[32];
    snprintf(model, sizeof(model), "%d|%d|%d", m_LanhouseMenuSelected,
             overlays.isOverlayEnabled(Overlay::OverlayMinimalStats) ? 1 : 0,
             m_LanhouseMenuController);
    // Texto antes de ligar: setOverlayState(true) já desenha com ele.
    SDL_utf8strlcpy(overlays.getOverlayText(Overlay::OverlayMenu), model, overlays.getOverlayMaxTextLength());
    overlays.setOverlayState(Overlay::OverlayMenu, true);
}

void SdlInputHandler::lanhouseCloseMenu()
{
    if (!m_LanhouseMenuOpen) {
        return;
    }

    m_LanhouseMenuOpen = false;
    Session::get()->getOverlayManager().setOverlayState(Overlay::OverlayMenu, false);

    // O que ainda está apertado (ex.: o B que fechou o menu) só vai pro jogo
    // depois de ser solto, senão o jogo veria um clique que não foi dele.
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        if (m_GamepadState[i].controller != nullptr) {
            m_GamepadState[i].lanhouseMask = m_GamepadState[i].buttons;
            sendGamepadState(&m_GamepadState[i]);
        }
    }
}

void SdlInputHandler::lanhouseRefreshMenu()
{
    Overlay::OverlayManager& overlays = Session::get()->getOverlayManager();
    char model[32];
    snprintf(model, sizeof(model), "%d|%d|%d", m_LanhouseMenuSelected,
             overlays.isOverlayEnabled(Overlay::OverlayMinimalStats) ? 1 : 0,
             m_LanhouseMenuController);
    overlays.updateOverlayText(Overlay::OverlayMenu, model);
}

void SdlInputHandler::lanhouseMenuMove(int delta)
{
    m_LanhouseMenuSelected = (m_LanhouseMenuSelected + delta + LANHOUSE_MENU_ITEM_COUNT) % LANHOUSE_MENU_ITEM_COUNT;
    lanhouseRefreshMenu();
}

void SdlInputHandler::lanhouseMenuActivate()
{
    Overlay::OverlayManager& overlays = Session::get()->getOverlayManager();

    if (m_LanhouseMenuSelected == LANHOUSE_MENU_ITEM_TELEMETRY) {
        // Começa desligada em toda sessão e nunca é guardada em lugar nenhum.
        overlays.setOverlayState(Overlay::OverlayMinimalStats,
                                 !overlays.isOverlayEnabled(Overlay::OverlayMinimalStats));
        lanhouseRefreshMenu();
    }
    else {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "LanHouse menu: sair do jogo");

        // Igual ao atalho "quit and exit" do Moonlight: encerra o stream,
        // fecha o jogo no host e sai do processo - o wrapper percebe a saída
        // e encerra a sessão de tempo no site.
        Session::get()->setShouldExit(true);

        SDL_Event quitEvent;
        SDL_zero(quitEvent);
        quitEvent.type = SDL_QUIT;
        quitEvent.quit.timestamp = SDL_GetTicks();
        SDL_PushEvent(&quitEvent);
    }
}

bool SdlInputHandler::lanhouseHandleMenuKey(SDL_KeyboardEvent* event)
{
    // Atalho: Ctrl+Alt+M (Ctrl+Option+M no Mac), sem Shift/GUI - Shift já é
    // dos atalhos originais do Moonlight (Ctrl+Alt+Shift+M = modo do mouse).
    // Usa o scancode pra funcionar em qualquer layout (Option+M vira "µ" no Mac).
    if (event->keysym.scancode == SDL_SCANCODE_M &&
            (event->keysym.mod & KMOD_CTRL) && (event->keysym.mod & KMOD_ALT) &&
            !(event->keysym.mod & (KMOD_SHIFT | KMOD_GUI))) {
        if (event->state == SDL_PRESSED) {
            // Segurar a tecla repete o keydown: só o primeiro conta.
            if (!event->repeat) {
                m_LanhouseSuppressMKeyUp = true;
                if (m_LanhouseMenuOpen) {
                    lanhouseCloseMenu();
                }
                else {
                    lanhouseOpenMenu(0);
                }
            }
            return true;
        }
    }

    if (event->state == SDL_RELEASED && event->keysym.scancode == SDL_SCANCODE_M && m_LanhouseSuppressMKeyUp) {
        m_LanhouseSuppressMKeyUp = false;
        return true;
    }

    if (!m_LanhouseMenuOpen) {
        return false;
    }

    // Menu aberto: o teclado é do menu, nada chega no jogo.
    if (event->state == SDL_PRESSED && !event->repeat) {
        switch (event->keysym.scancode) {
        case SDL_SCANCODE_UP:
            lanhouseMenuMove(-1);
            break;
        case SDL_SCANCODE_DOWN:
            lanhouseMenuMove(1);
            break;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:
        case SDL_SCANCODE_SPACE:
            lanhouseMenuActivate();
            break;
        case SDL_SCANCODE_ESCAPE:
            lanhouseCloseMenu();
            break;
        default:
            break;
        }
    }
    return true;
}

bool SdlInputHandler::lanhouseHandleMenuButton(SDL_ControllerButtonEvent* event, GamepadState* state)
{
    if (!m_LanhouseMenuOpen) {
        return false;
    }

    // Mantém o estado dos botões em dia pra saber o que está apertado quando
    // o menu fechar (ver lanhouseCloseMenu).
    // (o chamador já checou que o botão tem mapeamento em k_ButtonMap)
    if (event->state == SDL_PRESSED) {
        state->buttons |= k_ButtonMap[event->button];
    }
    else {
        state->buttons &= ~k_ButtonMap[event->button];
    }

    if (event->state == SDL_PRESSED) {
        switch (event->button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            lanhouseMenuMove(-1);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            lanhouseMenuMove(1);
            break;
        case SDL_CONTROLLER_BUTTON_A:
            lanhouseMenuActivate();
            break;
        case SDL_CONTROLLER_BUTTON_B:
            lanhouseCloseMenu();
            break;
        default:
            break;
        }
    }
    return true;
}
