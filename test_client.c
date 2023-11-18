#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define NMIPADDRESS "127.0.0.2"
#define SERVER_PORT 4000

int main() {
    printf("Start\n");
    struct sockaddr_in serverAddr;
    int sock;
    const char* message = "COPY ./src/hello ./src/jainit\n";

    printf("Starting client...\n");

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Could not create socket");
        return 1;
    }

    printf("Socket created.\n");
    serverAddr.sin_addr.s_addr = inet_addr(NMIPADDRESS);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);

    printf("Connecting to %s:%d\n", NMIPADDRESS, SERVER_PORT);

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connect failed. Error");
        return 1;
    }

    printf("Connected to the server.\n");

    if (send(sock, message, strlen(message), 0) < 0) {
        perror("Send failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Message sent. Waiting for response...\n");

    // Receiving response from server
    char buffer[1024];
    int totalRead = 0;
    while (1) {
        int readSize = recv(sock, buffer + totalRead, sizeof(buffer) - totalRead - 1, 0);
        if (readSize > 0) {
            totalRead += readSize;
            buffer[totalRead] = '\0';
            if (totalRead > 0 && buffer[totalRead - 1] == '\n') { // End of message character
                break;
            }
        } else if (readSize == 0) {
            printf("Server closed the connection\n");
            break;
        } else {
            perror("recv failed");
            break;
        }
    }

    printf("Server reply: %s\n", buffer);

    close(sock);
    printf("Disconnected from server.\n");

    return 0;
}
