#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int namingServerSocket;
struct sockaddr_in namingServerAddress;


#define READ_REQUEST_CODE 1
#define WRITE_REQUEST_CODE 2
#define INFO_REQUEST_CODE 3
#define CREATE_REQUEST_CODE 4
#define DELETE_REQUEST_CODE 5
#define COPY_REQUEST_CODE 6


// Initialize client and connect to the Naming Server
void connectToNamingServer(const char* serverIp, int serverPort) {
    namingServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (namingServerSocket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    namingServerAddress.sin_family = AF_INET;
    namingServerAddress.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverIp, &namingServerAddress.sin_addr);

    if (connect(namingServerSocket, (struct sockaddr*)&namingServerAddress, sizeof(namingServerAddress)) < 0) {
        perror("Connection to Naming Server failed");
        exit(EXIT_FAILURE);
    }
    printf("Connected to Naming Server.\n");
}

// Close the connection to the Naming Server
void disconnectFromNamingServer() {
    close(namingServerSocket);
    printf("Disconnected from Naming Server.\n");
}

// Helper function to send requests to the Naming Server
void sendRequestToServer(int operationCode, const char* path) {
    // In a real implementation, you'd serialize the operationCode and path into a protocol buffer or similar format
    char buffer[1024];
    sprintf(buffer, "%d:%s", operationCode, path); // Example serialization
    send(namingServerSocket, buffer, strlen(buffer), 0);
}

// Helper function to receive responses from the Naming Server
void receiveResponseFromServer(char* buffer, size_t bufferSize) {
    recv(namingServerSocket, buffer, bufferSize, 0);
}



void sendReadRequest(const char* filePath) {
    sendRequestToServer(READ_REQUEST_CODE, filePath);
    // Here you would wait for and handle the response, which could include details about the Storage Server to connect to for reading the file.
}

void sendWriteRequest(const char* filePath) {
    sendRequestToServer(WRITE_REQUEST_CODE, filePath);
    // Similar to sendReadRequest, expect a response with information about where to send your write operation.
}

// The sendInfoRequest, sendCreateRequest, etc. functions would look similar to sendReadRequest
// but with different operation codes and potentially additional data to specify the operation.

void sendInfoRequest(const char* filePath) {
    sendRequestToServer(INFO_REQUEST_CODE, filePath);
    
    char response[1024];
    receiveResponseFromServer(response, sizeof(response));
    printf("Info Response: %s\n", response);
}

void sendCreateRequest(const char* filePath) {
    sendRequestToServer(CREATE_REQUEST_CODE, filePath);
    
    char response[1024];
    receiveResponseFromServer(response, sizeof(response));
    printf("Create Response: %s\n", response);
}

void sendDeleteRequest(const char* filePath) {
    sendRequestToServer(DELETE_REQUEST_CODE, filePath);
    
    char response[1024];
    receiveResponseFromServer(response, sizeof(response));
    printf("Delete Response: %s\n", response);
}

void sendCopyRequest(const char* sourcePath, const char* destinationPath) {
    char buffer[2048];
    sprintf(buffer, "%d:%s:%s", COPY_REQUEST_CODE, sourcePath, destinationPath);
    send(namingServerSocket, buffer, strlen(buffer), 0);
    
    char response[1024];
    receiveResponseFromServer(response, sizeof(response));
    printf("Copy Response: %s\n", response);
}



int main(int argc, char *argv[]) {
    // Setting up connection details
    const char* serverIp = "127.0.0.1";
    int serverPort = 12345;

    // Connect to Naming Server
    connectToNamingServer(serverIp, serverPort);

    // Testing Read Request
    printf("\nTesting Read Request...\n");
    sendReadRequest("/path/to/read/file.txt");

    // Testing Write Request
    printf("\nTesting Write Request...\n");
    sendWriteRequest("/path/to/write/file.txt");

    // Testing Info Request
    printf("\nTesting Info Request...\n");
    sendInfoRequest("/path/to/info/file.txt");

    // Testing Create Request
    printf("\nTesting Create Request...\n");
    sendCreateRequest("/path/to/create/newfile.txt");

    // Testing Delete Request
    printf("\nTesting Delete Request...\n");
    sendDeleteRequest("/path/to/delete/file.txt");

    // Testing Copy Request
    printf("\nTesting Copy Request...\n");
    sendCopyRequest("/path/from/copy/file.txt", "/path/to/copy/file.txt");

    // Disconnect from Naming Server
    disconnectFromNamingServer();
}

