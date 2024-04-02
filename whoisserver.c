/*
Name: Saloni Sanger
Course: Computer Networks
Date: April 4th, 2024
File: whoisserver.c
Description: whoisserver is a multi-client server. 
    It uses fork() to create child processes that handle each 
    client's connection and perfom send and recieve tasks.
    It uses dup2() to redirect the output of running the linux whois
    command back to the client, with the requested arguments.
*/

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <errno.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h> 
#include <arpa/inet.h> 
#include <sys/wait.h> 
#include <signal.h>

#include "allocate.h"
#include "protocol.h"

#define BACKLOG 20 // minimum 20 concurrent connections
#define ID "4137153"


void get_addresses(struct addrinfo**);

int make_bound_socket(struct addrinfo*);

void prepare_for_connection(int, struct sigaction*);

void main_accept_loop(int);

void recieve_and_dispatch(int);

void execute_and_send(int, struct request*);

void sigchld_handler(int);

int main(void) {
    struct addrinfo* servinfo;
    get_addresses(&servinfo); // mutates servinfo, no return needed
    if (servinfo == NULL) { // if servinfo empty after retrieval, return error
        perror("two: server: no resources"); 
        return -1;
    }

    int sockfd;
    if ((sockfd = make_bound_socket(servinfo)) == -1) {
        fprintf(stderr, "server: failed to bind\n"); 
        exit(1);
    };

    // being here means socket has binded, ready to listen
    struct sigaction sa; // structure that specifies how to handle a signal
    prepare_for_connection(sockfd, &sa);

    freeaddrinfo(servinfo);

    main_accept_loop(sockfd);

    return 0;
}

void my_itoa(char* dest, int i){
    sprintf(dest, "%d", i);
}

void get_addresses(struct addrinfo** servinfo) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv; // return value
    char port[6];
    my_itoa(port, allocate_port(ID)); // allocate_port takes &char* but returns int, we need char* again

    printf("attempting to start on port %s", port);
    std::cout << std::endl;

    if ((rv = getaddrinfo(NULL, port, &hints, servinfo)) != 0) { // getaddrinfo() creates structs and places them in a linked list starting at servinfo
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
        exit(1);
    }

    return;
}

int make_bound_socket(struct addrinfo* servinfo) { 
    int sockfd; 
    struct addrinfo* p;
    int yes = 1;

    if (servinfo == NULL) { // if servinfo empty return error
        perror("server: no resources");
        return -1;
    }

    // loop through results and bind to first we can
    for(p = servinfo; p != NULL; p = p->ai_next) { 
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) { // try to create socket with struct from servinfo list
            perror("server: socket");
            continue;
        }
        // as long as servinfo has at least 1 struct, sockfd can be returned because it will be -1 here
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, 
                sizeof(yes)) == -1) { // allow multiple connections on one port
            perror("setsockopt");
            exit(1); 
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) { 
            close(sockfd);
            perror("server: bind");
            continue; 
        }

        break; // successfully did the 3 functions for a socket, move on
    }

    if(p == NULL) {
        fprintf(stderr, "server: failed to bind\n"); 
        exit(1); 
    }

    return sockfd;
}

void prepare_for_connection(int sockfd, struct sigaction* sa) {
    if (listen(sockfd, BACKLOG) == -1) { 
        perror("listen");
        exit(1);
    }

    /*
    We can define how a child process is handled after it exits, 
    this prevents the main process from being forced to exit, 
    so we can keep listening and accept new requests :)
    */
    sa->sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa->sa_mask); // says to not block any signals, sa_mask is set of signals to be blocked
    sa->sa_flags = SA_RESTART; // makes sure that program won't fail once child process terminates, accept/recieve/fork loop will continue
    if (sigaction(SIGCHLD, sa, NULL) == -1) { // now we've built the struct sa to send into sigaction(), which actually sets our parameters
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");
}

void main_accept_loop(int sockfd) {
    int new_fd; // listen on sock_fd, new connection on new_fd 
    struct sockaddr_storage their_addr; // connector's address information 
    socklen_t sin_size;
    char s[INET_ADDRSTRLEN]; // IPv4

    while(1) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        std::cout << "got request" << std::endl;

        if (new_fd == -1) {
            perror("accept");
            continue; 
        }

        inet_ntop(their_addr.ss_family, 
            &(((struct sockaddr_in*)(struct sockaddr *)&their_addr)->sin_addr), 
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        int outer_chld_status = fork();
        if(outer_chld_status < 0) {
            fprintf(stderr, "server: outer child failed to fork\n"); 
            exit(1); 
        }

        if (outer_chld_status == 0) { 
            // this is the child process now, if fork returned 0
            close(sockfd); // child doesn't need copy of the listener 
            recieve_and_dispatch(new_fd);
            exit(0); 
        } else {
            close(new_fd);
        }
    }
}

void recieve_and_dispatch(int new_fd) { // inside outer fork
    //recieve request struct
    struct request req;
    int rv = recv(new_fd, (void*)&req, sizeof(struct request), MSG_WAITALL);
    // error check for recv()
    if (rv <= 0) { 
        fprintf(stderr, "server: recieve within outer fork\n"); 
        exit(1); 
    }
    // return to host order
    // strings don't have endianness
    req.num_args = ntohl(req.num_args);

    int inner_chld_status = fork();
    if(inner_chld_status < 0) {
        fprintf(stderr, "server: inner child failed to fork\n"); 
        exit(1); 
    }

    if(inner_chld_status == 0) {
        execute_and_send(new_fd, &req);
    } else {
        close(new_fd);
    }
}

void execute_and_send(int new_fd, struct request* req) { // inside inner fork
    /* 
    execvp() take  a NULL terminated, char* array. it's easier for the client to send a char array, 
    so we need to convert it the char array from the request struct to to a char* array for the execvp() call. 
    This also allows us to set a NULL termination index and shorten the array if necessary. 
    Shortening the array might be necessary because we did not memset the argument array in the request
    structure to 0, so it may hold garbage data.
    */
    char* arg_ptrs[(req->num_args)+2]; // +1 because execvp() requires command name at beginning, +1 for NULL termination
    arg_ptrs[0] = req->command; 
    for (int i = 0; i < req->num_args; i++) {
        arg_ptrs[i+1] = req->arguments[i];
    }
    arg_ptrs[(req->num_args)+1] = NULL;

    // redirect standard out and error to socket before execvp() terminates this process
    dup2(new_fd, STDOUT_FILENO);
    dup2(new_fd, STDERR_FILENO);

    int status_code = execvp(req->command, arg_ptrs);
    if (status_code == -1) {
        printf("Process did not terminate correctly\n");
        exit(1);
    }
}

void sigchld_handler(int s) { //waits until child is cleaned up
    // waitpid() might overwrite errno, so we save and restore it:
    // errno is a weird global variable, it needs to not be changed by waitpid()
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0); // (-1, _, _) means wait for any child process, WNOHANG means to return immediately if no child exited
    errno = saved_errno; 
}