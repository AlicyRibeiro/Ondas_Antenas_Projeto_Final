#pragma once

#include <atomic>
#include <chrono>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static constexpr int    MAX_LOG_LINES  = 512;
static constexpr int    MAX_HISTORY    = 256;
static constexpr double ACCESS_TIMEOUT = 3.0;

enum class ConnectionStatus { Disconnected, Connecting, Connected, Error };
enum class AccessResult     { None, Granted, Denied };

struct AccessRecord
{
    std::string timestamp;
    std::string uid;
    bool        granted;
    std::string note;
};

struct AppState
{
    std::mutex mtx;

    ConnectionStatus connStatus  = ConnectionStatus::Disconnected;
    std::string      serialPort  = "/dev/ttyUSB0";
    int              baudRate    = 115200;

    AccessResult  lastResult     = AccessResult::None;
    std::string   lastUID        = "—";
    double        resultTimer    = 0.0;
    int           totalGranted   = 0;
    int           totalDenied    = 0;
    int           totalReads     = 0;
    float         readRate[128]  = {};
    int           readRateHead   = 0;

    uint8_t      masterUID[4] = {0x12, 0x34, 0x56, 0x78};
    bool         uidChanged   = false;

    std::deque<AccessRecord> history;
    std::deque<std::pair<ImU32,std::string>> logLines;

    std::chrono::steady_clock::time_point startTime;
    bool running = false;
};

inline void LogRaw(AppState& st, ImU32 color, const std::string& msg)
{
    if (st.logLines.size() >= MAX_LOG_LINES)
        st.logLines.pop_front();
    st.logLines.push_back({color, msg});
}

bool SerialOpen(const std::string& port, int baud);
void SerialClose();
bool SerialConnected();

inline std::string NowString()
{
    auto now  = std::chrono::system_clock::now();
    auto t    = std::chrono::system_clock::to_time_t(now);
    char buf[24];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}
