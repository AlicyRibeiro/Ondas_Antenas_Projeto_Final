##############################################################################
# Makefile — Projeto Bare-Metal STM32F103C8T6 (Blue Pill)
# RFID RC522 + LED RGB
#
# Toolchain: arm-none-eabi-gcc (GNU Arm Embedded Toolchain)
# Uso:
#   make          → compila e gera o .elf e o .bin
#   make flash    → grava via ST-Link (requer openocd)
#   make clean    → remove todos os artefatos de build
##############################################################################

# --- Toolchain ---
PREFIX   = arm-none-eabi-
CC       = $(PREFIX)gcc
AS       = $(PREFIX)gcc -x assembler-with-cpp
OBJCOPY  = $(PREFIX)objcopy
OBJDUMP  = $(PREFIX)objdump
SIZE     = $(PREFIX)size

# --- Nome do Projeto ---
TARGET   = rfid_rgb_stm32

# --- Diretórios ---
BUILD_DIR = build

# --- Fontes ---
C_SOURCES = \
    main.c \
    rc522.c \
    rgb.c \
    usart.c \
    lora.c

ASM_SOURCES = \
    startup_stm32f103c8tx.s

# --- Includes ---
# Ajuste o caminho do CMSIS conforme sua instalação do STM32CubeF1
# Exemplo para STM32CubeIDE padrão no Linux:
#   ~/.stm32cubeide/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32/...
# Para uso standalone com o pacote CMSIS da ST:
CMSIS_DEVICE_INC = Drivers/CMSIS/Device/ST/STM32F1xx/Include
CMSIS_CORE_INC   = Drivers/CMSIS/Include

C_INCLUDES = \
    -I. \
    -I$(CMSIS_DEVICE_INC) \
    -I$(CMSIS_CORE_INC)

# --- Defines ---
C_DEFS = \
    -DSTM32F103xB

# --- Flags do Compilador C ---
CPU      = -mcpu=cortex-m3
FPU      =
FLOAT_ABI=
MCU      = $(CPU) -mthumb $(FPU) $(FLOAT_ABI)

CFLAGS   = $(MCU) $(C_DEFS) $(C_INCLUDES)
CFLAGS  += -Wall -Wextra -Wpedantic
CFLAGS  += -fdata-sections -ffunction-sections
CFLAGS  += -std=c11
CFLAGS  += -O2
# Para debug, substitua -O2 por:
# CFLAGS  += -Og -g3 -gdwarf-2

# --- Flags do Assembler ---
ASFLAGS  = $(MCU) $(C_DEFS) $(C_INCLUDES) -Wall -fdata-sections -ffunction-sections

# --- Flags do Linker ---
LDSCRIPT = STM32F103C8TX_FLASH.ld
LIBS     = -lc -lm -lnosys
LIBDIR   =
LDFLAGS  = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS)
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref
LDFLAGS += -Wl,--gc-sections

# --- Objetos de Saída ---
OBJECTS  = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))

VPATH    = $(sort $(dir $(C_SOURCES)) $(dir $(ASM_SOURCES)))

##############################################################################
# Regras
##############################################################################

.PHONY: all flash clean size

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin
	@echo ""
	@echo "=== Build concluído com sucesso ==="
	$(SIZE) $(BUILD_DIR)/$(TARGET).elf

# Compilação dos arquivos C
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "[CC]  $<"
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

# Montagem dos arquivos .s
$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	@echo "[AS]  $<"
	$(AS) -c $(ASFLAGS) $< -o $@

# Linkagem → gera o ELF
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) $(LDSCRIPT)
	@echo "[LD]  $@"
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

# Converte ELF → BIN (para gravação via gravador)
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	@echo "[BIN] $@"
	$(OBJCOPY) -O binary -S $< $@

# Converte ELF → HEX (alternativa para gravadores que aceitam Intel HEX)
$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	@echo "[HEX] $@"
	$(OBJCOPY) -O ihex $< $@

# Cria o diretório de build se não existir
$(BUILD_DIR):
	mkdir -p $@

# --- Gravação via OpenOCD + ST-Link ---
flash: $(BUILD_DIR)/$(TARGET).bin
	openocd \
	    -f interface/stlink.cfg \
	    -f target/stm32f1x.cfg \
	    -c "program $(BUILD_DIR)/$(TARGET).bin verify reset exit 0x08000000"

# --- Limpeza ---
clean:
	rm -rf $(BUILD_DIR)
	@echo "Diretório de build removido."

# --- Exibe tamanho do binário ---
size: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) $<