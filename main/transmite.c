#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// ================= PINOS DA PICO W =================
#define SPI_PORT spi0
#define MISO 16
#define NSS  17 // CS
#define SCK  18
#define MOSI 19
#define RST  20

// ================= SPI WRITE =================
void spi_write_byte(uint8_t addr, uint8_t data) {
    uint8_t buffer[2];
    buffer[0] = addr | 0x80; // Bit 7 em 1 para escrita
    buffer[1] = data;
    
    gpio_put(NSS, 0); 
    spi_write_blocking(SPI_PORT, buffer, 2);
    gpio_put(NSS, 1); 
}

// ================= SPI READ =================
uint8_t spi_read_byte(uint8_t addr) {
    uint8_t tx = addr & 0x7F; // Bit 7 em 0 para leitura
    uint8_t rx;
    
    gpio_put(NSS, 0); 
    spi_write_blocking(SPI_PORT, &tx, 1);     
    spi_read_blocking(SPI_PORT, 0x00, &rx, 1); 
    gpio_put(NSS, 1); 
    
    return rx;
}

// ================= LORA INIT =================
void lora_init(void) {
    // Sleep mode + LoRa
    spi_write_byte(0x01, 0x80);
    sleep_ms(10);

    // Standby mode
    spi_write_byte(0x01, 0x81);
    sleep_ms(10);

    // ================= FREQUENCY 433 MHz =================
    // FRF = 0x6C8000
    spi_write_byte(0x06, 0x6C); // RegFrfMsb
    spi_write_byte(0x07, 0x80); // RegFrfMid
    spi_write_byte(0x08, 0x00); // RegFrfLsb

    // ================= PA CONFIG =================
    spi_write_byte(0x09, 0x8F);

    // ================= FIFO =================
    spi_write_byte(0x0E, 0x80); // TX base
    spi_write_byte(0x0F, 0x00); // RX base

    // ================= LONG RANGE =================
    // BW = 62.5 kHz
    // CR = 4/5
    spi_write_byte(0x1D, 0x62);

    // SF12 + CRC ON
    spi_write_byte(0x1E, 0xB4);

    // AGC + LowDataRateOptimize
    spi_write_byte(0x26, 0x0C);

    // Max Power
    spi_write_byte(0x4D, 0x87);

    // DIO0 -> TxDone
    spi_write_byte(0x40, 0x40);

    // Standby
    spi_write_byte(0x01, 0x81);
}

// ================= LORA TRANSMIT =================
void lora_transmit(uint8_t *data, uint8_t len) {
    // FIFO pointer
    spi_write_byte(0x0D, 0x80);

    // Write payload into FIFO
    for(uint8_t i = 0; i < len; i++) {
        spi_write_byte(0x00, data[i]);
    }

    // Payload length
    spi_write_byte(0x22, len);

    // TX mode
    spi_write_byte(0x01, 0x83);

    // Espera transmissão terminar (polling na flag TxDone)
    while((spi_read_byte(0x12) & 0x08) == 0) {
        sleep_ms(1); // Pequeno atraso para não travar a CPU
    }

    // Limpa flag TxDone
    spi_write_byte(0x12, 0x08);

    // Volta standby
    spi_write_byte(0x01, 0x81);
}

// ================= MAIN =================
int main() {
    stdio_init_all();

    // Inicializa a interface SPI em 1 MHz
    spi_init(SPI_PORT, 1000 * 1000);
    
    // Configura os pinos SPI
    gpio_set_function(MISO, GPIO_FUNC_SPI);
    gpio_set_function(SCK, GPIO_FUNC_SPI);
    gpio_set_function(MOSI, GPIO_FUNC_SPI);

    // Configura o Chip Select (NSS)
    gpio_init(NSS);
    gpio_set_dir(NSS, GPIO_OUT);
    gpio_put(NSS, 1); // Desativado por padrão

    // Configura o pino de Reset
    gpio_init(RST);
    gpio_set_dir(RST, GPIO_OUT);

    // Rotina de Reset do hardware
    gpio_put(RST, 0);
    sleep_ms(10);
    gpio_put(RST, 1);
    sleep_ms(10);

    // Inicializa LoRa com os parâmetros desejados
    lora_init();

    // Lê a versão do chip (Para testar se o SPI está funcionando)
    uint8_t version = spi_read_byte(0x42);
    printf("VERSION: 0x%02X\n", version);

    uint8_t msg[] = "HELLO";

    while (1) {
        lora_transmit(msg, sizeof(msg) - 1);
        printf("Pacote enviado\n");
        sleep_ms(3000);
    }
    
    return 0;
}