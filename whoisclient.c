/*
Name: Saloni Sanger
Course: Computer Networks
Date: April 4th, 2024
File: whoisclient.c
Description: whoisclient recieves a command and argument from the command line, 
    constructs a request message, sends it to server program, 
    recieves the server's response, and displays the server's reponse.
*/

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <errno.h> 
#include <string.h> 
#include <netdb.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h>
#include <cstring>
#include <arpa/inet.h>

#include "protocol.h"

#define MAXDATASIZE 10000 

void process_cline_request(int, char**, struct request*, struct recipient*);

void get_addresses(struct addrinfo**, struct recipient*);

int connect_socket(struct addrinfo*);

void send_request(int, struct request*);

void recieve_and_print(int);

struct recipient { // the hostname and port from the command line request is the server/"recipient" that we send to
    char* host;
    char* port;
};

int main(int argc, char* argv[]) {
    struct request req;
    struct recipient recip; // recipient server
    process_cline_request(argc, argv, &req, &recip);

    struct addrinfo* servinfo;
    get_addresses(&servinfo, &recip);

    int sockfd;
    if ((sockfd = connect_socket(servinfo)) == -1) {
        fprintf(stderr, "client: failed to connect\n"); 
        exit(1);
    };

    send_request(sockfd, &req);

    recieve_and_print(sockfd);

    return 0;
}

void process_cline_request(int argc, char** argv, struct request* req, struct recipient* recip) {
    if (argc < 4 || argc > 14) {
        fprintf(stderr,"usage: requires at least 4 arguments and at most 14, including an option for the whois command.\n");
        exit(1);
    }

    char* connection = argv[1];
    char* token = std::strtok(connection, ":");
    printf("host = %s\n", token);
    recip->host = token;
    token = std::strtok(NULL, ":");
    if (token == NULL) { // no ":"
        fprintf(stderr,"usage: client hostname and port format should be hostname:server_port\n");
        exit(1);
    }
    printf("port = %s\n", token);
    recip->port = token; // keep port a char* bc that's the form it needs for subsequent functions

    char* command = argv[2];
    if (strcmp(command, "whois") != 0) {
        fprintf(stderr,"Internal error: the command is not supported!\n");
        exit(1);
    }

    /*
    Bounds check, need everything from the command line to be 50 characters or less
    so null character space isn't taken in request struct.
    */  
    if (strlen(command) > MAX_ARG_LENGTH) {
        fprintf(stderr,"Internal error: command is over %d characters!\n", MAX_ARG_LENGTH);
        exit(1);
    }

    // use memcopy to copy contents at the pointer into req member, avoids us having to change char* to char __[51] <- weird
    memcpy(((void*)&(req->command)), (void*)command, strlen(command)+1); // +1 for null character, identifying size of string

    req->num_args = argc-3;
    for (int i = 4; i <= argc; i++) { // fill out arguments in request struct
        int arg_idx = i-4;
        int cmd_line_idx = i-1;
        // continuing bounds checks
        if (strlen(argv[cmd_line_idx]) > MAX_ARG_LENGTH) {
            fprintf(stderr,"Internal error: command argument is over %d characters!\n", MAX_ARG_LENGTH);
            exit(1);
        }
        memcpy(((void*)&(req->arguments[arg_idx])), (void*)(argv[cmd_line_idx]), strlen(argv[cmd_line_idx])+1); // +1 for null character when identifying size of string
    }
    
    return;
}

void get_addresses(struct addrinfo** servinfo, struct recipient* recip) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int rv; // return value

    if ((rv = getaddrinfo(recip->host, recip->port, &hints, servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
        exit(1);
    }
    /*
    servinfo holds sockets available at the host:port combination requested from the command line,
    now we want to connect to one and send our request.
    Then, server will fork, execute, and return from its new_fd, which is connected to the same host:port combination.
    */
    return; 
}

int connect_socket(struct addrinfo* servinfo) {
    int sockfd; 
    struct addrinfo* p;

    if (servinfo == NULL) { // if servinfo empty return error
        perror("client: no resources");
        return -1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) { 
            perror("client: socket"); 
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) { 
            close(sockfd);
            perror("client: connect");
            continue; 
        }

        break; // reaches here if successfully connected to a socket
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n"); 
        return 2;
    }

    char s[INET_ADDRSTRLEN];
    inet_ntop(p->ai_family, &(((struct sockaddr_in*)(struct sockaddr *)&p)->sin_addr), 
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo);

    return sockfd;
}

void send_request(int sockfd, struct request* req) { // write() the request into the socket
    req->num_args = htonl(req->num_args); // strings don't have endianness, only need to convert our int/size variable from host to network byte order, ints are 32 bits -> long
    write(sockfd, (void*)req, sizeof(request)); // writes/sends request into socket
}

void recieve_and_print(int sockfd) {
    int numbytes;
    char buf[MAXDATASIZE]; 
    memset(&buf, 0, sizeof buf);
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) { 
        perror("recv");
        exit(1); 
    }
    
    buf[numbytes] = '\0';

    printf("client: received \n%s", buf);

    if (numbytes == MAXDATASIZE-1) {
        fprintf(stderr, "\n\nBuffer is full.\nMAXDATASIZE needs to be increased to see more.\nCurrent MAXDATASIZE value is %d.\n", MAXDATASIZE);
    }

    close(sockfd);

    return;
}