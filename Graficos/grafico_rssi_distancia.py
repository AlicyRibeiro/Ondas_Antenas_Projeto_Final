"""
============================================================
 Gráfico: RSSI / SNR ao Longo do Tempo — Teste Dinâmico de Campo
 Projeto Final — Ondas e Antenas (BluePASS)
============================================================

Diferente do gráfico "RSSI vs Distância" (que exige distância física
controlada e medida com trena), este gráfico usa os dados que JÁ
FORAM COLETADOS no log real do console — sem precisar de nenhuma
medição adicional.

O que ele mostra:
    - RSSI (dBm) e SNR (dB) ao longo do tempo de teste, enquanto o
      transmissor foi sendo afastado do receptor.
    - Marcadores verticais nos momentos em que pacotes FALHARAM
      (erro de CRC ou payload inválido), extraídos diretamente do
      log de diagnóstico do firmware.

Por que isso é útil para o relatório:
    Evidencia visualmente que o enlace LoRa se manteve estável e
    decodificando pacotes válidos mesmo com SNR próximo de zero ou
    negativo, e que as falhas de pacote se concentram nos momentos
    de sinal mais degradado — uma demonstração prática da robustez
    da modulação CSS discutida na Fundamentação Teórica.

COMO USAR:
    Os dados abaixo já foram extraídos do log real que você me
    enviou. Se rodar um novo teste, basta atualizar as listas
    TEMPO_S, RSSI_DBM, SNR_DB e EVENTOS_FALHA_S.
============================================================
"""

import numpy as np
import matplotlib.pyplot as plt

# ─────────────────────────────────────────────────────────────
# 1) DADOS EXTRAÍDOS DO LOG REAL (console do Pico)
# ─────────────────────────────────────────────────────────────
# Tempo de cada leitura, em segundos desde o boot (convertido de mm:ss)
TEMPO_S = [1, 7, 17, 30, 38, 57, 77, 82, 111, 116, 127, 132, 137,
           147, 152, 157, 168, 173, 178, 184, 192, 197]

RSSI_DBM = [-70, -70, -88, -84, -102, -94, -97, -98, -99, -100,
            -103, -103, -102, -93, -102, -110.75, -103, -102, -103,
            -102, -100, -106]

SNR_DB = [10.0, 10.0, 8.5, 9.5, 4.0, 8.0, 7.8, 7.0, 6.8, 6.0,
          3.5, 5.3, 5.0, 8.3, 3.8, -1.8, 5.5, 5.8, 5.3, 4.3, 6.0, 1.3]

# Momentos (em segundos) em que pacotes falharam (CRC inválido ou
# payload não-ASCII), extraídos das linhas [DIAG]/[LORA] do log
EVENTOS_FALHA_S = [84, 86, 218, 219]


# ─────────────────────────────────────────────────────────────
# 2) Plotagem
# ─────────────────────────────────────────────────────────────
plt.rcParams.update({
    "font.size": 11,
    "axes.grid": True,
    "grid.alpha": 0.3,
})

fig, ax1 = plt.subplots(figsize=(8, 4.8))

# RSSI (eixo esquerdo)
ax1.plot(TEMPO_S, RSSI_DBM, color="#1f77b4", marker="o", markersize=4,
          linewidth=1.4, label="RSSI (dBm)")
ax1.set_xlabel("Tempo de teste (s)")
ax1.set_ylabel("RSSI (dBm)", color="#1f77b4")
ax1.tick_params(axis="y", labelcolor="#1f77b4")

# SNR (eixo direito)
ax2 = ax1.twinx()
ax2.plot(TEMPO_S, SNR_DB, color="#2ca02c", marker="s", markersize=4,
          linewidth=1.4, label="SNR (dB)")
ax2.set_ylabel("SNR (dB)", color="#2ca02c")
ax2.tick_params(axis="y", labelcolor="#2ca02c")
ax2.axhline(0, color="#2ca02c", linestyle=":", linewidth=0.8, alpha=0.6)

# Eventos de falha de pacote (linhas verticais tracejadas em vermelho)
for i, t_falha in enumerate(EVENTOS_FALHA_S):
    ax1.axvline(t_falha, color="#d62728", linestyle="--", linewidth=1.2,
                alpha=0.8, label="Falha de pacote" if i == 0 else None)

ax1.set_title("Estabilidade do Enlace Durante Teste Dinâmico de Campo")

# Legenda combinada dos dois eixos
linhas1, rotulos1 = ax1.get_legend_handles_labels()
linhas2, rotulos2 = ax2.get_legend_handles_labels()
ax1.legend(linhas1 + linhas2, rotulos1 + rotulos2, loc="lower left",
           fontsize=9)

plt.tight_layout()
plt.savefig("rssi_snr_vs_tempo.png", dpi=300)
plt.savefig("rssi_snr_vs_tempo.pdf")
plt.show()

print("Gráfico salvo como 'rssi_snr_vs_tempo.png' e 'rssi_snr_vs_tempo.pdf'")
print("Total de leituras válidas: {}".format(len(TEMPO_S)))
print("Total de falhas de pacote marcadas: {}".format(len(EVENTOS_FALHA_S)))