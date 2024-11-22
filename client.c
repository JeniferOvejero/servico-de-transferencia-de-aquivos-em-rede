#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>

#define SERVER_IP "127.0.0.1"    // localhost
//#define SERVER_IP "172.20.44.6" // IP UFSC


#define SERVER_PORT 8080
#define BUFFER_SIZE 128

void send_file(int socket, const char *file_path) {
    char buffer[BUFFER_SIZE];
    FILE *file = fopen(file_path, "rb");

    if (file == NULL) {
        perror("Erro ao abrir o arquivo");
        return;
    }

    // envia o nome do arquivo
    send(socket, file_path, strlen(file_path), 0);

    // le o arquivo em blocos de 128 bytes e envia
    size_t bytes_read;
    while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) { // le e armazena no buffer
        if (send(socket, buffer, bytes_read, 0) == -1) {
            perror("Erro ao enviar dados");
            fclose(file);
            return;
        }
        printf("Enviado %zu bytes...\n", bytes_read);
    }

    printf("Transferência concluída!\n");

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <arquivo_origem> <ip_destino>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *file_path = argv[1];
    const char *server_ip = argv[2];

    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    // Configuração do endereço do servidor
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) { // converte o endereço IP do servidor para binário
        perror("Erro ao configurar o IP do servidor");
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    // conecta ao servidor
    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Erro ao conectar ao servidor");
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    printf("Conectado ao servidor %s:%d\n", server_ip, SERVER_PORT);

    // envia o arquivo
    send_file(client_sock, file_path);

    close(client_sock);

    return 0;
}
