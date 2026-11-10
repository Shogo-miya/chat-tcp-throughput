# Coleta as informacoes da maquina para o relatorio.
# Uso: python3 info_maquina.py   (gera info_maquina.txt)
import subprocess

def rodar(comando):
    # roda um comando e devolve a saida como texto
    try:
        r = subprocess.run(comando, shell=True, capture_output=True, text=True)
        texto = r.stdout.strip()
        if texto == "":
            return "nao disponivel"
        return texto
    except Exception:
        return "nao foi possivel obter"

saida = open("info_maquina.txt", "w")

def escrever(titulo, texto):
    print("== " + titulo + " ==")
    print(texto)
    print()
    saida.write("== " + titulo + " ==\n" + texto + "\n\n")

escrever("Data", rodar("date"))
escrever("Modelo do computador",
         rodar("cat /sys/devices/virtual/dmi/id/sys_vendor /sys/devices/virtual/dmi/id/product_name 2>/dev/null"))
escrever("Processador", rodar("lscpu | grep -E 'Model name|^CPU\\(s\\)|Thread|Core|Socket|MHz'"))
escrever("Memoria RAM", rodar("free -h"))
escrever("Sistema operacional", rodar("grep PRETTY_NAME /etc/os-release"))
escrever("Kernel", rodar("uname -r"))
escrever("Compilador", rodar("gcc --version | head -1"))
escrever("Python", rodar("python3 --version"))
escrever("Governor de CPU (modo de energia)",
         rodar("cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null"))
escrever("Interface de loopback (usada no teste)", rodar("ip addr show lo | grep -E 'inet |mtu'"))
escrever("Interfaces de rede", rodar("ip -br addr"))

saida.close()
print("Pronto! Informacoes salvas em info_maquina.txt")
