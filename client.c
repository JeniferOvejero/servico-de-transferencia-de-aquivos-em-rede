#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 12345

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[1024];

    // Cria o socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    // Configura o endereço do servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // Conecta ao servidor
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Erro ao conectar ao servidor");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Conectado ao servidor!\n");

    // Comunicação
    char *message = "Olá, servidor!";
    send(sock, message, strlen(message), 0);

    recv(sock, buffer, sizeof(buffer), 0);
    printf("Resposta do servidor: %s\n", buffer);

    // Fecha o socket
    close(sock);

    return 0;
}
