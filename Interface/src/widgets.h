#pragma once

#include "imgui.h"
#include "theme.h"
#include <cmath>
#include <string>
#include <cstdio>

enum class BannerState { Idle, Granted, Denied };

inline void DrawLED(ImDrawList* dl, ImVec2 center, float r,
                    ImU32 color, ImU32 colorDim, bool pulse = false)
{
    float t = static_cast<float>(ImGui::GetTime());
    float glowR = r;
    float glowA = 1.0f;

    if (pulse)
    {
        float wave = 0.5f + 0.5f * sinf(t * 3.0f);
        glowR = r + 4.0f * wave;
        glowA = 0.4f + 0.4f * wave;
    }

    ImU32 halo = (color & 0x00FFFFFFu) | (static_cast<ImU32>(glowA * 60) << 24);
    dl->AddCircleFilled(center, glowR + 4.0f, halo, 20);
    dl->AddCircleFilled(center, r + 2.0f, colorDim, 20);
    dl->AddCircleFilled(center, r, color, 20);

    ImVec2 specPos = {center.x - r * 0.3f, center.y - r * 0.3f};
    dl->AddCircleFilled(specPos, r * 0.3f, IM_COL32(255,255,255,80), 10);
}

inline void StatusRow(const char* label, const char* value,
                      ImU32 ledColor, ImU32 ledDim, bool pulse = false)
{
    ImDrawList* dl     = ImGui::GetWindowDrawList();
    ImVec2      cursor = ImGui::GetCursorScreenPos();
    float       lineH  = ImGui::GetTextLineHeight();
    ImVec2      ledPos = {cursor.x + INDICATOR_R + 2.0f,
                          cursor.y + lineH * 0.5f};

    DrawLED(dl, ledPos, INDICATOR_R, ledColor, ledDim, pulse);

    ImGui::SetCursorScreenPos({cursor.x + INDICATOR_R * 2.0f + 10.0f, cursor.y});
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MID);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    float labelW = ImGui::CalcTextSize(label).x;
    ImGui::SameLine(0, 8);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                          fmaxf(0.0f, 130.0f - labelW - 10.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_HI);
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();
}

inline void StatCard(const char* id, const char* label,
                     const char* value, const char* sub,
                     ImU32 accentColor, float width, float height = 80.0f)
{
    ImGui::PushID(id);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(p, {p.x + width, p.y + height},
                      U32_BG_WIDGET, ROUNDING);
    dl->AddRect(p, {p.x + width, p.y + height},
                (accentColor & 0x00FFFFFFu) | 0x50000000u, ROUNDING, 0, 1.5f);
    dl->AddRectFilled(p, {p.x + 3.0f, p.y + height},
                      accentColor, ROUNDING, ImDrawFlags_RoundCornersLeft);

    float tx = p.x + 12.0f;
    float ty = p.y + 8.0f;

    dl->AddText({tx, ty}, U32_TEXT_DIM, label);
    ty += ImGui::GetTextLineHeight() + 4.0f;

    ImFont* font = ImGui::GetFont();
    float   fscale = 1.6f;
    ImVec2  vsz = font->CalcTextSizeA(font->LegacySize * fscale, 1e9f, 0, value);
    dl->AddText(font, font->LegacySize * fscale, {tx, ty}, accentColor, value);
    ty += vsz.y + 2.0f;

    dl->AddText({tx, ty}, U32_TEXT_DIM, sub);

    ImGui::Dummy({width, height});
    ImGui::PopID();
}

inline void AccessBanner(BannerState state, const std::string& uid,
                         double timerRemaining, float width)
{
    float height = 110.0f;
    ImVec2 p  = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float t = static_cast<float>(ImGui::GetTime());

    ImU32 bgColor, borderColor, textColor;
    const char* icon;
    const char* statusText;

    switch (state)
    {
    case BannerState::Granted:
        bgColor    = IM_COL32(0, 255, 136, 15);
        borderColor= U32_GREEN;
        textColor  = U32_GREEN;
        icon       = "✓";
        statusText = "ACESSO LIBERADO";
        break;
    case BannerState::Denied:
        bgColor    = IM_COL32(255, 68, 68, 15);
        borderColor= U32_RED;
        textColor  = U32_RED;
        icon       = "✗";
        statusText = "ACESSO NEGADO";
        break;
    default:
        bgColor    = U32_BG_WIDGET;
        borderColor= U32_BORDER;
        textColor  = U32_TEXT_DIM;
        icon       = "◉";
        statusText = "AGUARDANDO TAG...";
        break;
    }

    float borderW = 1.5f;
    if (state != BannerState::Idle)
    {
        float pulse = 0.5f + 0.5f * sinf(t * 4.0f);
        borderColor = (borderColor & 0x00FFFFFFu) |
                      (static_cast<ImU32>(180 + 75 * pulse) << 24);
        borderW = 2.0f;
    }

    dl->AddRectFilled(p, {p.x + width, p.y + height}, bgColor, ROUNDING);
    dl->AddRect(p, {p.x + width, p.y + height}, borderColor, ROUNDING, 0, borderW);

    ImFont* font = ImGui::GetFont();
    float   iconScale = 2.8f;
    ImVec2  iconSz = font->CalcTextSizeA(font->LegacySize * iconScale, 1e9f, 0, icon);
    ImVec2  iconPos = {p.x + 20.0f, p.y + (height - iconSz.y) * 0.5f};
    dl->AddText(font, font->LegacySize * iconScale, iconPos, textColor, icon);

    float   txtX  = iconPos.x + iconSz.x + 16.0f;
    ImVec2  statusSz = font->CalcTextSizeA(font->LegacySize * 1.3f, 1e9f, 0, statusText);
    dl->AddText(font, font->LegacySize * 1.3f, {txtX, p.y + 18.0f}, textColor, statusText);

    std::string uidLabel = "UID: " + uid;
    dl->AddText({txtX, p.y + 18.0f + statusSz.y + 6.0f}, U32_TEXT_MID, uidLabel.c_str());

    if (state != BannerState::Idle && timerRemaining > 0.0)
    {
        float ratio = static_cast<float>(timerRemaining / ACCESS_TIMEOUT);
        float barY  = p.y + height - 6.0f;
        float barX1 = p.x + ROUNDING;
        float barX2 = p.x + width - ROUNDING;
        float barFill = barX1 + (barX2 - barX1) * ratio;

        dl->AddRectFilled({barX1, barY}, {barX2, barY + 3.0f}, U32_BG_WIDGET, 2.0f);
        dl->AddRectFilled({barX1, barY}, {barFill, barY + 3.0f}, borderColor, 2.0f);
    }

    ImGui::Dummy({width, height});
}

inline void DrawRadarSweep(ImDrawList* dl, ImVec2 center, float radius,
                           bool active, bool tagDetected)
{
    float t     = static_cast<float>(ImGui::GetTime());
    float angle = fmodf(t * 1.2f, static_cast<float>(M_PI) * 2.0f);

    int rings = 4;
    for (int i = 1; i <= rings; i++)
    {
        float r = radius * i / rings;
        ImU32 ringCol = (i == rings) ? IM_COL32(0, 212, 255, 40) : IM_COL32(0, 212, 255, 15);
        dl->AddCircle(center, r, ringCol, 64, 1.0f);
    }

    float ch = radius * 0.06f;
    dl->AddLine({center.x - radius, center.y}, {center.x - ch, center.y}, IM_COL32(0,212,255,30), 1.0f);
    dl->AddLine({center.x + ch, center.y}, {center.x + radius, center.y}, IM_COL32(0,212,255,30), 1.0f);
    dl->AddLine({center.x, center.y - radius}, {center.x, center.y - ch}, IM_COL32(0,212,255,30), 1.0f);
    dl->AddLine({center.x, center.y + ch}, {center.x, center.y + radius}, IM_COL32(0,212,255,30), 1.0f);

    if (!active) {
        dl->AddLine({center.x - 8, center.y - 8}, {center.x + 8, center.y + 8}, IM_COL32(255, 68, 68, 120), 2.0f);
        dl->AddLine({center.x + 8, center.y - 8}, {center.x - 8, center.y + 8}, IM_COL32(255, 68, 68, 120), 2.0f);
        return;
    }

    int segments = 48;
    float sweepAngle = static_cast<float>(M_PI) * 2.0f / 3.0f;
    for (int i = 0; i < segments; i++)
    {
        float a0 = angle - sweepAngle * (i    ) / segments;
        float a1 = angle - sweepAngle * (i + 1) / segments;
        float alpha = static_cast<float>(segments - i) / segments;
        ImU32 col = IM_COL32(0, 212, 255, static_cast<int>(alpha * 55));

        ImVec2 v0 = center;
        ImVec2 v1 = {center.x + cosf(a0) * radius, center.y + sinf(a0) * radius};
        ImVec2 v2 = {center.x + cosf(a1) * radius, center.y + sinf(a1) * radius};
        dl->AddTriangleFilled(v0, v1, v2, col);
    }

    ImVec2 tip = {center.x + cosf(angle) * radius, center.y + sinf(angle) * radius};
    dl->AddLine(center, tip, IM_COL32(0, 212, 255, 180), 1.5f);

    if (tagDetected)
    {
        float pulse = 0.5f + 0.5f * sinf(t * 8.0f);
        float bx = center.x + radius * 0.45f;
        float by = center.y - radius * 0.35f;
        dl->AddCircleFilled({bx, by}, 4.0f + 3.0f * pulse, IM_COL32(0, 255, 136, static_cast<int>(180 * pulse)), 16);
        dl->AddCircle({bx, by}, 4.0f, U32_GREEN, 16, 1.0f);
    }
}

inline void SectionHeader(const char* title, float width = 0.0f)
{
    if (width <= 0.0f)
        width = ImGui::GetContentRegionAvail().x;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float lh = ImGui::GetTextLineHeight();

    dl->AddText({p.x, p.y}, U32_CYAN, title);
    float tw = ImGui::CalcTextSize(title).x;

    float lineY = p.y + lh * 0.5f;
    dl->AddLine({p.x + tw + 8.0f, lineY}, {p.x + width, lineY}, IM_COL32(0,212,255,40), 1.0f);

    ImGui::Dummy({width, lh + 4.0f});
}

inline void MiniPlot(const char* label, const float* data, int count, int head,
                     float minV, float maxV, ImVec2 size, ImU32 lineColor = U32_CYAN)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(p, {p.x + size.x, p.y + size.y}, U32_BG_WIDGET, ROUNDING_SM);
    dl->AddRect(p, {p.x + size.x, p.y + size.y}, U32_BORDER, ROUNDING_SM);

    float range = maxV - minV;
    if (range < 1e-6f) range = 1.0f;

    for (int g = 1; g < 3; g++)
    {
        float gy = p.y + size.y * g / 3.0f;
        dl->AddLine({p.x + 1, gy}, {p.x + size.x - 1, gy}, IM_COL32(255,255,255,8), 1.0f);
    }

    if (count > 1)
    {
        ImVec2 prev = {};
        bool   hasPrev = false;
        for (int i = 0; i < count; i++)
        {
            int idx = (head - count + i + 128) % 128;
            float v  = data[idx];
            float nx = p.x + size.x * i / (count - 1);
            float ny = p.y + size.y - size.y * (v - minV) / range;
            ny = fmaxf(p.y + 2, fminf(p.y + size.y - 2, ny));
            ImVec2 cur = {nx, ny};
            if (hasPrev)
                dl->AddLine(prev, cur, lineColor, 1.5f);
            prev    = cur;
            hasPrev = true;
        }
    }

    dl->AddText({p.x + 4, p.y + 3}, U32_TEXT_DIM, label);
    ImGui::Dummy(size);
}
