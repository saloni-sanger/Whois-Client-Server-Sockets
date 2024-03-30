/*
runs in command line
takes form:
whoisclient hostname:server_port whois [option] argument-list

sends command and its (option and) arguments to the whois server, running on server_port at computer hostname
recieve information
print it out

number of arguments for whois command will not exceed 10
length of each argument will not exceed 50 characters

whoisclient recieves command and argument from command line, constructs request message
sned it to server program, recieves response, displays it, do not modify response in any way
program should exit after displaying
*/

/* 

MY PORT NUMBER IS 10488

replace get_in_addr with &(((struct sockaddr_in*)sa)->sin_addr)
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

struct request process_cline_request(int, char**);

int get_addresses(struct addrinfo*, struct request);

struct request {
    char* host;
    char* port;
    //told we can assume max 10 arguments, each max 50 characters 
    char* arguments[11]; //includes option (whois [option] argument-list) 10+1 = 11
};

int main(int argc, char* argv[]) {
    /*
    int sockfd, numbytes;
    char buf[100]; //might need to change
    struct addrinfo *p; 
    int rv; //return value
    char s[INET_ADDRSTRLEN];
    */

    struct request req;
    req = process_cline_request(argc, argv);
    struct addrinfo *servinfo;
    get_addresses(servinfo, req);
}

int get_addresses(struct addrinfo* servinfo, struct request req) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints); //make sure the struct is empty
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int rv; //return value

    if ((rv = getaddrinfo(req.host, req.port, &hints, &servinfo)) != 0) { //send address of servinfo pointer, making it a pointer pointer
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
        return 1;
    }
    return 0;
}

struct request process_cline_request(int argc, char* argv[]) {
    struct request req;
    char* connection = argv[1];
    char* token = std::strtok(connection, ":");
    req.host = token;
    token = std::strtok(NULL, ":");
    if (token == NULL) { //no ":"
        fprintf(stderr,"usage: client hostname and port format should be hostname:server_port\n");
        exit(1);
    }
    req.port = token; //keep port a char pointer bc that's the form it needs for subsequent functions
    /*
    size_t colon_pos = connection.find(':');
    if (colon_pos == string::npos) { //second argument is hostname:port, if colon not found, request invalid
        fprintf(stderr,"usage: client hostname and port format should be hostname:server_port\n");
        exit(1);
    }
    
    req.host = connection.substr(0, colon_pos); //fill out host and port
    req.port = stoi(connection.substr(colon_pos + 1));
    */

    if (argv[2] != "whois") {
        fprintf(stderr,"Internal error: the command is not supported!'\n");
        exit(1);
    }

    for (int i = 4; i <= argc; i++) { //fill out arguments in request struct
        req.arguments[i-4] = argv[i-1];
    }
    
    return req;
}