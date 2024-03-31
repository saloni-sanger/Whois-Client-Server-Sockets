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

#include "protocol.h"

#define MAXDATASIZE 1000 //might need to change //idea: send error if buffer to big

void process_cline_request(int, char**, struct request*, struct recipient*);

void get_addresses(struct addrinfo**, struct recipient*);

int connect_socket(struct addrinfo*);

void send_request(int, struct request*);

void recieve_and_print(int);

struct recipient { //the hostname and port in the command line is of the server "recipient" that we send to
    char* host;
    char* port;
};

int main(int argc, char* argv[]) {
    struct request req;
    struct recipient recip; //recipient server
    process_cline_request(argc, argv, &req, &recip);
    struct addrinfo* servinfo;
    get_addresses(&servinfo, &recip);
    int sockfd;
    if ((sockfd = connect_socket(servinfo)) == -1) {
        fprintf(stderr, "client: failed to connect\n"); 
        exit(1); //do i need to return???
    };
    send_request(sockfd, &req);//send request
    recieve_and_print(sockfd);
    return 0;
}

//maxdatasize set to 100, this may need to change, I'm supposed to be able to show the entire whois return
void recieve_and_print(int sockfd) {
    int numbytes;
    char buf[MAXDATASIZE]; //might need to change
    memset(&buf, 0, sizeof buf);
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, MSG_WAITALL)) == -1) { 
        perror("recv");
        exit(1); 
    }
    
    buf[numbytes] = '\0';

    printf("client: received '%s'\n",buf);

    close(sockfd);

    return;
}

void send_request(int sockfd, struct request* req) { //write() the request into the sockfd
    //strings don't have endianness
    //only need to call htonl() on our int/size variable, ints are 32 bit so long
    req->num_args = htonl(req->num_args); //adjust to netowrk order before sending, undo to host order when server recieves
    write(sockfd, (void*)req, sizeof(request)); //writes request into socket
}

int connect_socket(struct addrinfo* servinfo) {
    int sockfd; //can make negative to return an error  -----------------------<
    struct addrinfo* p;

    if (servinfo == NULL) { //if servinfo empty return error
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

        break; //reaches here is successfully connected to a socket
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n"); 
        return 2;
    }

    char s[INET_ADDRSTRLEN];
    inet_ntop(p->ai_family, &(((struct sockaddr_in*)p)->sin_addr), 
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo);

    return sockfd;
}

void get_addresses(struct addrinfo** servinfo, struct recipient* recip) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints); //make sure the struct is empty
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int rv; //return value

    if ((rv = getaddrinfo(recip->host, recip->port, &hints, servinfo)) != 0) { //send address of servinfo pointer, making it a pointer pointer
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
        exit(1);
    }
    return; //servinfo holds sockets available at the host:port requested from command line, now we want to connect and send request
    //then server will fork, execute, and return (return from its new_fd? how will client recieve)
}

void process_cline_request(int argc, char** argv, struct request* req, struct recipient* recip) { //send command and arguments ins truct only
    
    printf("argc: %d\n", argc); //%d for base 10
    for (int i = 0; i < argc; i++) {
        printf("argv [%d] = %s\n", i, argv[i]);
    }

    if (argc < 4 || argc > 14) {
        fprintf(stderr,"usage: requires at least 4 arguments and at most 14, including an option for this whois command.\n");
        exit(1);
    }

    char* connection = argv[1];
    char* token = std::strtok(connection, ":");
    printf("host = %s\n", token);
    recip->host = token;
    token = std::strtok(NULL, ":");
    if (token == NULL) { //no ":"
        fprintf(stderr,"usage: client hostname and port format should be hostname:server_port\n");
        exit(1);
    }
    printf("port = %s\n", token);
    recip->port = token; //keep port a char pointer bc that's the form it needs for subsequent functions
    /*
    size_t colon_pos = connection.find(':');
    if (colon_pos == string::npos) { //second argument is hostname:port, if colon not found, request invalid
        fprintf(stderr,"usage: client hostname and port format should be hostname:server_port\n");
        exit(1);
    }
    
    req.host = connection.substr(0, colon_pos); //fill out host and port
    req.port = stoi(connection.substr(colon_pos + 1));
    */

    char* command = argv[2];
    printf("%s\n", command); //string format, then variatic arguments so its % things in order
    //if (command != "whois") { //this is checking if pointer addresses are equivalent smh
    if (strcmp(command, "whois") != 0) {
        fprintf(stderr,"Internal error: the command is not supported!\n");
        exit(1);
    }
    //bounds check, need command line everything to be 50 or less characters so null character space isn't taken
    if (strlen(command) > MAX_ARG_LENGTH) {
        fprintf(stderr,"Internal error: command is over %d characters!\n", MAX_ARG_LENGTH);
        exit(1);
    }
    //use memcopy to copy contents at the pointer into req member, avoids us having to change char* to char __[51] <- weird
    memcpy(((void*)&(req->command)), (void*)command, strlen(command)+1); //+1 for null character when identifying size of string

    req->num_args = argc-3;
    for (int i = 4; i <= argc; i++) { //fill out arguments in request struct
        int arg_idx = i-4;
        int cmd_line_idx = i-1;
        //+1 for null character when identifying size of string
        //bounds check, need command line everything to be 50 or less characters so null character space isn't taken
        if (strlen(argv[cmd_line_idx]) > MAX_ARG_LENGTH) {
            fprintf(stderr,"Internal error: command argument is over %d characters!\n", MAX_ARG_LENGTH);
            exit(1);
        }
        memcpy(((void*)&(req->arguments[arg_idx])), (void*)(argv[cmd_line_idx]), strlen(argv[cmd_line_idx])+1);
    }
    
    return;
}