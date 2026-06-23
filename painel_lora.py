import tkinter as tk
import serial 

# ================= CONFIGURAÇÃO DA PORTA =================
PORTA = '/dev/ttyACM0'  # Altere para a porta em que a Pico está conectada!
BAUD_RATE = 115200

# Variável global de conexão
conexao = None

try:
    conexao = serial.Serial(PORTA, BAUD_RATE, timeout=0.1)
    print(f"Conectado à Pico na porta {PORTA}")
except Exception as e:
    print(f"ALERTA: Não foi possível abrir a porta {PORTA}. Verifique se a IDE do Arduino não está usando ela.")

# ================= CONFIGURAÇÃO DA JANELA =================
janela = tk.Tk()
janela.title("Painel de Telemetria LoRa - Controle de Acesso")
janela.geometry("500x350")
janela.configure(bg="#1e272e") # Cor de fundo (Dark mode)

# Variáveis que vão mudar na tela
var_tag = tk.StringVar(value="Aguardando leitura...")
var_rssi = tk.StringVar(value="-- dBm")
var_snr = tk.StringVar(value="-- dB")

# ================= LAYOUT (TÍTULOS E VALORES) =================
# Título Principal
tk.Label(janela, text="SISTEMA LORA - RF", font=("Helvetica", 20, "bold"), fg="#00d8d6", bg="#1e272e").pack(pady=15)

# Bloco da TAG
tk.Label(janela, text="Última TAG Recebida:", font=("Helvetica", 12), fg="#808e9b", bg="#1e272e").pack()
tk.Label(janela, textvariable=var_tag, font=("Helvetica", 24, "bold"), fg="#0be881", bg="#1e272e").pack(pady=5)

# Bloco do RSSI (Potência)
tk.Label(janela, text="Potência do Sinal Recebido (RSSI):", font=("Helvetica", 12), fg="#808e9b", bg="#1e272e").pack()
tk.Label(janela, textvariable=var_rssi, font=("Helvetica", 24, "bold"), fg="#ff3f34", bg="#1e272e").pack(pady=5)

# Bloco do SNR (Ruído)
tk.Label(janela, text="Qualidade do Sinal (SNR):", font=("Helvetica", 12), fg="#808e9b", bg="#1e272e").pack()
tk.Label(janela, textvariable=var_snr, font=("Helvetica", 24, "bold"), fg="#ffd32a", bg="#1e272e").pack(pady=5)

# ================= FUNÇÃO DE LEITURA (LOOP) =================
def ler_dados_da_pico():
    if conexao and conexao.in_waiting > 0:
        try:
            # Lê a linha que chegou da Pico
            linha = conexao.readline().decode('utf-8').strip()
            
            # Filtra e extrai os dados exatos baseados nos 'printf' do C
            if "DADO RECEBIDO :" in linha:
                dado_limpo = linha.replace("DADO RECEBIDO :", "").strip()
                var_tag.set(dado_limpo)
                
            elif "POTENCIA (RSSI):" in linha:
                rssi_limpo = linha.replace("POTENCIA (RSSI):", "").strip()
                var_rssi.set(rssi_limpo)
                
            elif "RUIDO (SNR)" in linha:
                snr_limpo = linha.replace("RUIDO (SNR)    :", "").replace("RUIDO (SNR):", "").strip()
                var_snr.set(snr_limpo)
                
        except Exception as e:
            pass # Ignora erros de decodificação momentâneos (ruído na serial)
            
    # Agenda esta mesma função para rodar novamente em 50 milissegundos
    janela.after(50, ler_dados_da_pico)

# ================= INÍCIO DA EXECUÇÃO =================
if conexao is None:
    var_tag.set("ERRO DE PORTA SERIAL")
else:
    # Inicia a leitura contínua
    janela.after(50, ler_dados_da_pico)

# Mantém a janela aberta
janela.mainloop()