#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define NMIPADDRESS "127.0.0.1"
#define SSIPADDRESS "127.0.0.2"

// Incomming Connection
#define CLIENT_PORT 4000
#define NM_PORT 5000
#define SS_PORT 6000

// OutGoing Connection
#define NAMING_SERVER_PORT 8000
#define MOUNT "./src"

typedef struct FileSystem
{
	// Simplified for example
	char fileTree[1000]; // Placeholder for file tree representation
} FileSystem;

typedef struct StorageServer
{
	char ipAddress[16]; // IPv4 Address
	int nmPort; // Port for NM Connection
	int clientPort; // Port for Client Connection
    int ssPort; // Port for SS Connection
	int numPaths;
	char accessiblePaths[100][100]; // List of accessible paths
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

void update_accessible_paths_recursive(char* path)
{
	DIR* dir;
	struct dirent* entry;

	// Open the directory
	dir = opendir(path);

	if(dir == NULL)
	{
		perror("opendir");
		return;
	}

	// Iterate over entries in the directory
	while((entry = readdir(dir)) != NULL)
	{
		// Skip "." and ".."
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
		{
			continue;
		}

		// Create the full path of the entry
		char full_path[PATH_MAX];
		snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

		// update to accessible paths
		strcpy(ss.accessiblePaths[ss.numPaths], full_path);
		printf("PATH %s\n ", full_path);
		ss.numPaths++;

		// If it's a directory, recursively call the function
		if(entry->d_type == DT_DIR)
		{
			update_accessible_paths_recursive(full_path);
		}
	}

	// Close the directory
	closedir(dir);
}
void registerStorageServer(char* ipAddress, int nmPort, int clientPort, char* accessiblePaths)
{
	strcpy(ss.ipAddress, ipAddress);
	ss.nmPort = nmPort;
	ss.clientPort = clientPort;
	ss.ssPort = SS_PORT;
	strcpy(ss.accessiblePaths[0], MOUNT);
	// update initial accessible paths
	ss.numPaths = 1;
	// char * path ;
	// strcpy(path,accessiblePaths);
	update_accessible_paths_recursive(accessiblePaths);

	// for (int i = 0; i < ss.numPaths; i++) {
	//     printf("%s\n", ss.accessiblePaths[i]);
	// }
}

void initializeStorageServer()
{
	// Initialize SS_1
	registerStorageServer(SSIPADDRESS, NM_PORT, CLIENT_PORT, MOUNT);
}

int serializeStorageServer(StorageServer* server, char* buffer)
{
	int offset = 0;
	offset += snprintf(buffer + offset, sizeof(server->ipAddress), "%s,", server->ipAddress);
	offset += snprintf(
		buffer + offset, sizeof(buffer) - offset, "%d,%d,", server->nmPort, server->clientPort);
	offset +=
		snprintf(buffer + offset, sizeof(server->accessiblePaths), "%s", server->accessiblePaths);
	return offset;
}

// Function for the storage server to report its status to the naming server.
void reportToNamingServer(StorageServer* server)
{
	int sock;
	struct sockaddr_in server_addr;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0)
	{
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(NAMING_SERVER_PORT);
	server_addr.sin_addr.s_addr = inet_addr(NMIPADDRESS);

	if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("Connection to Naming Server failed");
		close(sock);
		exit(EXIT_FAILURE);
	}

	char buffer[sizeof(StorageServer)];
	printf("Size of StorageServer : %d\n", sizeof(StorageServer));
	printf("Size of buffer : %d\n", sizeof(buffer));
	printf("Size of server : %d\n", sizeof(*server));
	printf("address of server : %d\n", server->numPaths);
	memcpy(buffer, server, sizeof(StorageServer));
	if(send(sock, buffer, sizeof(buffer), 0) < 0)
	{
		perror("Failed to send storage server data");
	}

	close(sock);
}

void* executeClientRequest(void* arg)
{
	ThreadArg* threadArg = (ThreadArg*)arg;
	char* request = threadArg->request;
	int clientSocket = threadArg->socket;

	char command[1024], path[1024];
	sscanf(request, "%s %s", command, path);

	printf("Command: %s %s\n", command, path);

	if(strcmp(command, "READ") == 0)
	{
		FILE* file = fopen(path, "r");
		if(file == NULL)
		{
			perror("Error opening file");
			const char* errMsg = "Failed to open file";
			printf("Error :  %s\n", errMsg);
			send(clientSocket, errMsg, strlen(errMsg), 0);
		}
		else
		{
			char fileContent[4096]; // Adjust size as needed
			size_t bytesRead = fread(fileContent, 1, sizeof(fileContent) - 1, file);
			if(ferror(file))
			{
				perror("Read error");
				printf("Error :  %s\n", "Read error");
				send(clientSocket, "Read error", 10, 0);
			}
			else if(bytesRead == 0)
			{
				printf("No content\n");
				send(clientSocket, "No content", 10, 0);
			}
			else
			{
				fileContent[bytesRead] = '\0';
				printf("File content: %s\n", fileContent);
				send(clientSocket, fileContent, bytesRead, 0);
			}
			fclose(file);
		}
	}
	else if(strcmp(command, "GETSIZE") == 0)
	{
		struct stat statbuf;
		if(stat(path, &statbuf) == -1)
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
	else if(strcmp(command, "WRITE") == 0)
	{
		char content[4096]; // Adjust size as needed
		sscanf(request, "%*s %*s %[^\t\n]", content); // Reads the content part of the request

		FILE* file = fopen(path, "w");
		if(file == NULL)
		{
			perror("Error opening file for writing");
			send(clientSocket, "Error opening file for writing", 30, 0);
		}
		else
		{
			size_t bytesWritten = fwrite(content, 1, strlen(content), file);
			if(ferror(file))
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

	free(threadArg); // Free the allocated memory
	close(clientSocket); // Close the connection
	return NULL;
}

void* executeNMRequest(void* arg)
{
	ThreadArg* threadArg = (ThreadArg*)arg;
	char* request = threadArg->request;
	int NMSocket = threadArg->socket;

	char command[1024];
	char path[1024];// Source path
	char path2[1024];// Destination path
    char destination_ip[1024]; // Destination IP
    char destination_port[100]; // Destination server port 

    // sprintf(reply,
	// 					"%s %d %s %s %d",
	// 					destination->server.ipAddress,
    //                     destination->server.clientPort,
    //                     sourcePath,
    //                     destinationPath,
    //                     source->server.clientPort);
	if(strncmp(request, "COPY", sizeof("COPY")))
	{
    printf("GOT here  %s\n", request);
		sscanf(request, "%s %s %s %s %s", command , destination_ip, destination_port, path, path2 );
        printf("Command: %s %s %s %s %s\n", command ,  destination_ip, destination_port, path, path2);
        printf("GOT COPY\n");
	}
	else
	{
		sscanf(request, "%s %s", command, path);

		printf("Command: %s %s\n", command, path);
	}

	if(strcmp(command, "CREATE") == 0)
	{
		int pathLength = strlen(path);
		if(path[pathLength - 1] == '/')
		{ // Check if the path ends with '/'
			if(mkdir(path, 0777) == -1)
			{ // Attempt to create a directory
				perror("Error creating directory");
			}
			else
			{
				printf("Directory created: %s\n", path);
			}
		}
		else
		{ // Treat as file
			FILE* file = fopen(path, "w");
			if(file == NULL)
			{
				perror("Error creating file");
			}
			else
			{
				printf("File created: %s\n", path);
				fclose(file);
			}
		}
	}
	else if(strcmp(command, "DELETE") == 0)
	{
		struct stat path_stat;
		stat(path, &path_stat);
		if(S_ISDIR(path_stat.st_mode))
		{ // Check if it's a directory
			// rmdir only works on empty directories. For non-empty directories, you'll need a more complex function
			if(rmdir(path) == -1)
			{
				perror("Error deleting directory");
			}
			else
			{
				printf("Directory deleted: %s\n", path);
			}
		}
		else
		{ // Treat as file
			if(remove(path) == 0)
			{
				printf("File deleted: %s\n", path);
			}
			else
			{
				perror("Error deleting file");
			}
		}
	}
	else if(strncmp(command, "COPY" , strlen("COPY")) == 0)
	{
        int d_port = atoi(destination_port);
        
        // open the file in read mode which path is "path"
        FILE *fptr1 = fopen(path, "r");
        printf("GOT COPY\n");
        // printf("Command: %s %s %s %s\n", destination_ip, destination_port, path, path);
        // check if the file is opened or not
        if (fptr1 == NULL)
        {
            printf("Cannot open file %s \n", path);
            exit(0);
        }

        // copy the contents of the file in buffer to send it to the destination server
        char buffer[1024];
        int nread = fread(buffer, 1, sizeof(buffer), fptr1);
        printf("%s is the buffer\n", buffer  );
        // close the file
        fclose(fptr1);
        // now we need to send the file to the destination server but first we need to connect to the destination server and send the file path 
        // and then we need to send the file to the destination server
        // create a socket
        int sock;
        struct sockaddr_in serverAddr;
        // create a socket
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            perror("Could not create socket");
            return NULL;
        }
        // set the server address
        serverAddr.sin_addr.s_addr = inet_addr(destination_ip);
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(d_port);
        // connect to the destination server
        if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
        {
            perror("Connect failed. Error");
            return NULL;
        }
        // send the file path to the destination server
        if(send(sock, path2, strlen(path2), 0) < 0)
        {
            perror("Send failed");
            close(sock);
            exit(EXIT_FAILURE);
        }
        // recieve the response from the destination server 
        char buffer2[1024];
        int totalRead = 0;
        if(recv(sock, buffer2, sizeof(buffer2), 0) < 0)
        {
            perror("recv failed");
            close(sock);
            exit(EXIT_FAILURE);
        }
        printf("Server reply: %s\n", buffer2);
        if (strncmp(buffer2, "OK" , strlen("OK")) == 0)
        {
            // send the file to the destination server
            printf("Sending file to the destination server\n");
            printf("GOT OK\n");
            if(send(sock, buffer, nread, 0) < 0)
            {
                perror("Send failed");
                close(sock);
                exit(EXIT_FAILURE);
            }
            // recieve the response from the destination server 
            char buffer3[1024];
            int totalRead = 0;
            if(recv(sock, buffer3, sizeof(buffer3), 0) < 0)
            {
                perror("recv failed");
                close(sock);
                exit(EXIT_FAILURE);
            }
            printf("Server reply: %s\n", buffer3);
        }
        else
        {
            printf("Server reply: %s\n", buffer2);
        }
	}
	// Add more conditions as needed
	free(threadArg); // Free the allocated memory
	close(NMSocket); // Close the connection
	return NULL;
}

void* executeSSRequest(void* arg)
{
	ThreadArg* threadArg = (ThreadArg*)arg;
	char* request = threadArg->request;
	// Similar structure to executeClientRequest
	char command[1024];
	char path[1024];
	sscanf(request, "%s",path);

	printf("Command:  %s\n", path);

	if(strcmp(command, "CREATE") == 0)
	{
		// Handle creating a file/directory
	}
	else if(strcmp(command, "DELETE") == 0)
	{
		// Handle deleting a file/directory
	}
	else if(strcmp(command, "COPY") == 0)
	{
		// Handle copying a file/directory
	}
	// Add more conditions as needed
	free(threadArg); // Free the allocated memory
	return NULL;
}

void* handleClientConnections(void* args)
{
	int server_fd, new_socket, opt = 1;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	// Creating socket file descriptor
	if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Forcefully attaching socket to the CLIENT_PORT
	if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(SSIPADDRESS); // Listening on SSIPADDRESS
	address.sin_port = htons(CLIENT_PORT);

	// Bind the socket to the port
	if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	// Listen for incoming connections
	if(listen(server_fd, 3) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	while(1)
	{
		if((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0)
		{
			perror("accept");
			continue;
		}

		// Execute the Command in a new thread so that SS is always listening for new connections
		ThreadArg* arg = malloc(sizeof(ThreadArg));
		read(new_socket, arg->request, sizeof(arg->request));
		arg->socket = new_socket;

		pthread_t tid;
		if(pthread_create(&tid, NULL, (void* (*)(void*))executeClientRequest, arg) != 0)
		{
			perror("Failed to create thread for client request");
		}

		pthread_detach(tid); // Detach the thread
	}

	return NULL;
}

void* handleNamingServerConnections(void* args)
{
	int server_fd, new_socket, opt = 1;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	// Creating socket file descriptor
	if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Forcefully attaching socket to the CLIENT_PORT
	if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(SSIPADDRESS); // Listening on SSIPADDRESS
	address.sin_port = htons(NM_PORT);

	// Bind the socket to the port
	if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	// Listen for incoming connections
	if(listen(server_fd, 3) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	while(1)
	{
		if((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0)
		{
			perror("accept");
			continue;
		}

		// Execute the Command in a new thread so that SS is always listening for new connections
		ThreadArg* arg = malloc(sizeof(ThreadArg));
		arg->socket = new_socket;
        
		if(recv(new_socket, arg->request, sizeof(arg->request), 0) < 0)
        {
            perror("recv failed");
            close(new_socket);
            exit(EXIT_FAILURE);
        }
        printf("GOT this here %s\n", arg->request);
		pthread_t tid;
		if(pthread_create(&tid, NULL, (void* (*)(void*))executeNMRequest, arg) != 0)
		{
			perror("Failed to create thread for client request");
		}

		pthread_detach(tid); // Detach the thread
	}

	return NULL;
}

void* handleStorageServerConnections(void* args)
{
	int server_fd, new_socket, opt = 1;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	// Creating socket file descriptor
	if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Forcefully attaching socket to the CLIENT_PORT
	if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(SSIPADDRESS); // Listening on SSIPADDRESS
	address.sin_port = htons(SS_PORT);

	// Bind the socket to the port
	if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	// Listen for incoming connections
	if(listen(server_fd, 3) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	while(1)
	{
		if((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0)
		{
			perror("accept");
			continue;
		}

		// Execute the Command in a new thread so that SS is always listening for new connections
		ThreadArg* arg = malloc(sizeof(ThreadArg));
		arg->socket = new_socket;
		if(recv(new_socket, arg->request, sizeof(arg->request), 0) < 0)
        {
            perror("recv failed");
            close(new_socket);
            exit(EXIT_FAILURE);
        }
        printf("GOT this here from source %s\n", arg->request);
        
        
        if(send(new_socket, "OK", 2, 0) < 0)
        {
            perror("Send failed");
            close(new_socket);
            exit(EXIT_FAILURE);
        }
        

        // now recieve the file from the source server
        char buffer[1024];
        int totalRead = 0;
        if(recv(new_socket, buffer, sizeof(buffer), 0) < 0)
        {
            perror("recv failed");
            close(new_socket);
            exit(EXIT_FAILURE);
        }
        printf("Server reply: %s\n", buffer);
        // now we need to write the file to the destination server
        // open the file in write mode which path is "path"
        FILE *fptr1 = fopen(arg->request, "w");
        // check if the file is opened or not
        if (fptr1 == NULL)
        {
            printf("Cannot open file %s \n", arg->request);
            exit(0);
        }
        // copy the contents of the file in buffer to send it to the destination server
        int nread = fwrite(buffer, 1, sizeof(buffer), fptr1);
        // close the file
        fclose(fptr1);
        // send the response to the source server
        if(send(new_socket, "OK", 2, 0) < 0)
        {
            perror("Send failed");
            close(new_socket);
            exit(EXIT_FAILURE);
        }
        close(new_socket);


		// pthread_t tid;
		// if(pthread_create(&tid, NULL, (void* (*)(void*))executeSSRequest, arg) != 0)
		// {
		// 	perror("Failed to create thread for client request");
		// }

		// pthread_detach(tid); // Detach the thread
	}

	return NULL;
}

// The main function could set up the storage server.
int main(int argc, char* argv[])
{
	printf("Storage Server\n");
	initializeStorageServer();
	printf("%d \n This ", ss.numPaths);
	// Report to the naming server
	reportToNamingServer(&ss);

	pthread_t thread1, thread2, thread3;

	// Create thread for handling client connections
	if(pthread_create(&thread1, NULL, handleClientConnections, NULL) != 0)
	{
		perror("Failed to create client connections thread");
		return 1;
	}

	// Create thread for handling Naming Server connections
	if(pthread_create(&thread2, NULL, handleNamingServerConnections, NULL) != 0)
	{
		perror("Failed to create Naming Server connections thread");
		return 1;
	}

	// Create thread for handling Naming Server connections
	if(pthread_create(&thread3, NULL, handleStorageServerConnections, NULL) != 0)
	{
		perror("Failed to create Naming Server connections thread");
		return 1;
	}

	// Wait for threads to finish (optional based on your design)
	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
	pthread_join(thread3, NULL);

	// Further code to accept connections and handle requests.

	return 0;
}
