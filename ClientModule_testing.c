#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_PORT 12345
#define BUFFER_SIZE 1024

// Function to handle each client request
void handleClientRequest(int clientSocket) {
    char buffer[BUFFER_SIZE];
    int bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
    if (bytesRead < 0) {
        perror("Error reading from socket");
        return;
    }

    buffer[bytesRead] = '\0';
    printf("Received request: %s\n", buffer);

    // Simple response based on the request code
    char* response;
    if (strncmp(buffer, "1:", 2) == 0) {  // Read request
        response = "Read operation successful";
    } else if (strncmp(buffer, "2:", 2) == 0) {  // Write request
        response = "Write operation successful";
    } else if (strncmp(buffer, "3:", 2) == 0) {  // Info request
        response = "Info operation successful";
    } else if (strncmp(buffer, "4:", 2) == 0) {  // Create request
        response = "Create operation successful";
    } else if (strncmp(buffer, "5:", 2) == 0) {  // Delete request
        response = "Delete operation successful";
    } else if (strncmp(buffer, "6:", 2) == 0) {  // Copy request
        response = "Copy operation successful";
    } else {
        response = "Unknown operation";
    }

    send(clientSocket, response, strlen(response), 0);
}

int main() {
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddress, clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);

    // Create a socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to an address
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(SERVER_PORT);

    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("Socket bind failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(serverSocket, 5) < 0) {
        perror("Error in listen");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    printf("Server is listening on port %d...\n", SERVER_PORT);

    // Accept incoming connections and handle them
    while (1) {
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLength);
        if (clientSocket < 0) {
            perror("Error in accept");
            continue;
        }

        handleClientRequest(clientSocket);
        close(clientSocket);
    }

    close(serverSocket);
    return 0;
}
