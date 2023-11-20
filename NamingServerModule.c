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

// File pointer for log file
FILE *logfile;
char *logmessage;

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

PathToServerMap* serversByPath = NULL;


// LRU Caching
typedef struct LRUCache {
    StorageServer data;
	char keyPath[100];
    struct LRUCache *prev, *next;
} LRUCache;


LRUCache *head = NULL;
int cacheSize = 0;
int cacheCapacity = 0;


void initializeLRUCache(int capacity) {
    head = NULL;
    cacheSize = 0;
    cacheCapacity = capacity;
}

int accessStorageServerCache(char* keyPath) {
    LRUCache* temp = head;
    LRUCache* prevNode = NULL;

    // Search for the server in the cache
    while (temp != NULL && strcmp(temp->keyPath, keyPath) != 0) {
        prevNode = temp;
        temp = temp->next;
    }

    if (temp == NULL) { // Server not found in cache
		return 0;
    }
	if (prevNode != NULL) {
		prevNode->next = temp->next;
		if (temp->next != NULL) {
			temp->next->prev = prevNode;
		}
		temp->next = head;
		temp->prev = NULL;
		head->prev = temp;
		head = temp;
	}
	return 1;
}

void addServertoCache(char* keyPath, StorageServer server) {
	LRUCache* newNode = (LRUCache*)malloc(sizeof(LRUCache));
	newNode->data = server;
	strcpy(newNode->keyPath, keyPath);
	newNode->next = head;
	newNode->prev = NULL;

	if (head != NULL) {
	    head->prev = newNode;
	}
	head = newNode;

	if (cacheSize == cacheCapacity) { // Remove least recently used server
	    LRUCache* toRemove = head;
	    while (toRemove->next != NULL) {
	        toRemove = toRemove->next;
	    }
	    if (toRemove->prev != NULL) {
	        toRemove->prev->next = NULL;
	    }
	    free(toRemove);
	} else {
	    cacheSize++;
	}
}

void freeLRUCache() {
    LRUCache* current = head;
    while (current != NULL) {
        LRUCache* next = current->next;
        free(current);
        current = next;
    }
    head = NULL;
    cacheSize = 0;
}


void loggingfunction()
{
	// Open the log file in append mode
	logfile = fopen("log.txt", "a");

	// If file could not be opened
	if (logfile == NULL)
	{
		printf("Could not open log file\n");
		return;
	}

	// Append to the log file, by adding a timestamp
	time_t ltime;
	ltime = time(NULL);
	char *time = asctime(localtime(&ltime));
	time[strlen(time) - 1] = '\0'; // Remove the newline character at the end

	// log message
	fprintf(logfile, "%s: %s\n", time, logmessage);

	// Close the log file
	fclose(logfile);

	// make log message pointer NULL
	logmessage = NULL;
}

void *handleClientRequest();
// FileSystem fileSystem[MAX_STORAGE_SERVERS];
StorageServer storageServers[MAX_STORAGE_SERVERS];
int storageServerCount = 0;

void initializeNamingServer()
{
	printf("Initializing Naming Server...\n");

	// check the existence of a log file called log.txt. If aready there, then delete it and create a new one
	if (access("log.txt", F_OK) != -1)
	{
		remove("log.txt");
	}
	logmessage = "Initializing Naming Server...";
	loggingfunction();
	// Initialize file system and directory structure
	// memset(fileSystem, 0, sizeof(fileSystem));

	// Initialize storage server list
	memset(storageServers, 0, sizeof(storageServers));
	storageServerCount = 0;
}

pthread_mutex_t storageServerMutex = PTHREAD_MUTEX_INITIALIZER;

void update_list_of_accessiblepaths(int index)
{
	// Removing Hashes for index SS
	PathToServerMap *current_server, *tmp;
	HASH_ITER(hh, serversByPath, current_server, tmp)
	{
		// if current_server->path is present in the list of storageServers[index].accessiblePaths then remove it
		for (int i = 0; i < storageServers[index].numPaths; i++)
		{
			if (strcmp(current_server->path, storageServers[index].accessiblePaths[i]) == 0)
			{
				HASH_DEL(serversByPath, current_server);
				free(current_server);
			}
		}
	}

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
	if (sock == -1)
	{
		printf("Could not create socket");
	}

	// Connect to remote server
	if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
	{
		perror("connect failed. Error");
		return;
	}
	// Send the request
	if (send(sock, message, strlen(message), 0) < 0)
	{
		puts("Send failed");
		return;
	}
	if (recv(sock, server_reply, 2000, 0) < 0)
	{
		puts("recv failed");
	}
	// this is the number of paths
	int numPaths = atoi(server_reply);
	storageServers[index].numPaths = numPaths;
	// send a request og numpatsh
	// receive the paths
	if (send(sock, server_reply, strlen(server_reply), 0) < 0)
	{
		puts("Send failed");
		return;
	}
	memset(server_reply, '\0', sizeof(server_reply));

	if (recv(sock, server_reply, 2000, 0) < 0)
	{
		puts("recv failed");
	}
	if (strcmp(server_reply, "OK") != 0)
	{
		printf("Error receiving paths\n");
		return;
	}

	memset(
		storageServers[index].accessiblePaths, '\0', sizeof(storageServers[index].accessiblePaths));
	for (int i = 0; i < numPaths; i++)
	{
		memset(server_reply, '\0', sizeof(server_reply));
		if (recv(sock, server_reply, 2000, 0) < 0)
		{
			puts("recv failed");

			return;
		}

		strcpy(storageServers[index].accessiblePaths[i], server_reply);
		char return_message[1000];
		strcpy(return_message, "OK");

		// Adding it in the Hash
		PathToServerMap *s = malloc(sizeof(PathToServerMap));
		strcpy(s->path, server_reply);

		// Copy the newServer data into the hash table entry
		s->server = storageServers[index]; // Direct copy of the server

		HASH_ADD_STR(serversByPath, path, s);
		// Adding Hash Ended


		if(send(sock, return_message, strlen(return_message), 0) < 0)
		{
			puts("Send failed");
			return;
		}
	}
	close(sock);
}

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
			char concatented_string1[1024];
			sprintf(concatented_string1, "Received storage server %d details:", storageServerCount);
			logmessage = concatented_string1;
			loggingfunction();

			char concatented_string[1024];
			printf("IP Address: %s\n", newServer.ipAddress);
			snprintf(concatented_string, sizeof(concatented_string), "IP Address: %s", newServer.ipAddress);
			logmessage = concatented_string;
			loggingfunction();

			printf("NM Port: %d\n", newServer.nmPort);
			snprintf(concatented_string, sizeof(concatented_string), "NM Port: %d", newServer.nmPort);
			logmessage = concatented_string;
			loggingfunction();

			printf("Client Port: %d\n", newServer.clientPort);
			snprintf(concatented_string, sizeof(concatented_string), "Client Port: %d", newServer.clientPort);
			logmessage = concatented_string;
			loggingfunction();

			// printf("Accessible Paths: %s\n", newServer.accessiblePaths);
			printf("NUM PATHS: %d\n", newServer.numPaths);
			snprintf(concatented_string, sizeof(concatented_string), "NUM PATHS: %d", newServer.numPaths);
			logmessage = concatented_string;
			loggingfunction();

			for (int path_no = 0; path_no < newServer.numPaths; path_no++)
			{
				printf("Accessible Paths: %s\n", newServer.accessiblePaths[path_no]);
				snprintf(concatented_string, sizeof(concatented_string), "Accessible Paths: %s", newServer.accessiblePaths[path_no]);
				logmessage = concatented_string;
				loggingfunction();
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
			logmessage = "Registration Successful";
			loggingfunction();
		}
		else
		{
			pthread_mutex_unlock(&storageServerMutex);
			// Send server limit reached message
			const char *limitMessage = "Storage server limit reached";
			logmessage = "Storage server limit reached";
			loggingfunction();
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
		logmessage = "Socket creation failed";
		loggingfunction(5);
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
		logmessage = "Bind failed";
		loggingfunction(5);
		exit(EXIT_FAILURE);
	}

	// Start listening for incoming connections
	if (listen(server_fd, MAX_STORAGE_SERVERS) < 0)
	{
		perror("listen");
		logmessage = "Listen failed";
		loggingfunction(5);
		exit(EXIT_FAILURE);
	}

	while (1)
	{
		if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
		{
			perror("accept");
			logmessage = "Accept failed";
			loggingfunction(5);
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
	// Checking LRU Cache
	if (accessStorageServerCache(path)) {
		// Malloc ServerByPath
		s = malloc(sizeof(PathToServerMap));
		// Set as head->data
		s->server = head->data;
		strcpy(s->path, path);
		foundFlag = 1;
	}
	else {
		HASH_FIND_STR(serversByPath, path, s);
	}
	if(s != NULL)
	{
		char t[1024];
		strcpy(ip_Address_ss, s->server.ipAddress);
		port_ss = s->server.clientPort;
		*foundFlag = 1;
		printf("Found server for path %s\n", path);
		snprintf(t, sizeof(t), "Found server for path %s", path);
		logmessage = t;
		loggingfunction();
		printf("IP: %s\n", ip_Address_ss);
		snprintf(t, sizeof(t), "IP: %s", ip_Address_ss);
		logmessage = t;
		loggingfunction();
		printf("Port: %d\n", port_ss);
		snprintf(t, sizeof(t), "Port: %d", port_ss);
		logmessage = t;
		loggingfunction();

		// Adding server to cache
		addServertoCache(path, s->server);
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
	int clientsock = sock;
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

			char t[1024];
			snprintf(t, sizeof(t), "Received command: %s", buffer);
			logmessage = t;
			loggingfunction();

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
				snprintf(t, sizeof(t), "Command: %s", tokenArray[0]);
				logmessage = t;
				loggingfunction();
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
				snprintf(t, sizeof(t), "Path: %s", path);
				logmessage = t;
				loggingfunction();


				// compare the paths of all storage servers from storageServers array
				// if the path is found in the accessiblePaths of a storage server

				int foundFlag = 0;
				char ip_Address_ss[16];
				int port_ss;

				PathToServerMap* s;

				// Checking LRU Cache
				if (accessStorageServerCache(path)) {
					// Malloc ServerByPath
					s = (PathToServerMap *)malloc(sizeof(PathToServerMap));
					// Set as head->data
					s->server = head->data;
					strcpy(s->path, path);
					foundFlag = 1;
				}
				else {
					HASH_FIND_STR(serversByPath, path, s);
				}

				if(s != NULL)
				{
					strcpy(ip_Address_ss, s->server.ipAddress);
					port_ss = s->server.clientPort;
					foundFlag = 1;
					printf("Found server for path %s\n", path);
					snprintf(t, sizeof(t), "Found server for path %s", path);
					logmessage = t;
					loggingfunction();
					printf("IP: %s\n", ip_Address_ss);
					snprintf(t, sizeof(t), "IP: %s", ip_Address_ss);
					logmessage = t;
					loggingfunction();
					printf("Port: %d\n", port_ss);
					snprintf(t, sizeof(t), "Port: %d", port_ss);
					logmessage = t;
					loggingfunction();

					// Adding server to cache
					addServertoCache(path, s->server);
				}

				if (foundFlag == 1)
				{
					// send the port and ip address back to the client
					char reply[1024];
					sprintf(reply, "%s %d", s->server.ipAddress, s->server.clientPort);
					if (send(sock, reply, strlen(reply), 0) < 0)
					{
						puts("Send failed");
						logmessage = "Send failed";
						loggingfunction();
						return NULL;
					}
				}
				else
				{
					char reply[1024];
					strcpy(reply, "File not Found");
					snprintf(t, sizeof(t), "File not Found");
					logmessage = t;
					loggingfunction();
					if (send(sock, reply, strlen(reply), 0) < 0)
					{
						puts("Send failed");
						logmessage = "Send failed";
						loggingfunction();
						return NULL;
					}
				}
			}
			else if (strcmp(tokenArray[0], "CREATE") == 0)
			{
				char t[1024];
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
				snprintf(t, sizeof(t), "Path: %s", path_copy);
				logmessage = t;
				loggingfunction();
				PathToServerMap *s;

				// Checking LRU Cache
				if (accessStorageServerCache(path)) {
					// Malloc ServerByPath
					s = malloc(sizeof(PathToServerMap));
					// Set as head->data
					s->server = head->data;
					strcpy(s->path, path);
					foundFlag = 1;
				}
				else {
					HASH_FIND_STR(serversByPath, path, s);
				}


				if (s != NULL)
				{
					strcpy(ip_Address_ss, s->server.ipAddress);
					port_ss = s->server.nmPort;
					foundFlag = 1;
					printf("Found server for path %s\n", path);
					snprintf(t, sizeof(t), "Found server for path %s", path);
					logmessage = t;
					loggingfunction();
					printf("IP: %s\n", ip_Address_ss);
					snprintf(t, sizeof(t), "IP: %s", ip_Address_ss);
					logmessage = t;
					loggingfunction();
					printf("Port: %d\n", port_ss);
					snprintf(t, sizeof(t), "Port: %d", port_ss);
					logmessage = t;
					loggingfunction();
				}
				else
				{
					foundFlag = 1;
					strcpy(ip_Address_ss, storageServers[0].ipAddress);
					port_ss = storageServers[0].nmPort;
					printf("It will be created in Storage Server 1\n");
					snprintf(t, sizeof(t), "It will be created in Storage Server 1");
					logmessage = t;
					loggingfunction();
					printf("IP: %s\n", ip_Address_ss);
					snprintf(t, sizeof(t), "IP: %s", ip_Address_ss);
					logmessage = t;
					loggingfunction();
					printf("Port: %d\n", port_ss);
					snprintf(t, sizeof(t), "Port: %d", port_ss);
					logmessage = t;
					loggingfunction();
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
						logmessage = "Could not create socket";
						loggingfunction();
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
						logmessage = "connect failed. Error";
						loggingfunction();
						return NULL;
					}

					// Send some data
					if (send(storageserversocket, buffer_copy, strlen(buffer_copy), 0) < 0)
					{
						puts("Send failed");
						logmessage = "Send failed";
						loggingfunction();
						return NULL;
					}

					// Receive a reply from the server
					if (recv(storageserversocket, storageserverreply, 2000, 0) < 0)
					{
						puts("recv failed");
						logmessage = "recv failed";
						loggingfunction();
					}

					// Send back to client
					if (send(sock, storageserverreply, strlen(storageserverreply), 0) < 0)
					{
						puts("Send failed");
						logmessage = "Send failed";
						loggingfunction();
						return NULL;
					}
				}
				else if (foundFlag == 0)
				{
				}

				update_list_of_accessiblepaths(0);
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
				PathToServerMap* s;

				// Checking LRU Cache
				if (accessStorageServerCache(path)) {
					// Malloc ServerByPath
					s = malloc(sizeof(PathToServerMap));
					// Set as head->data
					s->server = head->data;
					strcpy(s->path, path);
					foundFlag = 1;
				}
				else {
					HASH_FIND_STR(serversByPath, path, s);
				}

				if(s != NULL)
				{
					strcpy(ip_Address_ss, s->server.ipAddress);
					port_ss = s->server.nmPort;
					foundFlag = 1;
					printf("Found server for path %s\n", path);
					snprintf(t, sizeof(t), "Found server for path %s", path);
					logmessage = t;
					loggingfunction();
					printf("IP: %s\n", ip_Address_ss);
					snprintf(t, sizeof(t), "IP: %s", ip_Address_ss);
					logmessage = t;
					loggingfunction();
					printf("Port: %d\n", port_ss);
					snprintf(t, sizeof(t), "Port: %d", port_ss);
					logmessage = t;
					loggingfunction();
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
						logmessage = "Could not create socket";
						loggingfunction();
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
						logmessage = "connect failed. Error";
						loggingfunction();
						return NULL;
					}

					// Send some data
					if (send(storageserversocket, buffer_copy, strlen(buffer_copy), 0) < 0)
					{
						puts("Send failed");
						logmessage = "Send failed";
						loggingfunction();
						return NULL;
					}

					// Receive a reply from the server
					if (recv(storageserversocket, storageserverreply, 2000, 0) < 0)
					{
						puts("recv failed");
						logmessage = "recv failed";
						loggingfunction();
					}

					// Send back to client
					if (send(sock, storageserverreply, strlen(storageserverreply), 0) < 0)
					{
						puts("Send failed");
						logmessage = "Send failed";
						loggingfunction();
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
						logmessage = "Send failed";
						loggingfunction();
						return NULL;
					}
				}
				update_list_of_accessiblepaths(0);
			}
			else if (strcmp(tokenArray[0], "COPY") == 0)
			{
				char t[1024];
				for (int i = 0; i < 3; i++)
				{
					printf("Token %d: %s\n", i, tokenArray[i]);
					sprintf(t, "Token %d: %s", i, tokenArray[i]);
					logmessage = t;
					loggingfunction();

				}
				// finding out the storage server for the file
				char sourcePath[1024] = {'\0'};
				strcpy(sourcePath, tokenArray[1]);
				printf("Source path: %s\n", sourcePath);
				sprintf(t, "Source path: %s", sourcePath);
				logmessage = t;
				loggingfunction();
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
				sprintf(t, "Source path: %s", sourcePath);
				logmessage = t;
				loggingfunction();
				printf("Destination path: %s\n", destinationPath);
				sprintf(t, "Destination path: %s", destinationPath);
				logmessage = t;
				loggingfunction();
				printf("length of source path: %d\n", strlen(sourcePath));
				sprintf(t, "length of source path: %d", strlen(sourcePath));
				logmessage = t;
				loggingfunction();
				printf("length of destination path: %d\n", strlen(destinationPath));
				sprintf(t, "length of destination path: %d", strlen(destinationPath));
				logmessage = t;
				loggingfunction();
				for (int i = 0; i < storageServerCount; i++)
				{
					printf("Storage server %d\n", i);
					sprintf(t, "Storage server %d", i);
					logmessage = t;
					loggingfunction();
					for (int j = 0; j < storageServers[i].numPaths; j++)
					{
						printf("Path %d: %s\n", j, storageServers[i].accessiblePaths[j]);
						sprintf(t, "Path %d: %s", j, storageServers[i].accessiblePaths[j]);
						logmessage = t;
						loggingfunction();
						printf("length of path: %d\n",
							   strlen(storageServers[i].accessiblePaths[j]));
						sprintf(t, "length of path: %d", strlen(storageServers[i].accessiblePaths[j]));
						logmessage = t;
						loggingfunction();
						if (strncmp(storageServers[i].accessiblePaths[j],
									sourcePath,
									strlen(sourcePath)) == 0)
						{
							printf("Found source path\n");
							sprintf(t, "Found source path");
							logmessage = t;
							loggingfunction();

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
				sprintf(t, "Found flagasdfasdf: %d", foundFlag);
				logmessage = t;
				loggingfunction();

				if (foundFlag == 0)
				{
					printf("Source path not found lol \n");
					sprintf(t, "Source path not found lol");
					logmessage = t;
					loggingfunction();
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
							sprintf(t, "Found destination path");
							logmessage = t;
							loggingfunction();
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
				// printf("Reply: %s\n", reply);
				int serversock;
				struct sockaddr_in server;
				char server_reply[2000] = {'\0'};

				// Create socket
				serversock = socket(AF_INET, SOCK_STREAM, 0);
				if (serversock == -1)
				{
					printf("Could not create socket");
				}

				server.sin_addr.s_addr = inet_addr(source->server.ipAddress);
				server.sin_family = AF_INET;
				server.sin_port = htons(source->server.nmPort);

				// Connect to remote server
				if (connect(serversock, (struct sockaddr *)&server, sizeof(server)) < 0)
				{
					perror("connect failed. Error");
					return NULL;
				}

				// Send some data
				// printf("The length of reply is %d\n", strlen(reply));
				if (send(serversock, reply, strlen(reply), 0) < 0)
				{
					puts("Send failed");
					return NULL;
				}

				// Receive a reply from the server
				char s_reply[2000];
				memset(s_reply, '\0', sizeof(s_reply));
				if (recv(serversock, s_reply, 2000, 0) < 0)
				{
					puts("recv failed");
				}
				send(clientsock, s_reply, strlen(s_reply), 0);

				close(serversock);
				update_list_of_accessiblepaths(0);
			}
			else if (strcmp(tokenArray[0], "LISTALL") == 0)
			{
				char t[1024];

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
					logmessage = "Send failed";
					loggingfunction();

					return NULL;
				}
			}
		}
		if (readSize < 0)
		{
			perror("recv failed");
			logmessage = "recv failed";
			loggingfunction();
			close(sock);
			free(socketDesc);
			return NULL;
		}
		else if (readSize == 0)
		{
			printf("Client disconnected\n");
			logmessage = "Client disconnected";
			loggingfunction();
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
		logmessage = "Socket creation failed";
		loggingfunction();
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(NMIPADDRESS);
	address.sin_port = htons(NAMING_CLIENT_LISTEN_PORT);

	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		logmessage = "Bind failed";
		loggingfunction();
		exit(EXIT_FAILURE);
	}

	if (listen(server_fd, MAX_CLIENTS) < 0)
	{
		perror("listen");
		logmessage = "Listen failed";
		loggingfunction();
		exit(EXIT_FAILURE);
	}

	printf("Naming Server started listening...\n");
	logmessage = "Naming Server started listening...";
	loggingfunction();

	while (1)
	{
		if ((clientSocket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
		{
			perror("accept");
			logmessage = "Accept failed";
			loggingfunction();
			continue;
		}
		printf("Client connected...\n");
		logmessage = "Client connected...";
		loggingfunction();


		int *new_sock = malloc(sizeof(int));
		*new_sock = clientSocket;

		pthread_t client_thread;
		if (pthread_create(&client_thread, NULL, handleClientInput, (void *)new_sock) < 0)
		{
			perror("could not create thread");
			logmessage = "Could not create thread";
			loggingfunction();
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
		logmessage = "Could not create socket";
		loggingfunction();
	}

	server.sin_addr.s_addr = inet_addr(serverIP);
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	// Connect to remote server
	if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
	{
		perror("connect failed. Error");
		logmessage = "connect failed. Error";
		loggingfunction();
		return;
	}

	// Send some data
	if (send(sock, command, strlen(command), 0) < 0)
	{
		puts("Send failed");
		logmessage = "Send failed";
		loggingfunction();
		return;
	}

	// Receive a reply from the server
	if (recv(sock, server_reply, 2000, 0) < 0)
	{
		puts("recv failed");
		logmessage = "recv failed";
		loggingfunction();
	}

	puts("Server reply :");
	puts(server_reply);

	char sr[1024];
	snprintf(sr, sizeof(sr), "Server reply: %s", server_reply);
	logmessage = sr;
	loggingfunction();

	close(sock);
}

void createFileOrDirectory(const char *serverIP, int port, const char *path, int isDirectory)
{
	char command[1024];
	sprintf(command, "CREATE %s %d", path, isDirectory);
	logmessage = command;
	loggingfunction();
	sendCommandToServer(serverIP, port, command);
}

void deleteFileOrDirectory(const char *serverIP, int port, const char *path)
{
	char command[1024];
	sprintf(command, "DELETE %s", path);
	logmessage = command;
	loggingfunction();
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
	logmessage = command;
	loggingfunction();
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
