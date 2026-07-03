/**
 * @file    usart.h
 * @brief   Driver Bare-Metal para USART1 no STM32F103C8T6 (Blue Pill)
 *
 * Pinout:
 *   PA9  → TX (USART1_TX, AF push-pull 50MHz)
 *   PA10 → RX (USART1_RX, Input floating)
 *
 * Protocolo de saída (ASCII, uma linha por evento, terminado em '\n'):
 *   "RFID:UID=12:34:56:78:GRANT\n"  → acesso liberado
 *   "RFID:UID=AB:CD:EF:01:DENY\n"   → acesso negado
 *   "RFID:HEARTBEAT\n"              → keep-alive (a cada ~5s)
 *   "RFID:ERR=<msg>\n"              → erro do firmware
 *
 * Configuração: 115200 baud, 8N1, sem controle de fluxo.
 */

#ifndef USART_H
#define USART_H

#include <stm32f1xx.h>
#include <stdint.h>

/**
 * @brief  Inicializa o USART1 a 115200 baud, 8N1.
 *         Configura PA9 (TX) como AF push-pull e PA10 (RX) como input floating.
 * @note   Requer que o clock do GPIOA (RCC->APB2ENR IOPAEN) já esteja
 *         habilitado antes de chamar esta função.
 */
void USART1_Init(void);

/**
 * @brief  Envia um único caractere via USART1 (bloqueante).
 * @param  c  Caractere a enviar
 */
void USART1_SendChar(char c);

/**
 * @brief  Envia uma string terminada em '\0' via USART1 (bloqueante).
 * @param  str  String a enviar (não precisa incluir '\n' — adicione manualmente)
 */
void USART1_SendString(const char *str);

/**
 * @brief  Envia um evento de leitura RFID formatado no protocolo do projeto.
 * @param  uid      Ponteiro para 4 bytes do UID lido
 * @param  granted  1 = acesso liberado (GRANT), 0 = acesso negado (DENY)
 *
 * Exemplo de saída: "RFID:UID=12:34:56:78:GRANT\n"
 */
void USART1_SendRFIDEvent(const uint8_t *uid, uint8_t granted);

/**
 * @brief  Envia um heartbeat de keep-alive: "RFID:HEARTBEAT\n"
 */
void USART1_SendHeartbeat(void);

/**
 * @brief  Envia uma mensagem de erro formatada: "RFID:ERR=<msg>\n"
 * @param  msg  Texto do erro (sem caracteres ':' ou '\n' dentro da mensagem)
 */
void USART1_SendError(const char *msg);

#endif /* USART_H */
