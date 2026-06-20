cmake_minimum_required(VERSION 3.13)

# Importa as configurações do Pico SDK
include(pico_sdk_import.cmake)

project(lora_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Inicializa o SDK
pico_sdk_init()

# Define o nome do executável e os arquivos fonte
add_executable(lora_node
main/transmite.c
)

# Adiciona as bibliotecas necessárias para uso do hardware
target_link_libraries(lora_node pico_stdlib hardware_spi hardware_gpio)

# Habilita a saída pela USB (útil para debug via terminal)
pico_enable_stdio_usb(lora_node 1)
pico_enable_stdio_uart(lora_node 0)

# Cria os arquivos .uf2, .bin, .hex, etc.
pico_add_extra_outputs(lora_node)
