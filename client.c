#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>

#define RESET "\033[0m"
#define VERDE "\033[0;32m"
#define VERMELHO "\033[0;31m"
#define CIANO "\033[0;36m"
#define MAGENTA "\033[0;35m"

// #define SERVER_IP "127.0.0.1"    // localhost
// #define SERVER_IP "172.20.44.6" // IP UFSC
// #define SERVER_IP "192.168.2.126" // IP eric
#define SERVER_IP "192.168.56.1" // IP mari

#define SERVER_PORT 8080
#define BUFFER_SIZE 128

void send_file(int socket, const char *file_path, const char *file_name)
{
    char buffer[BUFFER_SIZE];
    FILE *file = fopen(file_path, "rb");
    long file_size = 0;

    if (file == NULL)
    {
        perror(VERMELHO "Erro ao abrir o arquivo" RESET);
        return;
    }

    fseek(file, 0, SEEK_END);
    file_size = ftell(file); // Tamanho do arquivo atual
    // printf("%ld\n", file_size);

    // envia o nome do arquivo
    send(socket, file_name, strlen(file_name), 0);
    sleep(1);
    // envia o tamanho final do arquivo
    send(socket, &file_size, sizeof(file_size), 0);
    rewind(file);

    // send(socket, file_path, strlen(file_path), 0);

    // le o arquivo em blocos de 128 bytes e envia
    size_t bytes_read;
    while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0)
    {             // le e armazena no buffer
        sleep(3); // teste normal
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

void receive_server_message(int socket)
{
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(socket, buffer, BUFFER_SIZE - 1, 0); // Sem MSG_DONTWAIT para bloquear até receber algo

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
    if (argc != 4)
    {
        fprintf(stderr, "Uso: %s <arquivo_origem> <ip_destino> <nome_arquivo_destivo>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *file_path = argv[1];
    const char *server_ip = argv[2];
    const char *file_name = argv[3];

    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1)
    {
        perror(VERMELHO "Erro ao criar socket" RESET);
        exit(EXIT_FAILURE);
    }

    // Configuração do endereço do servidor
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    { // converte o endereço IP do servidor para binário
        perror(VERMELHO "Erro ao configurar o IP do servidor" RESET);
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    // conecta ao servidor
    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror(VERMELHO "Erro ao conectar ao servidor" RESET);
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    printf(CIANO "Conectado ao servidor %s:%d\n" RESET, server_ip, SERVER_PORT);

    // recebe mensagem do servidor, se houver
    receive_server_message(client_sock);

    // envia o arquivo
    send_file(client_sock, file_path, file_name);

    close(client_sock);

    return 0;
}
