# Projeto: Leitor RFID RC522 + LED RGB — STM32F103C8T6 (Bare-Metal)

## Estrutura de Arquivos

```
rfid_rgb_stm32/
├── main.c                      ← Loop principal, clock, SysTick, máquina de estados
├── rc522.h / rc522.c           ← Driver SPI1 bare-metal para MFRC522
├── rgb.h  / rgb.c              ← Controle do LED RGB ânodo comum (PA0/PA1/PA2)
├── startup_stm32f103c8tx.s     ← Startup assembly (tabela de vetores + Reset_Handler)
├── STM32F103C8TX_FLASH.ld      ← Linker script manual (64KB Flash / 20KB SRAM)
└── Makefile                    ← Build via arm-none-eabi-gcc + OpenOCD
```

---

## Pinout de Hardware

| Sinal       | Pino STM32 | Direção         | Observação                        |
|-------------|-----------|-----------------|-----------------------------------|
| LED R       | PA0       | Output PP 2MHz  | Ânodo comum: LOW = liga           |
| LED G       | PA1       | Output PP 2MHz  | Ânodo comum: LOW = liga           |
| LED B       | PA2       | Output PP 2MHz  | Ânodo comum: LOW = liga           |
| RC522 CS    | PA4       | Output PP 2MHz  | NSS manual, ativo em LOW          |
| RC522 SCK   | PA5       | AF PP 50MHz     | SPI1 Clock                        |
| RC522 MISO  | PA6       | Input Floating  | SPI1 MISO                         |
| RC522 MOSI  | PA7       | AF PP 50MHz     | SPI1 MOSI                         |
| RC522 RST   | PB0       | Output PP 2MHz  | Reset hardware, ativo em LOW      |

> **LED RGB Ânodo Comum**: o pino comum (+) vai para 3.3V.
> Cada canal de cor precisa de um resistor (tipicamente 220Ω–470Ω) em série.

---

## Configuração de Clocks

```
HSE (8 MHz cristal) → PLL × 9 → SYSCLK = 72 MHz
                                  ├── AHB  = 72 MHz  (HPRE  = /1)
                                  ├── APB1 = 36 MHz  (PPRE1 = /2)
                                  └── APB2 = 72 MHz  (PPRE2 = /1)
Flash: 2 wait states + prefetch buffer habilitado
SysTick: interrupção a cada 1 ms (reload = 71.999)
SPI1: BR = fPCLK2/8 = 9 MHz (dentro dos 10 MHz máx do RC522)
```

---

## Como Usar no STM32CubeIDE (sem CubeMX)

### Opção A: Projeto Bare-Metal Manual

1. Abra o STM32CubeIDE e selecione **File → New → STM32 Project**.
2. Escolha o chip **STM32F103C8T6** e clique em **Next**.
3. Na tela de setup, selecione **Empty** como tipo de projeto.
4. **Remova** todos os arquivos gerados automaticamente (`main.c`, `stm32f1xx_it.c`, etc.).
5. Copie os arquivos deste projeto para a pasta `Core/Src/` e `Core/Inc/`.
6. **Substitua** o linker script gerado pelo `STM32F103C8TX_FLASH.ld` deste projeto.
7. Substitua o arquivo de startup pelo `startup_stm32f103c8tx.s`.
8. Nas propriedades do projeto (**Project → Properties → C/C++ Build → Settings**):
   - Em **MCU GCC Compiler → Preprocessor**: adicione `STM32F103xB`
   - Em **MCU GCC Compiler → Include paths**: adicione os caminhos do CMSIS

### Opção B: Build via Makefile (linha de comando)

```bash
# Pré-requisito: arm-none-eabi-gcc instalado
# No Ubuntu/Debian:
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi

# Ajuste CMSIS_DEVICE_INC e CMSIS_CORE_INC no Makefile
# para apontar para os headers do STM32CubeF1

make              # compila
make flash        # grava via ST-Link + OpenOCD
make clean        # limpa
```

---

## Ajustando o UID Mestre

Em `main.c`, localize a linha:

```c
static const uint8_t MASTER_UID[4U] = { 0x12U, 0x34U, 0x56U, 0x78U };
```

Para descobrir o UID do seu cartão, adicione temporariamente ao `STATE_IDLE`:

```c
// Após MFRC522_Anticoll, envie os bytes via UART ou pisque o LED
// para identificar o UID do cartão desejado.
// Depois substitua os 4 bytes acima pelo UID lido.
```

---

## Lógica de Operação (Máquina de Estados)

```
         ┌─────────────────────────────────────┐
         │            STATE_IDLE               │
         │   LED Azul piscando (500ms on/off)  │
         └────────────┬────────────────────────┘
                      │ MFRC522_Request() OK
                      │ MFRC522_Anticoll() OK
              ┌───────┴────────┐
      UID OK  │                │  UID ERRADO
              ▼                ▼
     ┌──────────────┐   ┌──────────────┐
     │ STATE_ACCESS │   │ STATE_DENIED │
     │  LED Verde   │   │  LED Vermelho│
     │   3 segundos │   │   3 segundos │
     └──────┬───────┘   └──────┬───────┘
            │                  │
            └────────┬─────────┘
                     │ timeout 3s
                     ▼
              volta ao IDLE
```

---

## Protocolo SPI do MFRC522

```
ESCRITA: CS_LOW → [0|addr(6)|0] → [data] → CS_HIGH
LEITURA: CS_LOW → [1|addr(6)|0] → [dummy] → CS_HIGH (retorna dado)

CPOL=0, CPHA=0 (SPI Modo 0), MSB first, max 10 MHz
```

---

## Dependências de Software

- `arm-none-eabi-gcc` (GNU Arm Embedded Toolchain ≥ 10)
- Headers CMSIS do STM32CubeF1 (`stm32f1xx.h` e afins)
- `openocd` (opcional, apenas para gravação via `make flash`)
