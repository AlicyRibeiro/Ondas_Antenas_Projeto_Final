#pragma once
/**
 * @file   views.h
 * @brief  Implementação das três telas principais da GUI RFID com suporte a múltiplos usuários.
 *
 * Cada função Draw*View() renderiza uma tela completa dentro da área de
 * conteúdo principal. Elas recebem o AppState (com lock externo) e o
 * snapshot de valores para evitar lock longo durante o render.
 */

#include "imgui.h"
#include "backend.h"
#include "theme.h"
#include "widgets.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map> // Adicionado para suportar o dicionário de usuários

// ─────────────────────────────────────────────────────────────────────────────
// Banco de Dados Local de Usuários Nomeados
// ─────────────────────────────────────────────────────────────────────────────

static const std::unordered_map<std::string, std::string> g_nomesUsuarios = {
    {"95:93:E0:85", "Administrador (Mestre)"},
    {"E0:F0:8F:E1", "≽^ kay ^≼"},
    {"20:49:84:F6", "⊹ ࣪ ˖꒰ঌ sabrina ໒꒱.⋆˚࿔"}
};

// ─────────────────────────────────────────────────────────────────────────────
// Snapshot dos dados voláteis — copiado do AppState com lock breve
// ─────────────────────────────────────────────────────────────────────────────
struct FrameSnapshot
{
    ConnectionStatus connStatus;
    AccessResult      lastResult;
    std::string       lastUID;
    double            resultTimer;
    int               totalGranted, totalDenied, totalReads;
    float             readRate[128];
    int               readRateHead;
    uint8_t           masterUID[4];
    double            uptimeSeconds;
    std::vector<AccessRecord> history;
    std::vector<std::pair<ImU32,std::string>> logLines;
};

inline FrameSnapshot SnapshotState(AppState& st)
{
    std::lock_guard<std::mutex> lk(st.mtx);
    FrameSnapshot s;
    s.connStatus   = st.connStatus;
    s.lastResult   = st.lastResult;
    s.lastUID      = st.lastUID;
    s.resultTimer  = st.resultTimer;
    s.totalGranted = st.totalGranted;
    s.totalDenied  = st.totalDenied;
    s.totalReads   = st.totalReads;
    memcpy(s.readRate, st.readRate, sizeof(st.readRate));
    s.readRateHead = st.readRateHead;
    memcpy(s.masterUID, st.masterUID, 4);
    s.uptimeSeconds= std::chrono::duration<double>(
        std::chrono::steady_clock::now() - st.startTime).count();
    s.history  = {st.history.begin(), st.history.end()};
    s.logLines = {st.logLines.begin(), st.logLines.end()};
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// VIEW 1 — MONITORAMENTO
// ─────────────────────────────────────────────────────────────────────────────
inline void DrawMonitorView(const FrameSnapshot& snap)
{
    float totalW = ImGui::GetContentRegionAvail().x;

    // ── Linha superior: radar + painel de status ──────────────────────────────
    float radarSize = 200.0f;
    float rightW    = totalW - radarSize - 16.0f;

    // Radar
    ImGui::BeginChild("##radar_panel", {radarSize, radarSize}, false,
                    ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 p   = ImGui::GetCursorScreenPos();
        ImVec2 c   = {p.x + radarSize * 0.5f, p.y + radarSize * 0.5f};
        float  rad = radarSize * 0.45f;

        bool connected  = snap.connStatus == ConnectionStatus::Connected;
        bool tagVisible = snap.lastResult != AccessResult::None;

        DrawRadarSweep(ImGui::GetWindowDrawList(), c, rad, connected, tagVisible);

        // Rótulo de status no topo do radar
        const char* statusLabel = connected ? "CAMPO RF ATIVO" : "SEM SINAL";
        ImU32 labelCol = connected ? U32_CYAN : U32_RED;
        ImVec2 lsz = ImGui::CalcTextSize(statusLabel);
        ImGui::GetWindowDrawList()->AddText(
            {c.x - lsz.x * 0.5f, p.y + 4.0f}, labelCol, statusLabel);

        ImGui::Dummy({radarSize, radarSize});
    }
    ImGui::EndChild();

    ImGui::SameLine(0, 16.0f);

    
    ImGui::BeginChild("##status_panel", {rightW, radarSize}, false,
                    ImGuiWindowFlags_NoScrollbar);
    {
        SectionHeader("STATUS DO SISTEMA", rightW);

    
        bool isConn = snap.connStatus == ConnectionStatus::Connected;
        const char* connStr =
            snap.connStatus == ConnectionStatus::Connected  ? "Conectado"  :
            snap.connStatus == ConnectionStatus::Connecting ? "Conectando" :
            snap.connStatus == ConnectionStatus::Error      ? "Erro"       : "Desconectado";
        ImU32 connCol = isConn ? U32_GREEN :
                        (snap.connStatus == ConnectionStatus::Connecting ? U32_AMBER : U32_RED);
        ImU32 connDim = isConn ? U32_GREEN_DIM :
                        (snap.connStatus == ConnectionStatus::Connecting ? U32_AMBER : U32_RED_DIM);
        StatusRow("Conexão serial", connStr, connCol, connDim, isConn);

        ImGui::Spacing();

        // RC522
        StatusRow("Módulo RC522", isConn ? "Operacional" : "Offline",
                isConn ? U32_GREEN : U32_RED_DIM,
                isConn ? U32_GREEN_DIM : U32_RED_DIM, false);
        ImGui::Spacing();

        // Antenas
        StatusRow("Antenas TX1/TX2", isConn ? "Ativas (3.3V)" : "—",
                isConn ? U32_CYAN : U32_TEXT_DIM,
                isConn ? U32_CYAN_DIM : U32_BG_WIDGET, false);
        ImGui::Spacing();

        // SPI
        StatusRow("SPI1 Clock", isConn ? "9 MHz" : "—",
                isConn ? U32_CYAN : U32_TEXT_DIM,
                isConn ? U32_CYAN_DIM : U32_BG_WIDGET, false);
        ImGui::Spacing();

        // Uptime
        int   mins = static_cast<int>(snap.uptimeSeconds) / 60;
        int   secs = static_cast<int>(snap.uptimeSeconds) % 60;
        char  upbuf[24];
        snprintf(upbuf, sizeof(upbuf), "%02d:%02d", mins, secs);
        StatusRow("Uptime", upbuf, U32_CYAN_DIM, U32_BG_WIDGET, false);
    }
    ImGui::EndChild();

    ImGui::Spacing();

    // ── Stat cards ────────────────────────────────────────────────────────────
    float cardW = (totalW - 32.0f) / 3.0f;
    char vbuf[32];

    snprintf(vbuf, sizeof(vbuf), "%d", snap.totalReads);
    StatCard("total",   "Total de Leituras", vbuf, "tags detectadas",
            U32_CYAN, cardW);

    ImGui::SameLine(0, 8.0f);
    snprintf(vbuf, sizeof(vbuf), "%d", snap.totalGranted);
    StatCard("granted", "Acessos Liberados", vbuf, "UIDs válidos",
            U32_GREEN, cardW);

    ImGui::SameLine(0, 8.0f);
    snprintf(vbuf, sizeof(vbuf), "%d", snap.totalDenied);
    StatCard("denied",  "Acessos Negados",  vbuf, "UIDs inválidos",
            U32_RED, cardW);

    ImGui::Spacing();

    BannerState bs = snap.lastResult == AccessResult::Granted ? BannerState::Granted :
                        snap.lastResult == AccessResult::Denied  ? BannerState::Denied :
                        BannerState::Idle;
        
    
    std::string bannerUID = snap.lastUID;
    auto it = g_nomesUsuarios.find(bannerUID);
    if (it != g_nomesUsuarios.end()) {
        bannerUID += " (" + it->second + ")";
    }
    AccessBanner(bs, bannerUID, snap.resultTimer, totalW);

    ImGui::Spacing();

    SectionHeader("TAXA DE LEITURA (tags/s)", totalW);
    MiniPlot("tags/s", snap.readRate, 64, snap.readRateHead,
            0.0f, 2.0f, {totalW, 60.0f}, U32_CYAN);
}

// ─────────────────────────────────────────────────────────────────────────────
// VIEW 2 — CONFIGURAÇÕES
// ─────────────────────────────────────────────────────────────────────────────
inline void DrawConfigView(AppState& st)
{
    float totalW = ImGui::GetContentRegionAvail().x;

    // ── Seção: Conexão Serial ─────────────────────────────────────────────────
    SectionHeader("CONEXÃO SERIAL", totalW);

    static char portBuf[64] = "/dev/ttyUSB0";
    static const int baudOptions[] = {9600,19200,38400,57600,115200,230400,921600};
    static int  baudIdx = 4; // 115200

    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MID);
    ImGui::TextUnformatted("Porta Serial");
    ImGui::PopStyleColor();
    ImGui::SameLine(160.0f);
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputText("##port", portBuf, sizeof(portBuf));

    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MID);
    ImGui::TextUnformatted("Baud Rate");
    ImGui::PopStyleColor();
    ImGui::SameLine(160.0f);
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##baud", std::to_string(baudOptions[baudIdx]).c_str()))
    {
        for (int i = 0; i < 7; i++)
        {
            bool sel = (i == baudIdx);
            if (ImGui::Selectable(std::to_string(baudOptions[i]).c_str(), sel))
                baudIdx = i;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();

    bool connected = false;
    {
        std::lock_guard<std::mutex> lk(st.mtx);
        connected = (st.connStatus == ConnectionStatus::Connected ||
                    st.connStatus == ConnectionStatus::Connecting);
    }

    ImGui::PushStyleColor(ImGuiCol_Button,
        connected ? ImVec4{0.6f,0.1f,0.1f,1.0f} : COL_CYAN_DIM);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        connected ? ImVec4{0.9f,0.2f,0.2f,1.0f} : COL_CYAN);
    ImGui::PushStyleColor(ImGuiCol_Text,
        connected ? COL_TEXT_HI : ImVec4{0,0,0,1});
    if (ImGui::Button(connected ? "  DESCONECTAR  " : "  CONECTAR  ", {140.0f, 32.0f}))
    {
        std::lock_guard<std::mutex> lk(st.mtx);
        st.serialPort = portBuf;
        st.baudRate   = baudOptions[baudIdx];
        LogRaw(st, U32_AMBER,
                std::string("[ CFG ] Solicitação de ") +
                (connected ? "desconexão" : "conexão") +
                " — porta: " + portBuf);
        }
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Seção: UID Mestre ─────────────────────────────────────────────────────
    SectionHeader("UID MESTRE (controle de acesso)", totalW);

    static int uidBytes[4] = {0x12, 0x34, 0x56, 0x78};

    {
        static bool initialized = false;
        if (!initialized)
        {
            std::lock_guard<std::mutex> lk(st.mtx);
            for (int i = 0; i < 4; i++)
                uidBytes[i] = st.masterUID[i];
            initialized = true;
        }
    }

    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MID);
    ImGui::TextUnformatted("Bytes do UID");
    ImGui::PopStyleColor();
    ImGui::SameLine(160.0f);

    bool changed = false;
    for (int i = 0; i < 4; i++)
    {
        ImGui::PushID(i);
        char label[8];
        snprintf(label, sizeof(label), "[%d]", i);
        ImGui::SetNextItemWidth(60.0f);
        if (ImGui::InputInt(label, &uidBytes[i], 0, 0,
                            ImGuiInputTextFlags_CharsHexadecimal))
        {
            uidBytes[i] = std::max(0, std::min(255, uidBytes[i]));
            changed = true;
        }
        ImGui::PopID();
        if (i < 3) ImGui::SameLine(0, 6.0f);
    }

    ImGui::Spacing();
    char preview[20];
    snprintf(preview, sizeof(preview), "%02X:%02X:%02X:%02X",
            uidBytes[0], uidBytes[1], uidBytes[2], uidBytes[3]);
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MID);
    ImGui::TextUnformatted("Preview");
    ImGui::PopStyleColor();
    ImGui::SameLine(160.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, COL_CYAN);
    ImGui::TextUnformatted(preview);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button,        COL_BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  COL_CYAN_DIM);
    if (ImGui::Button("  APLICAR UID  ", {140.0f, 32.0f}) || changed)
    {
        std::lock_guard<std::mutex> lk(st.mtx);
        for (int i = 0; i < 4; i++)
            st.masterUID[i] = static_cast<uint8_t>(uidBytes[i]);
        st.uidChanged = true;
        LogRaw(st, U32_AMBER,
            std::string("[ CFG ] UID mestre atualizado: ") + preview);
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Seção: Parâmetros RC522 ───────────────────────────────────────────────
    SectionHeader("PARÂMETROS RC522 (informativo)", totalW);

    struct Param { const char* name; const char* value; const char* desc; };
    static const Param params[] = {
        {"SYSCLK",     "72 MHz",   "Via PLL x9 (HSE 8 MHz)"},
        {"SPI1 Clock", "9 MHz",    "APB2 / 8  — CPOL=0 CPHA=0 MSB-first"},
        {"Modulação",  "ASK 100%", "ISO 14443-A"},
        {"CRC Preset", "0x6363",   "ISO 14443-3 Type A"},
        {"T_Reload",   "30 ms",    "Timeout do temporizador interno"},
        {"NSS",        "PA4",      "GPIO manual (software NSS)"},
        {"RST",        "PB0",      "Reset hardware ativo-LOW"},
    };

    if (ImGui::BeginTable("##params", 3,
                        ImGuiTableFlags_BordersInner |
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Parâmetro", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Valor",     ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Descrição", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (auto& p : params)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MID);
            ImGui::TextUnformatted(p.name);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_CYAN);
            ImGui::TextUnformatted(p.value);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(2);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
            ImGui::TextUnformatted(p.desc);
            ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// VIEW 3 — HISTÓRICO
// ─────────────────────────────────────────────────────────────────────────────
inline void DrawHistoryView(const FrameSnapshot& snap)
{
    float totalW = ImGui::GetContentRegionAvail().x;

    SectionHeader("HISTÓRICO DE ACESSOS", totalW);

    // Filtros
    static bool showGranted = true;
    static bool showDenied  = true;
    static char filterUID[16] = "";

    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MID);
    ImGui::TextUnformatted("Filtrar:");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_CheckMark, COL_GREEN);
    ImGui::Checkbox("Liberados", &showGranted);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_CheckMark, COL_RED);
    ImGui::Checkbox("Negados", &showDenied);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 16.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MID);
    ImGui::TextUnformatted("UID:");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("##fuid", filterUID, sizeof(filterUID));

    ImGui::Spacing();

    
    float pctGranted = snap.totalReads > 0
        ? 100.0f * snap.totalGranted / snap.totalReads : 0.0f;

    char pctBuf[32];
    snprintf(pctBuf, sizeof(pctBuf), "Taxa de aprovação: %.1f%%", pctGranted);
    ImGui::PushStyleColor(ImGuiCol_Text, COL_GREEN);
    ImGui::TextUnformatted(pctBuf);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 20.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, COL_GREEN);
    ImGui::ProgressBar(pctGranted / 100.0f, {200.0f, 14.0f}, "");
    ImGui::PopStyleColor();

    ImGui::Spacing();


    if (ImGui::BeginTable("##history", 4,
                            ImGuiTableFlags_BordersOuter |
                            ImGuiTableFlags_BordersInner |
                            ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_SizingFixedFit,
                            {0.0f, ImGui::GetContentRegionAvail().y - 8.0f}))
        {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Horário",  ImGuiTableColumnFlags_WidthFixed,   165.0f);
        ImGui::TableSetupColumn("UID",      ImGuiTableColumnFlags_WidthFixed,   115.0f);
        ImGui::TableSetupColumn("Resultado",ImGuiTableColumnFlags_WidthFixed,    110.0f);
        ImGui::TableSetupColumn("Detalhe",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        std::string fuid(filterUID);
        std::transform(fuid.begin(), fuid.end(), fuid.begin(), ::toupper);

        for (const auto& rec : snap.history)
        {
            if (!showGranted && rec.granted) continue;
            if (!showDenied  && !rec.granted) continue;
            if (!fuid.empty())
            {
                std::string ruid = rec.uid;
                std::transform(ruid.begin(), ruid.end(), ruid.begin(), ::toupper);
                if (ruid.find(fuid) == std::string::npos) continue;
            }

            ImGui::TableNextRow();

            // Colona 0: Horário
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
            ImGui::TextUnformatted(rec.timestamp.c_str());
            ImGui::PopStyleColor();

            // Coluna 1: UID
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_HI);
            ImGui::TextUnformatted(rec.uid.c_str());
            ImGui::PopStyleColor();

            // Coluna 2: Resultado
            ImGui::TableSetColumnIndex(2);
            if (rec.granted)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, COL_GREEN);
                ImGui::TextUnformatted("✓ LIBERADO");
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, COL_RED);
                ImGui::TextUnformatted("✗ NEGADO");
            }
            ImGui::PopStyleColor();

            // Coluna 3: Detalhe (Modificado para ler do dicionário dinâmico)
            ImGui::TableSetColumnIndex(3);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
            
            // Busca o UID atual no mapa de nomes
            auto usuarioIt = g_nomesUsuarios.find(rec.uid);
            if (usuarioIt != g_nomesUsuarios.end())
            {
                // Se encontrou o UID no mapa, exibe o nome correspondente
                ImGui::TextUnformatted(usuarioIt->second.c_str());
            }
            else
            {
                // Se não encontrou, mantém a string padrão enviada pelo backend
                ImGui::TextUnformatted(rec.note.c_str());
            }
            ImGui::PopStyleColor();
        }

        ImGui::EndTable();
    }
}
