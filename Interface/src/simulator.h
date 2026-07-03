#pragma once

#include "backend.h"
#include <cstdlib>
#include <cmath>

static const uint8_t FAKE_UIDS[4][4] = {
    {0xAB, 0xCD, 0xEF, 0x01},
    {0xDE, 0xAD, 0xBE, 0xEF},
    {0x00, 0xFF, 0x00, 0xFF},
    {0xCA, 0xFE, 0xBA, 0xBE},
};

inline std::string FormatUID(const uint8_t uid[4])
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X",
            uid[0], uid[1], uid[2], uid[3]);
    return buf;
}

inline bool UIDEqual(const uint8_t a[4], const uint8_t b[4])
{
    return memcmp(a, b, 4) == 0;
}

class RFIDSimulator
{
public:
    explicit RFIDSimulator(AppState& state) : m_state(state) {}

    void Start()
    {
        m_state.startTime = std::chrono::steady_clock::now();
        m_state.running   = true;
        m_thread = std::thread(&RFIDSimulator::Run, this);
    }

    void Stop()
    {
        m_stop.store(true);
        if (m_thread.joinable()) m_thread.join();
    }

    ~RFIDSimulator() { Stop(); }

private:
    AppState&        m_state;
    std::thread      m_thread;
    std::atomic_bool m_stop{false};

    void SimulateConnect()
    {
        {
            std::lock_guard<std::mutex> lk(m_state.mtx);
            m_state.connStatus = ConnectionStatus::Connecting;
            LogRaw(m_state, U32_CYAN,  "[ SYS ] Iniciando conexão com STM32...");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        {
            std::lock_guard<std::mutex> lk(m_state.mtx);
            LogRaw(m_state, U32_TEXT_MONO, "[ SYS ] Abrindo " + m_state.serialPort +
                   " @ " + std::to_string(m_state.baudRate) + " baud");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        {
            std::lock_guard<std::mutex> lk(m_state.mtx);
            LogRaw(m_state, U32_TEXT_MONO, "[ RC522 ] SPI1 inicializado — 9 MHz CPOL=0 CPHA=0");
            LogRaw(m_state, U32_TEXT_MONO, "[ RC522 ] Firmware v0.1 — STM32F103C8T6");
            LogRaw(m_state, U32_TEXT_MONO, "[ RC522 ] SYSCLK = 72 MHz via PLL x9");
            LogRaw(m_state, U32_TEXT_MONO, "[ RC522 ] Antenas TX1/TX2 ativas");
            LogRaw(m_state, U32_GREEN,     "[ SYS ] Conexão estabelecida. Aguardando TAG...");
            m_state.connStatus = ConnectionStatus::Connected;
        }
    }

    void Run()
    {
        SimulateConnect();

        auto nextRead = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(2000);
        auto lastRateUpdate = std::chrono::steady_clock::now();
        int  readsThisSec = 0;

        while (!m_stop.load())
        {
            auto now = std::chrono::steady_clock::now();

            double secElapsed = std::chrono::duration<double>(
                now - lastRateUpdate).count();
            if (secElapsed >= 1.0)
            {
                std::lock_guard<std::mutex> lk(m_state.mtx);
                m_state.readRate[m_state.readRateHead % 128] =
                    static_cast<float>(readsThisSec) / static_cast<float>(secElapsed);
                m_state.readRateHead++;
                readsThisSec   = 0;
                lastRateUpdate = now;

                if (m_state.resultTimer > 0.0)
                {
                    m_state.resultTimer -= secElapsed;
                    if (m_state.resultTimer <= 0.0)
                    {
                        m_state.resultTimer = 0.0;
                        m_state.lastResult  = AccessResult::None;
                        m_state.lastUID     = "—";
                    }
                }
            }

            if (now >= nextRead)
            {
                SimulateRead();
                readsThisSec++;

                int delay = 2000 + (rand() % 4000);
                nextRead = now + std::chrono::milliseconds(delay);
            }

            static auto lastHB = std::chrono::steady_clock::now();
            if (std::chrono::duration<double>(now - lastHB).count() > 5.0)
            {
                std::lock_guard<std::mutex> lk(m_state.mtx);
                LogRaw(m_state, U32_TEXT_DIM, "[ HB  ] RFID:HEARTBEAT — sistema operacional");
                lastHB = now;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::lock_guard<std::mutex> lk(m_state.mtx);
        m_state.connStatus = ConnectionStatus::Disconnected;
        LogRaw(m_state, U32_AMBER, "[ SYS ] Conexão encerrada pelo usuário.");
    }

    void SimulateRead()
    {
        bool useMaster = (rand() % 10) < 6;

        uint8_t uid[4];
        if (useMaster)
        {
            std::lock_guard<std::mutex> lk(m_state.mtx);
            memcpy(uid, m_state.masterUID, 4);
        }
        else
        {
            memcpy(uid, FAKE_UIDS[rand() % 4], 4);
        }

        std::string uidStr = FormatUID(uid);

        std::lock_guard<std::mutex> lk(m_state.mtx);
        bool granted = UIDEqual(uid, m_state.masterUID);

        m_state.lastUID    = uidStr;
        m_state.lastResult = granted ? AccessResult::Granted : AccessResult::Denied;
        m_state.resultTimer = ACCESS_TIMEOUT;
        m_state.totalReads++;

        AccessRecord rec;
        rec.timestamp = NowString();
        rec.uid       = uidStr;
        rec.granted   = granted;
        rec.note      = granted ? "UID mestre" : "UID desconhecido";

        if (granted)
        {
            m_state.totalGranted++;
            LogRaw(m_state, U32_GREEN,
                   "[ RFID ] UID=" + uidStr + "  →  ACESSO LIBERADO ✓");
        }
        else
        {
            m_state.totalDenied++;
            LogRaw(m_state, U32_RED,
                   "[ RFID ] UID=" + uidStr + "  →  ACESSO NEGADO ✗");
        }

        if (m_state.history.size() >= MAX_HISTORY)
            m_state.history.pop_back();
        m_state.history.push_front(rec);
    }
};
