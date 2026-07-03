#pragma once
/**
 * @file  lockscreen.h
 * @brief Tela de autenticação RFID — exibida antes do dashboard.
 */

#include "imgui.h"
#include "theme.h"
#include "widgets.h"
#include "backend.h"
#include <cmath>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Estados do lock screen
// ─────────────────────────────────────────────────────────────────────────────
enum class LockState
{
    Locked,      // aguardando tag
    Denied,      // tag errada — feedback vermelho
    Unlocking,   // animação de abertura (transição)
    Unlocked,    // dashboard visível
};

// ─────────────────────────────────────────────────────────────────────────────
// Contexto de animação do lock screen
// ─────────────────────────────────────────────────────────────────────────────
struct LockContext
{
    LockState state          = LockState::Locked;
    float     transitionT    = 0.0f;  // [0..1] progresso da animação unlock/lock
    float     deniedTimer    = 0.0f;  // segundos restantes no estado Denied
    float     shakeOffset    = 0.0f;  // deslocamento X do shake de negação
    float     shakeTimer     = 0.0f;  // tempo restante do shake
    std::string lastUID      = "";    // último UID lido (para exibir)
    float     fadeAlpha      = 1.0f;  // opacidade do lock screen (fade-out no unlock)
    bool      unlockDone     = false; // sinaliza que a transição terminou

    // Chama a cada frame, atualiza timers internos
    void Update(float dt)
    {
        if (state == LockState::Denied)
        {
            deniedTimer -= dt;
            if (deniedTimer <= 0.0f)
            {
                deniedTimer = 0.0f;
                state       = LockState::Locked;
            }
        }

        if (state == LockState::Unlocking)
        {
            transitionT += dt * 1.4f; // ~0.7s de animação
            fadeAlpha    = 1.0f - transitionT;
            if (transitionT >= 1.0f)
            {
                transitionT  = 1.0f;
                fadeAlpha    = 0.0f;
                state        = LockState::Unlocked;
                unlockDone   = true;
            }
        }

        // Shake: decai rapidamente
        if (shakeTimer > 0.0f)
        {
            shakeTimer  -= dt;
            float phase = shakeTimer * 40.0f;
            shakeOffset = sinf(phase) * 8.0f * fmaxf(0.0f, shakeTimer / 0.4f);
            if (shakeTimer <= 0.0f) shakeOffset = 0.0f;
        }
    }

    // Chamado quando tag correta é lida
    void TriggerUnlock(const std::string& uid)
    {
        if (state != LockState::Locked) return;
        lastUID     = uid;
        state       = LockState::Unlocking;
        transitionT = 0.0f;
        fadeAlpha   = 1.0f;
        unlockDone  = false;
    }

    // Chamado quando tag errada é lida
    void TriggerDenied(const std::string& uid)
    {
        if (state != LockState::Locked) return;
        lastUID    = uid;
        state      = LockState::Denied;
        deniedTimer = 2.0f;
        shakeTimer  = 0.4f;
    }

    // Chamado pelo botão "Bloquear" no dashboard
    void Lock()
    {
        state       = LockState::Locked;
        transitionT = 0.0f;
        fadeAlpha   = 1.0f;
        lastUID     = "";
        unlockDone  = false;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Desenha o cadeado animado (ícone central do lock screen)
// ─────────────────────────────────────────────────────────────────────────────
static void DrawPadlock(ImDrawList* dl, ImVec2 center, float size,
                        LockState state, float transitionT)
{
    float t = static_cast<float>(ImGui::GetTime());

    // Cores baseadas no estado
    ImU32 bodyColor, archColor;
    float archOpenAngle = 0.0f; // quanto o arco "abre" no unlock

    switch (state)
    {
    case LockState::Denied:
        bodyColor  = IM_COL32(255, 68, 68, 220);
        archColor  = IM_COL32(255, 68, 68, 180);
        break;
    case LockState::Unlocking:
        bodyColor  = IM_COL32(0, 255, 136, 220);
        archColor  = IM_COL32(0, 255, 136, 180);
        archOpenAngle = transitionT * 0.6f; // arco abre durante a transição
        break;
    case LockState::Unlocked:
        bodyColor  = IM_COL32(0, 255, 136, 200);
        archColor  = IM_COL32(0, 255, 136, 160);
        archOpenAngle = 0.6f;
        break;
    default: // Locked
        bodyColor  = IM_COL32(0, 212, 255, 200);
        archColor  = IM_COL32(0, 212, 255, 160);
        break;
    }

    float hw = size * 0.38f; // meia largura do corpo
    float hh = size * 0.30f; // meia altura do corpo
    float cx = center.x;
    float cy = center.y + size * 0.08f; // corpo ligeiramente para baixo

    // Corpo do cadeado (retângulo arredondado)
    dl->AddRectFilled(
        {cx - hw, cy - hh},
        {cx + hw, cy + hh},
        bodyColor, size * 0.07f
    );

    // Buraco da fechadura
    float keyR = size * 0.08f;
    dl->AddCircleFilled(center, keyR, IM_COL32(13, 19, 27, 255), 16);
    // Haste da chave
    dl->AddRectFilled(
        {cx - keyR * 0.35f, cy},
        {cx + keyR * 0.35f, cy + size * 0.14f},
        IM_COL32(13, 19, 27, 255)
    );

    // Arco superior do cadeado
    float archR  = size * 0.28f;
    float archCy = cy - hh - size * 0.01f; // base do arco encosta no topo do corpo
    float startAngle = static_cast<float>(M_PI) + archOpenAngle;
    float endAngle   = 0.0f   - archOpenAngle;

    // Desenha o arco como polyline
    int   segs   = 24;
    float thick  = size * 0.07f;
    ImVec2 prev  = {};
    bool hasPrev = false;
    for (int i = 0; i <= segs; i++)
    {
        float a  = startAngle + (endAngle - startAngle) * i / segs;
        ImVec2 pt = {cx + cosf(a) * archR, archCy + sinf(a) * archR};
        if (hasPrev)
            dl->AddLine(prev, pt, archColor, thick);
        prev    = pt;
        hasPrev = true;
    }

    // Pulso de glow (apenas no estado Locked)
    if (state == LockState::Locked)
    {
        float pulse = 0.5f + 0.5f * sinf(t * 1.8f);
        ImU32 glow  = IM_COL32(0, 212, 255, static_cast<int>(18 * pulse));
        dl->AddCircleFilled(center, size * 0.55f, glow, 32);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawLockScreen — renderiza a tela de lock completa
// Retorna true enquanto deve ser exibida (state != Unlocked com fade completo)
// ─────────────────────────────────────────────────────────────────────────────
inline bool DrawLockScreen(LockContext& ctx, const FrameSnapshot& snap,
                           float winW, float winH)
{
    // Depois do unlock, não exibe mais
    if (ctx.state == LockState::Unlocked && ctx.fadeAlpha <= 0.0f)
        return false;

    float t = static_cast<float>(ImGui::GetTime());

    // Overlay escuro com alpha controlado pela transição
    ImU32 overlayCol = IM_COL32(12, 17, 23,
                                static_cast<int>(255 * ctx.fadeAlpha));
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        {0, 0}, {winW, winH}, overlayCol
    );

    // Calcula posição central com shake horizontal
    float cx  = winW * 0.5f + ctx.shakeOffset;
    float cy  = winH * 0.5f;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // ── Radar de fundo (maior, atrás do cadeado) ─────────────────────────────
    bool  connected = snap.connStatus == ConnectionStatus::Connected;
    bool  tagActive = snap.lastResult != AccessResult::None;
    float radarR    = fminf(winW, winH) * 0.28f;

    // Fade alpha aplicado ao radar
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ctx.fadeAlpha);
    DrawRadarSweep(dl, {cx, cy}, radarR, connected, tagActive);
    ImGui::PopStyleVar();

    // ── Cadeado central ───────────────────────────────────────────────────────
    float lockSize = 52.0f;
    float lockAlpha = ctx.fadeAlpha;
    // No estado Unlocking: cadeado faz fade-out
    if (ctx.state == LockState::Unlocking)
        lockAlpha = 1.0f - ctx.transitionT * 1.5f;
    lockAlpha = fmaxf(0.0f, fminf(1.0f, lockAlpha));

    if (lockAlpha > 0.01f)
    {
        // Círculo de fundo do cadeado
        ImU32 circleCol = (ctx.state == LockState::Denied)
            ? IM_COL32(255, 68, 68, static_cast<int>(30 * lockAlpha))
            : IM_COL32(0, 212, 255, static_cast<int>(18 * lockAlpha));
        dl->AddCircleFilled({cx, cy}, lockSize * 0.88f, circleCol, 48);

        ImU32 circleBorder = (ctx.state == LockState::Denied)
            ? IM_COL32(255, 68, 68, static_cast<int>(120 * lockAlpha))
            : (ctx.state == LockState::Unlocking)
                ? IM_COL32(0, 255, 136, static_cast<int>(160 * lockAlpha))
                : IM_COL32(0, 212, 255, static_cast<int>(80 * lockAlpha));
        dl->AddCircle({cx, cy}, lockSize * 0.88f, circleBorder, 48, 1.5f);

        DrawPadlock(dl, {cx, cy}, lockSize, ctx.state, ctx.transitionT);
    }

    {
        const char* title = "RFID ACCESS CONTROL";
        ImVec2 tsz = ImGui::CalcTextSize(title);
        float  ty  = cy - radarR - 48.0f;
        ImU32 tc = IM_COL32(230, 240, 255, static_cast<int>(200 * ctx.fadeAlpha));
        dl->AddText({cx - tsz.x * 0.5f, ty}, tc, title);

        const char* sub = "STM32F103C8T6 + MFRC522";
        ImVec2 ssz = ImGui::CalcTextSize(sub);
        ImU32 sc = IM_COL32(97, 115, 138, static_cast<int>(180 * ctx.fadeAlpha));
        dl->AddText({cx - ssz.x * 0.5f, ty + ImGui::GetTextLineHeight() + 4.0f},
                    sc, sub);
    }

    // ── Mensagem de status (abaixo do radar) ──────────────────────────────────
    {
        float msgY = cy + radarR + 20.0f;

        const char* mainMsg;
        ImU32       mainColor;

        switch (ctx.state)
        {
        case LockState::Denied:
        {
            mainMsg   = "ACESSO NEGADO";
            mainColor = IM_COL32(255, 68, 68, static_cast<int>(255 * ctx.fadeAlpha));
            // UID lido
            std::string uidLine = "UID desconhecido: " + ctx.lastUID;
            ImVec2 usz = ImGui::CalcTextSize(uidLine.c_str());
            ImU32  uc  = IM_COL32(255, 68, 68, static_cast<int>(150 * ctx.fadeAlpha));
            dl->AddText({cx - usz.x * 0.5f, msgY + 26.0f}, uc, uidLine.c_str());
            break;
        }
        case LockState::Unlocking:
            mainMsg   = "ACESSO LIBERADO";
            mainColor = IM_COL32(0, 255, 136,
                                 static_cast<int>(255 * ctx.fadeAlpha));
            break;
        default:
        {
            // Pisca suavemente "APROXIME A TAG"
            float pulse = 0.55f + 0.45f * sinf(t * 1.5f);
            mainMsg   = "APROXIME A TAG";
            mainColor = IM_COL32(0, 212, 255,
                                 static_cast<int>(220 * pulse * ctx.fadeAlpha));
            break;
        }
        }

        ImVec2 msz = ImGui::CalcTextSize(mainMsg);
        dl->AddText({cx - msz.x * 0.5f, msgY}, mainColor, mainMsg);
    }

    {
        bool isConn = snap.connStatus == ConnectionStatus::Connected;
        float ix    = winW - 20.0f;
        float iy    = winH - 14.0f;
        ImU32 lc    = isConn ? U32_GREEN : U32_RED;
        ImU32 ld    = isConn ? U32_GREEN_DIM : U32_RED_DIM;
        DrawLED(dl, {ix, iy}, 4.0f, lc, ld, isConn);

        const char* connTxt = isConn ? "STM32 conectado" : "desconectado";
        ImVec2 ctSz = ImGui::CalcTextSize(connTxt);
        ImU32  ctC  = IM_COL32(97, 115, 138,
                               static_cast<int>(160 * ctx.fadeAlpha));
        dl->AddText({ix - ctSz.x - 14.0f, iy - 7.0f}, ctC, connTxt);
    }

    return true; 
}
