/**
 * @file    main.c
 * @brief   Firmware Principal — Leitor RFID RC522 com LED RGB no STM32F103C8T6
 *
 * Funcionalidade:
 *   - LED pisca em AZUL aguardando uma tag RFID
 *   - Tag com UID correto  → LED acende em VERDE por 3 segundos
 *   - Tag com UID errado   → LED acende em VERMELHO por 3 segundos
 *
 * Clocks:
 *   - SYSCLK = 72 MHz via PLL (HSE 8 MHz × 9)
 *   - AHB    = 72 MHz (HPRE = /1)
 *   - APB1   = 36 MHz (PPRE1 = /2, limite do barramento)
 *   - APB2   = 72 MHz (PPRE2 = /1)
 *   - SysTick → interrupção a cada 1 ms
 *
 * Hardware:
 *   LED RGB (ânodo comum): PA0=R, PA1=G, PA2=B (lógica invertida)
 *   RFID RC522 (SPI1):     PA4=CS, PA5=SCK, PA6=MISO, PA7=MOSI, PB0=RST
 *
 * @note    Todo o código de periférico usa APENAS <stm32f1xx.h> (CMSIS).
 *          Nenhuma função HAL ou LL é utilizada.
 *
 * Autor:   Projeto Bare-Metal STM32F103C8T6
 */

#include <stm32f1xx.h>
#include <stdint.h>
#include "rgb.h"
#include "rc522.h"
#include "usart.h"
#include "lora.h"

/* =========================================================================
 * UID Mestre (hardcoded) — altere conforme o cartão/chaveiro desejado
 * ========================================================================= */
static const uint8_t MASTER_UID[4U] = { 0x12U, 0x34U, 0x56U, 0x78U };

/* =========================================================================
 * Contador de Milissegundos (SysTick)
 * Declarado volatile pois é modificado dentro de uma ISR.
 * ========================================================================= */
volatile uint32_t SysTick_tick = 0U;

/* =========================================================================
 * Handler do SysTick — Incrementa o contador a cada 1 ms
 * O nome SysTick_Handler é vinculado pelo linker ao vetor de interrupção.
 * ========================================================================= */
void SysTick_Handler(void)
{
    SysTick_tick++;
}

/* =========================================================================
 * delay_ms — Delay bloqueante baseado em SysTick
 * ========================================================================= */
void delay_ms(uint32_t ms)
{
    uint32_t start = SysTick_tick;
    while ((SysTick_tick - start) < ms)
    {
        /* aguarda — o SysTick_Handler atualiza SysTick_tick em background */
        __NOP();
    }
}

/* =========================================================================
 * SystemClock_Config — Configura SYSCLK a 72 MHz usando PLL + HSE
 *
 * Sequência conforme RM0008 §7.3:
 *   1. Liga o HSE e aguarda ficar estável (HSERDY)
 *   2. Configura latência do Flash (2 wait states para 72 MHz)
 *   3. Configura prescalers AHB/APB1/APB2
 *   4. Configura PLL: fonte = HSE, multiplicador = ×9 → 8×9 = 72 MHz
 *   5. Liga o PLL e aguarda ficar estável (PLLRDY)
 *   6. Seleciona PLL como fonte do SYSCLK
 *   7. Aguarda confirmação de troca (SWS = 10b)
 * ========================================================================= */
static void SystemClock_Config(void)
{
    /*
     * Usa HSI (oscilador interno de 8 MHz) como fonte do PLL.
     * Não depende de cristal externo — funciona em qualquer Blue Pill.
     *
     * SYSCLK = (HSI/2) * PLL_MUL = 4 MHz * 12 = 48 MHz
     *   HSI/2  → fonte obrigatória do PLL quando usando HSI
     *   PLLx12 → 4 * 12 = 48 MHz (dentro do limite de 72 MHz)
     *
     * Clocks resultantes:
     *   AHB  = 48 MHz (HPRE  = /1)
     *   APB1 = 24 MHz (PPRE1 = /2)
     *   APB2 = 48 MHz (PPRE2 = /1)
     *
     * USART1 BRR para 115200 @ APB2=48MHz:
     *   USARTDIV = 48.000.000 / (16 * 115200) = 26.04
     *   Mantissa = 26 = 0x1A, Fração = 0.04*16 ≈ 1
     *   BRR = (26 << 4) | 1 = 0x1A1
     *
     * SysTick para 1ms @ 48MHz:
     *   LOAD = 48000 - 1
     *
     * NOTA: Se quiser 72 MHz com cristal externo depois, basta descomentar
     *       o bloco HSE abaixo e comentar o bloco HSI.
     */

    /* --- 1. Garante que HSI está ligado e estável (já é o default) --- */
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY))
    {
        __NOP();
    }

    /* --- 2. Latência do Flash: 1 wait state para 48 MHz (24–48 MHz) --- */
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;

    /* --- 3. Prescalers --- */
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2; /* APB1 = AHB/2 = 24 MHz */

    /* --- 4. PLL: fonte = HSI/2, multiplicador = x12 → 4*12 = 48 MHz --- */
    RCC->CFGR &= ~RCC_CFGR_PLLSRC;    /* PLL source = HSI/2 (bit=0) */
    RCC->CFGR &= ~RCC_CFGR_PLLMULL;
    RCC->CFGR |=  RCC_CFGR_PLLMULL12; /* x12 */

    /* --- 5. Liga PLL e aguarda lock --- */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY))
    {
        __NOP();
    }

    /* --- 6. Seleciona PLL como SYSCLK --- */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |=  RCC_CFGR_SW_PLL;

    /* --- 7. Aguarda confirmação --- */
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
    {
        __NOP();
    }
}

/* =========================================================================
 * Peripheral_Clock_Enable — Habilita os clocks dos periféricos usados
 * ========================================================================= */
static void Peripheral_Clock_Enable(void)
{
    /*
     * RCC->APB2ENR — APB2 Peripheral Clock Enable Register:
     *   bit[2]  = IOPAEN → habilita clock do GPIOA
     *   bit[3]  = IOPBEN → habilita clock do GPIOB
     *   bit[12] = SPI1EN → habilita clock do SPI1
     *   bit[14] = USART1EN → habilita clock do USART1
     *   bit[0]  = AFIOEN → habilita clock do AFIO (para funções alternativas de SPI)
     */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN
                 |  RCC_APB2ENR_IOPBEN
                 |  RCC_APB2ENR_SPI1EN
                 |  RCC_APB2ENR_USART1EN
                 |  RCC_APB2ENR_AFIOEN;

    /* SPI2 está no barramento APB1 */
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    /*
     * Pequeno delay após habilitar clock (boa prática: esperar 2 ciclos AHB)
     * Reference: RM0008 §7.3.10
     */
    __NOP();
    __NOP();
}

/* =========================================================================
 * SysTick_Config_1ms — Configura SysTick para gerar interrupção a cada 1 ms
 * ========================================================================= */
static void SysTick_Config_1ms(void)
{
    /*
     * SysTick opera a partir do clock AHB (72 MHz).
     * Para 1 ms: reload = 72.000.000 / 1000 - 1 = 71.999
     *
     * Registradores do SysTick (Core Peripheral — ARM Cortex-M3):
     *   SysTick->LOAD  = valor de recarga (RELOAD)
     *   SysTick->VAL   = valor atual do contador (zera ao escrever)
     *   SysTick->CTRL  = Control and Status Register
     *     bit[0] = ENABLE    → inicia o contador
     *     bit[1] = TICKINT   → habilita interrupção ao chegar em 0
     *     bit[2] = CLKSOURCE → 1 = clock do processador (AHB), 0 = clock externo /8
     */
    SysTick->LOAD = 48000U - 1U;         /* 1 ms com SYSCLK = 48 MHz */
    SysTick->VAL  = 0U;                  /* reseta o contador */
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk /* usa clock AHB (72 MHz) */
                  | SysTick_CTRL_TICKINT_Msk    /* habilita interrupção */
                  | SysTick_CTRL_ENABLE_Msk;    /* inicia o contador */
}

/* =========================================================================
 * Máquina de Estados do Firmware
 * ========================================================================= */
typedef enum
{
    STATE_IDLE   = 0U, /**< Aguardando tag: LED pisca em AZUL */
    STATE_ACCESS,      /**< UID correto:    LED acende em VERDE */
    STATE_DENIED,      /**< UID errado:     LED acende em VERMELHO */
} AppState_t;

/* =========================================================================
 * Programa Principal
 * ========================================================================= */
int main(void)
{
    /* Variáveis locais */
    uint8_t     tag_type[2U];           /* Tipo de tag (ATQA) */
    uint8_t     uid[5U];                /* UID[4] + BCC[1] */
    uint8_t     rc_status;              /* Status das funções do RC522 */
    AppState_t  state      = STATE_IDLE;
    uint32_t    state_tick = 0U;        /* Marca temporal da entrada no estado */
    uint32_t    hb_tick    = 0U;        /* Marca temporal do último heartbeat USART */

    /* -------------------------------------------------------------------
     * 1. Inicialização do Sistema
     * ------------------------------------------------------------------- */

    /* Configura SYSCLK a 72 MHz via PLL + HSE */
    SystemClock_Config();

    /* Habilita clocks dos periféricos antes de qualquer acesso a GPIO/SPI */
    Peripheral_Clock_Enable();

    /* Configura SysTick para 1 ms (base de tempo do sistema) */
    SysTick_Config_1ms();

    /* Inicializa LED RGB (configura PA0..PA2 como saídas, apaga tudo) */
    RGB_Init();

    /* Inicializa RC522: configura GPIO/SPI e prepara o chip */
    MFRC522_Init();

    /* Inicializa USART1 (115200 8N1) */
    USART1_Init();
    USART1_SendString("RFID:BOOT\n");

    /* Inicializa módulo LoRa SX1278 via SPI2 */
    if (LoRa_Init() == LORA_OK)
    {
        USART1_SendString("RFID:DBG=LORA_OK\n");
    }
    else
    {
        USART1_SendError("LORA_INIT_FAIL_VERSION");
    }

    /* Coloca o LoRa em modo RX aguardando ACK do Pico */
    LoRa_StartReceive();

    /* -----------------------------------------------------------------------
     * DIAGNÓSTICO: lê o registrador VERSION do RC522.
     * Valor esperado: 0x91 (versão 1) ou 0x92 (versão 2).
     * Se retornar 0x00 ou 0xFF → SPI com problema (fiação ou clock).
     * --------------------------------------------------------------------- */
    {
        static const char hex_digits[] = "0123456789ABCDEF";
        uint8_t ver = MFRC522_ReadReg(MFRC522_REG_VERSION);
        char msg[] = "RFID:DBG=RC522_VERSION=0xXX\n";
        msg[24] = hex_digits[(ver >> 4U) & 0x0FU];
        msg[25] = hex_digits[ver & 0x0FU];
        USART1_SendString(msg);

        if (ver == 0x91U || ver == 0x92U)
        {
            USART1_SendString("RFID:DBG=RC522_OK\n");
        }
        else if (ver == 0x00U)
        {
            USART1_SendString("RFID:ERR=RC522_VERSION=0x00_SPI_MOSI_MISO_CHECK\n");
        }
        else if (ver == 0xFFU)
        {
            USART1_SendString("RFID:ERR=RC522_VERSION=0xFF_CS_OR_POWER_CHECK\n");
        }
        else
        {
            USART1_SendString("RFID:DBG=RC522_VERSION_UNKNOWN_BUT_RESPONDING\n");
        }
    }

    /* Pequena pausa para estabilização */
    delay_ms(100U);

    /* -------------------------------------------------------------------
     * 2. Loop Principal — Máquina de Estados
     * ------------------------------------------------------------------- */
    while (1)
    {
        /* ---------------------------------------------------------------
         * Heartbeat USART a cada 5 segundos (independente do estado)
         * --------------------------------------------------------------- */
        if ((SysTick_tick - hb_tick) >= 5000U)
        {
            hb_tick = SysTick_tick;
            USART1_SendHeartbeat();
        }

        switch (state)
        {
            /* ---------------------------------------------------------------
             * STATE_IDLE: Aguarda tag, LED pisca em AZUL
             * --------------------------------------------------------------- */
            case STATE_IDLE:
            {
                /* Blink não-bloqueante em azul (500 ms on/off) */
                RGB_BlinkBlue(500U);

                /* Tenta detectar uma tag no campo RF */
                rc_status = MFRC522_Request(PICC_CMD_REQA, tag_type);

                if (rc_status == MFRC522_OK)
                {
                    /* Tag detectada → executa anti-colisão para obter UID */
                    rc_status = MFRC522_Anticoll(uid);

                    if (rc_status == MFRC522_OK)
                    {
                        /*
                         * Tag detectada e anti-colisão OK.
                         * Envia o UID via LoRa para o Pico decidir.
                         * Formato: "UID:XX:XX:XX:XX\n"
                         */
                        static const char hex[] = "0123456789ABCDEF";
                        uint8_t pkt[18]; /* "UID:XX:XX:XX:XX\n" = 17 chars + \0 */
                        uint8_t pi = 0U;
                        uint8_t bi;

                        /* Monta a string do pacote LoRa */
                        pkt[pi++] = 'U'; pkt[pi++] = 'I';
                        pkt[pi++] = 'D'; pkt[pi++] = ':';
                        for (bi = 0U; bi < 4U; bi++)
                        {
                            pkt[pi++] = hex[(uid[bi] >> 4U) & 0x0FU];
                            pkt[pi++] = hex[uid[bi] & 0x0FU];
                            if (bi < 3U) pkt[pi++] = ':';
                        }
                        pkt[pi++] = '\n';

                        /* LED amarelo = aguardando resposta do Pico */
                        RGB_SET_YELLOW();

                        /* Transmite via LoRa */
                        LoRa_Transmit(pkt, pi);

                        /* Log pela USART (debug) */
                        USART1_SendString("RFID:DBG=LORA_TX_SENT\n");

                        /* Volta para RX e aguarda ACK do Pico (timeout 3s) */
                        LoRa_StartReceive();

                        uint8_t  ack_buf[16];
                        uint8_t  ack_len = 0U;
                        int16_t  ack_rssi = 0;
                        uint32_t ack_timeout = 3000U;
                        uint8_t  ack_ok = 0U;

                        while (ack_timeout > 0U)
                        {
                            if (LoRa_Receive(ack_buf, &ack_len, &ack_rssi) == LORA_OK)
                            {
                                ack_ok = 1U;
                                break;
                            }
                            delay_ms(1U);
                            ack_timeout--;
                        }

                        if (ack_ok)
                        {
                            /*
                             * Compara o ACK recebido:
                             *   "ACK:GRANT\n" → acesso liberado
                             *   "ACK:DENY\n"  → acesso negado
                             */
                            uint8_t granted = (ack_len >= 9U &&
                                               ack_buf[4] == 'G'); /* GRANT */

                            if (granted)
                            {
                                state      = STATE_ACCESS;
                                state_tick = SysTick_tick;
                                RGB_SET_GREEN();
                                USART1_SendRFIDEvent(uid, 1U);
                            }
                            else
                            {
                                state      = STATE_DENIED;
                                state_tick = SysTick_tick;
                                RGB_SET_RED();
                                USART1_SendRFIDEvent(uid, 0U);
                            }
                        }
                        else
                        {
                            /* Timeout — Pico não respondeu */
                            USART1_SendError("LORA_ACK_TIMEOUT");
                            state      = STATE_DENIED;
                            state_tick = SysTick_tick;
                            RGB_SET_RED();
                        }

                        /* Reinicia RX para próximo ciclo */
                        LoRa_StartReceive();
                        MFRC522_Halt();
                    }
                    else
                    {
                        USART1_SendError("ANTICOLL_FAIL");
                    }
                }
                /* Log periódico a cada ~2s para confirmar que o loop está rodando */
                else if ((SysTick_tick - state_tick) >= 2000U)
                {
                    state_tick = SysTick_tick;
                    USART1_SendString("RFID:DBG=IDLE_SCANNING\n");
                }
                break;
            }

            /* ---------------------------------------------------------------
             * STATE_ACCESS: LED verde por 3 segundos
             * --------------------------------------------------------------- */
            case STATE_ACCESS:
            {
                if ((SysTick_tick - state_tick) >= 3000U)
                {
                    /* 3 segundos passaram → volta ao estado de espera */
                    RGB_ALL_OFF();
                    state = STATE_IDLE;
                }
                break;
            }

            /* ---------------------------------------------------------------
             * STATE_DENIED: LED vermelho por 3 segundos
             * --------------------------------------------------------------- */
            case STATE_DENIED:
            {
                if ((SysTick_tick - state_tick) >= 3000U)
                {
                    /* 3 segundos passaram → volta ao estado de espera */
                    RGB_ALL_OFF();
                    state = STATE_IDLE;
                }
                break;
            }

            /* ---------------------------------------------------------------
             * Estado inválido (defesa por programação)
             * --------------------------------------------------------------- */
            default:
            {
                RGB_ALL_OFF();
                state = STATE_IDLE;
                break;
            }
        }
    }

    /* Nunca alcançado — apenas para satisfazer o compilador */
    return 0;
}