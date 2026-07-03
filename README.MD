# BluePASS — Sistema de Controle de Acesso RFID via LoRa

Sistema de controle de acesso que usa um cartão/tag **RFID** lido por um
Arduino, transmitido por **rádio LoRa (433 MHz)** até um **Raspberry Pi
Pico**, que decide se libera ou nega o acesso e aciona uma **trava
elétrica**. O Pico também expõe um log estruturado que pode ser lido por
um **painel gráfico no computador** (`bluepass_monitor.py`).

```
┌──────────────┐        LoRa 433MHz        ┌──────────────────┐        USB Serial       ┌────────────────┐
│   Arduino    │ ───────────────────────▶  │  Raspberry Pi     │ ───────────────────────▶ │  bluepass_      │
│  + Leitor    │                            │  Pico              │                          │  monitor.py     │
│  RFID        │ ◀─────────────────────── │  (SX1278 + Trava)  │                           │  (PC / Desktop) │
└──────────────┘      ACK (permitido/negado)└──────────────────┘                           └────────────────┘
```

---

## Sumário

- [Visão geral](#visão-geral)
- [Hardware necessário](#hardware-necessário)
- [Pinout (Raspberry Pi Pico)](#pinout-raspberry-pi-pico)
- [Como o sistema funciona](#como-o-sistema-funciona)
- [Protocolo de comunicação](#protocolo-de-comunicação)
- [Entendendo a saída do terminal](#entendendo-a-saída-do-terminal)
- [RSSI x SNR — qual a diferença e por que os dois importam](#rssi-x-snr--qual-a-diferença-e-por-que-os-dois-importam)
- [Configuração do firmware](#configuração-do-firmware)
- [Instalando e rodando o firmware no Pico](#instalando-e-rodando-o-firmware-no-pico)
- [Instalando e rodando o painel gráfico (monitor)](#instalando-e-rodando-o-painel-gráfico-monitor)
- [Solução de problemas (FAQ)](#solução-de-problemas-faq)
- [Estrutura dos arquivos](#estrutura-dos-arquivos)

---

## Visão geral

O projeto é dividido em duas partes que rodam em lugares diferentes:

| Componente | Onde roda | Linguagem | Função |
|---|---|---|---|
| `bluepass_receptor_pico.py` | **Dentro do Raspberry Pi Pico** | MicroPython | Recebe o UID do cartão via LoRa, decide se libera o acesso, aciona a trava e envia a confirmação de volta |
| `bluepass_monitor.py` | **No computador** (Windows/Mac/Linux) | Python 3 (CPython) | Lê a porta serial USB do Pico e mostra os eventos numa janela gráfica, destacando RSSI/SNR |

Esses dois arquivos **nunca rodam no mesmo interpretador**. Um erro comum
é tentar rodar o `bluepass_monitor.py` de dentro do Thonny enquanto ele
está conectado ao Pico — isso falha, porque o MicroPython do Pico não
tem `tkinter`, `threading` nem `pyserial`. Veja a seção
[Solução de problemas](#solução-de-problemas-faq).

---

## Hardware necessário

- 1x Raspberry Pi Pico (RP2040)
- 1x Módulo LoRa SX1276/SX1278 (ex.: módulo "Ra-01", 433 MHz)
- 1x Relé ou módulo de trava elétrica
- 1x Arduino (Uno/Nano/etc.) com leitor RFID (ex.: RC522) — não incluso
  neste repositório, mas é a outra ponta do enlace LoRa
- Fonte de alimentação adequada para o Pico e para a trava
- Cabo USB para alimentar/programar o Pico e para o monitor ler a serial

---

## Pinout (Raspberry Pi Pico)

### SPI0 — Módulo LoRa SX1278

| Pino do Pico | Função | Pino do SX1278 |
|---|---|---|
| GP2 | SPI SCK | SCK |
| GP3 | SPI MOSI | MOSI |
| GP4 | SPI MISO | MISO |
| GP5 | Chip Select | NSS (CS) |
| GP6 | Reset | RESET |
| GP7 | IRQ (não usado ativamente — ver nota) | DIO0 |
| 3.3V | Alimentação | VCC |
| GND | Terra | GND |

> **Nota:** o firmware faz *polling* do registrador de flags do rádio em
> vez de depender só da interrupção física em DIO0. Isso evita
> falso-negativo caso o pino IRQ não esteja bem conectado, mas o fio
> continua definido no pinout para compatibilidade futura.

### Trava / relé

| Pino do Pico | Função |
|---|---|
| GP15 | Saída para o módulo de relé/trava (nível **alto = trava aberta**) |

### Outros

| Pino do Pico | Função |
|---|---|
| GP25 (LED onboard) | Pisca continuamente se o rádio SX1278 **não** for detectado na inicialização (erro fatal) |

---

## Como o sistema funciona

1. O Arduino lê o UID de um cartão RFID e transmite via LoRa uma mensagem
   no formato `UID:XX:XX:XX:XX`.
2. O Pico está sempre em **modo de recepção contínua**, monitorando o
   rádio em polling (a cada ~10 ms).
3. Ao receber um pacote:
   - Verifica se houve erro de CRC (pacote corrompido) → descarta.
   - Verifica se o payload não está vazio (ruído de RF) → descarta.
   - Lê o **RSSI** e o **SNR** do pacote recebido.
   - Compara o UID recebido com o `UID_AUTORIZADO` configurado no firmware.
4. Se o UID bate:
   - Imprime um resumo visual do evento (**ACESSO LIBERADO**).
   - Envia de volta, via LoRa, a confirmação `ACESSO PERMITIDO`.
   - Abre a trava (GP15 em nível alto) por `TEMPO_TRAVA_ABERTA_MS` (padrão: 3000 ms).
5. Se o UID não bate:
   - Imprime o resumo visual (**ACESSO NEGADO**).
   - Envia de volta `ACESSO NEGADO`.
   - A trava permanece fechada.
6. Passado o tempo configurado, a trava fecha sozinha, de forma
   **não-bloqueante** (o loop principal continua rodando normalmente
   enquanto espera o temporizador).
7. A cada `INTERVALO_HEARTBEAT_MS` (padrão: 5000 ms), o Pico imprime uma
   linha de "pulso" confirmando que o sistema continua ativo, mesmo sem
   nenhum evento de acesso.

---

## Protocolo de comunicação

### Arduino → Pico (via LoRa)

```
UID:E0:F0:8F:E1
```

### Pico → Arduino (via LoRa, resposta/ACK)

```
ACESSO PERMITIDO
```
ou
```
ACESSO NEGADO
```

### Pico → Console/Monitor (via USB serial)

Linhas **legíveis por humanos** (com timestamp e tag), usadas só para
acompanhar no Thonny:

```
[00:05] [LORA   ] TX ACK: ACESSO PERMITIDO
[00:08] [TRAVA  ] Fechada automaticamente
```

Linhas **"de máquina"**, sem timestamp, começando com `RFID:` — usadas
pelo `bluepass_monitor.py` (ou qualquer outro painel externo) para
interpretar os eventos automaticamente:

```
RFID: BOOT
RFID: HEARTBEAT
RFID: UID=E0:F0:8F:E1  RESULT=PERMITIDO  RSSI=-70  SNR=9.5
```

> Essas linhas "de máquina" são um contrato entre o firmware e o
> monitor — se você mudar o formato de uma, precisa atualizar o parser
> (regex) do outro lado também.

---

## Entendendo a saída do terminal

### Cabeçalho e boot

```
====================================================
                BLUEPASS · RECEPTOR
====================================================
[00:00] [SYS    ] Iniciando sistema...
[00:00] [LORA   ] SX1278 inicializado — 433 MHz
RFID: BOOT
[00:00] [TRAVA  ] Controlador inicializado
[00:00] [DIAG   ] Modo de diagnóstico ATIVO
```

- `[00:00]` → tempo decorrido desde o boot do Pico (mm:ss), **não** é
  hora do relógio.
- `[SYS]`, `[LORA]`, `[TRAVA]`, `[DIAG]`, `[ACESSO]`, `[STATUS]` → a
  origem/categoria da mensagem.
- `RFID: BOOT` → aviso de máquina de que o rádio subiu com sucesso.
- `Modo de diagnóstico ATIVO` → quando `MODO_DIAGNOSTICO = True`,
  mensagens extras aparecem para pacotes descartados por erro de CRC ou
  por virem vazios (ruído de RF). Útil para depurar o enlace LoRa sem
  precisar de um script de teste separado.

### Heartbeat (pulsação periódica)

```
[00:05] [STATUS ] Sistema ativo, aguardando cartões...
RFID: HEARTBEAT
```

Confirma, em intervalos regulares, que o sistema não travou — mesmo sem
nenhum cartão ter sido lido.

### Evento de acesso

```
┌ ACESSO LIBERADO ──────────────────────────────────
│ Horário   : 00:05
│ Resultado : LIBERADO ✓
│ UID       : E0:F0:8F:E1
│ RSSI      :  -70 dBm (ótimo)
│ SNR       :   9.5 dB (ótimo)
└───────────────────────────────────────────────────
RFID: UID=E0:F0:8F:E1  RESULT=PERMITIDO  RSSI=-70  SNR=9.5
[00:05] [LORA   ] TX ACK: ACESSO PERMITIDO
[00:05] [TRAVA  ] Aberta por 3000 ms
[00:08] [TRAVA  ] Fechada automaticamente
```

- **UID** — identificador do cartão lido pelo Arduino.
- **RSSI** — força bruta do sinal recebido, em dBm.
- **SNR** — relação sinal-ruído, em dB (veja a seção dedicada abaixo).
- **TX ACK** — confirma que o Pico respondeu de volta ao Arduino.
- **TRAVA** — mostra abertura, duração e fechamento automático.

---

## RSSI x SNR — qual a diferença e por que os dois importam

Em rádios comuns, RSSI já seria suficiente para avaliar a qualidade do
link. **Em LoRa isso não é verdade**, porque a modulação por
espalhamento espectral (*chirp spread spectrum*) permite decodificar
pacotes mesmo quando o sinal está **abaixo do nível de ruído** — algo
que a maioria das outras tecnologias de rádio não consegue.

| Métrica | O que mede | Faixa típica útil |
|---|---|---|
| **RSSI** (Received Signal Strength Indicator) | Força bruta do sinal recebido, em dBm. Quanto mais perto de 0, mais forte. | -30 a -130 dBm |
| **SNR** (Signal-to-Noise Ratio) | O quanto o sinal está acima (ou abaixo) do ruído de fundo, em dB. LoRa tolera valores bem negativos. | +10 a -20 dB, dependendo do Spreading Factor |

### Classificação usada no firmware

**RSSI** (`qualidade_rssi()`):
| Faixa | Rótulo |
|---|---|
| ≥ -80 dBm | ótimo |
| -80 a -105 dBm | ok |
| < -105 dBm | fraco |

**SNR** (`qualidade_snr()`):
| Faixa | Rótulo |
|---|---|
| ≥ 5 dB | ótimo |
| 0 a 5 dB | bom |
| -10 a 0 dB | aceitável |
| < -10 dB | no limite |

### Por que olhar os dois juntos

- Um RSSI "fraco" (ex.: -110 dBm) com **SNR positivo** pode ser um link
  perfeitamente saudável — é só um transmissor distante, mas limpo.
- Um RSSI "ótimo" (ex.: -60 dBm) com **SNR muito negativo** pode indicar
  interferência forte no ambiente, mesmo com o transmissor por perto.
- Por isso o firmware lê e mostra **ambos** lado a lado em cada evento,
  em vez de decidir a qualidade do link só pelo RSSI.

### Correção aplicada ao RSSI

O SX1278 guarda o SNR (`RegPktSnrValue`, registrador `0x19`) como um
byte com sinal, em passos de 0,25 dB. Quando o SNR é negativo (pacote
decodificado abaixo do ruído), o valor bruto de RSSI subestima a
potência real — por isso o firmware soma o SNR ao RSSI nesse caso,
seguindo a correção recomendada pela Semtech para a porta de baixa
frequência (<525 MHz), onde este módulo opera (433 MHz).

---

## Configuração do firmware

Todas as opções ajustáveis ficam no topo do arquivo
`bluepass_receptor_pico.py`:

```python
MODO_DIAGNOSTICO = True          # Liga/desliga logs extras de depuração do rádio
UID_AUTORIZADO = "E0:F0:8F:E1"   # UID do cartão liberado (deve bater com o do Arduino)
TEMPO_TRAVA_ABERTA_MS = 3000     # Quanto tempo a trava fica aberta após liberar acesso
LORA_FREQUENCIA_HZ = 433_000_000 # Frequência do rádio (deve ser igual à do Arduino)
INTERVALO_HEARTBEAT_MS = 5000    # De quanto em quanto tempo o heartbeat é impresso
```

---

## Instalando e rodando o firmware no Pico

1. Abra o **Thonny**.
2. No canto inferior direito, selecione o interpretador
   **"MicroPython (Raspberry Pi Pico)"**.
3. Abra o arquivo `bluepass_receptor_pico.py`.
4. Rode com **F5** ou o botão "Run".
5. O console do Thonny deve mostrar o cabeçalho `BLUEPASS · RECEPTOR` e,
   em seguida, `Pronto. Aguardando pacotes do Arduino...`.

Se aparecer:
```
RuntimeError: SX1278 não detectado. VERSION=0x...
```
verifique a fiação do SPI (GP2–GP7) e a alimentação de 3.3V do módulo
LoRa. O LED onboard (GP25) vai piscar continuamente enquanto esse erro
persistir.

---

## Instalando e rodando o painel gráfico (monitor)

O `bluepass_monitor.py` **não roda no Pico** — ele é um programa comum de
Python que roda no seu computador e lê a porta serial USB.

1. Instale a dependência (uma vez só):
   ```bash
   pip install pyserial
   ```
2. Conecte o Pico ao computador via USB, com o firmware já rodando.
3. No terminal do seu sistema operacional (não no Thonny), rode:
   ```bash
   python bluepass_monitor.py
   ```
4. Na janela que abrir, escolha a porta serial correspondente ao Pico e
   clique em **Conectar**.

A janela mostra:
- Medidor visual de **RSSI**.
- Último evento (**LIBERADO**/**NEGADO**), UID e horário.
- Contadores de liberados/negados.
- Indicador de heartbeat (pisca a cada pulso recebido).
- Log bruto da serial, com o trecho `RSSI=... dBm` destacado.

---

## Solução de problemas (FAQ)

**"ImportError: no module named 'threading'" ao rodar o monitor**
Você tentou rodar `bluepass_monitor.py` com o interpretador MicroPython
do Pico selecionado no Thonny. Troque para **"Local Python 3"** no
seletor de interpretador do Thonny, ou rode o arquivo direto num
terminal do sistema (`python bluepass_monitor.py`), fora do Thonny.

**"RuntimeError: SX1278 não detectado"**
Problema de fiação SPI ou alimentação do módulo LoRa. Confira o pinout
na seção correspondente deste README.

**"Por que o console fica imprimindo HEARTBEAT sem parar?"**
É esperado — é o firmware confirmando periodicamente que está ativo,
mesmo sem eventos de acesso. Ajuste `INTERVALO_HEARTBEAT_MS` se quiser
espaçar mais essas mensagens, ou remova o bloco correspondente no loop
principal se preferir suprimi-las.

**"KeyboardInterrupt" no console do Pico**
Não é um erro — é o efeito de apertar Ctrl+C ou o botão "Stop" no
Thonny, interrompendo manualmente o `while True:` do loop principal.

**O monitor não lista nenhuma porta serial**
Confirme que o Pico está conectado via USB e que o driver serial do
sistema operacional reconheceu a placa. No Windows, verifique o
Gerenciador de Dispositivos; no Linux/Mac, rode `ls /dev/tty*` (Mac) ou
`ls /dev/ttyACM*` (Linux) com o Pico plugado.

---

## Estrutura dos arquivos

```
.
├── bluepass_receptor_pico.py   # Firmware MicroPython — roda DENTRO do Pico
├── bluepass_monitor.py         # Painel gráfico — roda NO COMPUTADOR
└── README.md                   # Este arquivo
```