#pragma once

#include "imgui.h"
#include <cstdint>

static constexpr ImVec4 COL_BG_DEEP     = {0.047f, 0.067f, 0.090f, 1.0f}; 
static constexpr ImVec4 COL_BG_BASE     = {0.055f, 0.082f, 0.110f, 1.0f}; 
static constexpr ImVec4 COL_BG_PANEL    = {0.075f, 0.106f, 0.141f, 1.0f}; 
static constexpr ImVec4 COL_BG_WIDGET   = {0.094f, 0.133f, 0.176f, 1.0f}; 
static constexpr ImVec4 COL_BG_HOVER    = {0.118f, 0.165f, 0.220f, 1.0f}; 

static constexpr ImVec4 COL_CYAN        = {0.000f, 0.831f, 1.000f, 1.0f}; 
static constexpr ImVec4 COL_CYAN_DIM    = {0.000f, 0.831f, 1.000f, 0.3f}; 
static constexpr ImVec4 COL_CYAN_GLOW    = {0.000f, 0.831f, 1.000f, 0.12f};

static constexpr ImVec4 COL_GREEN       = {0.000f, 1.000f, 0.533f, 1.0f}; 
static constexpr ImVec4 COL_GREEN_DIM   = {0.000f, 1.000f, 0.533f, 0.25f};
static constexpr ImVec4 COL_RED         = {1.000f, 0.267f, 0.267f, 1.0f}; 
static constexpr ImVec4 COL_RED_DIM     = {1.000f, 0.267f, 0.267f, 0.25f};
static constexpr ImVec4 COL_AMBER       = {0.941f, 0.647f, 0.000f, 1.0f}; 
static constexpr ImVec4 COL_AMBER_DIM   = {0.941f, 0.647f, 0.000f, 0.25f};

static constexpr ImVec4 COL_TEXT_HI     = {0.900f, 0.940f, 1.000f, 1.0f}; 
static constexpr ImVec4 COL_TEXT_MID    = {0.600f, 0.680f, 0.780f, 1.0f}; 
static constexpr ImVec4 COL_TEXT_DIM    = {0.380f, 0.450f, 0.540f, 1.0f}; 
static constexpr ImVec4 COL_TEXT_MONO    = {0.400f, 0.900f, 0.600f, 1.0f}; 

static constexpr ImVec4 COL_BORDER      = {0.000f, 0.831f, 1.000f, 0.18f};
static constexpr ImVec4 COL_BORDER_HI    = {0.000f, 0.831f, 1.000f, 0.55f};
static constexpr ImVec4 COL_SEPARATOR    = {0.200f, 0.270f, 0.360f, 0.60f};

static constexpr ImU32 U32_CYAN         = IM_COL32(0,   212, 255, 255);
static constexpr ImU32 U32_CYAN_DIM     = IM_COL32(0,   212, 255,  60);
static constexpr ImU32 U32_CYAN_GLOW     = IM_COL32(0,   212, 255,  20);
static constexpr ImU32 U32_GREEN        = IM_COL32(0,   255, 136, 255);
static constexpr ImU32 U32_GREEN_DIM     = IM_COL32(0,   255, 136,  50);
static constexpr ImU32 U32_RED           = IM_COL32(255,  68,  68, 255);
static constexpr ImU32 U32_RED_DIM       = IM_COL32(255,  68,  68,  50);
static constexpr ImU32 U32_AMBER         = IM_COL32(240, 165,    0, 255);
static constexpr ImU32 U32_BG_PANEL      = IM_COL32( 19,  27,  36, 255);
static constexpr ImU32 U32_BG_WIDGET     = IM_COL32( 24,  34,  45, 255);
static constexpr ImU32 U32_TEXT_HI       = IM_COL32(230, 240, 255, 255);
static constexpr ImU32 U32_TEXT_MID      = IM_COL32(153, 173, 199, 255);
static constexpr ImU32 U32_TEXT_DIM      = IM_COL32( 97, 115, 138, 255);
static constexpr ImU32 U32_TEXT_MONO     = IM_COL32(102, 230, 153, 255);
static constexpr ImU32 U32_BORDER        = IM_COL32(  0, 212, 255,  46);
static constexpr ImU32 U32_SEPARATOR     = IM_COL32( 51,  69,  92, 153);

static constexpr float SIDEBAR_W       = 220.0f;
static constexpr float TOPBAR_H        = 48.0f;
static constexpr float STATUSBAR_H     = 26.0f;
static constexpr float ROUNDING        = 6.0f;
static constexpr float ROUNDING_SM     = 3.0f;
static constexpr float INDICATOR_R     = 6.0f; 
static constexpr float PADDING         = 12.0f;

inline void ApplyRFIDTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding    = ROUNDING;
    s.ChildRounding      = ROUNDING;
    s.FrameRounding      = ROUNDING_SM;
    s.PopupRounding      = ROUNDING;
    s.ScrollbarRounding = ROUNDING;
    s.GrabRounding       = ROUNDING_SM;
    s.TabRounding       = ROUNDING_SM;

    s.WindowPadding      = {PADDING, PADDING};
    s.FramePadding       = {8.0f, 4.0f};
    s.ItemSpacing        = {8.0f, 6.0f};
    s.ItemInnerSpacing  = {6.0f, 4.0f};
    s.ScrollbarSize     = 10.0f;
    s.GrabMinSize       = 8.0f;
    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 1.0f;
    s.PopupBorderSize   = 1.0f;
    s.TabBorderSize      = 0.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = COL_BG_BASE;
    c[ImGuiCol_ChildBg]              = COL_BG_PANEL;
    c[ImGuiCol_PopupBg]              = COL_BG_PANEL;
    c[ImGuiCol_Border]               = COL_BORDER;
    c[ImGuiCol_BorderShadow]         = {0,0,0,0};

    c[ImGuiCol_FrameBg]              = COL_BG_WIDGET;
    c[ImGuiCol_FrameBgHovered]       = COL_BG_HOVER;
    c[ImGuiCol_FrameBgActive]        = COL_CYAN_DIM;

    c[ImGuiCol_TitleBg]              = COL_BG_DEEP;
    c[ImGuiCol_TitleBgActive]        = COL_BG_PANEL;
    c[ImGuiCol_TitleBgCollapsed]     = COL_BG_DEEP;

    c[ImGuiCol_MenuBarBg]            = COL_BG_PANEL;

    c[ImGuiCol_ScrollbarBg]          = COL_BG_DEEP;
    c[ImGuiCol_ScrollbarGrab]        = COL_CYAN_DIM;
    c[ImGuiCol_ScrollbarGrabHovered] = COL_CYAN;
    c[ImGuiCol_ScrollbarGrabActive]  = COL_CYAN;

    c[ImGuiCol_CheckMark]            = COL_CYAN;
    c[ImGuiCol_SliderGrab]           = COL_CYAN;
    c[ImGuiCol_SliderGrabActive]     = COL_CYAN;

    c[ImGuiCol_Button]               = COL_BG_HOVER;
    c[ImGuiCol_ButtonHovered]        = COL_CYAN_DIM;
    c[ImGuiCol_ButtonActive]         = COL_CYAN;

    c[ImGuiCol_Header]               = COL_CYAN_DIM;
    c[ImGuiCol_HeaderHovered]        = {COL_CYAN.x, COL_CYAN.y, COL_CYAN.z, 0.5f};
    c[ImGuiCol_HeaderActive]         = COL_CYAN;

    c[ImGuiCol_Separator]            = COL_SEPARATOR;
    c[ImGuiCol_SeparatorHovered]     = COL_CYAN;
    c[ImGuiCol_SeparatorActive]      = COL_CYAN;

    c[ImGuiCol_ResizeGrip]           = COL_CYAN_DIM;
    c[ImGuiCol_ResizeGripHovered]    = COL_CYAN;
    c[ImGuiCol_ResizeGripActive]     = COL_CYAN;

    c[ImGuiCol_Tab]                  = COL_BG_WIDGET;
    c[ImGuiCol_TabHovered]           = COL_CYAN_DIM;
    c[ImGuiCol_TabSelected]          = COL_CYAN_DIM;
    c[ImGuiCol_TabSelectedOverline]  = COL_CYAN;
    c[ImGuiCol_TabDimmed]            = COL_BG_DEEP;
    c[ImGuiCol_TabDimmedSelected]    = COL_BG_WIDGET;

    c[ImGuiCol_PlotLines]            = COL_CYAN;
    c[ImGuiCol_PlotLinesHovered]     = COL_GREEN;
    c[ImGuiCol_PlotHistogram]        = COL_CYAN;
    c[ImGuiCol_PlotHistogramHovered] = COL_GREEN;

    c[ImGuiCol_TableHeaderBg]        = COL_BG_DEEP;
    c[ImGuiCol_TableBorderStrong]    = COL_BORDER_HI;
    c[ImGuiCol_TableBorderLight]     = COL_BORDER;
    c[ImGuiCol_TableRowBg]           = {0,0,0,0};
    c[ImGuiCol_TableRowBgAlt]        = {1,1,1,0.03f};

    c[ImGuiCol_Text]                 = COL_TEXT_HI;
    c[ImGuiCol_TextDisabled]         = COL_TEXT_DIM;
    c[ImGuiCol_TextSelectedBg]       = COL_CYAN_DIM;

    c[ImGuiCol_DragDropTarget]       = COL_AMBER;
    c[ImGuiCol_NavHighlight]         = COL_CYAN;
    c[ImGuiCol_NavWindowingHighlight]= COL_CYAN;
    c[ImGuiCol_NavWindowingDimBg]    = {0,0,0,0.5f};
    c[ImGuiCol_ModalWindowDimBg]     = {0,0,0,0.5f};
}
