#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#define SPI_PORT spi0
#define MISO 16
#define NSS  17
#define SCK  18
#define MOSI 19
#define RST  20

void spi_write_byte(uint8_t addr, uint8_t data) {
    // Declaração separada para evitar avisos do IntelliSense no VS Code
    uint8_t buffer[2];
    buffer[0] = addr | 0x80;
    buffer[1] = data;
    
    gpio_put(NSS, 0); 
    spi_write_blocking(SPI_PORT, buffer, 2);
    gpio_put(NSS, 1); 
}

uint8_t spi_read_byte(uint8_t addr) {
    uint8_t tx = addr & 0x7F;
    uint8_t rx;
    gpio_put(NSS, 0); 
    spi_write_blocking(SPI_PORT, &tx, 1);     
    spi_read_blocking(SPI_PORT, 0x00, &rx, 1); 
    gpio_put(NSS, 1); 
    return rx;
}

void lora_init_rx(void) {
    spi_write_byte(0x01, 0x80); sleep_ms(10);
    spi_write_byte(0x01, 0x81); sleep_ms(10);

    spi_write_byte(0x06, 0x6C);
    spi_write_byte(0x07, 0x80);
    spi_write_byte(0x08, 0x00);

    spi_write_byte(0x1D, 0x62);
    spi_write_byte(0x1E, 0xB4);
    spi_write_byte(0x26, 0x0C);
    
    spi_write_byte(0x01, 0x85); // RX CONTINUOUS
}

void lora_receive() {
    uint8_t irq = spi_read_byte(0x12);
    
    // Verifica a flag RxDone
    if ((irq & 0x40) != 0) {
        uint8_t len = spi_read_byte(0x13);
        uint8_t rx_addr = spi_read_byte(0x10);
        
        spi_write_byte(0x0D, rx_addr);
        
        printf("Pacote Recebido: ");
        for (int i = 0; i < len; i++) {
            printf("%c", spi_read_byte(0x00));
        }
        printf("\n");
        
        // Limpa todas as flags de interrupção
        spi_write_byte(0x12, 0xFF); 
    }
}

int main() {
    stdio_init_all();
    spi_init(SPI_PORT, 1000 * 1000);
    
    gpio_set_function(MISO, GPIO_FUNC_SPI);
    gpio_set_function(SCK, GPIO_FUNC_SPI);
    gpio_set_function(MOSI, GPIO_FUNC_SPI);

    gpio_init(NSS); gpio_set_dir(NSS, GPIO_OUT); gpio_put(NSS, 1);
    gpio_init(RST); gpio_set_dir(RST, GPIO_OUT);
    
    gpio_put(RST, 0); sleep_ms(10);
    gpio_put(RST, 1); sleep_ms(10);

    // Teste de hardware (Validação do barramento SPI)
    uint8_t version = spi_read_byte(0x42);
    printf("\n=== DIAGNOSTICO DE HARDWARE ===\n");
    printf("Versao do Chip SX1276: 0x%02X\n", version);
    if(version != 0x12) {
        printf("ALERTA: Falha na comunicacao SPI. Verifique MISO, MOSI, SCK e NSS.\n");
    } else {
        printf("Status: SPI OK!\n");
    }
    printf("===============================\n\n");

    lora_init_rx();
    printf("Receptor Pico Iniciado. Aguardando pacotes em 433 MHz...\n");

    while (1) {
        lora_receive();
        sleep_ms(10); // Pausa leve para não sobrecarregar a CPU e facilitar a depuração
    }
    return 0;
}