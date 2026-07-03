/**
 * @file    usart.c
 * @brief   Implementação Bare-Metal do driver USART1
 *
 * Toda a configuração é feita via manipulação direta dos registradores
 * CMSIS — sem HAL, sem LL.
 */

#include "usart.h"
#include <string.h>

/* =========================================================================
 * USART1_Init
 * ========================================================================= */
void USART1_Init(void)
{
    /*
     * -----------------------------------------------------------------------
     * 1. Habilita o clock do USART1
     * -----------------------------------------------------------------------
     * USART1 está no barramento APB2 (RCC->APB2ENR, bit 14)
     */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /*
     * -----------------------------------------------------------------------
     * 2. Configuração dos pinos PA9 (TX) e PA10 (RX) via CRH
     * -----------------------------------------------------------------------
     *
     * PA9  (TX)  → Alternate Function push-pull, 50 MHz → CRH[7:4]  = 0xB
     * PA10 (RX)  → Input floating                        → CRH[11:8] = 0x4
     *
     * O registrador CRH controla os pinos PA8..PA15.
     * Cada pino ocupa 4 bits: [CNF1:CNF0 | MODE1:MODE0]
     *
     *   PA9  → bits [7:4]   (segundo nibble do CRH)
     *   PA10 → bits [11:8]  (terceiro nibble do CRH)
     */

    GPIOA->CRH &= ~((0xFU << 4U) | (0xFU << 8U));

    GPIOA->CRH |= (0xBU << 4U)  
               |  (0x4U << 8U);  

    /*
     * -----------------------------------------------------------------------
     * 3. Configuração do Baud Rate: 115200 @ PCLK2 = 72 MHz
     * -----------------------------------------------------------------------
     *
     * USART1 está no APB2, que roda a 72 MHz (PPRE2 = /1, conforme
     * SystemClock_Config() em main.c).
     *
     * Fórmula do BRR (USARTDIV):
     *   USARTDIV = fPCLK / (16 * baud)
     *            = 72.000.000 / (16 * 115200)
     *            = 39.0625
     *
     * Mantissa = 39        → 0x27
     * Fração   = 0.0625*16 = 1   → 0x1
     *
     * BRR = (Mantissa << 4) | Fração = (39 << 4) | 1 = 0x271
     */
    USART1->BRR = 0x271U; /* 115200 baud @ 72 MHz */

    /*
     * -----------------------------------------------------------------------
     * 4. Configuração do CR1: 8N1, habilita TX/RX e o periférico
     * -----------------------------------------------------------------------
     *
     *   UE    = 1 → USART Enable
     *   TE    = 1 → Transmitter Enable
     *   RE    = 1 → Receiver Enable
     *   M     = 0 → 8 data bits (padrão, não setado)
     *   PCE   = 0 → sem paridade (padrão, não setado)
     */
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    USART1->CR2 = 0U;

    USART1->CR3 = 0U;
}

/* =========================================================================
 * USART1_SendChar
 * ========================================================================= */
void USART1_SendChar(char c)
{

    while (!(USART1->SR & USART_SR_TXE))
    {
    
    }

    USART1->DR = (uint8_t)c;
}

/* =========================================================================
 * USART1_SendString
 * ========================================================================= */
void USART1_SendString(const char *str)
{
    while (*str != '\0')
    {
        USART1_SendChar(*str);
        str++;
    }
}

/* =========================================================================
 * USART1_SendRFIDEvent
 * ========================================================================= */
void USART1_SendRFIDEvent(const uint8_t *uid, uint8_t granted)
{
    
    static const char hex_digits[] = "0123456789ABCDEF";
    char buf[32];
    uint8_t idx = 0U;
    uint8_t i;

    
    const char *prefix = "RFID:UID=";
    while (*prefix != '\0')
    {
        buf[idx++] = *prefix++;
    }

    for (i = 0U; i < 4U; i++)
    {
        buf[idx++] = hex_digits[(uid[i] >> 4U) & 0x0FU];
        buf[idx++] = hex_digits[uid[i] & 0x0FU];
        if (i < 3U)
        {
            buf[idx++] = ':';
        }
    }

    buf[idx++] = ':';

    
    if (granted)
    {
        const char *suf = "GRANT";
        while (*suf != '\0') { buf[idx++] = *suf++; }
    }
    else
    {
        const char *suf = "DENY";
        while (*suf != '\0') { buf[idx++] = *suf++; }
    }

    buf[idx++] = '\n';
    buf[idx]   = '\0';

    USART1_SendString(buf);
}

/* =========================================================================
 * USART1_SendHeartbeat
 * ========================================================================= */
void USART1_SendHeartbeat(void)
{
    USART1_SendString("RFID:HEARTBEAT\n");
}

/* =========================================================================
 * USART1_SendError
 * ========================================================================= */
void USART1_SendError(const char *msg)
{
    USART1_SendString("RFID:ERR=");
    USART1_SendString(msg);
    USART1_SendString("\n");
}
