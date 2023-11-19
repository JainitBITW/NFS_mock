#include "uthash.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define MAX_STORAGE_SERVERS 10
#define MAX_CLIENTS 100
#define NAMING_CLIENT_LISTEN_PORT 8001
#define NAMING_SS_LISTEN_PORT 8000
#define HEARTBEAT_INTERVAL 5
#define MAX_CACHE_SIZE 100
#define NMIPADDRESS "127.0.0.1"
#define CMD_READ "READ"
#define CMD_WRITE "WRITE"

// typedef struct FileSystem
// {
//     // Simplified for example
//     char fileTree[1000]; // Placeholder for file tree representation
// } FileSystem;

typedef struct StorageServer
{
	char ipAddress[16]; // IPv4 Address
	int nmPort;			// Port for NM Connection
	int clientPort;		// Port for Client Connection
	int ssPort;			// Port for SS Connection
	int numPaths;
	char accessiblePaths[500][100]; // List of accessible paths
	// Other metadata as needed
	UT_hash_handle hh; // Hash handle for uthash
} StorageServer;

typedef struct PathToServerMap
{
	char path[1000];	  // The key
	StorageServer server; // The value
	UT_hash_handle hh;	  // Makes this structure hashable
} PathToServerMap;

PathToServerMap *serversByPath = NULL;

void *handleClientRequest();
// FileSystem fileSystem[MAX_STORAGE_SERVERS];
StorageServer storageServers[MAX_STORAGE_SERVERS];
int storageServerCount = 0;

void initializeNamingServer()
{
	printf("Initializing Naming Server...\n");
	// Initialize file system and directory structure
	// memset(fileSystem, 0, sizeof(fileSystem));

	// Initialize storage server list
	memset(storageServers, 0, sizeof(storageServers));
	storageServerCount = 0;
}

pthread_mutex_t storageServerMutex = PTHREAD_MUTEX_INITIALIZER;

void *handleStorageServer(void *socketDesc)
{
	int sock = *(int *)socketDesc;
	char buffer[sizeof(StorageServer)]; // Adjust size as needed
	int readSize;

	// Read data from the socket
	if ((readSize = recv(sock, buffer, sizeof(buffer), 0)) > 0)
	{

		// Parse data to extract Storage Server details
		// For example, if the data is sent as a comma-separated string
		char *token;
		char *rest = buffer;
		StorageServer newServer;

		memcpy(&newServer, buffer, sizeof(StorageServer));
		// Lock mutex before updating global storage server array
		pthread_mutex_lock(&storageServerMutex);
		if (storageServerCount < MAX_STORAGE_SERVERS)
		{
			storageServers[storageServerCount++] = newServer;
			pthread_mutex_unlock(&storageServerMutex);

			// Printing
			printf("Received storage server details:\n");
			printf("IP Address: %s\n", newServer.ipAddress);
			printf("NM Port: %d\n", newServer.nmPort);
			printf("Client Port: %d\n", newServer.clientPort);
			// printf("Accessible Paths: %s\n", newServer.accessiblePaths);
			printf("NUM PATHS: %d\n", newServer.numPaths);
			for (int path_no = 0; path_no < newServer.numPaths; path_no++)
			{
				printf("Accessible Paths: %s\n", newServer.accessiblePaths[path_no]);
			}

			for (int path_no = 0; path_no < newServer.numPaths; path_no++)
			{
				PathToServerMap *s = malloc(sizeof(PathToServerMap));
				strcpy(s->path, newServer.accessiblePaths[path_no]);

				// Copy the newServer data into the hash table entry
				s->server = newServer; // Direct copy of the server

				HASH_ADD_STR(serversByPath, path, s);
			}

			// Send ACK
			const char *ackMessage = "Registration Successful";
			send(sock, ackMessage, strlen(ackMessage), 0);
		}
		else
		{
			pthread_mutex_unlock(&storageServerMutex);
			// Send server limit reached message
			const char *limitMessage = "Storage server limit reached";
			send(sock, limitMessage, strlen(limitMessage), 0);
		}
	}

	close(sock);
	free(socketDesc);
	return 0;
}
// Thsi function is used to send
void send_request(char *destination_ip, int destination_port, char *buffer)
{
	int sock;
	struct sockaddr_in server;
	char server_reply[2000];

	// Create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
	{
		printf("Could not create socket");
	}

	server.sin_addr.s_addr = inet_addr(destination_ip);
	server.sin_family = AF_INET;
	server.sin_port = htons(destination_port);

	// Connect to remote server
	if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
	{
		perror("connect failed. Error");
		return;
	}

	// Send some data
	if (send(sock, buffer, strlen(buffer), 0) < 0)
	{
		puts("Send failed");
		return;
	}

	// Receive a reply from the server
	if (recv(sock, server_reply, 2000, 0) < 0)
	{
		puts("recv failed");
	}

	puts("Server reply :");
	puts(server_reply);

	close(sock);
}

void *startStorageServerListener()
{
	int server_fd, new_socket, *new_sock;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	// Creating socket file descriptor
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Bind the socket to the port
	address.sin_family = AF_INET;
	// address.sin_addr.s_addr = LOCALIPADDRESS;
	address.sin_addr.s_addr = inet_addr(NMIPADDRESS);

	address.sin_port = htons(NAMING_SS_LISTEN_PORT);

	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	// Start listening for incoming connections
	if (listen(server_fd, MAX_STORAGE_SERVERS) < 0)
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

		pthread_t sn_thread;
		new_sock = malloc(sizeof(int));
		*new_sock = new_socket;

		if (pthread_create(&sn_thread, NULL, handleStorageServer, (void *)new_sock) < 0)
		{
			perror("could not create thread");
			free(new_sock);
		}

		// Optionally, join the thread or detach it
		// pthread_join(sn_thread, NULL); // For synchronous handling
		pthread_detach(sn_thread); // For asynchronous handling
	}
}

// This function is used to get the storage server for a given path in form of path to server map
// ARGUMENTS:
// path: the path for which the storage server is to be found
// s : the path to server map uninitialized
// foundFlag : flag to indicate if the path is found or not
void get_path_ss(char *path, PathToServerMap *s, int *foundFlag)
{
	char *ip_Address_ss;
	int port_ss;
	HASH_FIND_STR(serversByPath, path, s);
	if (s != NULL)
	{
		strcpy(ip_Address_ss, s->server.ipAddress);
		port_ss = s->server.clientPort;
		*foundFlag = 1;
		printf("Found server for path %s\n", path);
		printf("IP: %s\n", ip_Address_ss);
		printf("Port: %d\n", port_ss);
	}
}
char *getDirectoryPath(const char *path)
{
	int length = strlen(path);

	// Find the last occurrence of '/'
	int lastSlashIndex = -1;
	for (int i = length - 1; i >= 0; i--)
	{
		if (path[i] == '/')
		{
			lastSlashIndex = i;
			break;
		}
	}

	if (lastSlashIndex != -1)
	{
		// Allocate memory for the new string
		char *directoryPath = (char *)malloc((lastSlashIndex + 1) * sizeof(char));

		// Copy the directory path into the new string
		strncpy(directoryPath, path, lastSlashIndex);

		// Add null terminator
		directoryPath[lastSlashIndex] = '\0';

		return directoryPath;
	}
	else
	{
		// No '/' found, the path is already a directory
		return strdup(path); // Duplicate the string to ensure the original is not modified
	}
}

void removeWhitespace(char *inputString)
{
	// Initialize indices for reading and writing in the string
	int readIndex = 0;
	int writeIndex = 0;

	// Iterate through the string
	while (inputString[readIndex] != '\0')
	{
		// If the current character is not a whitespace character, copy it
		if (!isspace(inputString[readIndex]))
		{
			inputString[writeIndex] = inputString[readIndex];
			writeIndex++;
		}

		// Move to the next character in the string
		readIndex++;
	}

	// Null-terminate the new string
	inputString[writeIndex] = '\0';
}

void *handleClientInput(void *socketDesc)
{
	int sock = *(int *)socketDesc;
	char buffer[1024];
	int readSize;
	while (1)
	{
		if ((readSize = recv(sock, buffer, sizeof(buffer), 0)) > 0)
		{
			buffer[readSize] = '\0';
			char buffer_copy[1024];
			strcpy(buffer_copy, buffer);
			printf("Received command: %s\n", buffer);

			// handling the command from client

			//  tokenising the command
			char *tokenArray[10];
			char *token = strtok(buffer, " ");
			int i = 0;
			while (token != NULL)
			{
				removeWhitespace(token);
				tokenArray[i++] = token;
				token = strtok(NULL, " ");
			}

			//  checking the command
			if (strcmp(tokenArray[0], "READ") == 0 || strcmp(tokenArray[0], "WRITE") == 0 ||
				strcmp(tokenArray[0], "GETSIZE") == 0)
			{
				printf("Command: %s\n", tokenArray[0]);
				// finding out the storage server for the file
				// path is tokenArray[1]
				char path[1024];
				strcpy(path, tokenArray[1]);
				// strip the path of white spaces
				int len = strlen(path);
				if (isspace(path[len - 1]))
				{
					path[len - 1] = '\0';
				}

				printf("Path: %s\n", path);

				// compare the paths of all storage servers from storageServers array
				// if the path is found in the accessiblePaths of a storage server

				int foundFlag = 0;
				char ip_Address_ss[16];
				int port_ss;
				// for(int i = 0; i < storageServerCount; i++)
				// {
				// 	for(int j = 0; j < storageServers[i].numPaths; j++)
				// 	{
				// 		if(strcmp(storageServers[i].accessiblePaths[j], path) == 0)
				// 		{
				// 			// If path is in accessiblePaths of storageServer
				// 			// send read request to that storage server
				// 			strcpy(ip_Address_ss, storageServers[i].ipAddress);
				// 			port_ss = storageServers[i].clientPort;
				// 			foundFlag = 1;
				// 			break;
				// 		}
				// 	}
				// 	if(foundFlag == 1)
				// 	{
				// 		break;
				// 	}
				// }
				// PathToServerMap *s;
				// get_path_ss(path, s, &foundFlag);

				PathToServerMap *s;
				HASH_FIND_STR(serversByPath, path, s);
				if (s != NULL)
				{
					strcpy(ip_Address_ss, s->server.ipAddress);
					port_ss = s->server.clientPort;
					foundFlag = 1;
					printf("Found server for path %s\n", path);
					printf("IP: %s\n", ip_Address_ss);
					printf("Port: %d\n", port_ss);
				}

				if (foundFlag == 1)
				{
					// send the port and ip address back to the client
					char reply[1024];
					sprintf(reply, "%s %d", s->server.ipAddress, s->server.clientPort);

					if (send(sock, reply, strlen(reply), 0) < 0)
					{
						puts("Send failed");
						return NULL;
					}
				}
				else
				{
					char reply[1024];
					strcpy(reply, "File not Found");
					if (send(sock, reply, strlen(reply), 0) < 0)
					{
						puts("Send failed");
						return NULL;
					}
				}
			}
			else if (strcmp(tokenArray[0], "CREATE") == 0)
			{
				char path[1024];
				strcpy(path, tokenArray[1]);
				// strip the path of white spaces
				int len = strlen(path);
				if (isspace(path[len - 1]))
				{
					path[len - 1] = '\0';
				}

				// find out the path to be found
				char path_copy[1024];
				strcpy(path_copy, path);
				int backslashCount = 0;
				for (int i = 0; i < strlen(path_copy); i++)
				{
					if (path_copy[i] == '/')
					{
						backslashCount++;
					}
				}

				// if the last character is backslash, then remove it
				int path_len = strlen(path_copy);
				if (path_copy[path_len - 1] == '/')
				{
					path_copy[path_len - 1] = '\0';
					backslashCount = backslashCount - 1;
				}

				if (backslashCount > 1)
				{
					char *lastSlash = strrchr(path_copy, '/');
					*lastSlash = '\0';
				}

				// find the storage server for the path
				int foundFlag = 0;
				char ip_Address_ss[16];
				int port_ss;
				printf("Path: %s\n", path_copy);
				PathToServerMap *s;
				HASH_FIND_STR(serversByPath, path_copy, s);
				if (s != NULL)
				{
					strcpy(ip_Address_ss, s->server.ipAddress);
					port_ss = s->server.nmPort;
					foundFlag = 1;
					printf("Found server for path %s\n", path);
					printf("IP: %s\n", ip_Address_ss);
					printf("Port: %d\n", port_ss);
				}
				else{
					foundFlag=1;
					strcpy(ip_Address_ss, storageServers[0].ipAddress);
					port_ss = storageServers[0].nmPort;
					printf("It will be created in Storage Server 1\n");
					printf("IP: %s\n", ip_Address_ss);
					printf("Port: %d\n", port_ss);
				}
				if (foundFlag == 1)
				{
					// connect to the storage server port and ip address
					int storageserversocket;
					struct sockaddr_in storageserveraddress;
					char storageserverreply[2000];

					// Create socket
					storageserversocket = socket(AF_INET, SOCK_STREAM, 0);
					if (storageserversocket == -1)
					{
						printf("Could not create socket");
					}

					storageserveraddress.sin_addr.s_addr = inet_addr(ip_Address_ss);
					storageserveraddress.sin_family = AF_INET;
					storageserveraddress.sin_port = htons(port_ss);

					// Connect to remote server
					if (connect(storageserversocket,
								(struct sockaddr *)&storageserveraddress,
								sizeof(storageserveraddress)) < 0)
					{
						perror("connect failed. Error");
						return NULL;
					}

					// Send some data
					if (send(storageserversocket, buffer_copy, strlen(buffer_copy), 0) < 0)
					{
						puts("Send failed");
						return NULL;
					}

					// Receive a reply from the server
					if (recv(storageserversocket, storageserverreply, 2000, 0) < 0)
					{
						puts("recv failed");
					}

					// Send back to client
					if (send(sock, storageserverreply, strlen(storageserverreply), 0) < 0)
					{
						puts("Send failed");
						return NULL;
					}

				}
				else if (foundFlag == 0)
				{

				}
			}
			else if (strcmp(tokenArray[0], "DELETE") == 0)
			{
				// finding out the storage server for the file
				char path[1024];
				strcpy(path, tokenArray[1]);
				// strip the path of white spaces
				int len = strlen(path);
				if (isspace(path[len - 1]))
				{
					path[len - 1] = '\0';
				}

				// find the storage server for the path
				int foundFlag = 0;
				char ip_Address_ss[16];
				int port_ss;
				PathToServerMap *s;
				HASH_FIND_STR(serversByPath, path, s);
				if (s != NULL)
				{
					strcpy(ip_Address_ss, s->server.ipAddress);
					port_ss = s->server.nmPort;
					foundFlag = 1;
					printf("Found server for path %s\n", path);
					printf("IP: %s\n", ip_Address_ss);
					printf("Port: %d\n", port_ss);
				}
				if (foundFlag == 1)
				{
					// connect to the storage server port and ip address
					int storageserversocket;
					struct sockaddr_in storageserveraddress;
					char storageserverreply[2000];

					// Create socket
					storageserversocket = socket(AF_INET, SOCK_STREAM, 0);
					if (storageserversocket == -1)
					{
						printf("Could not create socket");
					}

					storageserveraddress.sin_addr.s_addr = inet_addr(s->server.ipAddress);
					storageserveraddress.sin_family = AF_INET;
					storageserveraddress.sin_port = htons(port_ss);

					// Connect to remote server
					if (connect(storageserversocket,
								(struct sockaddr *)&storageserveraddress,
								sizeof(storageserveraddress)) < 0)
					{
						perror("connect failed. Error");
						return NULL;
					}

					// Send some data
					if (send(storageserversocket, buffer_copy, strlen(buffer_copy), 0) < 0)
					{
						puts("Send failed");
						return NULL;
					}

					// Receive a reply from the server
					if (recv(storageserversocket, storageserverreply, 2000, 0) < 0)
					{
						puts("recv failed");
					}

					// Send back to client
					if (send(sock, storageserverreply, strlen(storageserverreply), 0) < 0)
					{
						puts("Send failed");
						return NULL;
					}



				}
				else if (foundFlag == 0)
				{
					char reply[1024];
					strcpy(reply, "5");
					if (send(sock, reply, strlen(reply), 0) < 0)
					{
						puts("Send failed");
						return NULL;
					}
				}
			}
			else if (strcmp(tokenArray[0], "COPY") == 0)
			{
				for (int i = 0; i < 3; i++)
				{
					printf("Token %d: %s\n", i, tokenArray[i]);
				}
				// finding out the storage server for the file
				char sourcePath[1024] = {'\0'};
				strcpy(sourcePath, tokenArray[1]);
				printf("Source path: %s\n", sourcePath);
				char destinationPath[1024] = {'\0'};
				strcpy(destinationPath, tokenArray[2]);
				PathToServerMap *source;
				PathToServerMap *destination;
				char ip_Address_ss[16];
				int port_ss;
				if (isspace(sourcePath[strlen(sourcePath) - 1]))
				{
					sourcePath[strlen(sourcePath) - 1] = '\0';
				}

				if (isspace(destinationPath[strlen(destinationPath) - 1]))
				{
					destinationPath[strlen(destinationPath) - 1] = '\0';
				}

				int foundFlag = 0;
				PathToServerMap *s;
				printf("Source path: %s\n", sourcePath);
				printf("Destination path: %s\n", destinationPath);
				printf("length of source path: %d\n", strlen(sourcePath));
				printf("length of destination path: %d\n", strlen(destinationPath));
				for (int i = 0; i < storageServerCount; i++)
				{
					printf("Storage server %d\n", i);
					for (int j = 0; j < storageServers[i].numPaths; j++)
					{
						printf("Path %d: %s\n", j, storageServers[i].accessiblePaths[j]);
						printf("length of path: %d\n",
							   strlen(storageServers[i].accessiblePaths[j]));
						if (strncmp(storageServers[i].accessiblePaths[j],
									sourcePath,
									strlen(sourcePath)) == 0)
						{
							printf("Found source path\n");
							source = malloc(sizeof(PathToServerMap));
							strcpy(source->path, storageServers[i].accessiblePaths[j]);
							source->server = storageServers[i];

							foundFlag = 1;
							break;
						}
						if (foundFlag == 1)
						{
							break;
						}
					}
				}
				// HASH_FIND_STR(serversByPath, sourcePath, source);
				// if(s != NULL)
				// {
				// 	strcpy(ip_Address_ss, s->server.ipAddress);
				// 	port_ss = s->server.ssPort;
				// 	foundFlag = 1;
				// 	printf("Found server for path %s\n", sourcePath);
				// 	printf("IP: %s\n", ip_Address_ss);
				// 	printf("Port: %d\n", port_ss);
				// }
				printf("Found flagasdfasdf: %d\n", foundFlag);

				if (foundFlag == 0)
				{
					printf("Source path not found lol \n");
					return NULL;
				}

				foundFlag = 0;
				// HASH_FIND_STR(serversByPath, destinationPath, destination);
				// if(s != NULL)
				// {
				//     strcpy(ip_Address_ss, s->server.ipAddress);
				//     port_ss = s->server.ssPort;
				//     foundFlag = 1;
				//     printf("Found server for path %s\n", destinationPath);
				//     printf("IP: %s\n", ip_Address_ss);
				//     printf("Port: %d\n", port_ss);
				// }
				for (int i = 0; i < storageServerCount; i++)
				{
					// printf("Storage server %d\n", i);
					// printf("Number of paths: %d\n", storageServers[i].numPaths);
					for (int j = 0; j < storageServers[i].numPaths; j++)
					{
						// printf("Path %d: %s\n", j, storageServers[i].accessiblePaths[j]);
						if (strcmp(storageServers[i].accessiblePaths[j], destinationPath) == 0)
						{
							// If path is in accessiblePaths of storageServer
							// send read request to that storage server
							printf("Found destination path\n");
							destination = malloc(sizeof(PathToServerMap));
							strcpy(destination->path, storageServers[i].accessiblePaths[j]);
							destination->server = storageServers[i];

							foundFlag = 1;
							printf("Found server for path %s\n", destinationPath);
							break;
						}
					}
					if (foundFlag == 1)
					{
						printf("Found destination path\n");
						break;
					}
				}
				printf("Found flag: %d\n", foundFlag);

				if (foundFlag == 0)
				{
					char *parentDir = getDirectoryPath((char *)destinationPath);
					printf("Parent dir######: %s\n", parentDir);
					// search for parentDir in the storage servers
					for (int i = 0; i < storageServerCount; i++)
					{
						for (int j = 0; j < storageServers[i].numPaths; j++)
						{
							printf("Accessible path: %s\n", storageServers[i].accessiblePaths[j]);
							if (strcmp(storageServers[i].accessiblePaths[j], parentDir) == 0)
							{
								// If path is in accessiblePaths of storageServer
								// send read request to that storage server
								printf("Found parent dir\n");
								destination = malloc(sizeof(PathToServerMap));
								strcpy(destination->path, storageServers[i].accessiblePaths[j]);
								destination->server = storageServers[i];

								foundFlag = 1;
								printf("Found server for path %s\n", parentDir);
								break;
							}
						}
						if (foundFlag == 1)
						{
							printf("Found parent dir\n");
							break;
						}
					}
				}
				if (foundFlag == 0)
				{
					printf("Destination path not found\n");
					return NULL;
				}

				// send this information to the source storage server
				char reply[2000];
				memset(reply, '\0', sizeof(reply));
				sprintf(reply,
						"COPY %s %d %s %s",
						destination->server.ipAddress,
						destination->server.ssPort,
						sourcePath,
						destinationPath);
				printf("Reply: %s\n", reply);
				int sock;
				struct sockaddr_in server;
				char server_reply[2000] = {'\0'};

				// Create socket
				sock = socket(AF_INET, SOCK_STREAM, 0);
				if (sock == -1)
				{
					printf("Could not create socket");
				}

				server.sin_addr.s_addr = inet_addr(source->server.ipAddress);
				server.sin_family = AF_INET;
				server.sin_port = htons(source->server.nmPort);

				// Connect to remote server
				if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
				{
					perror("connect failed. Error");
					return NULL;
				}

				// Send some data
				printf("The length of reply is %d\n", strlen(reply));
				if (send(sock, reply, strlen(reply), 0) < 0)
				{
					puts("Send failed");
					return NULL;
				}

				close(sock);
			}
			else if (strcmp(tokenArray[0], "LISTALL") == 0)
			{
				// listing all the accessible paths
				char response[40960]; // sending one at a time
				response[0] = '\0';
				strcat(response, "*");

				for (int i = 0; i < storageServerCount; i++)
				{
					char index[5];
					snprintf(index, sizeof(index), "%d", i + 1);
					strcat(response, index);
					for (int j = 0; j < storageServers[i].numPaths; j++)
					{
						strcat(response, "$");
						strcat(response, storageServers[i].accessiblePaths[j]);
					}
					strcat(response, "*");
				}

				if (send(sock, response, strlen(response), 0) < 0)
				{
					puts("Send failed");
					return NULL;
				}
			}
		}
		if (readSize < 0)
		{
			perror("recv failed");
			close(sock);
			free(socketDesc);
			return NULL;
		}
		else if (readSize == 0)
		{
			printf("Client disconnected\n");
			break;
		}
	}
	close(sock);
	free(socketDesc);
	return NULL;
}

void *handleClientRequest()
{
	int server_fd, clientSocket;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(NMIPADDRESS);
	address.sin_port = htons(NAMING_CLIENT_LISTEN_PORT);

	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	if (listen(server_fd, MAX_CLIENTS) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	printf("Naming Server started listening...\n");

	while (1)
	{
		if ((clientSocket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
		{
			perror("accept");
			continue;
		}
		printf("Client connected...\n");

		int *new_sock = malloc(sizeof(int));
		*new_sock = clientSocket;

		pthread_t client_thread;
		if (pthread_create(&client_thread, NULL, handleClientInput, (void *)new_sock) < 0)
		{
			perror("could not create thread");
			free(new_sock);
		}

		pthread_detach(client_thread);
	}
}

// void sendCommandToStorageServers(const char *command) {
//     char word[1024]; // Adjust size as needed
//     char path[1024]; // Adjust size as needed

//     sscanf(command, "%s %s", word, path); // Extract WORD and PATH from the command
//     int sentFlag = 0;
//     for (int i = 0; i < storageServerCount; i++) {
//         if (strstr(storageServers[i].accessiblePaths, path) != NULL) {
//             // If path is in accessiblePaths of storageServers[i]
//             sendCommandToServer(storageServers[i].ipAddress, storageServers[i].clientPort, command);
//             sentFlag = 1;
//             break;
//         }
//     }
//     if(sentFlag==0) {
//         sendCommandToServer(storageServers[0].ipAddress, storageServers[0].clientPort, command);
//     }
// }

void sendCommandToServer(const char *serverIP, int port, const char *command)
{
	int sock;
	struct sockaddr_in server;
	char server_reply[2000];

	// Create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
	{
		printf("Could not create socket");
	}

	server.sin_addr.s_addr = inet_addr(serverIP);
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	// Connect to remote server
	if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
	{
		perror("connect failed. Error");
		return;
	}

	// Send some data
	if (send(sock, command, strlen(command), 0) < 0)
	{
		puts("Send failed");
		return;
	}

	// Receive a reply from the server
	if (recv(sock, server_reply, 2000, 0) < 0)
	{
		puts("recv failed");
	}

	puts("Server reply :");
	puts(server_reply);

	close(sock);
}

void createFileOrDirectory(const char *serverIP, int port, const char *path, int isDirectory)
{
	char command[1024];
	sprintf(command, "CREATE %s %d", path, isDirectory);
	sendCommandToServer(serverIP, port, command);
}

void deleteFileOrDirectory(const char *serverIP, int port, const char *path)
{
	char command[1024];
	sprintf(command, "DELETE %s", path);
	sendCommandToServer(serverIP, port, command);
}

void copyFileOrDirectory(const char *sourceIP,
						 int sourcePort,
						 const char *destinationIP,
						 int destinationPort,
						 const char *sourcePath,
						 const char *destinationPath)
{
	char command[1024];
	sprintf(
		command, "COPY %s %s:%d %s", sourcePath, destinationIP, destinationPort, destinationPath);
	sendCommandToServer(sourceIP, sourcePort, command);
}

int main()
{
	// Initialize the Naming Server
	initializeNamingServer();

	pthread_t storageThread, clientThread;

	// Create a thread to detect Storage Server
	if (pthread_create(&storageThread, NULL, startStorageServerListener, NULL))
	{
		fprintf(stderr, "Error creating storage server listener thread\n");
		return 1;
	}

	// Create a thread to handle client requests
	if (pthread_create(&clientThread, NULL, handleClientRequest, NULL))
	{
		fprintf(stderr, "Error creating client request handler thread\n");
		return 1;
	}

	// Wait for both threads to finish
	pthread_join(storageThread, NULL);
	pthread_join(clientThread, NULL);

	// The rest of the main function
	// ...

	return 0;
}
