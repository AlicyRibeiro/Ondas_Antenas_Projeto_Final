"""
============================================================
 BluePASS — Firmware Raspberry Pi Pico (MicroPython)
 Recepção RFID via LoRa + Controle de Trava
============================================================

Hardware:
    Raspberry Pi Pico (RP2040)
    Módulo LoRa SX1276/SX1278 (Ra-01)
    Relé / trava no GP15
    LED onboard (GP25) usado para indicar erro fatal

Pinout:
    ── SPI0 ─────────────────────────────────────────────
    GP2 → SX1278 SCK
    GP3 → SX1278 MOSI
    GP4 → SX1278 MISO
    GP5 → SX1278 NSS (CS)
    GP6 → SX1278 RESET
    GP7 → SX1278 DIO0 (IRQ)
    3.3V → SX1278 VCC
    GND  → SX1278 GND

    ── Trava ────────────────────────────────────────────
    GP15 → Módulo relé / trava (nível alto = aberta)
============================================================

Nota sobre o formato do terminal:
    As linhas que começam com "RFID:" e os textos entre colchetes
    (ex: "[ LORA ]", "[ ACESSO ]", "[ TRAVA ]") são o formato "de
    máquina" — usado pelo painel de monitoramento (bluepass_monitor.py)
    para interpretar os eventos. Esses textos-chave são mantidos
    intactos; o que foi melhorado é a organização visual ao redor
    deles (timestamps, divisores, alinhamento e espaçamento), para
    leitura direta no console do Thonny.

Nota sobre RSSI x SNR:
    RSSI sozinho não conta a história toda em LoRa — o rádio
    consegue decodificar pacotes mesmo com sinal fraco, desde que
    o SNR (relação sinal-ruído) esteja numa faixa aceitável. Por
    isso o firmware agora lê os dois valores do rádio e mostra
    ambos lado a lado no console.
============================================================
"""

from machine import Pin, SPI
import time

# ─────────────────────────────────────────────────────────────
# Modo de diagnóstico
# ─────────────────────────────────────────────────────────────
# Deixe True para imprimir FLAGS do rádio quando mudarem, útil
# para depurar problemas de link LoRa sem precisar de um arquivo
# de teste separado.
MODO_DIAGNOSTICO = True

# ─────────────────────────────────────────────────────────────
# Configurações do sistema
# ─────────────────────────────────────────────────────────────

# UID autorizado — deve ser idêntico ao UID_AUTORIZADO do Arduino
UID_AUTORIZADO = "E0:F0:8F:E1"

# Tempo (ms) que a trava permanece aberta após acesso liberado
TEMPO_TRAVA_ABERTA_MS = 3000

# Frequência LoRa — idêntica à do Arduino
LORA_FREQUENCIA_HZ = 433_000_000

# Intervalo do heartbeat (ms)
INTERVALO_HEARTBEAT_MS = 5000

# ─────────────────────────────────────────────────────────────
# Registradores do SX1278
# ─────────────────────────────────────────────────────────────
REG_FIFO                = 0x00
REG_OP_MODE             = 0x01
REG_FRF_MSB             = 0x06
REG_FRF_MID             = 0x07
REG_FRF_LSB             = 0x08
REG_PA_CONFIG           = 0x09
REG_OCP                 = 0x0B
REG_LNA                 = 0x0C
REG_FIFO_ADDR_PTR       = 0x0D
REG_FIFO_TX_BASE_ADDR   = 0x0E
REG_FIFO_RX_BASE_ADDR   = 0x0F
REG_FIFO_RX_CURRENT     = 0x10
REG_IRQ_FLAGS           = 0x12
REG_RX_NB_BYTES         = 0x13
REG_PKT_SNR_VALUE       = 0x19
REG_PKT_RSSI_VALUE      = 0x1A
REG_MODEM_CONFIG_1      = 0x1D
REG_MODEM_CONFIG_2      = 0x1E
REG_PREAMBLE_MSB        = 0x20
REG_PREAMBLE_LSB        = 0x21
REG_PAYLOAD_LENGTH      = 0x22
REG_MODEM_CONFIG_3      = 0x26
REG_DETECTION_OPTIMIZE  = 0x31
REG_DETECTION_THRESHOLD = 0x37
REG_SYNC_WORD           = 0x39
REG_DIO_MAPPING_1       = 0x40
REG_VERSION             = 0x42
REG_PA_DAC              = 0x4D

MODO_LONG_RANGE  = 0x80
MODO_SLEEP       = 0x00
MODO_STANDBY     = 0x01
MODO_TX          = 0x03
MODO_RX_CONTINUO = 0x05

IRQ_RX_DONE = 0x40
IRQ_TX_DONE = 0x08
IRQ_CRC_ERR = 0x20

# ─────────────────────────────────────────────────────────────
# Utilitários de formatação do terminal
# ─────────────────────────────────────────────────────────────
_MOMENTO_BOOT = time.ticks_ms()
_LARGURA_DIVISOR = 52


_FRASES_HEARTBEAT = (
    "Sistema ativo, aguardando cartões...",
    "Enlace LoRa estável, nenhum evento pendente",
    "Tudo operando normalmente",
    "Escutando o rádio, trava em repouso",
)
_indice_frase_heartbeat = 0


def marca_tempo():
    """Tempo decorrido desde o boot, no formato mm:ss."""
    decorrido_s = time.ticks_diff(time.ticks_ms(), _MOMENTO_BOOT) // 1000
    return "{:02d}:{:02d}".format(decorrido_s // 60, decorrido_s % 60)


def log(mensagem, tag=None):

    if tag:
        print("[{}] [{:<7}] {}".format(marca_tempo(), tag, mensagem))
    else:
        print("[{}] {}".format(marca_tempo(), mensagem))


def divisor(caractere="-"):
    print(caractere * _LARGURA_DIVISOR)


def cabecalho(titulo):
    print()
    print("=" * _LARGURA_DIVISOR)
    print(titulo.center(_LARGURA_DIVISOR))
    print("=" * _LARGURA_DIVISOR)


def log_gui(uid="", acesso_permitido=False, rssi=0, snr=0.0):
    """
    Linha compacta 'de máquina', consumida pelo painel de monitoramento.
    Agora inclui também o SNR, além de UID/RESULT/RSSI.
    """
    resultado = "PERMITIDO" if acesso_permitido else "NEGADO"
    print("RFID: UID={}  RESULT={}  RSSI={}  SNR={:.1f}".format(
        uid, resultado, rssi, snr))


def proxima_frase_heartbeat():
    """Retorna a próxima frase da rotação, sem repetir a mesma duas vezes seguidas."""
    global _indice_frase_heartbeat
    frase = _FRASES_HEARTBEAT[_indice_frase_heartbeat]
    _indice_frase_heartbeat = (_indice_frase_heartbeat + 1) % len(_FRASES_HEARTBEAT)
    return frase


_RSSI_BOM_LIMIAR = -80
_RSSI_OK_LIMIAR = -105


def qualidade_rssi(rssi):
    """Converte o valor numérico do RSSI numa palavra fácil de ler."""
    if rssi >= _RSSI_BOM_LIMIAR:
        return "ótimo"
    if rssi >= _RSSI_OK_LIMIAR:
        return "ok"
    return "fraco"


def formatar_rssi(rssi):
    """Ex.: '-63 dBm (ótimo)' — usado nas linhas de log legíveis por humanos."""
    return "{:>4} dBm ({})".format(rssi, qualidade_rssi(rssi))


# Faixas de SNR (dB) — LoRa consegue decodificar pacotes mesmo com
# SNR negativo (é a vantagem do espalhamento espectral), então os
# limiares aqui são mais tolerantes que os de RSSI.
_SNR_OTIMO_LIMIAR = 5
_SNR_BOM_LIMIAR = 0
_SNR_ACEITAVEL_LIMIAR = -10


def qualidade_snr(snr):
    """Converte o valor numérico do SNR numa palavra fácil de ler."""
    if snr >= _SNR_OTIMO_LIMIAR:
        return "ótimo"
    if snr >= _SNR_BOM_LIMIAR:
        return "bom"
    if snr >= _SNR_ACEITAVEL_LIMIAR:
        return "aceitável"
    return "no limite"


def formatar_snr(snr):
    """Ex.: ' 9.5 dB (bom)' — usado nas linhas de log legíveis por humanos."""
    return "{:>5.1f} dB ({})".format(snr, qualidade_snr(snr))


def caixa_evento_acesso(liberado, uid, rssi, snr):

    largura = _LARGURA_DIVISOR
    status_txt = "LIBERADO \u2713" if liberado else "NEGADO  \u2717"

    titulo = " ACESSO {} ".format("LIBERADO" if liberado else "NEGADO")
    preenchimento = max(0, largura - len(titulo) - 1)
    print("┌{}{}".format(titulo, "─" * preenchimento))

    linhas = (
        ("Horário", marca_tempo()),
        ("Resultado", status_txt),
        ("UID", uid),
        ("RSSI", formatar_rssi(rssi)),
        ("SNR", formatar_snr(snr)),
    )
    for rotulo, valor in linhas:
        print("│ {:<10}: {}".format(rotulo, valor))

    print("└{}".format("─" * (largura - 1)))


class RadioSX1278:
    def __init__(self):
        self.spi = SPI(0,
                        baudrate=5_000_000,
                        polarity=0,
                        phase=0,
                        sck=Pin(2),
                        mosi=Pin(3),
                        miso=Pin(4))

        self.cs   = Pin(5, Pin.OUT, value=1)
        self.rst  = Pin(6, Pin.OUT, value=1)
        # Pull-down evita leituras espúrias caso o pino fique flutuando
        self.dio0 = Pin(7, Pin.IN, Pin.PULL_DOWN)

        self._ultimo_flags_diag = None  # usado para não repetir log de diagnóstico

    def escrever_registrador(self, registrador, valor):
        self.cs.low()
        self.spi.write(bytes([registrador | 0x80, valor]))
        self.cs.high()

    def ler_registrador(self, registrador):
        self.cs.low()
        self.spi.write(bytes([registrador & 0x7F]))
        resultado = self.spi.read(1)
        self.cs.high()
        return resultado[0]

    def definir_modo(self, modo):
        self.escrever_registrador(REG_OP_MODE, MODO_LONG_RANGE | modo)

    def iniciar(self):
        self.rst.low()
        time.sleep_ms(1)
        self.rst.high()
        time.sleep_ms(10)

        versao = self.ler_registrador(REG_VERSION)
        if versao != 0x12:
            raise RuntimeError("Módulo Lora não detectado. VERSION={}".format(hex(versao)))

        self.definir_modo(MODO_SLEEP)
        time.sleep_ms(2)

        frf = int((LORA_FREQUENCIA_HZ * (1 << 19)) / 32_000_000)
        self.escrever_registrador(REG_FRF_MSB, (frf >> 16) & 0xFF)
        self.escrever_registrador(REG_FRF_MID, (frf >> 8) & 0xFF)
        self.escrever_registrador(REG_FRF_LSB, (frf >> 0) & 0xFF)

        self.escrever_registrador(REG_PA_CONFIG, 0x8F)
        self.escrever_registrador(REG_PA_DAC, 0x87)
        self.escrever_registrador(REG_OCP, 0x3B)
        self.escrever_registrador(REG_LNA, 0x23)

        # Parâmetros de Modulação CSS
        self.escrever_registrador(REG_MODEM_CONFIG_1, 0x72) # Largura de Banda + Coding Rate
        self.escrever_registrador(REG_MODEM_CONFIG_2, 0x74) # Spreading Factor
        self.escrever_registrador(REG_MODEM_CONFIG_3, 0x04)	 # Low Data Rate Optimize

        self.escrever_registrador(REG_PREAMBLE_MSB, 0x00)
        self.escrever_registrador(REG_PREAMBLE_LSB, 0x08)
        self.escrever_registrador(REG_SYNC_WORD, 0x12)

        self.escrever_registrador(REG_FIFO_TX_BASE_ADDR, 0x00)
        self.escrever_registrador(REG_FIFO_RX_BASE_ADDR, 0x00)

        self.escrever_registrador(REG_DETECTION_OPTIMIZE, 0xC3)
        self.escrever_registrador(REG_DETECTION_THRESHOLD, 0x0A)
        self.escrever_registrador(REG_DIO_MAPPING_1, 0x00)

        self.definir_modo(MODO_STANDBY)
        time.sleep_ms(2)
        return True

    def iniciar_recepcao_continua(self):
        self.escrever_registrador(REG_DIO_MAPPING_1, 0x00)
        self.escrever_registrador(REG_FIFO_RX_BASE_ADDR, 0x00)
        self.escrever_registrador(REG_FIFO_ADDR_PTR, 0x00)
        self.escrever_registrador(REG_IRQ_FLAGS, 0xFF)
        self.definir_modo(MODO_RX_CONTINUO)
        self._ultimo_flags_diag = None

    def verificar_recepcao(self):

        flags = self.ler_registrador(REG_IRQ_FLAGS)

        if not (flags & IRQ_RX_DONE):
            return None, None, None

        self.escrever_registrador(REG_IRQ_FLAGS, 0xFF)

        if flags & IRQ_CRC_ERR:
            if MODO_DIAGNOSTICO:
                log("Pacote descartado: erro de CRC (FLAGS={})".format(hex(flags)), tag="DIAG")
            return None, None, None

        num_bytes = self.ler_registrador(REG_RX_NB_BYTES)
        if num_bytes == 0:
            if MODO_DIAGNOSTICO:
                log("RxDone sem payload — provável ruído de RF (FLAGS={})".format(hex(flags)), tag="DIAG")
            return None, None, None

        self.escrever_registrador(REG_FIFO_ADDR_PTR,
                                   self.ler_registrador(REG_FIFO_RX_CURRENT))

        dados = bytearray(num_bytes)
        for i in range(num_bytes):
            dados[i] = self.ler_registrador(REG_FIFO)

        # ── SNR ──
        snr_bruto = self.ler_registrador(REG_PKT_SNR_VALUE)
        if snr_bruto > 127:
            snr_bruto -= 256  # converte de complemento de dois (byte com sinal)
        snr = snr_bruto / 4.0

        # ── RSSI (com correção quando SNR < 0) ──
        rssi_bruto = self.ler_registrador(REG_PKT_RSSI_VALUE)
        rssi = -164 + rssi_bruto
        if snr < 0:
            rssi = rssi + snr

        return bytes(dados), rssi, snr

    def transmitir(self, dados, timeout_ms=2000):
        if len(dados) > 64:
            dados = dados[:64]

        self.definir_modo(MODO_STANDBY)
        self.escrever_registrador(REG_DIO_MAPPING_1, 0x40)
        self.escrever_registrador(REG_FIFO_ADDR_PTR, 0x00)
        self.escrever_registrador(REG_PAYLOAD_LENGTH, len(dados))

        for b in dados:
            self.escrever_registrador(REG_FIFO, b)

        self.escrever_registrador(REG_IRQ_FLAGS, 0xFF)
        self.definir_modo(MODO_TX)

        t0 = time.ticks_ms()
        while True:
            flags = self.ler_registrador(REG_IRQ_FLAGS)
            if flags & IRQ_TX_DONE:
                break
            if time.ticks_diff(time.ticks_ms(), t0) > timeout_ms:
                self.definir_modo(MODO_STANDBY)
                return False
            time.sleep_ms(2)

        self.escrever_registrador(REG_IRQ_FLAGS, IRQ_TX_DONE)
        self.definir_modo(MODO_STANDBY)
        return True


# ─────────────────────────────────────────────────────────────
# Controle da trava 
# ─────────────────────────────────────────────────────────────
class Trava:
    def __init__(self, pino_numero=15):
        self.pino = Pin(pino_numero, Pin.OUT, value=0)
        self.momento_para_fechar = 0  # timestamp para fechamento assíncrono

    def abrir(self):
        self.pino.high()
        self.momento_para_fechar = time.ticks_add(time.ticks_ms(), TEMPO_TRAVA_ABERTA_MS)

    def fechar(self):
        self.pino.low()
        self.momento_para_fechar = 0

    def atualizar(self):
        """Monitora o tempo de abertura de forma não-bloqueante."""
        if self.momento_para_fechar > 0:
            if time.ticks_diff(time.ticks_ms(), self.momento_para_fechar) >= 0:
                self.fechar()
                log("Fechada automaticamente", tag="TRAVA")

    @property
    def esta_aberta(self):
        return self.pino.value() == 1


# ─────────────────────────────────────────────────────────────
# Processa um pacote recebido do Arduino
# ─────────────────────────────────────────────────────────────
def processar_pacote(radio, trava, dados, rssi, snr):
    try:
        mensagem = dados.decode("ascii").strip()
    except Exception:
        log("Pacote inválido (não ASCII)", tag="LORA")
        return

    if not mensagem.startswith("UID:"):
        divisor()
        log("Formato desconhecido: {}".format(mensagem), tag="LORA")
        divisor()
        return

    uid = mensagem[4:]  # Extrai "XX:XX:XX:XX"

    if uid == UID_AUTORIZADO:
        # ── ACESSO LIBERADO ──
        caixa_evento_acesso(liberado=True, uid=uid, rssi=rssi, snr=snr)
        log_gui(uid=uid, acesso_permitido=True, rssi=rssi, snr=snr)

        # 1. Envia o ACK(Reconhecimento) imediatamente 
        resposta = b"ACESSO PERMITIDO\n"
        radio.transmitir(resposta)
        log("TX ACK: {}".format(resposta.decode().strip()), tag="LORA")

        # 2. Dispara a trava de forma não-bloqueante
        trava.abrir()
        log("Aberta por {} ms".format(TEMPO_TRAVA_ABERTA_MS), tag="TRAVA")

    else:
        # ── ACESSO NEGADO ──
        caixa_evento_acesso(liberado=False, uid=uid, rssi=rssi, snr=snr)
        log_gui(uid=uid, acesso_permitido=False, rssi=rssi, snr=snr)

        resposta = b"ACESSO NEGADO\n"
        radio.transmitir(resposta)
        log("TX ACK: {}".format(resposta.decode().strip()), tag="LORA")

    # Retorna ao modo RX contínuo
    radio.iniciar_recepcao_continua()


# ─────────────────────────────────────────────────────────────
# Loop principal
# ─────────────────────────────────────────────────────────────
def main():
    cabecalho("BLUEPASS · RECEPTOR")
    log("Iniciando sistema...", tag="SYS")

    radio = RadioSX1278()
    try:
        radio.iniciar()
        log("Módulo Lora inicializado — 433 MHz", tag="LORA")
        print("RFID: BOOT")
    except RuntimeError as e:
        log(str(e), tag="ERR")
        led_erro = Pin(25, Pin.OUT)
        while True:
            led_erro.toggle()
            time.sleep_ms(200)

    trava = Trava(pino_numero=15)
    log("Controlador inicializado", tag="TRAVA")

    if MODO_DIAGNOSTICO:
        log("Modo de diagnóstico ATIVO", tag="DIAG")

    divisor("=")
    log("Pronto. Aguardando pacotes do Arduino...", tag="LORA")
    divisor("=")

    momento_ultimo_heartbeat = time.ticks_ms()
    radio.iniciar_recepcao_continua()

    while True:
        # 1. Verifica se há pacotes recebidos
        dados, rssi, snr = radio.verificar_recepcao()
        if dados is not None:
            processar_pacote(radio, trava, dados, rssi, snr)

        # 2. Atualiza o estado da trava (fecha se o tempo estourar)
        trava.atualizar()

        # 3. Heartbeat periódico
        if time.ticks_diff(time.ticks_ms(), momento_ultimo_heartbeat) >= INTERVALO_HEARTBEAT_MS:
            momento_ultimo_heartbeat = time.ticks_ms()
            # Linha legível por humanos, com frase rotativa
            log(proxima_frase_heartbeat(), tag="STATUS")
            # Tag de máquina — mantida intacta para o bluepass_monitor.py
            print("RFID: HEARTBEAT")

        time.sleep_ms(10)


if __name__ == "__main__":
    main()