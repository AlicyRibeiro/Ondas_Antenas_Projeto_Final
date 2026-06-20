#include <SPI.h>

const int NSS = 10;
const int RST = 9;

// ================= SPI WRITE =================
void spi_write_byte(uint8_t addr, uint8_t data) {
  digitalWrite(NSS, LOW);
  SPI.transfer(addr | 0x80);
  SPI.transfer(data);
  digitalWrite(NSS, HIGH);
}

// ================= SPI READ =================
uint8_t spi_read_byte(uint8_t addr) {
  digitalWrite(NSS, LOW);
  SPI.transfer(addr & 0x7F);
  uint8_t rx = SPI.transfer(0x00);
  digitalWrite(NSS, HIGH);
  return rx;
}

// ================= LORA INIT (RECEPTOR) =================
void lora_init_rx() {
  spi_write_byte(0x01, 0x80); delay(10); // Sleep mode
  spi_write_byte(0x01, 0x81); delay(10); // Standby mode

  // Frequência 433 MHz
  spi_write_byte(0x06, 0x6C);
  spi_write_byte(0x07, 0x80);
  spi_write_byte(0x08, 0x00);

  // Mesmas configurações de rádio do transmissor
  spi_write_byte(0x1D, 0x62); // BW = 62.5 kHz, CR = 4/5
  spi_write_byte(0x1E, 0xB4); // SF11, CRC ON
  spi_write_byte(0x26, 0x0C); // AGC ON

  // MODO RX CONTÍNUO (0x85)
  spi_write_byte(0x01, 0x85);
}

// ================= LORA RECEIVE =================
void lora_receive() {
  uint8_t irq = spi_read_byte(0x12); // Lê as flags de interrupção
  
  // Verifica se o pacote chegou (RxDone == bit 6 / 0x40)
  if ((irq & 0x40) != 0) {
    uint8_t len = spi_read_byte(0x13);      // Tamanho do pacote
    uint8_t rx_addr = spi_read_byte(0x10);  // Endereço de memória do pacote
    
    // Aponta para onde a mensagem está guardada
    spi_write_byte(0x0D, rx_addr);
    
    Serial.print("Pacote Recebido: ");
    for (int i = 0; i < len; i++) {
      char c = (char)spi_read_byte(0x00); // Lê letra por letra
      Serial.print(c);
    }
    Serial.println();
    
    // Limpa a flag de interrupção para escutar o próximo
    spi_write_byte(0x12, 0xFF);
  }
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  
  pinMode(NSS, OUTPUT);
  digitalWrite(NSS, HIGH);
  pinMode(RST, OUTPUT);
  
  // Reset do rádio
  digitalWrite(RST, LOW); delay(10);
  digitalWrite(RST, HIGH); delay(10);
  
  lora_init_rx();
  Serial.println("Receptor Arduino Iniciado. Aguardando pacotes...");
}

void loop() {
  lora_receive();
}
