/**
 * @file    lora.c
 * @brief   Implementação Bare-Metal do driver SX1276/SX1278 via SPI2
 *
 * Todo o acesso ao hardware é feito por manipulação direta de registradores
 * CMSIS. Nenhuma HAL ou LL é utilizada.
 *
 * Clock do SPI2: APB1 = 24 MHz (HSI PLL × 12 / 2).
 *   BR[2:0] = 001 → fPCLK/4 = 6 MHz — dentro do limite do SX1278 (10 MHz).
 */

#include "lora.h"
#include <stddef.h>

/* =========================================================================
 * delay_ms (declarado em main.c via SysTick)
 * ========================================================================= */
extern void delay_ms(uint32_t ms);

/* =========================================================================
 * SPI2_TransmitReceive — transfere 1 byte full-duplex (bloqueante)
 * ========================================================================= */
static uint8_t SPI2_TransmitReceive(uint8_t data)
{
    uint32_t timeout;

    /* Aguarda TXE (transmit buffer empty) */
    timeout = 10000U;
    while (!(SPI2->SR & SPI_SR_TXE))
        if (--timeout == 0U) return 0xFFU;

    *(__IO uint8_t *)&SPI2->DR = data;

    /* Aguarda RXNE (receive buffer not empty) */
    timeout = 10000U;
    while (!(SPI2->SR & SPI_SR_RXNE))
        if (--timeout == 0U) return 0xFFU;

    return (uint8_t)(SPI2->DR);
}

/* =========================================================================
 * LoRa_WriteReg / LoRa_ReadReg
 * ========================================================================= */
void LoRa_WriteReg(uint8_t reg, uint8_t val)
{
    LORA_CS_LOW();
    SPI2_TransmitReceive(reg | 0x80U); /* bit7=1 → escrita */
    SPI2_TransmitReceive(val);
    LORA_CS_HIGH();
}

uint8_t LoRa_ReadReg(uint8_t reg)
{
    uint8_t val;
    LORA_CS_LOW();
    SPI2_TransmitReceive(reg & 0x7FU); /* bit7=0 → leitura */
    val = SPI2_TransmitReceive(0x00U);
    LORA_CS_HIGH();
    return val;
}

/* =========================================================================
 * LoRa_SetMode — altera o modo de operação
 * ========================================================================= */
static void LoRa_SetMode(uint8_t mode)
{
    LoRa_WriteReg(REG_OP_MODE, MODE_LONG_RANGE_MODE | mode);
}

/* =========================================================================
 * LoRa_Init
 * ========================================================================= */
uint8_t LoRa_Init(void)
{
    /*
     * ── 1. Configura GPIO do SPI2 via GPIOB CRH ──────────────────────────
     *
     * PB12 (NSS)  → Output PP 10 MHz     → CRH[19:16] = 0x1
     * PB13 (SCK)  → AF PP 10 MHz         → CRH[23:20] = 0x9
     * PB14 (MISO) → Input floating        → CRH[27:24] = 0x4
     * PB15 (MOSI) → AF PP 10 MHz         → CRH[31:28] = 0x9
     *
     * PB1  (RST)  → Output PP 2 MHz      → CRL[7:4]   = 0x2
     * PB2  (DIO0) → Input floating        → CRL[11:8]  = 0x4
     */

    /* PB1 (RST) e PB2 (DIO0) no CRL */
    GPIOB->CRL &= ~(0x00000FF0U);
    GPIOB->CRL |=  (0x2U << 4U)  /* PB1: Output PP 2MHz */
               |   (0x4U << 8U); /* PB2: Input floating */

    /* PB12..PB15 no CRH */
    GPIOB->CRH &= ~(0xFFFF0000U);
    GPIOB->CRH |=  (0x1U << 16U) /* PB12: Output PP 10MHz (NSS manual) */
               |   (0x9U << 20U) /* PB13: AF PP 10MHz (SCK) */
               |   (0x4U << 24U) /* PB14: Input floating (MISO) */
               |   (0x9U << 28U);/* PB15: AF PP 10MHz (MOSI) */

    LORA_CS_HIGH();  /* CS inativo */
    LORA_RST_HIGH(); /* RST inativo */

    /*
     * ── 2. Configura SPI2 ─────────────────────────────────────────────────
     *
     * CR1:
     *   MSTR    = 1    → Master
     *   BR[2:0] = 001  → fPCLK1/4 = 24/4 = 6 MHz
     *   CPOL    = 0    → idle LOW
     *   CPHA    = 0    → captura na primeira borda
     *   SSM     = 1    → NSS por software
     *   SSI     = 1    → NSS interno = 1
     *   DFF     = 0    → 8 bits
     *   LSBFIRST= 0    → MSB first
     */
    RCC->APB1RSTR |=  RCC_APB1RSTR_SPI2RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_SPI2RST;

    SPI2->CR1 = SPI_CR1_MSTR
              | SPI_CR1_SSM
              | SPI_CR1_SSI
              | (0x1U << SPI_CR1_BR_Pos); /* BR = fPCLK/4 = 6 MHz */
    SPI2->CR2 = 0U;
    SPI2->CR1 |= SPI_CR1_SPE; /* habilita SPI2 */

    /*
     * ── 3. Reset hardware do SX1278 ──────────────────────────────────────
     * Datasheet §7.2.2: RST low ≥100 µs, depois high, aguarda 5 ms.
     */
    LORA_RST_LOW();
    delay_ms(1U);
    LORA_RST_HIGH();
    delay_ms(10U);

    /*
     * ── 4. Verifica versão do chip ────────────────────────────────────────
     * REG_VERSION deve retornar 0x12 para SX1276/78.
     */
    uint8_t ver = LoRa_ReadReg(REG_VERSION);
    if (ver != 0x12U)
        return LORA_ERR; /* chip não respondeu */

    /*
     * ── 5. Configura modo LoRa ────────────────────────────────────────────
     * Precisa estar em SLEEP para mudar para LoRa mode (bit7 do REG_OP_MODE).
     */
    LoRa_SetMode(MODE_SLEEP);
    delay_ms(2U);

    /* ── 6. Frequência: 433 MHz ── */
    LoRa_WriteReg(REG_FRF_MSB, LORA_FRF_MSB);
    LoRa_WriteReg(REG_FRF_MID, LORA_FRF_MID);
    LoRa_WriteReg(REG_FRF_LSB, LORA_FRF_LSB);

    /* ── 7. Potência de TX: 17 dBm via PA_BOOST ── */
    LoRa_WriteReg(REG_PA_CONFIG, 0x8FU); /* PA_BOOST + MaxPower=7 + OutputPower=15 */
    LoRa_WriteReg(REG_PA_DAC,    0x87U); /* habilita +20 dBm mode se necessário */
    LoRa_WriteReg(REG_OCP,       0x3BU); /* overcurrent protection: 240 mA */

    /* ── 8. LNA: ganho máximo + boost ── */
    LoRa_WriteReg(REG_LNA, 0x23U); /* LnaGain=001 (max), LnaBoostHf=11 */

    /* ── 9. Parâmetros LoRa: BW=125kHz, CR=4/5, SF=7 ── */
    /*
     * REG_MODEM_CONFIG_1:
     *   Bw[7:4]          = 0111 → 125 kHz
     *   CodingRate[3:1]  = 001  → 4/5
     *   ImplicitHeader   = 0    → explicit header
     */
    LoRa_WriteReg(REG_MODEM_CONFIG_1, 0x72U);

    /*
     * REG_MODEM_CONFIG_2:
     *   SpreadingFactor[7:4] = 0111 → SF7
     *   TxContinuousMode     = 0
     *   RxPayloadCrcOn       = 1    → CRC habilitado
     *   SymbTimeout[1:0]     = 00
     */
    LoRa_WriteReg(REG_MODEM_CONFIG_2, 0x74U);

    /*
     * REG_MODEM_CONFIG_3:
     *   LowDataRateOptimize = 0 (só necessário para SF11/SF12)
     *   AgcAutoOn           = 1 → AGC automático
     */
    LoRa_WriteReg(REG_MODEM_CONFIG_3, 0x04U);

    /* ── 10. Preâmbulo: 8 símbolos ── */
    LoRa_WriteReg(REG_PREAMBLE_MSB, 0x00U);
    LoRa_WriteReg(REG_PREAMBLE_LSB, 0x08U);

    /* ── 11. Sync word privado (evita interferência com redes LoRaWAN) ── */
    LoRa_WriteReg(REG_SYNC_WORD, LORA_SYNC_WORD);

    /* ── 12. Endereços base do FIFO ── */
    LoRa_WriteReg(REG_FIFO_TX_BASE_ADDR, 0x00U);
    LoRa_WriteReg(REG_FIFO_RX_BASE_ADDR, 0x00U);

    /* ── 13. Otimização de detecção para SF7 ── */
    LoRa_WriteReg(REG_DETECTION_OPTIMIZE,  0xC3U);
    LoRa_WriteReg(REG_DETECTION_THRESHOLD, 0x0AU);

    /* ── 14. DIO0 mapeado para RxDone (em RX) e TxDone (em TX) ── */
    LoRa_WriteReg(REG_DIO_MAPPING_1, 0x00U);

    /* ── 15. Vai para Standby ── */
    LoRa_SetMode(MODE_STDBY);
    delay_ms(2U);

    return LORA_OK;
}

/* =========================================================================
 * LoRa_Transmit
 * ========================================================================= */
uint8_t LoRa_Transmit(const uint8_t *data, uint8_t len)
{
    uint8_t i;

    if (len > LORA_MAX_PKT) len = LORA_MAX_PKT;

    /* Vai para standby antes de escrever no FIFO */
    LoRa_SetMode(MODE_STDBY);

    /* Aponta o ponteiro do FIFO para o endereço base TX */
    LoRa_WriteReg(REG_FIFO_ADDR_PTR, 0x00U);

    /* Define o tamanho do payload */
    LoRa_WriteReg(REG_PAYLOAD_LENGTH, len);

    /* Escreve os dados no FIFO byte a byte */
    for (i = 0U; i < len; i++)
        LoRa_WriteReg(REG_FIFO, data[i]);

    /* DIO0 → TxDone */
    LoRa_WriteReg(REG_DIO_MAPPING_1, 0x40U);

    /* Limpa flags de IRQ */
    LoRa_WriteReg(REG_IRQ_FLAGS, 0xFFU);

    /* Inicia transmissão */
    LoRa_SetMode(MODE_TX);

    /*
     * Aguarda TxDone via DIO0 (timeout ~2s).
     * Com SF7/BW125/CR4-5 e pacote de 20 bytes:
     *   ToA ≈ 56 ms → timeout generoso de 2000 ms.
     */
    uint32_t timeout = 2000U;
    while (timeout > 0U)
    {
        if (LORA_DIO0_READ())
        {
            /* Limpa flag TxDone */
            LoRa_WriteReg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
            LoRa_SetMode(MODE_STDBY);
            return LORA_OK;
        }
        delay_ms(1U);
        timeout--;
    }

    LoRa_SetMode(MODE_STDBY);
    return LORA_TIMEOUT;
}

/* =========================================================================
 * LoRa_StartReceive
 * ========================================================================= */
void LoRa_StartReceive(void)
{
    /* DIO0 → RxDone */
    LoRa_WriteReg(REG_DIO_MAPPING_1, 0x00U);

    /* Aponta FIFO para o endereço base RX */
    LoRa_WriteReg(REG_FIFO_ADDR_PTR, 0x00U);
    LoRa_WriteReg(REG_FIFO_RX_BASE_ADDR, 0x00U);

    /* Limpa flags */
    LoRa_WriteReg(REG_IRQ_FLAGS, 0xFFU);

    /* Inicia RX contínuo */
    LoRa_SetMode(MODE_RX_CONTINUOUS);
}

/* =========================================================================
 * LoRa_Receive (não-bloqueante — chama no loop)
 * ========================================================================= */
uint8_t LoRa_Receive(uint8_t *buf, uint8_t *len, int16_t *rssi)
{
    uint8_t flags;
    uint8_t i;

    /* Verifica se DIO0 está alto (RxDone) */
    if (!LORA_DIO0_READ())
        return LORA_TIMEOUT; /* nenhum pacote ainda */

    /* Lê os flags de IRQ */
    flags = LoRa_ReadReg(REG_IRQ_FLAGS);

    /* Limpa todos os flags */
    LoRa_WriteReg(REG_IRQ_FLAGS, 0xFFU);

    /* Verifica erro de CRC */
    if (flags & IRQ_PAYLOAD_CRC_ERR)
        return LORA_CRC_ERR;

    /* Quantos bytes foram recebidos? */
    *len = LoRa_ReadReg(REG_RX_NB_BYTES);
    if (*len > LORA_MAX_PKT) *len = LORA_MAX_PKT;

    /* Aponta o ponteiro do FIFO para onde o pacote foi armazenado */
    LoRa_WriteReg(REG_FIFO_ADDR_PTR,
                LoRa_ReadReg(REG_FIFO_RX_CURRENT_ADDR));

    /* Lê os bytes do FIFO */
    for (i = 0U; i < *len; i++)
        buf[i] = LoRa_ReadReg(REG_FIFO);

    /* RSSI do pacote: RSSI(dBm) = -157 + RegPktRssiValue (para HF/433MHz) */
    if (rssi != NULL)
        *rssi = (int16_t)(-164 + (int16_t)LoRa_ReadReg(REG_PKT_RSSI_VALUE));

    return LORA_OK;
}

/* =========================================================================
 * LoRa_Sleep / LoRa_RSSI
 * ========================================================================= */
void LoRa_Sleep(void)
{
    LoRa_SetMode(MODE_SLEEP);
}

int16_t LoRa_RSSI(void)
{
    return (int16_t)(-164 + (int16_t)LoRa_ReadReg(REG_RSSI_WIDEBAND));
}