/**
 * ============================================================
 *  BluePASS — Firmware Arduino Uno (ATmega328P)
 *  Controle de Acesso RFID com Comunicação LoRa (Otimizado)
 * ============================================================
 *
 * Hardware:
 *   Arduino Uno (ATmega328P, 5V, 16MHz)
 *   Módulo RFID MFRC522 (RC522)
 *   Módulo LoRa SX1276/SX1278 (Ra-01)
 *   LED RGB Ânodo Comum
 *
 * Pinout:
 *   ── SPI compartilhado ──────────────────────────────────────
 *   Pino 11 (MOSI) → RC522 MOSI + SX1278 MOSI  [1kΩ em série]
 *   Pino 12 (MISO) → RC522 MISO + SX1278 MISO
 *   Pino 13 (SCK)  → RC522 SCK  + SX1278 SCK   [1kΩ em série]
 *
 *   ── RC522 ──────────────────────────────────────────────────
 *   Pino 10 → RC522 SDA (CS)   [1kΩ em série]
 *   Pino  8 → RC522 RST        [1kΩ em série]
 *   3.3V    → RC522 VCC
 *   GND     → RC522 GND
 *
 *   ── SX1278 (LoRa) ──────────────────────────────────────────
 *   Pino  9 → SX1278 NSS (CS)  [1kΩ em série]
 *   Pino  7 → SX1278 RESET     [1kΩ em série]
 *   Pino  2 → SX1278 DIO0 (IRQ — entrada, sem resistor)
 *   3.3V    → SX1278 VCC
 *   GND     → SX1278 GND
 *
 *   ── LED RGB (Ânodo Comum) ───────────────────────────────────
 *   Pino  3 → R (220Ω em série)
 *   Pino  4 → G (220Ω em série)
 *   Pino  5 → B (220Ω em série)
 *   5V      → COM (ânodo)
 *   Lógica: LOW = liga a cor, HIGH = apaga (ânodo comum)
 * ============================================================
 */

#include <SPI.h>
#include <MFRC522.h>
#include <LoRa.h>

// ─────────────────────────────────────────────────────────────
// Pinos
// ─────────────────────────────────────────────────────────────
#define RC522_CS_PIN    10
#define RC522_RST_PIN    8

#define LORA_CS_PIN      9
#define LORA_RST_PIN     7
#define LORA_DIO0_PIN    2

#define RGB_R_PIN        3
#define RGB_G_PIN        4
#define RGB_B_PIN        5

// ─────────────────────────────────────────────────────────────
// UID Mestre — Atualizado com a sua tag real
// ─────────────────────────────────────────────────────────────
const uint8_t UID_AUTORIZADO[4] = { 0xE0, 0xF0, 0x8F, 0xE1 };

// ─────────────────────────────────────────────────────────────
// Configurações
// ─────────────────────────────────────────────────────────────
#define LORA_FREQ       433E6   // 433 MHz (Ra-01/SX1278)
#define LORA_SF              7   // Spreading Factor 7
#define LORA_BW         125E3   // Bandwidth 125 kHz
#define LORA_CR              5   // Coding Rate 4/5
#define LORA_SYNC_WORD   0x12   // sync word privado (≠ 0x34 LoRaWAN)
#define LORA_TX_POWER      17   // dBm

#define ACCESS_TIME_MS   3000   // tempo com LED aceso após leitura
#define ACK_TIMEOUT_MS   3000   // timeout aguardando ACK do Pico
#define HB_INTERVAL_MS   5000   // intervalo do heartbeat serial

// ─────────────────────────────────────────────────────────────
// Objetos
// ─────────────────────────────────────────────────────────────
MFRC522 rfid(RC522_CS_PIN, RC522_RST_PIN);

// ─────────────────────────────────────────────────────────────
// Máquina de estados
// ─────────────────────────────────────────────────────────────
enum Estado { IDLE, AGUARDANDO_ACK, ACESSO_LIBERADO, ACESSO_NEGADO };
Estado estado = IDLE;
unsigned long estadoInicio = 0;

// ─────────────────────────────────────────────────────────────
// Controle de tempo
// ─────────────────────────────────────────────────────────────
unsigned long ultimoHeartbeat = 0;
unsigned long ultimoBlink     = 0;
bool          blinkState      = false;

// ─────────────────────────────────────────────────────────────
// Helpers: LED RGB (ânodo comum — lógica invertida)
// ─────────────────────────────────────────────────────────────
void ledOff() {
    digitalWrite(RGB_R_PIN, HIGH);
    digitalWrite(RGB_G_PIN, HIGH);
    digitalWrite(RGB_B_PIN, HIGH);
}

void ledRed()    { ledOff(); digitalWrite(RGB_R_PIN, LOW); }
void ledGreen()  { ledOff(); digitalWrite(RGB_G_PIN, LOW); }
void ledBlue()   { ledOff(); digitalWrite(RGB_B_PIN, LOW); }
void ledYellow() { ledOff(); digitalWrite(RGB_R_PIN, LOW); digitalWrite(RGB_G_PIN, LOW); }

void ledBlinkBlue(uint32_t intervalo) {
    if (millis() - ultimoBlink >= intervalo) {
        ultimoBlink = millis();
        blinkState = !blinkState;
        if (blinkState) ledBlue();
        else            ledOff();
    }
}

void ledBlinkYellow(uint32_t intervalo) {
    if (millis() - ultimoBlink >= intervalo) {
        ultimoBlink = millis();
        blinkState = !blinkState;
        if (blinkState) ledYellow();
        else            ledOff();
    }
}

// ─────────────────────────────────────────────────────────────
// Controle Rígido do Barramento SPI
// ─────────────────────────────────────────────────────────────
void selecionarRC522() {
    digitalWrite(LORA_CS_PIN, HIGH);  // Libera o LoRa
    digitalWrite(RC522_CS_PIN, LOW);  // Seleciona o RFID
}

void selecionarLoRa() {
    digitalWrite(RC522_CS_PIN, HIGH); // Libera o RFID
    digitalWrite(LORA_CS_PIN, LOW);   // Seleciona o LoRa
}

void liberarSPI() {
    digitalWrite(RC522_CS_PIN, HIGH);
    digitalWrite(LORA_CS_PIN, HIGH);
}

// ─────────────────────────────────────────────────────────────
// Helpers: Serial para GUI
// ─────────────────────────────────────────────────────────────
void serialLog(const char* tipo, const char* msg) {
    Serial.print("RFID:");
    Serial.print(tipo);
    if (msg && msg[0] != '\0') {
        Serial.print("=");
        Serial.print(msg);
    }
    Serial.print("\n");
}

void serialUID(const uint8_t* uid, bool granted) {
    char buf[32];
    snprintf(buf, sizeof(buf),
             "UID=%02X:%02X:%02X:%02X:%s",
             uid[0], uid[1], uid[2], uid[3],
             granted ? "GRANT" : "DENY");
    Serial.print("RFID:");
    Serial.print(buf);
    Serial.print("\n");
}

// ─────────────────────────────────────────────────────────────
// Helpers: UID
// ─────────────────────────────────────────────────────────────
bool compararUID(const uint8_t* a, const uint8_t* b, uint8_t len) {
    for (uint8_t i = 0; i < len; i++)
        if (a[i] != b[i]) return false;
    return true;
}

void formatarUID(const uint8_t* uid, char* out) {
    snprintf(out, 12, "%02X:%02X:%02X:%02X",
             uid[0], uid[1], uid[2], uid[3]);
}

// ─────────────────────────────────────────────────────────────
// LoRa: transmite pacote UID para o Pico
// ─────────────────────────────────────────────────────────────
bool loraTXUID(const uint8_t* uid) {
    char pkt[20];
    snprintf(pkt, sizeof(pkt), "UID:%02X:%02X:%02X:%02X\n",
             uid[0], uid[1], uid[2], uid[3]);

    selecionarLoRa();

    LoRa.beginPacket();
    LoRa.print(pkt);
    bool ok = LoRa.endPacket(); // bloqueante — espera TX_DONE antes de retornar

    liberarSPI();
    serialLog("DBG", ok ? "LORA_TX_OK" : "LORA_TX_FAIL");
    return ok;
}

// ─────────────────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial);

    // Configuração Inicial do LED
    pinMode(RGB_R_PIN, OUTPUT);
    pinMode(RGB_G_PIN, OUTPUT);
    pinMode(RGB_B_PIN, OUTPUT);
    ledOff();

    // Configura pinos de seleção como saída e limpa barramento
    pinMode(RC522_CS_PIN, OUTPUT);
    pinMode(LORA_CS_PIN,  OUTPUT);
    liberarSPI();

    SPI.begin();

    // ── Inicializa RC522 ─────────────────────────────────────
    selecionarRC522();
    rfid.PCD_Init();
    byte ver = rfid.PCD_ReadRegister(MFRC522::VersionReg);
    liberarSPI();

    if (ver == 0x91 || ver == 0x92) {
        char dbg[24];
        snprintf(dbg, sizeof(dbg), "RC522_VER=0x%02X", ver);
        serialLog("DBG", dbg);
    } else {
        char err[24];
        snprintf(err, sizeof(err), "RC522_VER=0x%02X_ERR", ver);
        serialLog("ERR", err);
    }

    // ── Inicializa LoRa SX1278 ───────────────────────────────
    LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);

    selecionarLoRa();
    if (!LoRa.begin(LORA_FREQ)) {
        serialLog("ERR", "LORA_INIT_FAIL");
        while (true) {
            ledRed();   delay(200);
            ledOff();   delay(200);
        }
    }

    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setCodingRate4(LORA_CR);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.enableCrc();
    LoRa.receive(); // Coloca em modo escuta continuo
    liberarSPI();

    serialLog("DBG", "LORA_OK_433MHz_SF7_BW125");
    serialLog("BOOT", "");
}

// ─────────────────────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────────────────────
void loop() {

    // ── Heartbeat serial ─────────────────────────────────────
    if (millis() - ultimoHeartbeat >= HB_INTERVAL_MS) {
        ultimoHeartbeat = millis();
        serialLog("HEARTBEAT", "");
    }

    // ── Máquina de estados ───────────────────────────────────
    switch (estado) {

        // ── IDLE: aguardando tag ──────────────────────────────
        case IDLE: {
            ledBlinkBlue(500);

            selecionarRC522();
            if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
                liberarSPI();
                break;
            }

            uint8_t* uid  = rfid.uid.uidByte;

            char uidStr[12];
            formatarUID(uid, uidStr);
            serialLog("DBG", uidStr);

            ledYellow();

            // Envia UID via LoRa para o Pico
            if (loraTXUID(uid)) {
                estado       = AGUARDANDO_ACK;
                estadoInicio = millis();
                selecionarLoRa();
                LoRa.receive(); // Garante modo RX ativado para receber resposta
                liberarSPI();
            } else {
                // Fallback local instantâneo se o rádio falhar no envio
                bool granted = compararUID(uid, UID_AUTORIZADO, 4);
                serialUID(uid, granted);
                if (granted) ledGreen(); else ledRed();
                estado = granted ? ACESSO_LIBERADO : ACESSO_NEGADO;
                estadoInicio = millis();
            }

            // Finaliza comunicação com cartão atual
            selecionarRC522();
            rfid.PICC_HaltA();
            rfid.PCD_StopCrypto1();
            liberarSPI();
            break;
        }

        // ── AGUARDANDO_ACK: checagem assíncrona por pacote ─────
        case AGUARDANDO_ACK: {
            ledBlinkYellow(100); // Pisca amarelo de forma fluida sem travar o processamento

            selecionarLoRa();
            int pktSize = LoRa.parsePacket();
            
            // Filtro de tamanho mínimo para evitar processar ruídos elétricos soltos no ar
            if (pktSize >= 8) {
                char ack[16];
                uint8_t i = 0;
                while (LoRa.available() && i < sizeof(ack) - 1) {
                    ack[i++] = (char)LoRa.read();
                }
                ack[i] = '\0';
                liberarSPI();

                // Filtro de consistência de string
                if (strncmp(ack, "ACK", 3) == 0) {
                    serialLog("DBG", ack);

                    if (strncmp(ack, "ACK:GRANT", 9) == 0) {
                        ledGreen();
                        serialLog("DBG", "ACK_GRANT_RECEBIDO");
                        estado       = ACESSO_LIBERADO;
                        estadoInicio = millis();
                    } else if (strncmp(ack, "ACK:DENY", 8) == 0) {
                        ledRed();
                        serialLog("DBG", "ACK_DENY_RECEBIDO");
                        estado       = ACESSO_NEGADO;
                        estadoInicio = millis();
                    }
                }
            } 
            // Limpa pacotes fragmentados residuais menores do buffer
            else if (pktSize > 0) {
                while (LoRa.available()) LoRa.read(); 
                liberarSPI();
            }
            // Timeout por tempo estourado aguardando resposta
            else if (millis() - estadoInicio >= ACK_TIMEOUT_MS) {
                liberarSPI();
                serialLog("ERR", "LORA_ACK_TIMEOUT");
                ledRed();
                estado       = ACESSO_NEGADO;
                estadoInicio = millis();
            }
            else {
                liberarSPI();
            }
            break;
        }

        // ── ACESSO_LIBERADO: Mantém verde por tempo regulado ──
        case ACESSO_LIBERADO: {
            ledGreen();
            if (millis() - estadoInicio >= ACCESS_TIME_MS) {
                ledOff();
                estado = IDLE;
            }
            break;
        }

        // ── ACESSO_NEGADO: Mantém vermelho por tempo regulado ──
        case ACESSO_NEGADO: {
            ledRed();
            if (millis() - estadoInicio >= ACCESS_TIME_MS) {
                ledOff();
                estado = IDLE;
            }
            break;
        }
    }
}
