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

// // Function to handle a request from the naming server - could be read, write, etc.
// void handleRequestFromNamingServer(int namingServerSocket) {
//     char buffer[1024];
//     // Read request from the naming server
//     ssize_t n = read(namingServerSocket, buffer, 1024);
//     if (n < 0) {
//         perror("Error reading from socket");
//         return;
//     }

//     // Parse the request and take appropriate action
//     // For example, if it's a read request, call readFile(...)
//     // Write response back to namingServerSocket
// }


// // Function to read a file from disk.
// void readFile(char *filePath, char *buffer, size_t bufferSize) {
//     // Open the file at filePath
//     FILE *file = fopen(filePath, "r");
//     if (file == NULL) {
//         // Handle error
//         return;
//     }

//     // Read contents into buffer
//     fread(buffer, sizeof(char), bufferSize, file);

//     // Close the file
//     fclose(file);
// }


// // Function to write to a file on disk.
// void writeFile(char *filePath, char *data, size_t dataSize) {
//     // Open or create the file at filePath
//     FILE *file = fopen(filePath, "w");
//     if (file == NULL) {
//         // Handle error
//         return;
//     }

//     // Write data to file
//     fwrite(data, sizeof(char), dataSize, file);

//     // Close the file
//     fclose(file);
// }


// // Function to get information about a file.
// void getFileInfo(char *filePath) {
//     // Use stat or similar system call to get file metadata
//     struct stat fileInfo;
//     if (stat(filePath, &fileInfo) == 0) {
//         // Extract and return required information from fileInfo
//     } else {
//         // Handle error
//     }
// }


// // Function to create a file or directory.
// void createFileOrDirectory(char *path, int isDirectory) {
//     if (isDirectory) {
//         if (mkdir(path, 0777) < 0) {
//             perror("Error creating directory");
//         }
//     } else {
//         FILE *file = fopen(path, "w");
//         if (file == NULL) {
//             perror("Error creating file");
//         } else {
//             fclose(file);
//         }
//     }
// }

// // Function to delete a file or directory.
// void deleteFileOrDirectory(char *path) {
//     struct stat pathStat;
//     if (stat(path, &pathStat) < 0) {
//         perror("Error getting path info");
//         return;
//     }

//     if (S_ISDIR(pathStat.st_mode)) {
//         if (rmdir(path) < 0) {
//             perror("Error removing directory");
//         }
//     } else {
//         if (unlink(path) < 0) {
//             perror("Error removing file");
//         }
//     }
// }

// // Function to copy a file or directory.
// void copyFileOrDirectory(char *sourcePath, char *destinationPath) {
//     FILE *source = fopen(sourcePath, "r");
//     if (source == NULL) {
//         perror("Error opening source file");
//         return;
//     }

//     FILE *dest = fopen(destinationPath, "w");
//     if (dest == NULL) {
//         perror("Error opening destination file");
//         fclose(source);
//         return;
//     }

//     char buffer[1024];
//     size_t bytesRead;
//     while ((bytesRead = fread(buffer, 1, sizeof(buffer), source)) > 0) {
//         fwrite(buffer, 1, bytesRead, dest);
//     }

//     fclose(source);
//     fclose(dest);
// }



// The main function could set up the storage server.
int main(int argc, char *argv[]) {
    initializeStorageServer();

    // Report to the naming server
    reportToNamingServer(&ss);

    // Further code to accept connections and handle requests.
    return 0;
}
