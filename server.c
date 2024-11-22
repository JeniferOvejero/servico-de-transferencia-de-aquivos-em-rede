#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 12345

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    char buffer[1024];
    socklen_t client_len = sizeof(client_addr);

    // Cria o socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    // Configura o endereço
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY; // aceita conexões de qualquer interface de rede - não precisa saber o IP

    // Associa o socket ao endereço
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Erro no bind");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Escuta conexões
    if (listen(server_sock, 5) == -1) {
        perror("Erro no listen");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Servidor escutando na porta %d...\n", PORT);

    // Aceita conexões
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
    if (client_sock == -1) {
        perror("Erro no accept");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Cliente conectado!\n");

    // Comunicação
    recv(client_sock, buffer, sizeof(buffer), 0);
    printf("Mensagem recebida: %s\n", buffer);

    char *response = "Olá, cliente!";
    send(client_sock, response, strlen(response), 0);

    // Fecha os sockets
    close(client_sock);
    close(server_sock);

    return 0;
}
