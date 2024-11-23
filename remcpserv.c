#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define RESET "\033[0m"
#define VERDE "\033[0;32m"
#define VERMELHO "\033[0;31m"
#define CIANO "\033[0;36m"
#define MAGENTA "\033[0;35m"

#define PORT 8080
#define MAX_CONN 5

int clientes_conectados = 0;
pthread_mutex_t clientes_conectados_lock = PTHREAD_MUTEX_INITIALIZER;

// transferência de arquivo para um cliente
void* handle_client(void* arg) {
    int client_sock = *(int *)arg;
    free(arg);

    char buffer[1024];
    ssize_t bytes_read;

    // le dados
    while ((bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0'; // garante que o buffer seja uma string válida
        printf("Recebido: %s\n", buffer);
    }

    printf(MAGENTA "Cliente desconectado.\n" RESET);
    close(client_sock);
    return NULL;
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror(VERMELHO"Erro ao criar socket"RESET);
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT); // converte o nro da porta para big-endian

    // associa o socket a porta
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror(VERMELHO"Erro ao vincular o socket"RESET);
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // ouvir conexões de clientes
    if (listen(server_sock, MAX_CONN) == -1) {
        perror(VERMELHO"Erro ao ouvir por conexões\n"RESET);
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Servidor iniciado. Aguardando conexões...\n");

    
    while (1) {
        // aceita uma conexão do cliente
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock == -1) {
            perror(VERMELHO"Erro ao aceitar a conexão"RESET);
            continue;
        }

        pthread_mutex_lock(&clientes_conectados_lock);
        if (clientes_conectados >= MAX_CONN) {
            // envia mensagem de erro e encerra a conexão
            const char *error_msg = VERMELHO"Erro: limite de clientes atingido"RESET;

            send(client_sock, error_msg, strlen(error_msg), 0);
            close(client_sock);
            pthread_mutex_unlock(&clientes_conectados_lock);

            continue; // pula para prox iteração
        }else{
            const char *success_msg = VERDE"Apto a receber arquivos"RESET;
            send(client_sock, success_msg, strlen(success_msg), 0);
            pthread_mutex_unlock(&clientes_conectados_lock);
        }

        clientes_conectados ++;
        pthread_mutex_unlock(&clientes_conectados_lock);

        printf(CIANO"Cliente conectado!\n"RESET);
        
        int *new_sock = malloc(sizeof(int));
        if (new_sock == NULL) {
            perror(VERMELHO"Erro ao alocar memória"RESET);
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
            perror(VERMELHO"Erro ao criar thread"RESET);
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
