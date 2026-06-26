#  Telemetria e Análise de Propagação RF com LoRa

Este projeto consiste no desenvolvimento de um sistema de telemetria sem fio utilizando módulos LoRa (433 MHz) para fins de estudo experimental em **Ondas Eletromagnéticas e Antenas**. 

A aplicação prática funciona como um sistema de controle de acesso (acionado via RFID), mas o núcleo científico do projeto visa coletar dados físicos da propagação de rádio (RSSI e SNR) e realizar uma análise comparativa de desempenho entre antenas comerciais e antenas dimensionadas artesanalmente.

##  Objetivos do Projeto

* **Estudo de Propagação:** Validar na prática conceitos de atenuação de sinal no espaço livre e a Equação de Friis através da coleta de dados de potência de sinal (RSSI) em diferentes distâncias.
* **Análise Comparativa de Antenas:** Comparar o desempenho de transmissão da antena omnidirecional de fábrica com uma antena monopolo de 1/4 de onda ($\lambda/4$) de 17,3 cm construída pela equipe, analisando ganho, diretividade e eficiência com plano de terra.
* **Telemetria Automatizada:** Desenvolver um painel de controle em Python capaz de ler dados da porta serial, exibir a integridade do link RF em tempo real e registrar logs automatizados em formato `.csv`.

##  Arquitetura do Sistema

O projeto é dividido em três módulos principais:

### 1. Transmissor (Gatilho e Irradiação)
* **Hardware:** Arduino Uno/Nano + Módulo RFID MFRC522 + Módulo Rádio LoRa (SX1276).
* **Funcionamento:** O RFID atua como evento de disparo. Ao ler uma tag válida, o Arduino envia os dados via SPI para o módulo LoRa, que modula e irradia o pacote na frequência de 433 MHz.

### 2. Receptor (Medição e Conversão)
* **Hardware:** Raspberry Pi Pico (RP2040) + Módulo Rádio LoRa (SX1276).
* **Funcionamento:** Intercepta a onda eletromagnética, decodifica o pacote e extrai os registradores físicos de **RSSI (Potência em dBm)** e **SNR (Relação Sinal-Ruído)**, enviando os dados brutos via interface USB (Serial) para o computador.

### 3. Painel de Controle (Software)
* **Tecnologia:** Python (Tkinter + PySerial).
* **Funcionamento:** Interface Gráfica de Usuário (GUI) rodando em ambiente Linux. Monitora a porta `ttyACM0`, exibe os dados eletromagnéticos ao vivo e salva os registros de distância, antena utilizada e RSSI no arquivo `experimento_antenas.csv` para posterior geração de gráficos comparativos.

---

##  Esquema de Ligações (Pinout)

### Compartilhamento SPI no Arduino (Transmissor)
O MFRC522 e o LoRa compartilham o mesmo barramento SPI, operando em 3.3V.

| Pino Arduino | Módulo LoRa | Módulo RFID (MFRC522) |
| :---: | :---: | :---: |
| **8** | - | SDA / SS |
| **7** | - | RST |
| **10** | NSS / CS | - |
| **9** | RST | - |
| **11** | MOSI | MOSI |
| **12** | MISO | MISO |
| **13** | SCK | SCK |

### Ligações na Raspberry Pi Pico (Receptor)

| Pino Pico (GP) | Módulo LoRa |
| :---: | :---: |
| **16** | MISO |
| **17** | NSS / CS |
| **18** | SCK |
| **19** | MOSI |
| **20** | RST |

---

##  Como Executar o Projeto

As instruções abaixo assumem o uso de um sistema operacional Linux (Ubuntu/Debian).

### 1. Preparação do Transmissor (Arduino)
1. Abra a IDE do Arduino.
2. Instale a biblioteca `MFRC522` via Gerenciador de Bibliotecas.
3. Conecte o Arduino, selecione a porta (`/dev/ttyUSB0`) e faça o upload do código fonte de transmissão.

### 2. Compilação do Receptor (Raspberry Pi Pico)
O código em C utiliza o SDK nativo da Pico e ferramentas de compilação via CMake.

```bash
# Clone o repositório e acesse a pasta do receptor
git clone <URL_DO_SEU_REPOSITORIO>
cd ondas_antenas/projeto_final/Lora_spi/Ondas_Antenas_Projeto_Final

# Crie a pasta de build
mkdir build && cd build

# Configure e compile o firmware
cmake ..
make -j4
```

   - Conecte a Pico segurando o botão BOOTSEL e arraste o arquivo .uf2 gerado para a unidade montada.

### 3. Execução do Painel Python

Recomenda-se o uso de um ambiente virtual (venv) para gerenciar as dependências.
```
# Ative o seu ambiente virtual
source venv_visao/bin/activate

# Instale a dependência de comunicação serial
pip install pyserial

# Garanta as permissões de leitura da porta (caso não use regras udev)
sudo chmod a+rw /dev/ttyACM0

# Inicie a interface gráfica
python3 painel_lora.py
```

## Metodologia de Coleta de Dados

1. Para os testes de campo realizados no campus:Posicione os módulos em distâncias pré-determinadas (ex: 10m, 20m, 30m).
2. Na interface Python, selecione a "Antena Original" e a distância atual.Passe a TAG RFID múltiplas vezes para gerar amostras estatísticas.
3. Troque o elemento irradiante pela "Antena Construída" (fio de cobre $\approx$ 17,3 cm) e repita o ensaio nos mesmos pontos.
4. Utilize o arquivo experimento_antenas.csv gerado para plotar os gráficos de Distância vs. RSSI.


## 👥 Equipe
