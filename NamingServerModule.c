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
	int nmPort; // Port for NM Connection
	int clientPort; // Port for Client Connection
	int ssPort_send; // Port for SS Connection
	int ssPort_recv; // Port for SS Connection
	int numPaths;
	char accessiblePaths[500][100]; // List of accessible paths
	// Other metadata as needed
	UT_hash_handle hh; // Hash handle for uthash
} StorageServer;

typedef struct PathToServerMap
{
	char path[1000]; // The key
	StorageServer server; // The value
	UT_hash_handle hh; // Makes this structure hashable
} PathToServerMap;

typedef struct ThreadArgs
{
	char* ipAddress;
	int port;
	char buffer[1024];
	int is_original;
	int clientsock;
	int storage_server_index;
	int task; // 0 -> WRITE 1 -> CREATE 2 -> DELETE
} ThreadArgs;
PathToServerMap* serversByPath = NULL;

void* handleClientRequest();
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

// LRU Caching
typedef struct LRUCache
{
	StorageServer data;
	char keyPath[100];
	struct LRUCache *prev, *next;
} LRUCache;

LRUCache* head = NULL;
int cacheSize = 0;
int cacheCapacity = 0;

void initializeLRUCache(int capacity)
{
	head = NULL;
	cacheSize = 0;
	cacheCapacity = capacity;
}

int accessStorageServerCache(char* keyPath)
{
	LRUCache* temp = head;
	LRUCache* prevNode = NULL;
	int ct = 0;
	// Search for the server in the cache
	while(temp != NULL && strcmp(temp->keyPath, keyPath) != 0)
	{
		prevNode = temp;
		temp = temp->next;
		ct++;
		if(ct == 11)
		{
			break;
		}
	}

	if(temp == NULL)
	{ // Server not found in cache
		return 0;
	}
	if(prevNode != NULL)
	{
		prevNode->next = temp->next;
		if(temp->next != NULL)
		{
			temp->next->prev = prevNode;
		}
		temp->next = head;
		temp->prev = NULL;
		head->prev = temp;
		head = temp;
	}
	return 1;
}

void addServertoCache(char* keyPath, StorageServer server)
{
	LRUCache* newNode = (LRUCache*)malloc(sizeof(LRUCache));
	newNode->data = server;
	strcpy(newNode->keyPath, keyPath);
	newNode->next = head;
	newNode->prev = NULL;

	if(head != NULL)
	{
		head->prev = newNode;
	}
	head = newNode;

	if(cacheSize == cacheCapacity)
	{ // Remove least recently used server
		LRUCache* toRemove = head;
		while(toRemove->next != NULL)
		{
			toRemove = toRemove->next;
		}
		if(toRemove->prev != NULL)
		{
			toRemove->prev->next = NULL;
		}
		free(toRemove);
	}
	else
	{
		cacheSize++;
	}
}

void freeLRUCache()
{
	LRUCache* current = head;
	while(current != NULL)
	{
		LRUCache* next = current->next;
		free(current);
		current = next;
	}
	head = NULL;
	cacheSize = 0;
}

void update_list_of_accessiblepaths(int index)
{
	// connect the storage server and get the list of accessible paths
	int sock;
	struct sockaddr_in server;
	char server_reply[2000];
	server.sin_addr.s_addr = inet_addr(storageServers[index].ipAddress);
	server.sin_family = AF_INET;
	server.sin_port = htons(storageServers[index].nmPort);
	// send a request like "GETPATHS"
	char message[1000];
	memset(message, '\0', sizeof(message));
	strcpy(message, "GETPATHS server");

	// Create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1)
	{
		printf("Could not create socket");
	}

	// Connect to remote server
	if(connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0)
	{
		perror("connect failed. Error");
		return;
	}
	// Send the request
	if(send(sock, message, strlen(message), 0) < 0)
	{
		puts("Send failed");
		return;
	}
	if(recv(sock, server_reply, 2000, 0) < 0)
	{
		puts("recv failed");
	}
	// this is the number of paths
	int numPaths = atoi(server_reply);
	storageServers[index].numPaths = numPaths;
	// send a request og numpatsh
	// receive the paths
	if(send(sock, server_reply, strlen(server_reply), 0) < 0)
	{
		puts("Send failed");
		return;
	}
	memset(server_reply, '\0', sizeof(server_reply));

	if(recv(sock, server_reply, 2000, 0) < 0)
	{
		puts("recv failed");
	}
	if(strcmp(server_reply, "OK") != 0)
	{
		printf("Error receiving paths\n");
		return;
	}

	memset(
		storageServers[index].accessiblePaths, '\0', sizeof(storageServers[index].accessiblePaths));
	for(int i = 0; i < numPaths; i++)
	{
		memset(server_reply, '\0', sizeof(server_reply));
		if(recv(sock, server_reply, 2000, 0) < 0)
		{
			puts("recv failed");

			return;
		}

		strcpy(storageServers[index].accessiblePaths[i], server_reply);
		char return_message[1000];
		strcpy(return_message, "OK");
		if(send(sock, return_message, strlen(return_message), 0) < 0)
		{
			puts("Send failed");
			return;
		}
	}
	close(sock);
}

void* handleStorageServer(void* socketDesc)
{
	int sock = *(int*)socketDesc;
	char buffer[sizeof(StorageServer)]; // Adjust size as needed
	int readSize;

	// Read data from the socket
	if((readSize = recv(sock, buffer, sizeof(buffer), 0)) > 0)
	{

		// Parse data to extract Storage Server details
		// For example, if the data is sent as a comma-separated string
		char* token;
		char* rest = buffer;
		StorageServer newServer;

		memcpy(&newServer, buffer, sizeof(StorageServer));
		// Lock mutex before updating global storage server array
		pthread_mutex_lock(&storageServerMutex);
		if(storageServerCount < MAX_STORAGE_SERVERS)
		{
			storageServers[storageServerCount++] = newServer;
			pthread_mutex_unlock(&storageServerMutex);

			// Printing
			printf("Received storage server details:\n");
			printf("IP Address: %s\n", newServer.ipAddress);
			printf("NM Port: %d\n", newServer.nmPort);
			printf("Client Port: %d\n", newServer.clientPort);
			printf("SS Port: %d\n", newServer.ssPort_send);
			printf("SS Port: %d\n", newServer.ssPort_recv);
			// printf("Accessible Paths: %s\n", newServer.accessiblePaths);
			printf("NUM PATHS: %d\n", newServer.numPaths);
			for(int path_no = 0; path_no < newServer.numPaths; path_no++)
			{
				printf("Accessible Paths: %s\n", newServer.accessiblePaths[path_no]);
			}

			for(int path_no = 0; path_no < newServer.numPaths; path_no++)
			{
				PathToServerMap* s = malloc(sizeof(PathToServerMap));
				strcpy(s->path, newServer.accessiblePaths[path_no]);

				// Copy the newServer data into the hash table entry
				s->server = newServer; // Direct copy of the server

				HASH_ADD_STR(serversByPath, path, s);
			}

			// Send ACK
			const char* ackMessage = "Registration Successful";
			send(sock, ackMessage, strlen(ackMessage), 0);
		}
		else
		{
			pthread_mutex_unlock(&storageServerMutex);
			// Send server limit reached message
			const char* limitMessage = "Storage server limit reached";
			send(sock, limitMessage, strlen(limitMessage), 0);
		}
	}

	close(sock);
	free(socketDesc);
	return 0;
}
// Thsi function is used to send
void send_request(char* destination_ip, int destination_port, char* buffer)
{
	int sock;
	struct sockaddr_in server;
	char server_reply[2000];

	// Create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1)
	{
		printf("Could not create socket");
	}

	server.sin_addr.s_addr = inet_addr(destination_ip);
	server.sin_family = AF_INET;
	server.sin_port = htons(destination_port);

	// Connect to remote server
	if(connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0)
	{
		perror("connect failed. Error");
		return;
	}

	// Send some data
	if(send(sock, buffer, strlen(buffer), 0) < 0)
	{
		puts("Send failed");
		return;
	}

	// Receive a reply from the server
	if(recv(sock, server_reply, 2000, 0) < 0)
	{
		puts("recv failed");
	}

	puts("Server reply :");
	puts(server_reply);

	close(sock);
}

void* startStorageServerListener()
{
	int server_fd, new_socket, *new_sock;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	// Creating socket file descriptor
	if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Bind the socket to the port
	address.sin_family = AF_INET;
	// address.sin_addr.s_addr = LOCALIPADDRESS;
	address.sin_addr.s_addr = inet_addr(NMIPADDRESS);

	address.sin_port = htons(NAMING_SS_LISTEN_PORT);

	if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	// Start listening for incoming connections
	if(listen(server_fd, MAX_STORAGE_SERVERS) < 0)
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

		pthread_t sn_thread;
		new_sock = malloc(sizeof(int));
		*new_sock = new_socket;

		if(pthread_create(&sn_thread, NULL, handleStorageServer, (void*)new_sock) < 0)
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

char* getDirectoryPath(const char* path)
{
	int length = strlen(path);

	// Find the last occurrence of '/'
	int lastSlashIndex = -1;
	for(int i = length - 1; i >= 0; i--)
	{
		if(path[i] == '/')
		{
			lastSlashIndex = i;
			break;
		}
	}

	if(lastSlashIndex != -1)
	{
		// Allocate memory for the new string
		char* directoryPath = (char*)malloc((lastSlashIndex + 1) * sizeof(char));

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

void removeWhitespace(char* inputString)
{
	// Initialize indices for reading and writing in the string
	int readIndex = 0;
	int writeIndex = 0;

	// Iterate through the string
	while(inputString[readIndex] != '\0')
	{
		// If the current character is not a whitespace character, copy it
		if(!isspace(inputString[readIndex]))
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
char* remove_prefix(const char* s, const char* prefix)
{
	size_t prefix_length = strlen(prefix);

	if(strncmp(s, prefix, prefix_length) == 0)
	{
		return strdup(s + prefix_length);
	}
	else
	{
		return strdup(s);
	}
}
void* send_request_async(void* arg)
{
	ThreadArgs* args = (ThreadArgs*)arg;
	//create a socket
	int sock;
	struct sockaddr_in server;
	char server_reply[2000];

	// Create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1)
	{
		printf("Could not create socket");
	}

	server.sin_addr.s_addr = inet_addr(args->ipAddress);
	server.sin_family = AF_INET;
	server.sin_port = htons(args->port);

	// Connect to remote server
	if(connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0)
	{
		perror("connect failed. Error");
		return NULL;
	}

	// Send some data
	if(send(sock, args->buffer, strlen(args->buffer), 0) < 0)
	{
		puts("Send failed");
		return NULL;
	}

	char storageserverreply[2000];
	// Receive a reply from the server
	memset(storageserverreply, '\0', sizeof(storageserverreply));
	if(recv(sock, storageserverreply, 2000, 0) < 0)
	{
		puts("recv failed");
	}

	printf("Reply received from storage server with id : %s ########## %d \n", storageserverreply, args->storage_server_index);

	// Its its not -1
	char command[100];
	char path[1000];

	sscanf(args->buffer, "%s %s", command, path);
	if(args->task == 1)
	{
		if(strcmp(storageserverreply, "-1") != 0)
		{
			// Add the path to the storage server
			strcpy(storageServers[args->storage_server_index]
					   .accessiblePaths[args->storage_server_index],
				   path);
			storageServers[args->storage_server_index].numPaths++;

			// Add the path to the storage server
			PathToServerMap* s = malloc(sizeof(PathToServerMap));
			strcpy(s->path, path);

			// Copy the newServer data into the hash table entry
			s->server = storageServers[args->storage_server_index]; // Direct copy of the server

			HASH_ADD_STR(serversByPath, path, s);

			// Send back to client
			if(args->is_original == 1)
			{
				if(send(args->clientsock, storageserverreply, strlen(storageserverreply), 0) < 0)
				{
					puts("Send failed");
					return NULL;
				}
			}
		}
	}
	else if(args->task == 2)
	{
		//DELETE task
		if(strcmp(storageserverreply, "-1") != 0)
		{
			
			// Send back to client
			if(args->is_original == 1)
			{
				if(send(args->clientsock, storageserverreply, strlen(storageserverreply), 0) < 0)
				{
					puts("Send failed");
					return NULL;
				}
			}
		}
	}

	return NULL;
}
// Function to remove leading and trailing whitespaces and reduce multiple spaces to a single space
void strip(char* str)
{
	int start = 0, end = strlen(str) - 1;

	// Remove leading whitespaces
	while(str[start] && isspace((unsigned char)str[start]))
	{
		start++;
	}
	// Remove leading whitespaces
	while(str[start] && isspace((unsigned char)str[start]))
	{
		start++;
	}

	// Remove trailing whitespaces
	while(end >= start && isspace((unsigned char)str[end]))
	{
		end--;
	}
	// Remove trailing whitespaces
	while(end >= start && isspace((unsigned char)str[end]))
	{
		end--;
	}

	// Condense multiple spaces and shift characters
	int i = 0, j = start;
	int inSpace = 0;
	while(j <= end)
	{
		if(isspace((unsigned char)str[j]))
		{
			if(!inSpace)
			{
				str[i++] = ' '; // Add one space and set flag
				inSpace = 1;
			}
		}
		else
		{
			str[i++] = str[j];
			inSpace = 0; // Reset space flag
		}
		j++;
	}
	str[i] = '\0'; // Null-terminate the string
	// Condense multiple spaces and shift characters
	i = 0;
	j = start;
	inSpace = 0;
	while(j <= end)
	{
		if(isspace((unsigned char)str[j]))
		{
			if(!inSpace)
			{
				str[i++] = ' '; // Add one space and set flag
				inSpace = 1;
			}
		}
		else
		{
			str[i++] = str[j];
			inSpace = 0; // Reset space flag
		}
		j++;
	}
	str[i] = '\0'; // Null-terminate the string
}

void extractPath(const char* input, char* path, size_t pathSize)
{
	// Find the first space
	const char* firstSpace = strchr(input, ' ');
	if(firstSpace == NULL)
	{
		printf("No spaces found.\n");
		return;
	}

	// Find the second space or the end of the string
	const char* secondSpace = strchr(firstSpace + 1, ' ');
	if(secondSpace == NULL)
	{
		// If only one space found, use the end of the string
		secondSpace = input + strlen(input);
	}

	// Calculate the length of the path
	size_t pathLength = secondSpace - firstSpace - 1;
	if(pathLength >= pathSize)
	{
		printf("Path buffer is too small.\n");
		return;
	}

	// Copy the path into the provided buffer
	strncpy(path, firstSpace + 1, pathLength);
	path[pathLength] = '\0'; // Null-terminate the string
}

void extractPathThird(const char* input, char* path, size_t pathSize)
{
	// Find the first space
	const char* firstSpace = strchr(input, ' ');
	if(firstSpace == NULL)
	{
		printf("No spaces found.\n");
		return;
	}

	// Find the second space
	const char* secondSpace = strchr(firstSpace + 1, ' ');
	if(secondSpace == NULL)
	{
		printf("Only one space found.\n");
		return;
	}

	// Find the third space or the end of the string
	const char* thirdSpace = strchr(secondSpace + 1, ' ');
	if(thirdSpace == NULL)
	{
		// If only two spaces found, use the end of the string
		thirdSpace = input + strlen(input);
	}

	// Calculate the length of the path
	size_t pathLength = thirdSpace - secondSpace - 1;
	if(pathLength >= pathSize)
	{
		printf("Path buffer is too small.\n");
		return;
	}

	// Copy the path into the provided buffer
	strncpy(path, secondSpace + 1, pathLength);
	path[pathLength] = '\0'; // Null-terminate the string
}

// void* handleClientInput(void* socketDesc)
// {
// 	int start = 0, end = strlen(str) - 1;

// 	// Remove leading whitespaces
// 	while(str[start] && isspace((unsigned char)str[start]))
// 	{
// 		start++;
// 	}

// 	// Remove trailing whitespaces
// 	while(end >= start && isspace((unsigned char)str[end]))
// 	{
// 		end--;
// 	}

// 	// Condense multiple spaces and shift characters
// 	int i = 0, j = start;
// 	int inSpace = 0;
// 	while(j <= end)
// 	{
// 		if(isspace((unsigned char)str[j]))
// 		{
// 			if(!inSpace)
// 			{
// 				str[i++] = ' '; // Add one space and set flag
// 				inSpace = 1;
// 			}
// 		}
// 		else
// 		{
// 			str[i++] = str[j];
// 			inSpace = 0; // Reset space flag
// 		}
// 		j++;
// 	}
// 	str[i] = '\0'; // Null-terminate the string
// }

void* handleClientInput(void* socketDesc);

void* handleClientInput(void* socketDesc)
{
	int sock = *(int*)socketDesc;
	int clientsock = sock;
	char buffer[1024];
	memset(buffer, '\0', sizeof(buffer));
	int readSize;
	while(1)
	{
		memset(buffer, '\0', sizeof(buffer));
		if((readSize = recv(sock, buffer, sizeof(buffer), 0)) > 0)
		{
			strip(buffer);
			char buffer_copy[1024];
			memset(buffer_copy, '\0', sizeof(buffer_copy));
			strcpy(buffer_copy, buffer);

			printf("Received command: %s\n", buffer);

			// // handling the command from client

			// //  tokenising the command
			// char* tokenArray[10];
			// char* token = strtok(buffer, " ");
			// int i = 0;
			// while(token != NULL)
			// {
			// 	removeWhitespace(token);
			// 	tokenArray[i++] = token;
			// 	token = strtok(NULL, " ");
			// }

			char path[1024];
			memset(path, '\0', sizeof(path));
			extractPath(buffer_copy, path, sizeof(path));

			//  checking the command
			if(strncmp(buffer_copy, "READ", 4) == 0 || strncmp(buffer_copy, "WRITE", 5) == 0 ||
			   strncmp(buffer_copy, "GETSIZE", 7) == 0)
			{
				printf("Command: %s\n", buffer_copy);
				printf("PATH : %s\n", path);
				printf("PATH : %s\n", path);
				// finding out the storage server for the file
				// path is tokenArray[1]
				// strcpy(path, tokenArray[1]);
				// // strip the path of white spaces
				// int len = strlen(path);
				// if(isspace(path[len - 1]))
				// {
				// 	path[len - 1] = '\0';
				// }

				// printf("Path: %s\n", path);

				// compare the paths of all storage servers from storageServers array
				// if the path is found in the accessiblePaths of a storage server

				int foundFlag = 0;
				char ip_Address_ss[16];
				memset(ip_Address_ss, '\0', sizeof(ip_Address_ss));
				int port_ss;

				PathToServerMap* s;
				HASH_FIND_STR(serversByPath, path, s);
				// Checking LRU Cache
				if(accessStorageServerCache(path))
				{
					// Malloc ServerByPath
					s = (PathToServerMap*)malloc(sizeof(PathToServerMap));
					// Set as head->data
					s->server = head->data;
					strcpy(s->path, path);
					foundFlag = 1;
				}
				if(s != NULL)
				{
					strcpy(ip_Address_ss, s->server.ipAddress);
					port_ss = s->server.clientPort;
					foundFlag = 1;
					printf("Found server for path %s\n", path);
					printf("IP: %s\n", ip_Address_ss);
					printf("Port: %d\n", port_ss);
				}

				if(foundFlag == 1)
				{
					// send the port and ip address back to the client
					char reply[1024];
					memset(reply, '\0', sizeof(reply));
					sprintf(reply, "%s %d", s->server.ipAddress, s->server.clientPort);

					if(send(sock, reply, strlen(reply), 0) < 0)
					{
						puts("Send failed");
						return NULL;
					}
				}
				else
				{
					char reply[1024];
					strcpy(reply, "File not Found");
					if(send(sock, reply, strlen(reply), 0) < 0)
					{
						puts("Send failed");
						return NULL;
					}
				}
			}
			else if(strncmp(buffer_copy, "CREATE", 6) == 0)
			{
				// char path[1024];
				// strcpy(path, tokenArray[1]);
				// // strip the path of white spaces
				// int len = strlen(path);
				// if(isspace(path[len - 1]))
				// {
				// 	path[len - 1] = '\0';
				// }

				// // find out the path to be found
				// char path_copy[1024];
				// strcpy(path_copy, path);
				// int backslashCount = 0;
				// for(int i = 0; i < strlen(path_copy); i++)
				// {
				// 	if(path_copy[i] == '/')
				// 	{
				// 		backslashCount++;
				// 	}
				// }

				// // if the last character is backslash, then remove it
				// int path_len = strlen(path_copy);
				// if(path_copy[path_len - 1] == '/')
				// {
				// 	path_copy[path_len - 1] = '\0';
				// 	backslashCount = backslashCount - 1;
				// }

				// if(backslashCount > 1)
				// {
				// 	char* lastSlash = strrchr(path_copy, '/');
				// 	*lastSlash = '\0';
				// }

				// find the storage server for the path
				int foundFlag = 0;
				char ip_Address_ss[16];
				int port_ss;
				printf("Path: %s\n", path);
				PathToServerMap* s;
				HASH_FIND_STR(serversByPath, path, s);
				// Checking LRU Cache
				if(accessStorageServerCache(path))
				{
					// Malloc ServerByPath
					s = (PathToServerMap*)malloc(sizeof(PathToServerMap));
					// Set as head->data
					s->server = head->data;
					strcpy(s->path, path);
					foundFlag = 1;
				}
				if(s != NULL)
				{
					strcpy(ip_Address_ss, s->server.ipAddress);
					port_ss = s->server.nmPort;
					foundFlag = 1;
					printf("Found server for path %s\n", path);
					printf("IP: %s\n", ip_Address_ss);
					printf("Port: %d\n", port_ss);
					printf("File Already Exists\n");
				}
				else
				{
					foundFlag = 0;
					strcpy(ip_Address_ss, storageServers[0].ipAddress);
					port_ss = storageServers[0].nmPort;
					printf("It will be created in Storage Server 1\n");
					printf("IP: %s\n", ip_Address_ss);
					printf("Port: %d\n", port_ss);
				}
				if(foundFlag == 0)
				{
					// connect to the storage server port and ip address

					// Connect to remote server
					if(storageServerCount >= 3)
					{

						int store_1 = 1;
						int store_2 = 2;

						char path1[1024];
						memset(path1, '\0', sizeof(path1));
						char path2[1024];
						memset(path2, '\0', sizeof(path2));
						char* without_prefix = remove_prefix(path, ".");
						printf("Without prefix: %s\n", without_prefix);
						sprintf(path1, "./Backup0%s", without_prefix);
						sprintf(path2, "./Backup1%s", without_prefix);
						printf("Path1: %s\n", path1);
						printf("Path2: %s\n", path2);

						//now we create 3 threads to send the request to all the 3 storage servers
						ThreadArgs* args1 = malloc(sizeof(ThreadArgs));
						args1->ipAddress = storageServers[store_1].ipAddress;
						args1->port = storageServers[store_1].nmPort;
						sprintf(args1->buffer, "CREATE %s", path1);
						args1->is_original = 0;
						args1->storage_server_index = store_1;
						args1->task = 1;
						pthread_t thread1;
						pthread_create(&thread1, NULL, send_request_async, (void*)args1);
						pthread_detach(thread1);

						ThreadArgs* args2 = malloc(sizeof(ThreadArgs));
						args2->ipAddress = storageServers[store_2].ipAddress;
						args2->port = storageServers[store_2].nmPort;
						sprintf(args2->buffer, "CREATE %s", path2);
						args2->is_original = 0;
						args2->storage_server_index = store_2;
						args2->task = 1;
						pthread_t thread2;
						pthread_create(&thread2, NULL, send_request_async, (void*)args2);
						pthread_detach(thread2);

						ThreadArgs* args3 = malloc(sizeof(ThreadArgs));
						args3->ipAddress = storageServers[0].ipAddress;
						args3->port = storageServers[0].nmPort;
						args3->is_original = 1;
						args3->clientsock = clientsock;
						args3->storage_server_index = 0;
						args3->task = 1;
						sprintf(args3->buffer, "CREATE %s", path);
						pthread_t thread3;
						pthread_create(&thread3, NULL, send_request_async, (void*)args3);
						pthread_detach(thread3);

						//now we wait for the 3 threads to finish
						pthread_join(thread1, NULL);
						pthread_join(thread2, NULL);
						pthread_join(thread3, NULL);

						//now we send the reply back to the client
					}
					else
					{
						int storageserversocket;
						struct sockaddr_in storageserveraddress;
						char storageserverreply[2000];
						int storage_server_index;

						// // Create socket
						storageserversocket = socket(AF_INET, SOCK_STREAM, 0);
						if(storageserversocket == -1)
						{
							printf("Could not create socket");
						}

						storageserveraddress.sin_addr.s_addr = inet_addr(ip_Address_ss);
						storageserveraddress.sin_family = AF_INET;
						storageserveraddress.sin_port = htons(port_ss);
						if(connect(storageserversocket,
								   (struct sockaddr*)&storageserveraddress,
								   sizeof(storageserveraddress)) < 0)
						{
							perror("connect failed. Error");
							return NULL;
						}

						// Send some data
						if(send(storageserversocket, buffer_copy, strlen(buffer_copy), 0) < 0)
						{
							puts("Send failed");
							return NULL;
						}

						// Receive a reply from the s/erver
						if(recv(storageserversocket, storageserverreply, 2000, 0) < 0)
						{
							puts("recv failed");
						}

						printf("Reply received from storage server: %s\n", storageserverreply);

						// Its its not -1
						if(strcmp(storageserverreply, "-1") != 0)
						{
							// Add the path to the storage server
							strcpy(storageServers[0].accessiblePaths[storageServers[0].numPaths],
								   path);
							storageServers[0].numPaths++;

							// Add the path to the storage server
							PathToServerMap* s = malloc(sizeof(PathToServerMap));
							strcpy(s->path, path);

							// Copy the newServer data into the hash table entry
							s->server = storageServers[0]; // Direct copy of the server

							HASH_ADD_STR(serversByPath, path, s);
						}

						// Send back to client
						if(send(sock, storageserverreply, strlen(storageserverreply), 0) < 0)
						{
							puts("Send failed");
							return NULL;
						}
					}
				}

				// update_list_of_accessiblepaths(0);
			}
			else if(strncmp(buffer_copy, "DELETE", 6) == 0)
			{
				// // finding out the storage server for the file

				// // strip the path of white spaces
				// int len = strlen(path);
				// if(isspace(path[len - 1]))
				// {
				// 	path[len - 1] = '\0';
				// }

				// find the storage server for the path
				int foundFlag = 0;
				char ip_Address_ss[16];
				int port_ss;
				PathToServerMap* s;
				HASH_FIND_STR(serversByPath, path, s);
				if(s != NULL)
				{
					strcpy(ip_Address_ss, s->server.ipAddress);
					port_ss = s->server.nmPort;
					foundFlag = 1;
					printf("Found server for path %s\n", path);
					printf("IP: %s\n", ip_Address_ss);
					printf("Port: %d\n", port_ss);
				}
				if(foundFlag == 1)
				{

					// we again create 3 threads to send the request to all the 3 storage servers
					if(storageServerCount >= 3)
					{
						ThreadArgs* args1 = malloc(sizeof(ThreadArgs));
						args1->ipAddress = storageServers[0].ipAddress;
						args1->port = storageServers[0].nmPort;
						sprintf(args1->buffer, "DELETE %s", path);
						args1->is_original = 1;
						args1->clientsock = clientsock;
						args1->storage_server_index = 0;
						args1->task = 2;
						pthread_t thread1;
						pthread_create(&thread1, NULL, send_request_async, (void*)args1);
						pthread_detach(thread1);

						// delete the backups
						char path1[1024];
						memset(path1, '\0', sizeof(path1));
						char path2[1024];
						memset(path2, '\0', sizeof(path2));
						char* without_prefix = remove_prefix(path, ".");
						printf("Without prefix: %s\n", without_prefix);
						sprintf(path1, "./Backup0%s", without_prefix);
						sprintf(path2, "./Backup1%s", without_prefix);
						printf("Path1: %s\n", path1);
						printf("Path2: %s\n", path2);

						//now we create 3 threads to send the request to all the 3 storage servers
						ThreadArgs* args2 = malloc(sizeof(ThreadArgs));
						args2->ipAddress = storageServers[1].ipAddress;
						args2->port = storageServers[1].nmPort;
						sprintf(args2->buffer, "DELETE %s", path1);
						args2->is_original = 0;
						args2->storage_server_index = 1;
						args2->task = 2;
						pthread_t thread2;
						pthread_create(&thread2, NULL, send_request_async, (void*)args2);
						pthread_detach(thread2);

						ThreadArgs* args3 = malloc(sizeof(ThreadArgs));
						args3->ipAddress = storageServers[2].ipAddress;
						args3->port = storageServers[2].nmPort;

						sprintf(args3->buffer, "DELETE %s", path2);
						args3->is_original = 0;
						args3->storage_server_index = 2;
						args3->task = 2;
						pthread_t thread3;
						pthread_create(&thread3, NULL, send_request_async, (void*)args3);
						pthread_detach(thread3);

						//now we wait for the 3 threads to finish
						pthread_join(thread1, NULL);
						pthread_join(thread2, NULL);
						pthread_join(thread3, NULL);

						update_list_of_accessiblepaths(0);
						update_list_of_accessiblepaths(1);
						update_list_of_accessiblepaths(2);
					}
					else
					{
						// connect to the storage server port and ip address
						int storageserversocket;
						struct sockaddr_in storageserveraddress;
						char storageserverreply[2000];

						// Create socket
						storageserversocket = socket(AF_INET, SOCK_STREAM, 0);
						if(storageserversocket == -1)
						{
							printf("Could not create socket");
						}

						storageserveraddress.sin_addr.s_addr = inet_addr(s->server.ipAddress);
						storageserveraddress.sin_family = AF_INET;
						storageserveraddress.sin_port = htons(port_ss);

						// Connect to remote server
						if(connect(storageserversocket,
								   (struct sockaddr*)&storageserveraddress,
								   sizeof(storageserveraddress)) < 0)
						{
							perror("connect failed. Error");
							return NULL;
						}

						// Send some data
						if(send(storageserversocket, buffer_copy, strlen(buffer_copy), 0) < 0)
						{
							puts("Send failed");
							return NULL;
						}

						// Receive a reply from the server
						if(recv(storageserversocket, storageserverreply, 2000, 0) < 0)
						{
							puts("recv failed");
						}

						// Send back to client
						if(send(sock, storageserverreply, strlen(storageserverreply), 0) < 0)
						{
							puts("Send failed");
							return NULL;
						}

						// find which storage server it belongs to 
						int storage_server_index;
						for(int i = 0; i < storageServerCount; i++)
						{
							if(strcmp(storageServers[i].ipAddress, ip_Address_ss) == 0 &&
							   storageServers[i].nmPort == port_ss)
							{
								storage_server_index = i;
								break;
							}
						}
						update_list_of_accessiblepaths(storage_server_index);
						
					}
				}
				else if(foundFlag == 0)
				{
					char reply[1024];
					strcpy(reply, "5");
					if(send(sock, reply, strlen(reply), 0) < 0)
					{
						puts("Send failed");
						return NULL;
					}
				}
				// update_list_of_accessiblepaths(0);
			}
			else if(strncmp(buffer_copy, "COPY", 4) == 0)
			{
				printf("COPY Started\n");
				char pathThird[1024];
				extractPathThird(buffer_copy, pathThird, sizeof(pathThird));

				printf("Path Third: %s\n", pathThird);

				char ip_Address_ss[16];
				int port_ss;
				printf("Path: %s\n", path);
				PathToServerMap* s;
				HASH_FIND_STR(serversByPath, path, s);
				if(s != NULL)
				{
					strcpy(ip_Address_ss, s->server.ipAddress);
					port_ss = s->server.ssPort_send;
					printf("Found server for path %s\n", path);
					printf("IP: %s\n", ip_Address_ss);
					printf("Port: %d\n", port_ss);
				}

				// char ip_Address_ss2[16];
				// int port_ss2;
				// printf("Path: %s\n", pathThird);
				// PathToServerMap* s2;
				// HASH_FIND_STR(serversByPath, pathThird, s1);
				// if(s2 != NULL)
				// {
				// 	strcpy(ip_Address_ss2, s2->server.ipAddress);
				// 	port_ss2 = s2->server.nmPort;
				// 	printf("Found server for path %s\n", pathThird);
				// 	printf("IP: %s\n", ip_Address_ss2);
				// 	printf("Port: %d\n", port_ss2);
				// }

				int s1Sock, s2Sock;
				struct sockaddr_in s1Addr, s2Addr;
				char buffer[1024];
				ssize_t bytesRead, bytesSent;

				// Create and connect socket to S1
				if((s1Sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
				{
					perror("Socket creation for S1 failed");
					exit(EXIT_FAILURE);
				}
				s1Addr.sin_family = AF_INET;
				s1Addr.sin_port = htons(port_ss);
				s1Addr.sin_addr.s_addr = inet_addr(ip_Address_ss);
				if(connect(s1Sock, (struct sockaddr*)&s1Addr, sizeof(s1Addr)) < 0)
				{
					perror("Connection to S1 failed");
					close(s1Sock);
					exit(EXIT_FAILURE);
				}

				// Create and connect socket to S2
				if((s2Sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
				{
					perror("Socket creation for S2 failed");
					exit(EXIT_FAILURE);
				}
				s2Addr.sin_family = AF_INET;
				s2Addr.sin_port = htons(storageServers[0].ssPort_recv);
				s2Addr.sin_addr.s_addr = inet_addr(storageServers[0].ipAddress);
				if(connect(s2Sock, (struct sockaddr*)&s2Addr, sizeof(s2Addr)) < 0)
				{
					perror("Connection to S2 failed");
					close(s2Sock);
					exit(EXIT_FAILURE);
				}

				printf("Sockets forming is started\n");

				// Send read request to S1
				send(s1Sock, path, strlen(path), 0);

				printf("1 Socket Formed\n");

				// Send write request to S2
				send(s2Sock, pathThird, strlen(pathThird), 0);

				printf("Sockets Formed For Copying\n");

				// Transfer data from S1 to S2 through NM
				while(1)
				{
					ssize_t bytesRead = recv(s1Sock, buffer, 1024, 0);
					if(bytesRead > 0)
					{
						printf("Sending\n");
						ssize_t bytesSent = send(s2Sock, buffer, bytesRead, 0);
						if(bytesSent < 0)
						{
							perror("Failed to send data to S2");
							break;
						}
						printf("Sent %ld bytes\n", bytesSent);
					}
					else if(bytesRead == 0)
					{
						printf("Connection closed by peer\n");
						break;
					}
					else
					{
						perror("recv failed");
						break;
					}
					printf("Next Iter\n");
				}

				printf("Data Transfer Complete\n");

				int numberToSend = 11;
				ssize_t bytesSent_1 = send(sock, &numberToSend, sizeof(numberToSend), 0);
				if(bytesSent_1 < 0)
				{
					perror("Failed to send data");
				}

				printf("Data Transfer Complete\n");
				close(s1Sock);
				printf("Socket Closed\n");

				// Close s2Sock to signal the Receiver
				close(s2Sock);
				printf("Socket Closed\n");
			}
			else if(strncmp(buffer_copy, "LISTALL", 7) == 0)
			{

				// listing all the accessible paths
				char response[40960]; // sending one at a time
				response[0] = '\0';
				strcat(response, "*");

				printf("Total Number of Storage Server: %d\n", storageServerCount);

				for(int i = 0; i < storageServerCount; i++)
				{
					char index[5];
					snprintf(index, sizeof(index), "%d", i + 1);
					strcat(response, index);
					for(int j = 0; j < storageServers[i].numPaths; j++)
					{
						strcat(response, "$");
						strcat(response, storageServers[i].accessiblePaths[j]);
					}
					strcat(response, "*");
				}
				if(send(sock, response, strlen(response), 0) < 0)
				{
					puts("Send failed");
					return NULL;
				}
			}
		}
		if(readSize < 0)
		{
			perror("recv failed");
			close(sock);
			free(socketDesc);
			return NULL;
		}
		else if(readSize == 0)
		{
			printf("Client disconnected\n");
			break;
		}
	}
	close(sock);
	free(socketDesc);
	return NULL;
}

void* handleClientRequest()
{
	int server_fd, clientSocket;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(NMIPADDRESS);
	address.sin_port = htons(NAMING_CLIENT_LISTEN_PORT);

	if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	if(listen(server_fd, MAX_CLIENTS) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	printf("Naming Server started listening...\n");

	while(1)
	{
		if((clientSocket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0)
		{
			perror("accept");
			continue;
		}
		printf("Client connected...\n");

		int* new_sock = malloc(sizeof(int));
		*new_sock = clientSocket;

		pthread_t client_thread;
		if(pthread_create(&client_thread, NULL, handleClientInput, (void*)new_sock) < 0)
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

void sendCommandToServer(const char* serverIP, int port, const char* command)
{
	int sock;
	struct sockaddr_in server;
	char server_reply[2000];

	// Create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1)
	{
		printf("Could not create socket");
	}

	server.sin_addr.s_addr = inet_addr(serverIP);
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	// Connect to remote server
	if(connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0)
	{
		perror("connect failed. Error");
		return;
	}

	// Send some data
	if(send(sock, command, strlen(command), 0) < 0)
	{
		puts("Send failed");
		return;
	}

	// Receive a reply from the server
	if(recv(sock, server_reply, 2000, 0) < 0)
	{
		puts("recv failed");
	}

	puts("Server reply :");
	puts(server_reply);

	close(sock);
}

void createFileOrDirectory(const char* serverIP, int port, const char* path, int isDirectory)
{
	char command[1024];
	sprintf(command, "CREATE %s %d", path, isDirectory);
	sendCommandToServer(serverIP, port, command);
}

void deleteFileOrDirectory(const char* serverIP, int port, const char* path)
{
	char command[1024];
	sprintf(command, "DELETE %s", path);
	sendCommandToServer(serverIP, port, command);
}

void copyFileOrDirectory(const char* sourceIP,
						 int sourcePort,
						 const char* destinationIP,
						 int destinationPort,
						 const char* sourcePath,
						 const char* destinationPath)
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
	if(pthread_create(&storageThread, NULL, startStorageServerListener, NULL))
	{
		fprintf(stderr, "Error creating storage server listener thread\n");
		return 1;
	}

	// Create a thread to handle client requests
	if(pthread_create(&clientThread, NULL, handleClientRequest, NULL))
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
