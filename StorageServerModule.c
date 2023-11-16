#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>

#define IP_ADDRESS "192.168.1.1"
#define CLIENT_PORT 4000
#define NM_PORT 8001
#define NAMING_SERVER_PORT 8000
#define MOUNT "/storage/1"

typedef struct FileSystem {
    // Simplified for example
    char fileTree[1000];  // Placeholder for file tree representation
} FileSystem;

typedef struct StorageServer {
    char ipAddress[16];  // IPv4 Address
    int nmPort;          // Port for NM Connection
    int clientPort;      // Port for Client Connection
    char accessiblePaths[1000]; // List of accessible paths
    // Other metadata as needed
} StorageServer;


void handleClientRequest();
FileSystem fileSystem;
StorageServer ss;


void registerStorageServer(char* ipAddress, int nmPort, int clientPort, char* accessiblePaths) {
    strcpy(ss.ipAddress, ipAddress);
    ss.nmPort = nmPort;
    ss.clientPort = clientPort;
    strcpy(ss.accessiblePaths, accessiblePaths);
}


void initializeStorageServer() {
    // Initialize SS_1
    registerStorageServer(IP_ADDRESS, NM_PORT, CLIENT_PORT, MOUNT);
}



int serializeStorageServer(StorageServer *server, char *buffer) {
    int offset = 0;
    offset += snprintf(buffer + offset, sizeof(server->ipAddress), "%s,", server->ipAddress);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%d,%d,", server->nmPort, server->clientPort);
    offset += snprintf(buffer + offset, sizeof(server->accessiblePaths), "%s", server->accessiblePaths);
    return offset;
}


// Function for the storage server to report its status to the naming server.
void reportToNamingServer(StorageServer *server) {
    int sock;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NAMING_SERVER_PORT);
    // server_addr.sin_addr.s_addr = inet_addr(LOCALIPADDRESS);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to Naming Server failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    char buffer[1024];
    serializeStorageServer(server, buffer);

    if (send(sock, buffer, strlen(buffer), 0) < 0) {
        perror("Failed to send storage server data");
    }

    close(sock);
}






// The main function could set up the storage server.
int main(int argc, char *argv[]) {
    initializeStorageServer();

    // Report to the naming server
    reportToNamingServer(&ss);

    // Further code to accept connections and handle requests.
    return 0;
}
