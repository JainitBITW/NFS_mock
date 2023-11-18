#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define NAMING_SERVER_PORT 8001
#define IP_ADDRESS "127.0.0.1"

char request[100]; // global variable to take input of from client

// read function when client requests to read a file
void clientRead(int clientSocket)
{

    // send the request to the Naming Server
    send(clientSocket, request, strlen(request), 0);

    // receive the response from the Naming Server
    char response[100];
    recv(clientSocket, response, sizeof(response), 0);

    printf("Response from Naming Server: %s\n", response);

    // tokenising the response into a token array
    char *tokenArrayresponse[10];
    char *tokenresponse = strtok(response, " ");
    int j = 0;
    while (tokenresponse != NULL)
    {
        tokenArrayresponse[j++] = tokenresponse;
        tokenresponse = strtok(NULL, " ");
    }

    // using the response, client tries to connect to the Storage Server TCP socket
    int storageServerSocket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in storageServerAddress;
    storageServerAddress.sin_family = AF_INET;
    storageServerAddress.sin_port = htons(atoi(tokenArrayresponse[1]));
    storageServerAddress.sin_addr.s_addr = inet_addr(tokenArrayresponse[0]);

    int connectionStatus = connect(storageServerSocket, (struct sockaddr *)&storageServerAddress, sizeof(storageServerAddress));
    if (connectionStatus < 0)
    {
        printf("Error in connection establishment\n");
        exit(1);
    }

    // client sends request to Storage Server
    send(storageServerSocket, request, strlen(request), 0);

    // client receives response from Storage Server
    char responseStorage[100];
    recv(storageServerSocket, responseStorage, sizeof(responseStorage), 0);
    printf("Response from Storage Server: %s\n", responseStorage);

    // client closes connection to Storage Server
    close(storageServerSocket);
}

// write function when client requests to write a file
void clientWrite(int clientSocket)
{
    // send request to Naming Server
    send(clientSocket, request, strlen(request), 0);

    // receive response from Naming Server
    char response[100];
    recv(clientSocket, response, sizeof(response), 0);

    // tokenising the response into a token array
    char *tokenArrayresponse[10];
    char *tokenresponse = strtok(response, " ");
    int j = 0;
    while (tokenresponse != NULL)
    {
        tokenArrayresponse[j++] = tokenresponse;
        tokenresponse = strtok(NULL, " ");
    }

    // using the response, client tries to connect to the Storage Server TCP socket
    int storageServerSocket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in storageServerAddress;
    storageServerAddress.sin_family = AF_INET;
    storageServerAddress.sin_port = htons(atoi(tokenArrayresponse[1]));
    storageServerAddress.sin_addr.s_addr = inet_addr(tokenArrayresponse[0]);

    int connectionStatus = connect(storageServerSocket, (struct sockaddr *)&storageServerAddress, sizeof(storageServerAddress));
    if (connectionStatus < 0)
    {
        printf("Error in connection establishment\n");
        exit(1);
    }

    // client sends request to Storage Server
    send(storageServerSocket, request, strlen(request), 0);

    // client receives response from Storage Server
    char responseStorage[100];
    recv(storageServerSocket, responseStorage, sizeof(responseStorage), 0);
    printf("Response from Storage Server: %s\n", responseStorage);

    // client closes connection to Storage Server
    close(storageServerSocket);
}

// getsize function when client requests to get the size of a file
void clientGetSize(int clientSocket)
{
    // send request to Naming Server
    strcpy(request, "COPY ./src/hello ./src/jainit\n");
    send(clientSocket, request, strlen(request), 0);

    // receive response from Naming Server
    char response[100];
    recv(clientSocket, response, sizeof(response), 0);

    // tokenising the response into a token array
    char *tokenArrayresponse[10];
    char *tokenresponse = strtok(response, " ");
    int j = 0;
    while (tokenresponse != NULL)
    {
        tokenArrayresponse[j++] = tokenresponse;
        tokenresponse = strtok(NULL, " ");
    }

    // using the response, client tries to connect to the Storage Server TCP socket
    int storageServerSocket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in storageServerAddress;
    storageServerAddress.sin_family = AF_INET;
    storageServerAddress.sin_port = htons(atoi(tokenArrayresponse[1]));
    storageServerAddress.sin_addr.s_addr = inet_addr(tokenArrayresponse[0]);

    int connectionStatus = connect(storageServerSocket, (struct sockaddr *)&storageServerAddress, sizeof(storageServerAddress));
    if (connectionStatus < 0)
    {
        printf("Error in connection establishment\n");
        exit(1);
    }

    // client sends request to Storage Server
    send(storageServerSocket, request, strlen(request), 0);

    // client receives response from Storage Server
    char responseStorage[100];
    recv(storageServerSocket, responseStorage, sizeof(responseStorage), 0);
    printf("Response from Storage Server: %s\n", responseStorage);

    // client closes connection to Storage Server
    close(storageServerSocket);
}

int main()
{
    // client tries to connect to Naming Server TCP socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0)
    {
        printf("Error in client socket creation\n");
        exit(1);
    }

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(NAMING_SERVER_PORT);
    serverAddress.sin_addr.s_addr = inet_addr(IP_ADDRESS);

    int connectionStatus = connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (connectionStatus < 0)
    {
        printf("Error in connection establishment\n");
        exit(1);
    }

    // client sends request to Naming Server
    while (1)
    {
        printf("Enter request: ");

        // taking input from client using fgets
        fgets(request, 100, stdin);

        char request_copy[100];
        strcpy(request_copy, request);

        // tokenising the request into a token array
        char *tokenArray[10];
        char *token = strtok(request_copy, " ");
        int i = 0;
        while (token != NULL)
        {
            tokenArray[i++] = token;
            token = strtok(NULL, " ");
        }

        // if the request is to read a file
        if (strcmp(tokenArray[0], "READ") == 0)
        {
            clientRead(clientSocket);
        }
        else if (strcmp(tokenArray[0], "WRITE") == 0)
        {
            clientWrite(clientSocket);
        }
        else if (strcmp(tokenArray[0], "GETSIZE") == 0)
        {
            clientGetSize(clientSocket);
        }
        else
        {
            printf("Invalid request\n");
        }

        fflush(stdin);
        memset(request, '\0', sizeof(request));

        
    }

    return 0;
}