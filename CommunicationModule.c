#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFFER_SIZE 1024

int openSocket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }
    return sockfd;
}

void bindSocket(int sockfd, int port) {
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }
}

int acceptConnection(int sockfd) {
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    listen(sockfd, 5);
    int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) {
        perror("ERROR on accept");
        exit(1);
    }
    return newsockfd;
}

ssize_t readFromSocket(int sockfd, char *buffer, size_t length) {
    bzero(buffer, length);
    ssize_t n = read(sockfd, buffer, length-1);
    if (n < 0) {
        perror("ERROR reading from socket");
        exit(1);
    }
    return n;
}

ssize_t writeToSocket(int sockfd, char *message) {
    ssize_t n = write(sockfd, message, strlen(message));
    if (n < 0) {
        perror("ERROR writing to socket");
        exit(1);
    }
    return n;
}

void closeSocket(int sockfd) {
    close(sockfd);
}

int main(int argc, char *argv[]) {
    int sockfd, portno;
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    portno = atoi(argv[1]);

    sockfd = openSocket();
    bindSocket(sockfd, portno);
    
    // Example usage: accept connections and echo received messages back
    int newsockfd = acceptConnection(sockfd);
    char buffer[BUFFER_SIZE];
    readFromSocket(newsockfd, buffer, BUFFER_SIZE);
    writeToSocket(newsockfd, buffer);

    closeSocket(newsockfd);
    closeSocket(sockfd);

    return 0;
}
