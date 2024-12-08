#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <poll.h>

#define RESET "\033[0m"
#define VERDE "\033[0;32m"
#define VERMELHO "\033[0;31m"
#define CIANO "\033[0;36m"
#define MAGENTA "\033[0;35m"

#define PORT 8080
#define MAX_CONN 5
#define MAX_TRANSF 300
#define BUFFER_SIZE 128

int clientes_conectados = 0;
pthread_mutex_t clientes_conectados_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t print_mutex;

typedef struct
{
    int thread_id;
    int socket;
} ThreadArgs;

void move_cursor(int row, int col)
{
    printf("\033[%d;%dH", row, col);
}

void progress_bar(int thread_id, const char *filename, int progress, int total_size)
{
    int bar_width = 30;
    char bar[bar_width + 1];
    int filled = (progress * bar_width) / total_size;

    memset(bar, '#', filled);
    memset(bar + filled, ' ', bar_width - filled);
    bar[bar_width] = '\0';

    pthread_mutex_lock(&print_mutex);

    move_cursor(thread_id + 20, 1);
    printf("(%s) Carregando... %c %d [%s] %dbytes/%dbytes    ",
           filename,
           "|/-\\"[progress % 4], // Spinner animado
           progress,
           bar,
           progress,
           total_size);
    fflush(stdout);

    pthread_mutex_unlock(&print_mutex);
}

void send_file(int socket)
{
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    long file_part_size = 0;

    // Recebe o path do arquivo a ser transferido ao cliente
    bytes_read = recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
    {
        perror(VERMELHO "Erro ao receber o path do arquivo" RESET);
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
    // sleep(1);

    bytes_read = recv(socket, &file_part_size, sizeof(file_part_size), 0);
    if (bytes_read <= 0)
    {
        perror("Erro ao receber o tamanho do arquivo .part");
        return;
    }

    if (file_part_size > 1)
    {
        printf("Tamanho do arquivo .part encontrado: %ld bytes\n", file_part_size);
        fseek(file, file_part_size, SEEK_SET);
    }

    int transfer_success = 1;
    while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0)
    {
        struct pollfd pfd;
        pfd.fd = socket;
        pfd.events = POLLIN | POLLERR | POLLHUP;

        sleep(1);
        int poll_result = poll(&pfd, 1, 0); // Timeout de 0 para verificação instantânea
        if (poll_result > 0)
        {
            if (pfd.revents & POLLHUP)
            {
                printf(VERMELHO "Cliente desconectou.\n" RESET);
                transfer_success = 0;
                break;
            }
            if (pfd.revents & POLLERR)
            {
                printf(VERMELHO "Erro no socket.\n" RESET);
                transfer_success = 0;
                break;
            }
        }

        if (send(socket, buffer, bytes_read, 0) == -1)
        {
            perror(VERMELHO "Erro ao enviar dados" RESET);
            transfer_success = 0;
            break;
        }
        printf("Enviando %zu bytes...\n", bytes_read);
    }

    if (transfer_success)
    {
        printf(VERDE "Transferência concluída!\n" RESET);
    }
    fclose(file);
}

void receive_file(ThreadArgs *args)
{
    char buffer[1024];
    ssize_t bytes_read;
    long file_size = 0;
    int thread_id = args->thread_id;
    int socket = args->socket;

    // Recebe o nome do arquivo
    bytes_read = recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
    {
        perror("Erro ao receber o nome do arquivo");
        return;
    }
    buffer[bytes_read] = '\0';
    char *arq_name = strdup(buffer);

    // Recebe o tamanho do arquivo
    bytes_read = recv(socket, &file_size, sizeof(file_size), 0);
    if (bytes_read <= 0)
    {
        perror("Erro ao receber o tamanho do arquivo");
        return;
    }

    // Recebe o destino do arquivo
    bytes_read = recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
    {
        perror("Erro ao receber o destino do arquivo");
        return;
    }
    buffer[bytes_read] = '\0';
    char *new_file_dir = strdup(buffer);

    // Monta o caminho completo do arquivo .part
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s.part", new_file_dir, arq_name);
    printf("Arquivo %s (%ld bytes) será copiado para: %s\n", arq_name, file_size, filepath);

    // Verifica se existe o arquivo .part
    FILE *file = NULL;
    long part_size = 0;

    if (access(filepath, F_OK) != -1)
    {
        file = fopen(filepath, "ab");
        if (file == NULL)
        {
            perror("Erro ao abrir o arquivo existente");
            return;
        }

        fseek(file, 0, SEEK_END);
        part_size = ftell(file); // Tamanho do arquivo .part
        printf(VERMELHO "Arquivo .part de %ld bytes existente.\n" RESET, part_size);
        // Envio do tamanho ao cliente (usando o endereço de part_size)
        if (send(socket, &part_size, sizeof(part_size), 0) == -1)
        {
            perror("Erro ao enviar part_size ao cliente");
            return;
        }
    }
    else
    {
        file = fopen(filepath, "wb");
        if (file == NULL)
        {
            perror("Erro ao criar o arquivo");
            return;
        }

        int signal = 1;
        // Envio do sinal para indicar que não há arquivo .part
        if (send(socket, &signal, sizeof(signal), 0) == -1)
        {
            perror("Erro ao enviar sinal ao cliente");
            return;
        }
    }

    long max_transf = MAX_TRANSF / clientes_conectados;
    printf("Max transf: %ld\n", max_transf);

    long file_size_count = 0;
    long file_size_decrement = 0;
    if (part_size > 0)
    {
        file_size_decrement = file_size - part_size;
        file_size_count = part_size;
    }
    else
    {
        file_size_decrement = file_size;
        file_size_count = 0;
    }

    if (send(socket, &max_transf, sizeof(max_transf), 0) == -1)
    {
        perror("Erro ao enviar taxa de transferência ao cliente");
        return;
    }

    while ((bytes_read = recv(socket, buffer, sizeof(buffer), 0)) > 0)
    {
        if (bytes_read > 0)
        {
            if (fwrite(buffer, sizeof(char), bytes_read, file) != bytes_read)
            {
                perror("Erro ao escrever no arquivo");
                fclose(file);
                return;
            }
            file_size_decrement -= bytes_read;
            file_size_count += bytes_read;
            progress_bar(thread_id, arq_name, file_size_count, file_size);
            fflush(file);
        }
        max_transf = MAX_TRANSF / clientes_conectados;
        send(socket, &max_transf, sizeof(max_transf), 0);
    }

    if (bytes_read == 0)
    {
        printf(VERDE "\nTransferência de arquivo concluída.\n" RESET);
    }
    else
    {
        perror("\nErro durante a transferência do arquivo");
    }

    fclose(file);

    if (!file_size_decrement)
    {
        char final_file_path[1024];
        snprintf(final_file_path, sizeof(final_file_path), "%s/%s", new_file_dir, arq_name);
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
    ThreadArgs *args = (ThreadArgs *)arg;
    int thread_id = args->thread_id;
    int client_sock = args->socket; // Só para mostrar como acessar, não será usado
    (void)socket;

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
        receive_file(args); // Chama a função de receber arquivo
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
    ThreadArgs thread_args[MAX_CONN];

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
        thread_args[clientes_conectados].thread_id = clientes_conectados;
        thread_args[clientes_conectados].socket = *new_sock;
        if (pthread_create(&client_thread, NULL, handle_client, &thread_args[clientes_conectados]) != 0)
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
