/*
multi-client server
use fork() to create child processes that handle each client's connection and perform send and recieve tasks
use linux whois service in your server program
use execvp() or other to run the whois daemin in the server program
output generated sent back to client, use dup2() to direct output through socket connection

close TCP connection with whoisclient after response message sent
required to support MINIMUM 20 concurrent clients

command not "whois"? send error msg 'Internal error: the command is not supported!'
*/

/*
cat printing out file contents =, can also concatenate the files
netcat (nc) "like cat but over the network"
nc <hostname> <port>
opens command prompt wherer you hit enter to SEND text over TCP to that hostname:port

nc -l port
listens on that port

nc -l 10488 -o test.txt, then run client, can see output in file

nc 127.0.0.1 10488 for exampel
*/

//Credit: starter code from "Beej's Guide to Network Programming Using Internet Sockets"
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

#define BACKLOG 20 //minimum 20 concurrent connections
#define ID "4137153"


void get_addresses(struct addrinfo**);

int make_bound_socket(struct addrinfo*); // -----------------------<

void prepare_for_connection(int, struct sigaction*);

void sigchld_handler(int);

void main_accept_loop(int);

void recieve_and_dispatch(int);

void execute_and_send(int, struct request*);

int main(void) {
    struct addrinfo* servinfo;
    get_addresses(&servinfo); //mutates servinfo, no return needed

    if (servinfo == NULL) { //if servinfo empty return error
        perror("two: server: no resources"); 
        return -1;
    }

    int sockfd;
    if ((sockfd = make_bound_socket(servinfo)) == -1) {
        fprintf(stderr, "server: failed to bind\n"); 
        exit(1);
    };
    //being here means socket has binded, ready to listen
    struct sigaction sa; 
    prepare_for_connection(sockfd, &sa);

    freeaddrinfo(servinfo);
    main_accept_loop(sockfd);

    return 0;
}

void main_accept_loop(int sockfd) {
    int new_fd; // listen on sock_fd, new connection on new_fd 
    struct sockaddr_storage their_addr; // connector's address information 
    socklen_t sin_size;
    char s[INET_ADDRSTRLEN]; //IPv4

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

        if (outer_chld_status == 0) { // this is the child process now, if fork returned 0
            close(sockfd); // child doesn't need copy of the listener 
            recieve_and_dispatch(new_fd);

            // maybe insert a waitpid on the pid of the process doing the execve to make sure its finished writing by the time we close()------------<
            // close(new_fd);
            exit(0); 
        } else {
            close(new_fd);
        }
    }
}

void recieve_and_dispatch(int new_fd) { //inside outer fork
    //recieve request struct
    struct request req;
    int rv = recv(new_fd, (void*)&req, sizeof(struct request), MSG_WAITALL);
    if (rv <= 0) { //error check for recv()
        fprintf(stderr, "server: recieve within outer fork\n"); 
        exit(1); 
    }
    //return to host order
    req.num_args = ntohl(req.num_args);
    
    //testing
    printf("outer child command: %s\n", req.command);
    printf("num_args: %d\n", req.num_args);
    for (int i = 0; i < req.num_args; i++) {
        printf("argument %d: %s\n", i+1, req.arguments[i]);
    }

    //execvp, function takes the name of a UNIX command to run as the first argument

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

void execute_and_send(int new_fd, struct request* req) { //inside inner fork
    //execvp take  a null terminated, char* array. it's easier for client to send char[], so we need to convert it 
    //to a char*[] for the execvp() call. This also allows us to set a NULL and shorten the array if necessary, bc we did not
    //memset the argument array to 0 initially.
    //+1 for null termination
    char* arg_ptrs[(req->num_args)+2]; //request array already checked for size of each string before sending
    arg_ptrs[0] = req->command;
    for (int i = 0; i < req->num_args; i++) {
        arg_ptrs[i+1] = req->arguments[i];
    }
    arg_ptrs[(req->num_args)+1] = NULL;

    //make sure we transferred from char[] to char* correctly (w/ NULL term)
    printf("inner child command: %s\n", req->command); 
    for (int i = 0; i < req->num_args+1; i++) {
        printf("argument [%d] = %s\n", i+1, arg_ptrs[i]); 
    }

    //redirect standard out to socket before execvp terminates this process
    dup2(new_fd, STDOUT_FILENO);
    dup2(new_fd, STDERR_FILENO);

    int status_code = execvp(req->command, arg_ptrs);
    if (status_code == -1) {
        printf("Process did not terminate correctly\n");
        exit(1);
    }
}

void prepare_for_connection(int sockfd, struct sigaction* sa) {
    if (listen(sockfd, BACKLOG) == -1) { 
        perror("listen");
        exit(1);
    }

    sa->sa_handler = sigchld_handler; // reap all dead processes 
    sigemptyset(&sa->sa_mask); 
    sa->sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, sa, NULL) == -1) { //second param is actually supposed to be a pointer, so can just give it sa
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");
}

int make_bound_socket(struct addrinfo* servinfo) { // -----------------------<
    int sockfd; //can make negative to return an error  -----------------------<
    struct addrinfo* p;
    int yes = 1;

    if (servinfo == NULL) { //if servinfo empty return error
        perror("server: no resources");
        return -1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) { //loop through results and bind to first we can
        //as long as servinfo has at least 1 struct, sockfd can be returned because it will be -1 here
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) { //try to create socket with struct from servinfo list
            perror("server: socket");
            continue;
        }
        //setsockopt()
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, 
                sizeof(yes)) == -1) { //allow multiple connections on one port
            perror("setsockopt");
            exit(1); 
            //why exit here instead of continue??
        }
        //bind()

        char s[INET_ADDRSTRLEN]; //testing
        inet_ntop(p->ai_family, &(((struct sockaddr_in*)(struct sockaddr *)&p)->sin_addr), 
                s, sizeof s);
        printf("ip %s\n", s);

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) { 
            close(sockfd);
            perror("server: bind");
            continue; 
        }

        break; //successfully did the 3 functions for a socket, move on
    }

    if(p == NULL) {
        fprintf(stderr, "server: failed to bind\n"); 
        exit(1); 
    }

    return sockfd;
}

void jank_itoa(char* dest, int i){ //rename before turnin -----------------------<
    sprintf(dest, "%d", i);
}

void get_addresses(struct addrinfo** servinfo) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints); //make sure the struct is empty
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv; //return value
    char port[6];
    jank_itoa(port, allocate_port(ID)); //allocate_port takes &char* but returns int, we need char* again

    printf("attempting to start on port %s", port);
    std::cout << std::endl;

    if ((rv = getaddrinfo(NULL, port, &hints, servinfo)) != 0) { //send address of servinfo pointer, making it a pointer pointer
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
        exit(1);
    }

    return;
}

void sigchld_handler(int s) {
    // waitpid() might overwrite errno, so we save and restore it:
    //errno is a weird globasl variable needs to not be changed by waitpid()
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno; 
}


//set up structs using getaddrinfo, return result/servinfo

//loop through servinfo and bind to the first socket we can (remember to discard useless stuff)
/*
servinfo is a linkedlist of structs
p-> is accessing stuff in one of the structs
the calls inside if statements still run, there is a catch in case they fail
create socket, set socket options, bind
set socket options, setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) sets SO_REUSEADDR to true, allowing multiple connections to one port
    protocol level SOL_SOCKET, set options to the socket level
    can set any level of options with this one function
//freeaddrinfo(result from earlier)

sigaction sa; //strcuture that specifies how to handle a signal
sa.sa_handler = sigchld_handler;
    //we define how a child process is handled after it exits, this prevents main process from being forced to exit, so we can listen and accept new requests :)
sigemptyset(&sa.sa_mask); //says to not block any signals, sa_mask is set of signals to be blocked
backlog 20
SA_RESTART makes sure that it won't fail one child process terminates, will continue accept/recieve/fork loop

sigchld_handler waits until child stuff is cleaned up (just use theirs)
(-1, _, _) wait for any child process
WNOHANG return immediately if no child exited
now we've built a struct to send into sigaction() which actually sets our parameters

replace get_in_addr with &(((struct sockaddr_in*)sa)->sin_addr)
*/