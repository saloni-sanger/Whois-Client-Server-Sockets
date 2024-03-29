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

//custom structs to return multiple things

#define BACKLOG 20 //minimum 20 concurrent connections
#define ID "4137153"


int get_addresses(struct addrinfo*);

int make_bound_socket(struct addrinfo*); // -----------------------<

void prepare_for_connection(int, struct sigaction*);

void sigchld_handler(int);

void main_accept_loop(int, struct sigaction*);

int main(void) {
    struct addrinfo* servinfo;
    get_addresses(servinfo); //mutates servinfo, no return needed
    int sockfd;
    if ((sockfd = make_bound_socket(servinfo)) == -1) {
        fprintf(stderr, "server: failed to bind\n"); 
        exit(1); //do i need to return???
    };
    //being here means socket has binded, ready to listen
    struct sigaction* sa; //need to free this now
    prepare_for_connection(sockfd, sa);

    // make main loop function here, figure out params  -----------------------<
    main_accept_loop(sockfd, sa);

    //freesigaction(sa); //create this
    freeaddrinfo(servinfo);
    return 0;
}

void main_accept_loop(int sockfd, struct sigaction* sa) {
    int new_fd; // listen on sock_fd, new connection on new_fd 
    struct sockaddr_storage their_addr; // connector's address information 
    socklen_t sin_size;
    char s[INET_ADDRSTRLEN]; //IPv4

    while(1) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue; 
        }

        //server needs to recieve request, fork child, process using dup2() and execvp(), then return response

        inet_ntop(their_addr.ss_family, 
            &(((struct sockaddr_in*)sa)->sin_addr), 
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        //does the fork process have a copy of our servinfo and sa?? do we need to free those??
        //notice that Beej had already freed stuff before this point
        if (!fork()) { // this is the child process now, if fork returned 0
            close(sockfd); // child doesn't need copy of the listener 
            
            //handle client request
            //need help, how do I get messages from the client?
            //let's move onto client before doing this part
            if (send(new_fd, "Hello, world!", 13, 0) == -1)
                perror("send");

            close(new_fd);
            exit(0); 
        }
        close(new_fd); //parent doesn't need this
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
                sizeof(int)) == -1) { //allow multiple connections on one port
            perror("setsockopt");
            exit(1); //do i need to return???
            //why exit here instead of continue??
        }
        //bind()
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) { 
            close(sockfd);
            perror("server: bind");
            continue; 
        }

        break; //successfully did the 3 functions for a socket, move on
    }

    if(p == NULL) {
        fprintf(stderr, "server: failed to bind\n"); 
        exit(1); //do i need to return???
    }

    return sockfd;
}

void jank_itoa(char* dest, int i){ //rename before turnin -----------------------<
    sprintf(dest, "%d", i);
}

int get_addresses(struct addrinfo* servinfo) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints); //make sure the struct is empty
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; //not 100 yet if this should be cs1.seattleu.edu, uses my IP

    int rv; //return value
    char port[6];
    jank_itoa(port, allocate_port(ID)); //allocate_port takes &char* but returns int, we need char* again

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) { //send address of servinfo pointer, making it a pointer pointer
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
        return 1;
    }
    return 0;
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