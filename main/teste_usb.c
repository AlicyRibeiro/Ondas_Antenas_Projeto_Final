#include <stdio.h>
#include "pico/stdlib.h"

int main() {
    // Inicializa a comunicação serial (Crucial para o printf via USB funcionar)
    stdio_init_all();

    // Configura o pino do LED interno (Pino 25 na Pico padrão)
    const uint LED_PIN = 25;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Dá um tempo para o sistema operacional reconhecer a porta USB
    sleep_ms(2000); 

    int contador = 0;

    while (true) {
        // Liga o LED
        gpio_put(LED_PIN, 1);
        printf("DADO RECEBIDO : TESTE DE BANCADA\n");
        printf("POTENCIA (RSSI): -%d dBm\n", 40 + (contador % 50)); // Simula um RSSI variando
        sleep_ms(1000);

        // Desliga o LED
        gpio_put(LED_PIN, 0);
        sleep_ms(1000);

        contador++;
    }
    
    return 0;
}
