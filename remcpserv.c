#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define MAX_CONN 5

int clientes_conectados = 0;
pthread_mutex_t clientes_conectados_lock = PTHREAD_MUTEX_INITIALIZER;

// função para lidar com a transferência de arquivo para um cliente
void* handle_client(void* arg) {
    return 0;
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT); // converte o nro da porta para big-endian

    // associa o socket a porta
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Erro ao vincular o socket");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // ouvir conexões de clientes
    if (listen(server_sock, MAX_CONN) == -1) {
        perror("Erro ao ouvir por conexões");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Servidor iniciado. Aguardando conexões...\n");

    
    while (1) {
        // aceita uma conexão do cliente
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock == -1) {
            perror("Erro ao aceitar a conexão");
            continue;
        }

        pthread_mutex_lock(&clientes_conectados_lock);
        if (clientes_conectados >= MAX_CONN) {
            // envia mensagem de erro e encerra a conexão
            const char *error_msg = "Erro: limite de clientes atingido";

            send(client_sock, error_msg, strlen(error_msg), 0);
            close(client_sock);
            pthread_mutex_unlock(&clientes_conectados_lock);

            continue; // pula para prox iteração
        }

        clientes_conectados ++;
        pthread_mutex_unlock(&clientes_conectados_lock);

        printf("Cliente conectado!\n");
        
        int *new_sock = malloc(sizeof(int));
        if (new_sock == NULL) {
            perror("Erro ao alocar memória");
            close(client_sock);

            pthread_mutex_lock(&clientes_conectados_lock);
            clientes_conectados--;
            pthread_mutex_unlock(&clientes_conectados_lock);

            continue; // pula para prox iteração
        }
        *new_sock = client_sock; // descritor do socket

        
        // cria thread para tratar o cliente
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, (void *)new_sock) != 0) {
            perror("Erro ao criar thread");
            free(new_sock);
            close(client_sock);

            pthread_mutex_lock(&clientes_conectados_lock);
            clientes_conectados--;
            pthread_mutex_unlock(&clientes_conectados_lock);

            continue;
        }
        
        pthread_detach(thread_id); // n espera a thread terminar, continua aceitando outros clientes
    }

    close(server_sock);
    return 0;
}
