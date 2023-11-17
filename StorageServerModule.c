#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <stdio.h>


#define NMIPADDRESS "127.0.0.1"
#define SSIPADDRESS "127.0.0.2"

// Incomming Connection
#define CLIENT_PORT 4000
#define NM_PORT 5000

// OutGoing Connection
#define NAMING_SERVER_PORT 8000
#define MOUNT "storage/1"

typedef struct FileSystem
{
    // Simplified for example
    char fileTree[1000]; // Placeholder for file tree representation
} FileSystem;

typedef struct StorageServer
{
    char ipAddress[16];         // IPv4 Address
    int nmPort;                 // Port for NM Connection
    int clientPort;             // Port for Client Connection
    char accessiblePaths[1000]; // List of accessible paths
    // Other metadata as needed
} StorageServer;

typedef struct
{
    char request[1024]; // Adjust size as needed
    int socket;
} ThreadArg;

void handleClientRequest();
FileSystem fileSystem;
StorageServer ss;

void registerStorageServer(char *ipAddress, int nmPort, int clientPort, char *accessiblePaths)
{
    strcpy(ss.ipAddress, ipAddress);
    ss.nmPort = nmPort;
    ss.clientPort = clientPort;
    strcpy(ss.accessiblePaths, accessiblePaths);
}

void initializeStorageServer()
{
    // Initialize SS_1
    registerStorageServer(SSIPADDRESS, NM_PORT, CLIENT_PORT, MOUNT);
}

int serializeStorageServer(StorageServer *server, char *buffer)
{
    int offset = 0;
    offset += snprintf(buffer + offset, sizeof(server->ipAddress), "%s,", server->ipAddress);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%d,%d,", server->nmPort, server->clientPort);
    offset += snprintf(buffer + offset, sizeof(server->accessiblePaths), "%s", server->accessiblePaths);
    return offset;
}

// Function for the storage server to report its status to the naming server.
void reportToNamingServer(StorageServer *server)
{
    int sock;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NAMING_SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(NMIPADDRESS);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection to Naming Server failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    char buffer[1024];
    serializeStorageServer(server, buffer);

    if (send(sock, buffer, strlen(buffer), 0) < 0)
    {
        perror("Failed to send storage server data");
    }

    close(sock);
}

void *executeClientRequest(void *arg)
{
    ThreadArg *threadArg = (ThreadArg *)arg;
    char *request = threadArg->request;
    int clientSocket = threadArg->socket;

    char command[1024], path[1024];
    sscanf(request, "%s %s", command, path);

    printf("Command: %s %s\n", command, path);

    if (strcmp(command, "READ") == 0)
    {
        FILE *file = fopen(path, "r");
        if (file == NULL)
        {
            perror("Error opening file");
            const char *errMsg = "Failed to open file";
            printf("Error :  %s\n", errMsg);
            send(clientSocket, errMsg, strlen(errMsg), 0);
        }
        else
        {
            char fileContent[4096]; // Adjust size as needed
            size_t bytesRead = fread(fileContent, 1, sizeof(fileContent) - 1, file);
            if (ferror(file))
            {
                perror("Read error");
                printf("Error :  %s\n", "Read error");
                send(clientSocket, "Read error", 10, 0);
            }
            else if (bytesRead == 0)
            {
                printf("No content\n");
                send(clientSocket, "No content", 10, 0);
            }
            else
            {
                fileContent[bytesRead] = '\0';
                printf("File content: %s\n", fileContent);
                send(clientSocket, fileContent, bytesRead, 0);
                printf("SGSAD");
            }
            fclose(file);
        }
    }
    else if (strcmp(command, "GETSIZE") == 0)
    {
        struct stat statbuf;
        if (stat(path, &statbuf) == -1)
        {
            perror("Error getting file size");
            send(clientSocket, "Error getting file size", 23, 0);
        }
        else
        {
            char response[1024];
            snprintf(response, sizeof(response), "Size of %s: %ld bytes", path, statbuf.st_size);
            send(clientSocket, response, strlen(response), 0);
        }
    }
    else if (strcmp(command, "WRITE") == 0)
    {
        char content[4096];                           // Adjust size as needed
        sscanf(request, "%*s %*s %[^\t\n]", content); // Reads the content part of the request

        FILE *file = fopen(path, "w");
        if (file == NULL)
        {
            perror("Error opening file for writing");
            send(clientSocket, "Error opening file for writing", 30, 0);
        }
        else
        {
            size_t bytesWritten = fwrite(content, 1, strlen(content), file);
            if (ferror(file))
            {
                perror("Write error");
                send(clientSocket, "Write error", 11, 0);
            }
            else
            {
                char response[1024];
                snprintf(response, sizeof(response), "Written %ld bytes to %s", bytesWritten, path);
                send(clientSocket, response, strlen(response), 0);
            }
            fclose(file);
        }
    }
    else
    {
        printf("Invalid command\n");
    }

    free(threadArg);     // Free the allocated memory
    close(clientSocket); // Close the connection
    return NULL;
}

void *executeNMRequest(void *arg)
{
    ThreadArg *threadArg = (ThreadArg *)arg;
    char *request = threadArg->request;
    // Similar structure to executeClientRequest
    char command[1024];
    char path[1024];
    sscanf(request, "%s %s", command, path);

    printf("Command: %s %s\n", command, path);

    if (strcmp(command, "CREATE") == 0)
    {
        // Handle creating a file/directory
    }
    else if (strcmp(command, "DELETE") == 0)
    {
        // Handle deleting a file/directory
    }
    else if (strcmp(command, "COPY") == 0)
    {
        // Handle copying a file/directory
    }
    // Add more conditions as needed
    free(threadArg); // Free the allocated memory
    return NULL;
}

void *handleClientConnections(void *args)
{
    int server_fd, new_socket, opt = 1;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the CLIENT_PORT
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(SSIPADDRESS); // Listening on SSIPADDRESS
    address.sin_port = htons(CLIENT_PORT);

    // Bind the socket to the port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            continue;
        }

        // Execute the Command in a new thread so that SS is always listening for new connections
        ThreadArg *arg = malloc(sizeof(ThreadArg));
        read(new_socket, arg->request, sizeof(arg->request));
        arg->socket = new_socket;

        pthread_t tid;
        if (pthread_create(&tid, NULL, (void *(*)(void *))executeClientRequest, arg) != 0)
        {
            perror("Failed to create thread for client request");
        }

        pthread_detach(tid); // Detach the thread
    }

    return NULL;
}

void *handleNamingServerConnections(void *args)
{
    int server_fd, new_socket, opt = 1;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the CLIENT_PORT
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(SSIPADDRESS); // Listening on SSIPADDRESS
    address.sin_port = htons(NM_PORT);

    // Bind the socket to the port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            continue;
        }

        // Execute the Command in a new thread so that SS is always listening for new connections
        ThreadArg *arg = malloc(sizeof(ThreadArg));
        arg->socket = new_socket;
        read(new_socket, arg->request, sizeof(arg->request));

        pthread_t tid;
        if (pthread_create(&tid, NULL, (void *(*)(void *))executeNMRequest, arg) != 0)
        {
            perror("Failed to create thread for client request");
        }

        pthread_detach(tid); // Detach the thread
    }

    return NULL;
}

// The main function could set up the storage server.
int main(int argc, char *argv[])
{
    initializeStorageServer();

    // Report to the naming server
    reportToNamingServer(&ss);

    pthread_t thread1, thread2;

    // Create thread for handling client connections
    if (pthread_create(&thread1, NULL, handleClientConnections, NULL) != 0)
    {
        perror("Failed to create client connections thread");
        return 1;
    }

    // Create thread for handling Naming Server connections
    if (pthread_create(&thread2, NULL, handleNamingServerConnections, NULL) != 0)
    {
        perror("Failed to create Naming Server connections thread");
        return 1;
    }

    // Wait for threads to finish (optional based on your design)
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    // Further code to accept connections and handle requests.

    return 0;
}
