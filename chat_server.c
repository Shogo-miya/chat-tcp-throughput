
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100

typedef struct Client {
    int fd;
    char name[32];
    struct Client *next;
} Client;

Client *clients = NULL;
int num_clients = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Envia todos os bytes, mesmo se send() enviar parcialmente. */
int send_all(int fd, const char *data, size_t len) {
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent,
                         MSG_NOSIGNAL);

        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (n == 0)
            return -1;

        sent += (size_t)n;
    }

    return 0;
}

/* Le uma linha terminada por '\n'. */
ssize_t read_line(int fd, char *buffer, size_t capacity) {
    size_t pos = 0;

    while (pos < capacity - 1) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);

        if (n == 0)
            return pos == 0 ? 0 : (ssize_t)pos;

        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (c == '\n')
            break;

        if (c != '\r')
            buffer[pos++] = c;
    }

    buffer[pos] = '\0';
    return (ssize_t)pos;
}

/* Distribui uma mensagem aos demais clientes. */
void broadcast(const char *message, Client *sender) {
    pthread_mutex_lock(&clients_mutex);

    Client *current = clients;

    while (current != NULL) {
        if (current != sender) {
            if (send_all(current->fd, message,
                         strlen(message)) == -1) {
                fprintf(stderr,
                        "Falha ao enviar para %s.\n",
                        current->name);
            }
        }

        current = current->next;
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    Client *client = arg;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + 64];
    int registered = 0;

    /* A primeira linha recebida e o nome do usuario. */
    ssize_t n = read_line(client->fd, buffer, sizeof(buffer));

    if (n <= 0 || buffer[0] == '\0') {
        close(client->fd);
        free(client);
        return NULL;
    }

    snprintf(client->name, sizeof(client->name),
             "%s", buffer);

    pthread_mutex_lock(&clients_mutex);

    if (num_clients >= MAX_CLIENTS) {
        pthread_mutex_unlock(&clients_mutex);

        const char *error = "Servidor cheio.\n";
        send_all(client->fd, error, strlen(error));
        close(client->fd);
        free(client);
        return NULL;
    }

    client->next = clients;
    clients = client;
    num_clients++;
    registered = 1;

    pthread_mutex_unlock(&clients_mutex);

    snprintf(message, sizeof(message),
             "*** %s entrou no chat. ***\n",
             client->name);
    broadcast(message, client);

    printf("Usuario conectado: %s\n", client->name);

    while ((n = read_line(client->fd, buffer,
                          sizeof(buffer))) > 0) {
        if (strcmp(buffer, "/quit") == 0)
            break;

        snprintf(message, sizeof(message),
                 "%s: %s\n", client->name, buffer);

        broadcast(message, client);
    }

    if (n == -1)
        perror("recv");

    if (registered) {
        pthread_mutex_lock(&clients_mutex);

        Client **current = &clients;

        while (*current != NULL && *current != client)
            current = &(*current)->next;

        if (*current == client) {
            *current = client->next;
            num_clients--;
        }

        pthread_mutex_unlock(&clients_mutex);

        snprintf(message, sizeof(message),
                 "*** %s saiu do chat. ***\n",
                 client->name);
        broadcast(message, NULL);
    }

    printf("Usuario desconectado: %s\n", client->name);

    close(client->fd);
    free(client);

    return NULL;
}


/* ---------- PARTE NOVA: teste de desempenho ---------- */

long long total_bytes = 0;   /* bytes recebidos de todos os clientes */
int ativos = 0;              /* clientes ainda conectados */
pthread_mutex_t total_mutex = PTHREAD_MUTEX_INITIALIZER;

/* retorna o tempo atual em segundos */
double tempo_atual() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

/* thread que so recebe dados e vai somando os bytes */
void *receive_data(void *arg) {
    int fd = *(int *)arg;
    char buffer[65536];
    ssize_t n;

    free(arg);

    while ((n = recv(fd, buffer, sizeof(buffer), 0)) > 0) {
        pthread_mutex_lock(&total_mutex);
        total_bytes += n;
        pthread_mutex_unlock(&total_mutex);
    }

    pthread_mutex_lock(&total_mutex);
    ativos--;
    pthread_mutex_unlock(&total_mutex);

    close(fd);
    return NULL;
}

void run_test(int server_fd, int num, int warmup, int duracao) {
    int i;
    long long bytes_ini, bytes_fim;
    double t_ini, t_fim, mbps;
    int ativos_fim;

    /* espera os clientes conectarem */
    for (i = 0; i < num; i++) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1) {
            perror("accept");
            exit(1);
        }

        int *p = malloc(sizeof(int));
        *p = client_fd;

        pthread_mutex_lock(&total_mutex);
        ativos++;
        pthread_mutex_unlock(&total_mutex);

        pthread_t tid;
        pthread_create(&tid, NULL, receive_data, p);
        pthread_detach(tid);
    }

    /* tempo de aquecimento: nao conta na medicao */
    sleep(warmup);

    pthread_mutex_lock(&total_mutex);
    bytes_ini = total_bytes;
    pthread_mutex_unlock(&total_mutex);
    t_ini = tempo_atual();

    sleep(duracao);

    pthread_mutex_lock(&total_mutex);
    bytes_fim = total_bytes;
    ativos_fim = ativos;
    pthread_mutex_unlock(&total_mutex);
    t_fim = tempo_atual();

    /* bytes * 8 = bits; divide pelo tempo e por 1 milhao = Mbps */
    mbps = (bytes_fim - bytes_ini) * 8.0 / (t_fim - t_ini) / 1000000.0;

    printf("RESULTADO,%d,%lld,%.3f,%.2f,%d\n", num,
           bytes_fim - bytes_ini, t_fim - t_ini, mbps, ativos_fim);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    int server_fd;
    struct sockaddr_in server_addr;

    if (argc != 2 && argc != 6) {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        fprintf(stderr, "ou:  %s <porta> teste <clientes> <warmup> <duracao>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);

    signal(SIGPIPE, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
               &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 10) == -1) {
        perror("listen");
        close(server_fd);
        return EXIT_FAILURE;
    }

    /* modo de teste: ./chat_server 5000 teste 3 3 15 */
    if (argc == 6 && strcmp(argv[2], "teste") == 0) {
        run_test(server_fd, atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
        close(server_fd);
        return 0;
    }

    printf("Chat TCP ativo na porta %d.\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(
            server_fd,
            (struct sockaddr *)&client_addr,
            &addr_len
        );

        if (client_fd == -1) {
            if (errno == EINTR)
                continue;

            perror("accept");
            continue;
        }

        Client *client = malloc(sizeof(Client));

        if (client == NULL) {
            perror("malloc");
            close(client_fd);
            continue;
        }

        client->fd = client_fd;
        client->name[0] = '\0';
        client->next = NULL;

        pthread_t tid;

        if (pthread_create(&tid, NULL,
                           handle_client, client) != 0) {
            fprintf(stderr, "Erro ao criar thread.\n");
            close(client_fd);
            free(client);
            continue;
        }

        pthread_detach(tid);
    }

    close(server_fd);
    pthread_mutex_destroy(&clients_mutex);

    return EXIT_SUCCESS;
}
