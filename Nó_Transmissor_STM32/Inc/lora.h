/**
 * @file    lora.h
 * @brief   Driver Bare-Metal para SX1276/SX1278 via SPI2 no STM32F103C8T6
 *
 * Pinout SPI2 (não conflita com RC522 no SPI1):
 *   PB12 → NSS  (CS manual, ativo em LOW)
 *   PB13 → SCK  (AF push-pull 10 MHz)
 *   PB14 → MISO (Input floating)
 *   PB15 → MOSI (AF push-pull 10 MHz)
 *   PB1  → RESET (GPIO output, ativo em LOW)
 *   PB2  → DIO0  (GPIO input — IRQ: TxDone / RxDone)
 *
 * Protocolo de mensagens (ASCII, terminadas em '\n'):
 *   TX (STM32 → Pico):  "UID:XX:XX:XX:XX\n"
 *   RX (Pico → STM32):  "ACK:GRANT\n" ou "ACK:DENY\n"
 *
 * Frequência: 433 MHz (Ra-01/SX1278) — ajuste LORA_FREQ para 868/915 MHz
 *             se usar Ra-02/SX1276 na sua região.
 *
 * Referência: SX1276/77/78/79 Datasheet Rev 7 (Semtech)
 */

#ifndef LORA_H
#define LORA_H

#include <stm32f1xx.h>
#include <stdint.h>

/* =========================================================================
 * Frequência de operação
 * ========================================================================= */
/** Frequência central em Hz — Ra-01 usa 433 MHz */
#define LORA_FREQ_HZ        433000000UL

/**
 * Fórmula do registrador de frequência (Frf):
 *   Frf = (freq * 2^19) / 32.000.000
 *   Para 433 MHz: Frf = (433000000 * 524288) / 32000000 = 7094272 = 0x6C8000
 */
#define LORA_FRF_MSB        0x6CU
#define LORA_FRF_MID        0x80U
#define LORA_FRF_LSB        0x00U

/* =========================================================================
 * Pinos de controle
 * ========================================================================= */
#define LORA_CS_PIN         12U   /* PB12 */
#define LORA_RST_PIN         1U   /* PB1  */
#define LORA_DIO0_PIN        2U   /* PB2  */

#define LORA_CS_LOW()       (GPIOB->BRR  = (1U << LORA_CS_PIN))
#define LORA_CS_HIGH()      (GPIOB->BSRR = (1U << LORA_CS_PIN))
#define LORA_RST_LOW()      (GPIOB->BRR  = (1U << LORA_RST_PIN))
#define LORA_RST_HIGH()     (GPIOB->BSRR = (1U << LORA_RST_PIN))
#define LORA_DIO0_READ()    ((GPIOB->IDR >> LORA_DIO0_PIN) & 1U)

/* =========================================================================
 * Endereços dos registradores do SX1278
 * ========================================================================= */
#define REG_FIFO                0x00U
#define REG_OP_MODE             0x01U
#define REG_FRF_MSB             0x06U
#define REG_FRF_MID             0x07U
#define REG_FRF_LSB             0x08U
#define REG_PA_CONFIG           0x09U
#define REG_OCP                 0x0BU
#define REG_LNA                 0x0CU
#define REG_FIFO_ADDR_PTR       0x0DU
#define REG_FIFO_TX_BASE_ADDR   0x0EU
#define REG_FIFO_RX_BASE_ADDR   0x0FU
#define REG_FIFO_RX_CURRENT_ADDR 0x10U
#define REG_IRQ_FLAGS           0x12U
#define REG_RX_NB_BYTES         0x13U
#define REG_PKT_SNR_VALUE       0x19U
#define REG_PKT_RSSI_VALUE      0x1AU
#define REG_MODEM_CONFIG_1      0x1DU
#define REG_MODEM_CONFIG_2      0x1EU
#define REG_PREAMBLE_MSB        0x20U
#define REG_PREAMBLE_LSB        0x21U
#define REG_PAYLOAD_LENGTH      0x22U
#define REG_MODEM_CONFIG_3      0x26U
#define REG_RSSI_WIDEBAND       0x2CU
#define REG_DETECTION_OPTIMIZE  0x31U
#define REG_DETECTION_THRESHOLD 0x37U
#define REG_SYNC_WORD           0x39U
#define REG_DIO_MAPPING_1       0x40U
#define REG_VERSION             0x42U
#define REG_PA_DAC              0x4DU

/* =========================================================================
 * Modos de operação (REG_OP_MODE)
 * ========================================================================= */
#define MODE_LONG_RANGE_MODE    0x80U  /* bit7=1 → LoRa mode */
#define MODE_SLEEP              0x00U
#define MODE_STDBY              0x01U
#define MODE_TX                 0x03U
#define MODE_RX_CONTINUOUS      0x05U
#define MODE_RX_SINGLE          0x06U

/* =========================================================================
 * Flags de IRQ (REG_IRQ_FLAGS)
 * ========================================================================= */
#define IRQ_TX_DONE_MASK        0x08U
#define IRQ_RX_DONE_MASK        0x40U
#define IRQ_PAYLOAD_CRC_ERR     0x20U

/* =========================================================================
 * Parâmetros LoRa
 * ========================================================================= */
#define LORA_SF                 7U    /* Spreading Factor 7 (mais rápido) */
#define LORA_BW                 0x70U /* Bandwidth 125 kHz */
#define LORA_CR                 0x02U /* Coding Rate 4/5 */
#define LORA_SYNC_WORD          0x12U /* sync word privado (≠ 0x34 LoRaWAN) */
#define LORA_TX_POWER           17U   /* dBm — PA_BOOST ativo */
#define LORA_MAX_PKT            64U   /* tamanho máximo do pacote em bytes */

/* =========================================================================
 * Códigos de retorno
 * ========================================================================= */
#define LORA_OK                 0U
#define LORA_ERR                1U
#define LORA_TIMEOUT            2U
#define LORA_CRC_ERR            3U

/* =========================================================================
 * Protótipos
 * ========================================================================= */

/**
 * @brief Inicializa SPI2, GPIOs, reseta o SX1278 e configura LoRa mode.
 * @return LORA_OK se chip responder (versão 0x12), LORA_ERR caso contrário.
 * @note   Requer que os clocks de GPIOB e SPI2 já estejam habilitados.
 */
uint8_t LoRa_Init(void);

/**
 * @brief Transmite um pacote via LoRa (bloqueante até TxDone ou timeout).
 * @param data   Buffer com os bytes a transmitir
 * @param len    Número de bytes (máx LORA_MAX_PKT)
 * @return LORA_OK em sucesso, LORA_TIMEOUT em timeout
 */
uint8_t LoRa_Transmit(const uint8_t *data, uint8_t len);

/**
 * @brief Tenta receber um pacote (não-bloqueante — verifica DIO0).
 * @param buf    Buffer de recepção
 * @param len    Ponteiro onde será armazenado o número de bytes recebidos
 * @param rssi   Ponteiro onde será armazenado o RSSI em dBm (pode ser NULL)
 * @return LORA_OK se pacote disponível, LORA_TIMEOUT se nenhum, LORA_CRC_ERR
 */
uint8_t LoRa_Receive(uint8_t *buf, uint8_t *len, int16_t *rssi);

/**
 * @brief Coloca o módulo em modo RX contínuo (aguardando pacotes).
 */
void LoRa_StartReceive(void);

/**
 * @brief Coloca o módulo em modo SLEEP (baixo consumo).
 */
void LoRa_Sleep(void);

/**
 * @brief Lê o RSSI atual do canal em dBm.
 * @return RSSI em dBm
 */
int16_t LoRa_RSSI(void);

/* Acesso direto aos registradores (uso interno e depuração) */
void    LoRa_WriteReg(uint8_t reg, uint8_t val);
uint8_t LoRa_ReadReg(uint8_t reg);

#endif /* LORA_H */