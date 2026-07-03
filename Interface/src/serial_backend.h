#pragma once
/**
 * @file  serial_backend.h
 * @brief Backend real: lê a porta serial (TTL-USB) e alimenta o AppState.
 *
 * Substitui o RFIDSimulator quando o hardware físico está conectado.
 * A GUI não precisa mudar nada — só troca qual backend é instanciado.
 *
 * Protocolo esperado do STM32 (uma linha ASCII por evento):
 *   "RFID:BOOT\n"                    → firmware inicializou
 *   "RFID:UID=12:34:56:78:GRANT\n"  → acesso liberado
 *   "RFID:UID=AB:CD:EF:01:DENY\n"   → acesso negado
 *   "RFID:HEARTBEAT\n"              → keep-alive (a cada ~5s)
 *   "RFID:ERR=<msg>\n"              → erro do firmware
 *
 * Dependências: POSIX termios (padrão em qualquer Linux)
 */

#include "backend.h"

#include <atomic>
#include <string>
#include <thread>

// POSIX serial
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

class SerialBackend
{
public:
    explicit SerialBackend(AppState& state) : m_state(state) {}

    void Start()
    {
        m_state.startTime = std::chrono::steady_clock::now();
        m_state.running   = true;
        m_thread = std::thread(&SerialBackend::Run, this);
    }

    void Stop()
    {
        m_stop.store(true);
        if (m_thread.joinable()) m_thread.join();
    }

    ~SerialBackend() { Stop(); }

private:
    AppState&        m_state;
    std::thread      m_thread;
    std::atomic_bool m_stop{false};
    int              m_fd = -1;

    // ── Abre e configura a porta serial ──────────────────────────────────────
    bool OpenPort(const std::string& port, int baud)
    {
        {
            std::lock_guard<std::mutex> lk(m_state.mtx);
            m_state.connStatus = ConnectionStatus::Connecting;
            LogRaw(m_state, U32_CYAN,
                   "[ SYS ] Abrindo " + port + " @ " +
                   std::to_string(baud) + " baud...");
        }

        // Abre sem bloquear (O_NOCTTY = não se torna terminal controlador)
        m_fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (m_fd < 0)
        {
            std::lock_guard<std::mutex> lk(m_state.mtx);
            LogRaw(m_state, U32_RED,
                   "[ ERR ] Falha ao abrir porta: " + std::string(strerror(errno)));
            m_state.connStatus = ConnectionStatus::Error;
            return false;
        }

        // Configura termios (8N1, sem controle de fluxo, raw mode)
        struct termios tty;
        memset(&tty, 0, sizeof(tty));
        if (tcgetattr(m_fd, &tty) != 0)
        {
            std::lock_guard<std::mutex> lk(m_state.mtx);
            LogRaw(m_state, U32_RED, "[ ERR ] tcgetattr falhou.");
            m_state.connStatus = ConnectionStatus::Error;
            close(m_fd); m_fd = -1;
            return false;
        }

        // Baud rate
        speed_t speed = B115200;
        switch (baud)
        {
            case 9600:   speed = B9600;   break;
            case 19200:  speed = B19200;  break;
            case 38400:  speed = B38400;  break;
            case 57600:  speed = B57600;  break;
            case 115200: speed = B115200; break;
            case 230400: speed = B230400; break;
            default:     speed = B115200; break;
        }
        cfsetispeed(&tty, speed);
        cfsetospeed(&tty, speed);

        // 8 bits de dados, sem paridade, 1 stop bit
        tty.c_cflag &= ~PARENB;          // sem paridade
        tty.c_cflag &= ~CSTOPB;          // 1 stop bit
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |=  CS8;             // 8 bits
        tty.c_cflag &= ~CRTSCTS;         // sem controle de fluxo RTS/CTS
        tty.c_cflag |=  CREAD | CLOCAL;  // habilita receptor, ignora CD

        // Modo raw (sem processamento de linha, sem eco)
        tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        tty.c_iflag &= ~(IXON | IXOFF | IXANY);  // sem controle SW
        tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
        tty.c_oflag &= ~OPOST;           // sem processamento de saída

        // Timeout: leitura retorna imediatamente se não tiver dados
        tty.c_cc[VMIN]  = 0;
        tty.c_cc[VTIME] = 0;

        if (tcsetattr(m_fd, TCSANOW, &tty) != 0)
        {
            std::lock_guard<std::mutex> lk(m_state.mtx);
            LogRaw(m_state, U32_RED, "[ ERR ] tcsetattr falhou.");
            m_state.connStatus = ConnectionStatus::Error;
            close(m_fd); m_fd = -1;
            return false;
        }

        // Limpa buffers pendentes
        tcflush(m_fd, TCIOFLUSH);

        // Volta para modo bloqueante agora que está configurado
        int flags = fcntl(m_fd, F_GETFL, 0);
        fcntl(m_fd, F_SETFL, flags & ~O_NONBLOCK);

        {
            std::lock_guard<std::mutex> lk(m_state.mtx);
            LogRaw(m_state, U32_GREEN,
                   "[ SYS ] Porta aberta com sucesso. Aguardando STM32...");
            m_state.connStatus = ConnectionStatus::Connected;
            m_state.serialPort = port;
            m_state.baudRate   = baud;
        }
        return true;
    }

    // ── Parseia uma linha recebida do STM32 ───────────────────────────────────
    void ParseLine(const std::string& line)
    {
        if (line.empty()) return;

        // RFID:BOOT
        if (line == "RFID:BOOT")
        {
            std::lock_guard<std::mutex> lk(m_state.mtx);
            LogRaw(m_state, U32_CYAN, "[ STM32 ] Firmware inicializado ✓");
            return;
        }

        // RFID:HEARTBEAT
        if (line == "RFID:HEARTBEAT")
        {
            std::lock_guard<std::mutex> lk(m_state.mtx);
            LogRaw(m_state, U32_TEXT_DIM, "[ HB  ] RFID:HEARTBEAT — sistema operacional");
            return;
        }

        // RFID:ERR=<msg>
        if (line.substr(0, 9) == "RFID:ERR=")
        {
            std::string msg = line.substr(9);
            std::lock_guard<std::mutex> lk(m_state.mtx);
            LogRaw(m_state, U32_RED, "[ ERR ] " + msg);
            return;
        }

        // RFID:UID=XX:XX:XX:XX:GRANT  ou  :DENY
        if (line.substr(0, 9) == "RFID:UID=")
        {
            // Formato: RFID:UID=XX:XX:XX:XX:GRANT
            //                    ^9       ^20 ^21
            if (line.size() < 24) return; // linha malformada

            std::string uidStr = line.substr(9, 11); // "XX:XX:XX:XX"
            std::string result = line.substr(21);     // "GRANT" ou "DENY"

            bool granted = (result == "GRANT");

            AccessRecord rec;
            rec.timestamp = NowString();
            rec.uid       = uidStr;
            rec.granted   = granted;
            rec.note      = granted ? "UID mestre" : "UID desconhecido";

            std::lock_guard<std::mutex> lk(m_state.mtx);

            m_state.lastUID    = uidStr;
            m_state.lastResult = granted ? AccessResult::Granted
                                         : AccessResult::Denied;
            m_state.resultTimer = ACCESS_TIMEOUT;
            m_state.totalReads++;

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
    }

    // ── Loop principal da thread ──────────────────────────────────────────────
    void Run()
    {
        std::string port;
        int         baud;
        {
            std::lock_guard<std::mutex> lk(m_state.mtx);
            port = m_state.serialPort;
            baud = m_state.baudRate;
        }

        // Tenta abrir a porta; fica tentando a cada 3s até conseguir ou parar
        while (!m_stop.load() && !OpenPort(port, baud))
        {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            // Relê a porta caso o usuário tenha mudado na GUI
            std::lock_guard<std::mutex> lk(m_state.mtx);
            port = m_state.serialPort;
            baud = m_state.baudRate;
        }

        if (m_stop.load()) return;

        // Leitura linha a linha com timeout para atualizar resultTimer
        std::string lineBuf;
        auto lastRateUpdate = std::chrono::steady_clock::now();

        while (!m_stop.load())
        {
            char c;
            ssize_t n = read(m_fd, &c, 1);

            if (n > 0)
            {
                if (c == '\n')
                {
                    // Remove '\r' se presente (Windows line endings)
                    if (!lineBuf.empty() && lineBuf.back() == '\r')
                        lineBuf.pop_back();
                    ParseLine(lineBuf);
                    lineBuf.clear();
                }
                else if (c != '\r')
                {
                    // Limite de segurança: descarta linha muito longa
                    if (lineBuf.size() < 128)
                        lineBuf += c;
                }
            }
            else if (n < 0)
            {
                // Erro de leitura — porta desconectada (ex: USB removido)
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    std::lock_guard<std::mutex> lk(m_state.mtx);
                    LogRaw(m_state, U32_RED,
                           "[ ERR ] Porta serial perdida: " +
                           std::string(strerror(errno)));
                    m_state.connStatus = ConnectionStatus::Error;
                    break;
                }
            }
            else
            {
                // n == 0: sem dados, aguarda 10ms antes de tentar de novo
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // Atualiza o resultTimer (decai 1× por segundo)
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(
                now - lastRateUpdate).count();
            if (elapsed >= 1.0)
            {
                lastRateUpdate = now;
                std::lock_guard<std::mutex> lk(m_state.mtx);
                if (m_state.resultTimer > 0.0)
                {
                    m_state.resultTimer -= elapsed;
                    if (m_state.resultTimer <= 0.0)
                    {
                        m_state.resultTimer = 0.0;
                        m_state.lastResult  = AccessResult::None;
                        m_state.lastUID     = "—";
                    }
                }
            }
        }

        if (m_fd >= 0)
        {
            close(m_fd);
            m_fd = -1;
        }

        std::lock_guard<std::mutex> lk(m_state.mtx);
        m_state.connStatus = ConnectionStatus::Disconnected;
        LogRaw(m_state, U32_AMBER, "[ SYS ] Conexão serial encerrada.");
    }
};
