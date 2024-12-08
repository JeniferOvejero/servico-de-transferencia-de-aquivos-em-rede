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

void progress_bar(const char *filename, int progress, int total)
{
    char spinner[] = {'|', '/', '-', '\\'};
    int spinner_index = progress % 4;               // Altera o spinner conforme o progresso
    int bar_width = 30;                             // Largura da barra de progresso
    int completed = (progress * bar_width) / total; // Quantidade de blocos completos

    // Barra de progresso
    char bar[bar_width + 1];
    for (int i = 0; i < bar_width; i++)
    {
        if (i < completed)
            bar[i] = '#';
        else
            bar[i] = ' ';
    }
    bar[bar_width] = '\0'; // Certifique-se de que é uma string terminada em '\0'

    // Imprime a linha
    printf("\r(%s) Carregando... %c %d [%s] %dbytes", filename, spinner[spinner_index], progress, bar, total);
    fflush(stdout); // Atualiza imediatamente a saída
}

void send_file(int socket, const char *file_path, const char *dest_dir)
{
    char buffer[BUFFER_SIZE];
    const char *file_name;
    size_t bytes_read;
    FILE *file = fopen(file_path, "rb");
    long file_size = 0;
    long file_part_size = 0;
    long max_transf = 0;

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

    bytes_read = recv(socket, &max_transf, sizeof(max_transf), 0);
    if (bytes_read <= 0)
    {
        perror("Erro ao receber a taxa de transferência do arquivo .part");
        return;
    }

    while ((bytes_read = fread(buffer, sizeof(char), max_transf, file)) > 0)
    {
        sleep(1);
        if (send(socket, buffer, bytes_read, 0) == -1)
        {
            perror(VERMELHO "Erro ao enviar dados" RESET);
            fclose(file);
            return;
        }
        printf("Enviado %zu bytes...\n", bytes_read);

        recv(socket, &max_transf, sizeof(max_transf), 0);
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

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s.part", dest_dir, arq_name);
    printf("Arquivo %s (%ld bytes) será copiado para: %s\n", arq_name, file_size, filepath);

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
            fflush(file);
            file_size_decrement -= bytes_read;
            file_size_count += bytes_read;
        }
        progress_bar(arq_name, file_size_count, file_size);
    }

    if (bytes_read == 0 && file_size_decrement == 0)
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
