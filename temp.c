#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int createSocket();
void bindSocket(int server_socket);
void listenForConnections(int server_socket);
void handleClient(int client_socket, struct sockaddr_in client_address);
void closeConnection(int client_socket, struct sockaddr_in client_address);

int main() {
    int server_socket = createSocket();
    bindSocket(server_socket);
    listenForConnections(server_socket);

    return 0;
}

int createSocket() {
    int server_socket;
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }
    return server_socket;
}

void bindSocket(int server_socket) {
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Error binding socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
}

void listenForConnections(int server_socket) {
    if (listen(server_socket, 10) == -1) {
        perror("Error listening for connections");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_address;
        socklen_t addr_size = sizeof(client_address);
        int client_socket;

        if ((client_socket = accept(server_socket, (struct sockaddr*)&client_address, &addr_size)) == -1) {
            perror("Error accepting connection");
            continue;
        }

        printf("Connection accepted from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        handleClient(client_socket, client_address);
        closeConnection(client_socket, client_address);
    }

    // The server_socket will not be closed in this version as it will run indefinitely.
}

void handleClient(int client_socket, struct sockaddr_in client_address) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) {
        // Process the received data (you can replace this with your own logic)
        // In this example, we just echo back the data.
        send(client_socket, buffer, bytes_received, 0);
    }

    if (bytes_received == -1) {
        perror("Error receiving data");
    }
}

void closeConnection(int client_socket, struct sockaddr_in client_address) {
    close(client_socket);
    printf("Connection closed by %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
}