# Le o resultados.csv, calcula media/min/max por numero de clientes e faz o grafico.
# Uso: python3 gerar_graficos.py
import statistics
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

dados = {}   # numero de clientes -> lista de Mbps
arq = open("resultados.csv")
arq.readline()   # pula o cabecalho
for linha in arq:
    p = linha.strip().split(",")
    n = int(p[0])
    ativos = int(p[5])
    if ativos != n:
        continue   # ignora medicao em que algum cliente caiu
    if n not in dados:
        dados[n] = []
    dados[n].append(float(p[4]))
arq.close()

clientes = sorted(dados.keys())
medias = []
maximos = []

print("Clientes | Repeticoes | Media (Mbps) | Desvio | Minimo | Maximo | Media por cliente")
saida = open("tabela.csv", "w")
saida.write("clientes,repeticoes,media,desvio,minimo,maximo,media_por_cliente\n")
for n in clientes:
    v = dados[n]
    media = statistics.mean(v)
    desvio = statistics.stdev(v) if len(v) > 1 else 0
    minimo = min(v)
    maximo = max(v)
    medias.append(media)
    maximos.append(maximo)
    print("%8d | %10d | %12.2f | %6.2f | %6.2f | %6.2f | %10.2f" %
          (n, len(v), media, desvio, minimo, maximo, media / n))
    saida.write("%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f\n" %
                (n, len(v), media, desvio, minimo, maximo, media / n))
saida.close()

plt.plot(clientes, medias, marker="o", label="Media")
plt.plot(clientes, maximos, marker="s", linestyle="--", label="Maximo")
plt.xlabel("Numero de clientes simultaneos")
plt.ylabel("Throughput agregado (Mbps)")
plt.title("Throughput agregado recebido pelo servidor")
plt.xticks(clientes)
plt.grid(True)
plt.legend()
plt.savefig("grafico.png")
print("Grafico salvo em grafico.png")
