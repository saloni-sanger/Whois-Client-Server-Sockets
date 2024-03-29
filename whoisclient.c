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