#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "192.168.1.1" // Replace with your server's IP address
#define SERVER_PORT 8001       // Replace with the port your server is listening on

void sendCommand(int sockfd, const char* command) {
    if (send(sockfd, command, strlen(command), 0) < 0) {
        perror("Send failed");
        exit(EXIT_FAILURE);
    }
}

int main() {
    struct sockaddr_in serverAddr;
    int sock;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Could not create socket");
        return 1;
    }

    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connect failed. Error");
        return 1;
    }

    printf("Connected to the server.\n");

    // Sending a series of test commands to the server
    sendCommand(sock, "READ /path/to/file\n");
    sendCommand(sock, "WRITE /path/to/otherfile Some data to write\n");
    // ... Add more commands as needed for testing

    printf("Commands sent.\n");

    // Close the socket
    close(sock);
    printf("Disconnected from server.\n");

    return 0;
}
