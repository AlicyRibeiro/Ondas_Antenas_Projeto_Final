##############################################################################
# Makefile — GUI RFID RC522  (Dear ImGui + GLFW3 + OpenGL3, Linux)
#
# Dependências do sistema (Ubuntu/Debian):
#   sudo apt install libglfw3-dev libgl-dev fonts-hack-ttf
#
# Uso:
#   make          → compila (modo Release)
#   make debug    → compila com símbolos de debug e sanitizers
#   make run      → compila e executa
#   make clean    → remove artefatos
##############################################################################

CXX      := g++
TARGET   := rfid_gui
BUILD    := build

IMGUI_DIR := vendor/imgui


SOURCES := \
    src/main.cpp \
    $(IMGUI_DIR)/imgui.cpp \
    $(IMGUI_DIR)/imgui_draw.cpp \
    $(IMGUI_DIR)/imgui_tables.cpp \
    $(IMGUI_DIR)/imgui_widgets.cpp \
    $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp \
    $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

OBJECTS := $(patsubst %.cpp, $(BUILD)/%.o, $(SOURCES))

INCLUDES := \
    -Isrc \
    -I$(IMGUI_DIR) \
    -I$(IMGUI_DIR)/backends


CXXFLAGS_COMMON := \
    -std=c++17 \
    $(INCLUDES) \
    -Wall -Wextra \
    -Wno-unused-parameter \
    -Wno-missing-field-initializers

ifeq ($(SERIAL),1)
    CXXFLAGS_COMMON += -DUSE_SERIAL_BACKEND
    $(info >>> Modo SERIAL REAL (/dev/ttyUSB0))
else
    $(info >>> Modo SIMULADOR (sem hardware))
endif

CXXFLAGS_RELEASE := $(CXXFLAGS_COMMON) -O2 -DNDEBUG
CXXFLAGS_DEBUG   := $(CXXFLAGS_COMMON) -Og -g3 -fsanitize=address,undefined


CXXFLAGS := $(CXXFLAGS_RELEASE)

LDFLAGS := -lGL -lglfw -ldl -lpthread

# ─────────────────────────────────────────────────────────────────────────────
# Regras
# ─────────────────────────────────────────────────────────────────────────────
.PHONY: all debug run clean

all: $(BUILD)/$(TARGET)
	@echo ""
	@echo "✓ Build concluído → $(BUILD)/$(TARGET)"
	@echo "  Execute com: ./$(BUILD)/$(TARGET)"

debug: CXXFLAGS := $(CXXFLAGS_DEBUG)
debug: LDFLAGS  += -fsanitize=address,undefined
debug: $(BUILD)/$(TARGET)
	@echo "✓ Build DEBUG → $(BUILD)/$(TARGET)"

run: all
	@./$(BUILD)/$(TARGET)

# Linkagem
$(BUILD)/$(TARGET): $(OBJECTS)
	@echo "[LD]  $@"
	@$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

# Compilação genérica — cria subdiretórios conforme necessário
$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf $(BUILD)
	@echo "✓ Diretório '$(BUILD)' removido."
