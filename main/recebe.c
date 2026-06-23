#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// ================= DEFINIÇÕES DE PINOS =================
#define SPI_PORT spi0
#define MISO 16
#define NSS  17
#define SCK  18
#define MOSI 19
#define RST  20

// ================= PROTÓTIPOS =================
void spi_write_byte(uint8_t addr, uint8_t data);
uint8_t spi_read_byte(uint8_t addr);
void lora_init_rx(void);
void lora_receive(void);

// ================= FUNÇÕES SPI =================
void spi_write_byte(uint8_t addr, uint8_t data) {
    uint8_t buffer[2] = { (uint8_t)(addr | 0x80), data };
    gpio_put(NSS, 0); 
    spi_write_blocking(SPI_PORT, buffer, 2);
    gpio_put(NSS, 1); 
}

uint8_t spi_read_byte(uint8_t addr) {
    uint8_t tx = (uint8_t)(addr & 0x7F);
    uint8_t rx;
    gpio_put(NSS, 0); 
    spi_write_blocking(SPI_PORT, &tx, 1);     
    spi_read_blocking(SPI_PORT, 0x00, &rx, 1); 
    gpio_put(NSS, 1); 
    return rx;
}

// ================= INICIALIZAÇÃO DO LORA (RECEPTOR) =================
void lora_init_rx(void) {
    spi_write_byte(0x01, 0x80); sleep_ms(10); // Sleep mode
    spi_write_byte(0x01, 0x81); sleep_ms(10); // Standby mode
    
    // Configura a mesma frequência do transmissor (433 MHz)
    spi_write_byte(0x06, 0x6C);
    spi_write_byte(0x07, 0x80);
    spi_write_byte(0x08, 0x00);
    
    // Configurações de Modem LoRa
    spi_write_byte(0x1D, 0x62);
    spi_write_byte(0x1E, 0xB4);
    spi_write_byte(0x26, 0x0C);
    
    // Coloca em modo de Recepção Contínua (RX Continuous)
    spi_write_byte(0x01, 0x85);
}

// ================= RECEPÇÃO E LED =================
void lora_receive(void) {
    uint8_t irq = spi_read_byte(0x12);
    
    // Verifica a flag RxDone (Pacote recebido com sucesso)
    if ((irq & 0x40) != 0) {
        uint8_t len = spi_read_byte(0x13);
        uint8_t rx_addr = spi_read_byte(0x10);
        spi_write_byte(0x0D, rx_addr);
        
        char buffer[32];
        for (int i = 0; i < len && i < 31; i++) {
            buffer[i] = (char)spi_read_byte(0x00);
        }
        buffer[len] = '\0';
        
        // --- EXTRAÇÃO DOS DADOS FÍSICOS (RF) ---
        int rssi = spi_read_byte(0x1A) - 157;
        int8_t snr_raw = (int8_t)spi_read_byte(0x19);
        float snr = snr_raw / 4.0;
        
        // Exibe no terminal as informações prontas para coleta
        printf("\n====================================\n");
        printf(" DADO RECEBIDO : %s\n", buffer);
        printf(" POTENCIA (RSSI): %d dBm\n", rssi);
        printf(" RUIDO (SNR)    : %.2f dB\n", snr);
        printf("====================================\n");
        
        // --- A MÁGICA DO LED: Pisca quando recebe! ---
        gpio_put(25, 1);  // Liga
        sleep_ms(300);    // Mantém aceso por 300 milissegundos
        gpio_put(25, 0);  // Desliga
        // ---------------------------------------------
        
        // Limpa a interrupção
        spi_write_byte(0x12, 0xFF); 
    }
}

// ================= FUNÇÃO PRINCIPAL =================
int main() {
    // Inicializa a USB para o printf
    stdio_init_all();
    
    // Inicializa o LED Interno da Pico (Pino 25)
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    
    // Inicializa o barramento SPI
    spi_init(SPI_PORT, 500 * 1000);
    gpio_set_function(MISO, GPIO_FUNC_SPI);
    gpio_set_function(SCK,  GPIO_FUNC_SPI);
    gpio_set_function(MOSI, GPIO_FUNC_SPI);
    
    // Inicializa o pino NSS (Chip Select)
    gpio_init(NSS);
    gpio_set_dir(NSS, GPIO_OUT);
    gpio_put(NSS, 1);
    
    // Inicializa o pino de Reset do LoRa
    gpio_init(RST);
    gpio_set_dir(RST, GPIO_OUT);
    gpio_put(RST, 0);
    sleep_ms(10);
    gpio_put(RST, 1);
    sleep_ms(10);
    
    // Configura o LoRa para ouvir
    lora_init_rx();
    
    // Dá tempo para você abrir o Monitor Serial/Painel
    sleep_ms(2000);
    printf("Receptor Iniciado. Aguardando a TAG RFID...\n");
    
    while (1) {
        lora_receive();
        sleep_ms(10); // Pequena pausa para não travar a CPU
    }
    return 0;
}