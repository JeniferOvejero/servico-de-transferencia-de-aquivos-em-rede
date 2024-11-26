#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <poll.h>

#define RESET "\033[0m"
#define VERDE "\033[0;32m"
#define VERMELHO "\033[0;31m"
#define CIANO "\033[0;36m"
#define MAGENTA "\033[0;35m"

#define PORT 8080
#define MAX_CONN 5
#define MAX_TRANSF 640
#define BUFFER_SIZE 128

int clientes_conectados = 0;
pthread_mutex_t clientes_conectados_lock = PTHREAD_MUTEX_INITIALIZER;

void send_file(int socket)
{
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Recebe o path do arquivo a ser transferido ao cliente
    bytes_read = recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
    {
        perror(VERMELHO"Erro ao receber o path do arquivo"RESET);
        return;
    }
    buffer[bytes_read] = '\0';
    printf("Mandando o arquivo: %s\n", buffer);
    char *file_path = strdup(buffer);

    FILE *file = fopen(file_path, "rb");
    long file_size = 0;

    if (file == NULL)
    {
        perror(VERMELHO "Erro ao abrir o arquivo" RESET);
        return;
    }

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    rewind(file);

    send(socket, &file_size, sizeof(file_size), 0);

    int transfer_success = 1;
    while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
        struct pollfd pfd;
        pfd.fd = socket;
        pfd.events = POLLIN | POLLERR | POLLHUP;

        sleep(1);
        int poll_result = poll(&pfd, 1, 0); // Timeout de 0 para verificação instantânea
        if (poll_result > 0) {
            if (pfd.revents & POLLHUP) {
                printf(VERMELHO"Cliente desconectou.\n"RESET);
                transfer_success = 0;
                break;
            }
            if (pfd.revents & POLLERR) {
                printf(VERMELHO"Erro no socket.\n"RESET);
                transfer_success = 0;
                break;
            }
        }

        if (send(socket, buffer, bytes_read, 0) == -1) {
            perror(VERMELHO"Erro ao enviar dados"RESET);
            transfer_success = 0;
            break;
        }
        printf("Enviando %zu bytes...\n", bytes_read);
    }

    if (transfer_success) {
        printf(VERDE "Transferência concluída!\n" RESET);
    }
    fclose(file);
}

void receive_file(int socket)
{
    char buffer[1024];
    ssize_t bytes_read;
    char file_name[256];
    long file_size = 0;

    // Recebe o nome do arquivo
    bytes_read = recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
    {
        perror("Erro ao receber o nome do arquivo");
        return;
    }
    buffer[bytes_read] = '\0';
    printf("Recebendo arquivo: %s\n", buffer);
    char *arq_name = strdup(buffer);

    // Recebe o tamanho do arquivo
    bytes_read = recv(socket, &file_size, sizeof(file_size), 0);
    if (bytes_read <= 0)
    {
        perror("Erro ao receber o tamanho do arquivo");
        return;
    }
    printf("Tamanho do arquivo: %ld bytes\n", file_size);

    // Recebe o destino do arquivo
    bytes_read = recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
    {
        perror("Erro ao receber o destino do arquivo");
        return;
    }
    buffer[bytes_read] = '\0';
    printf("Destino do arquivo: %s\n", buffer);
    char *new_file_dir = strdup(buffer);

    // Monta o caminho completo do arquivo .part
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s.part", new_file_dir, arq_name);
    printf("Arquivo será salvo em: %s\n", filepath);

    // Verifica se existe o arquivo .part
    FILE *file = NULL;
    long part_size = 0;
    if (access(filepath, F_OK) != -1)
    {
        printf("Arquivo .part encontrado, continuando a transferência.\n");
        file = fopen(filepath, "ab");
        if (file == NULL)
        {
            perror("Erro ao abrir o arquivo existente");
            return;
        }

        fseek(file, 0, SEEK_END);
        part_size = ftell(file); // Tamanho do arquivo atual
        printf("Tamanho do arquivo .part existente: %ld bytes\n", part_size);
        printf(VERMELHO "Os primeiros %ld bytes serão ignorados na transferência.\n" RESET, part_size);
    }
    else
    {
        // Cria novo arquivo
        file = fopen(filepath, "wb");
        if (file == NULL)
        {
            perror("Erro ao criar o arquivo");
            return;
        }
    }

    long bytes_ignore = 0; // Controla quantos bytes ignorar para saber onde continuar
    while ((bytes_read = recv(socket, buffer, sizeof(buffer), 0)) > 0)
    {
        if (part_size > 0)
        {
            bytes_ignore += bytes_read;
            file_size -= bytes_read;
            if (bytes_ignore == part_size)
            {
                part_size = 0;
            }
        }
        else if (bytes_read > 0)
        {
            if (fwrite(buffer, sizeof(char), bytes_read, file) != bytes_read)
            {
                perror("Erro ao escrever no arquivo");
                fclose(file);
                return;
            }
            file_size -= bytes_read;
        }
    }

    if (bytes_read == 0 && file_size == 0)
    {
        printf(VERDE "Transferência de arquivo concluída.\n" RESET);
    }
    else
    {
        perror("Erro durante a transferência do arquivo");
    }

    fclose(file);

    // Renomeia o arquivo
    if (file_size == 0)
    {
        char final_file_path[1024];
        snprintf(final_file_path, sizeof(final_file_path), "server_arqs/%s", arq_name);
        if (rename(filepath, final_file_path) != 0)
        {
            perror("Erro ao renomear o arquivo");
        }
        else
        {
            printf("Arquivo renomeado para '%s'.\n", final_file_path);
        }
    }

}

void *handle_client(void *arg)
{
    int client_sock = *(int *)arg;
    free(arg);

    char buffer[1024];
    ssize_t bytes_read;

    // Recebe a ação do cliente (enviar ou receber arquivo)
    bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
    {
        perror("Erro ao receber a o modo de serviço.");
        close(client_sock);
        return NULL;
    }
    buffer[bytes_read] = '\0';
    printf("Ação recebida: %s\n", buffer);

    if (strcmp(buffer, "send_file") == 0)
    {
        send_file(client_sock); // Chama a função de enviar arquivo
    }
    else if (strcmp(buffer, "receive_file") == 0)
    {
        receive_file(client_sock); // Chama a função de receber arquivo
    }
    else
    {
        printf("Ação desconhecida.\n");
        close(client_sock);
    }

    pthread_mutex_lock(&clientes_conectados_lock);
    clientes_conectados--;
    pthread_mutex_unlock(&clientes_conectados_lock);
    printf(MAGENTA "Cliente desconectado.\n" RESET);
    printf(MAGENTA "Clientes online: %d.\n" RESET, clientes_conectados);

    close(client_sock);
    return NULL;
}

int main()
{
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
    {
        perror(VERMELHO "Erro ao criar socket" RESET);
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror(VERMELHO "Erro ao vincular o socket" RESET);
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, MAX_CONN) == -1)
    {
        perror(VERMELHO "Erro ao ouvir por conexões\n" RESET);
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Servidor iniciado. Aguardando conexões...\n");

    while (1)
    {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == -1)
        {
            perror(VERMELHO "Erro ao aceitar a conexão" RESET);
            continue;
        }

        pthread_mutex_lock(&clientes_conectados_lock);
        if (clientes_conectados >= MAX_CONN)
        {
            const char *error_msg = VERMELHO "Erro: limite de clientes atingido" RESET;
            send(client_sock, error_msg, strlen(error_msg), 0);
            close(client_sock);
            pthread_mutex_unlock(&clientes_conectados_lock);
            continue;
        }
        else
        {
            const char *success_msg = VERDE "Apto a transferir arquivos" RESET;
            send(client_sock, success_msg, strlen(success_msg), 0);
            pthread_mutex_unlock(&clientes_conectados_lock);
        }

        clientes_conectados++;
        pthread_mutex_unlock(&clientes_conectados_lock);

        printf(CIANO "Cliente conectado!\n" RESET);

        int *new_sock = malloc(sizeof(int));
        if (new_sock == NULL)
        {
            perror(VERMELHO "Erro ao alocar memória" RESET);
            close(client_sock);
            pthread_mutex_lock(&clientes_conectados_lock);
            clientes_conectados--;
            pthread_mutex_unlock(&clientes_conectados_lock);
            continue;
        }
        *new_sock = client_sock;

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, (void *)new_sock) != 0)
        {
            perror(VERMELHO "Erro ao criar thread" RESET);
            free(new_sock);
            close(client_sock);
            pthread_mutex_lock(&clientes_conectados_lock);
            clientes_conectados--;
            pthread_mutex_unlock(&clientes_conectados_lock);
            continue;
        }

        pthread_detach(client_thread);
    }

    close(server_sock);
    return 0;
}
