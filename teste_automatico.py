# Teste automatico do chat TCP: roda de 1 a 10 clientes e salva os resultados.
# Uso: python3 teste_automatico.py
import subprocess
import time

IP = "127.0.0.1"
PORTA = 5000
BLOCO = 65536        # tamanho do bloco que cada cliente envia (bytes)
WARMUP = 3           # segundos descartados no inicio
DURACAO = 15         # segundos de medicao
REPETICOES = 5
MAX_CLIENTES = 10

arq = open("resultados.csv", "w")
arq.write("clientes,repeticao,bytes,tempo,mbps,ativos\n")

for n in range(1, MAX_CLIENTES + 1):
    for rep in range(1, REPETICOES + 1):
        print("Testando", n, "cliente(s) - repeticao", rep)

        # sobe o servidor em modo teste
        servidor = subprocess.Popen(
            ["./chat_server", str(PORTA), "teste", str(n), str(WARMUP), str(DURACAO)],
            stdout=subprocess.PIPE, text=True)
        time.sleep(1)

        # sobe os n clientes (eles ficam mandando dados um pouco alem da medicao)
        clientes = []
        for i in range(n):
            c = subprocess.Popen(
                ["./chat_client", IP, str(PORTA), "teste", str(BLOCO), str(WARMUP + DURACAO + 5)],
                stdout=subprocess.DEVNULL)
            clientes.append(c)

        # espera o servidor terminar e le a linha do resultado
        saida, erro = servidor.communicate()
        for linha in saida.split("\n"):
            if linha.startswith("RESULTADO"):
                p = linha.split(",")
                # p = RESULTADO, clientes, bytes, tempo, mbps, ativos
                arq.write("%s,%d,%s,%s,%s,%s\n" % (p[1], rep, p[2], p[3], p[4], p[5]))
                arq.flush()
                print("   ->", p[4], "Mbps")
                if int(p[5]) != n:
                    print("   (aviso: algum cliente caiu, resultado suspeito)")

        # encerra os clientes
        for c in clientes:
            c.terminate()
            c.wait()

        time.sleep(2)

arq.close()
print("Pronto! Resultados em resultados.csv")
