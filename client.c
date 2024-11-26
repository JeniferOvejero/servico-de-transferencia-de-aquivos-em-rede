#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <ctype.h>

#define RESET "\033[0m"
#define VERDE "\033[0;32m"
#define VERMELHO "\033[0;31m"
#define CIANO "\033[0;36m"
#define MAGENTA "\033[0;35m"

#define SERVER_PORT 8080
#define BUFFER_SIZE 128

const char *get_filename(const char *path)
{
    const char *last_slash = strrchr(path, '/');
    return (last_slash != NULL) ? (last_slash + 1) : "";
}

void send_file(int socket, const char *file_path, const char *dest_dir)
{
    char buffer[BUFFER_SIZE];
    const char *file_name;

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

    file_name = get_filename(file_path);

    const char *service = "receive_file";

    send(socket, service, strlen(service), 0); // Envio de ação a Handle Client
    sleep(1);
    send(socket, file_name, strlen(file_name), 0);
    sleep(1);
    send(socket, &file_size, sizeof(file_size), 0);
    send(socket, dest_dir, strlen(dest_dir), 0);

    size_t bytes_read;
    while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0)
    {
        sleep(1);
        if (send(socket, buffer, bytes_read, 0) == -1)
        {
            perror(VERMELHO "Erro ao enviar dados" RESET);
            fclose(file);
            return;
        }
        printf("Enviado %zu bytes...\n", bytes_read);
    }

    printf(VERDE "Transferência concluída!\n" RESET);
    fclose(file);
}

void receive_file(int socket, const char *file_path, const char *dest_dir)
{
    char buffer[BUFFER_SIZE];
    char *arq_name;
    ssize_t bytes_read;
    long file_size = 0;

    arq_name = strdup(get_filename(file_path));
    const char *service = "send_file";

    send(socket, service, strlen(service), 0); // Envio de ação a Handle Client
    sleep(1);
    send(socket, file_path, strlen(file_path), 0);

    // Recebe o tamanho do arquivo
    bytes_read = recv(socket, &file_size, sizeof(file_size), 0);
    if (bytes_read <= 0)
    {
        perror("Erro ao receber o tamanho do arquivo");
        close(socket);
        return;
    }
    printf("Tamanho do arquivo: %ld bytes\n", file_size);

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s.part", dest_dir, arq_name);
    printf("Arquivo será salvo em: %s\n", filepath);

    FILE *file = NULL;
    long part_size = 0;

    if (access(filepath, F_OK) != -1)
    {
        printf("Arquivo .part encontrado, continuando a transferência.\n");
        file = fopen(filepath, "ab");
        if (file == NULL)
        {
            perror("Erro ao abrir o arquivo existente");
            free(arq_name);
            return;
        }

        fseek(file, 0, SEEK_END);
        part_size = ftell(file);
        printf("Tamanho do arquivo .part existente: %ld bytes\n", part_size);
        printf(VERMELHO "Os primeiros %ld bytes serão ignorados na transferência.\n" RESET, part_size);
    }
    else
    {
        file = fopen(filepath, "wb");
        if (file == NULL)
        {
            perror("Erro ao criar o arquivo");
            free(arq_name);
            return;
        }
    }

    long bytes_ignore = 0;
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
            fflush(file);
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

    if (!file_size)
    {
        char final_file_path[1024];
        snprintf(final_file_path, sizeof(final_file_path), "%s/%s", dest_dir, arq_name);
        if (rename(filepath, final_file_path) != 0)
        {
            perror("Erro ao renomear o arquivo");
        }
        else
        {
            printf("Arquivo renomeado para '%s'.\n", final_file_path);
        }
    }
    free(arq_name);
}

void receive_server_message(int socket)
{
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(socket, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';
        printf("Mensagem do servidor: %s\n", buffer);
    }
    else if (bytes_received == 0)
    {
        printf("Conexão encerrada pelo servidor.\n");
    }
    else
    {
        perror(VERMELHO "Erro ao receber mensagem do servidor" RESET);
    }
}

int main(int argc, char *argv[])
{
    int service = 0;
    const char *server_ip;
    const char *file_path;

    if (argc != 4)
    {
        fprintf(stderr, "Use SEND: %s <path_file_client> <ip_server> <path_server>\n", argv[0]);
        fprintf(stderr, "Use RECEIVE: %s <ip_server> <path_file_server> <path_local>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int has_alpha = 0;
    for (int i = 0; argv[1][i] != '\0'; i++)
    {
        if (isalpha(argv[1][i]))
        {
            has_alpha = 1;
            break;
        }
    }

    if (has_alpha)
    {
        file_path = argv[1];
        server_ip = argv[2];
        service = 1;
    }
    else
    {
        file_path = argv[2];
        server_ip = argv[1];
    }

    const char *file_new_path = argv[3];

    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1)
    {
        perror(VERMELHO "Erro ao criar socket" RESET);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        perror(VERMELHO "Erro ao configurar o IP do servidor" RESET);
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror(VERMELHO "Erro ao conectar ao servidor" RESET);
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    printf(CIANO "Conectado ao servidor %s:%d\n" RESET, server_ip, SERVER_PORT);
    receive_server_message(client_sock);

    if (service == 1)
    {
        send_file(client_sock, file_path, file_new_path);
    }
    else
    {
        receive_file(client_sock, file_path, file_new_path);
    }

    close(client_sock);
    return 0;
}
