/*
Name: Saloni Sanger
Course: Computer Networks
Date: April 4th, 2024
File: protocol.h
Description: Defines TCP request protocol. 
    whoisclient must adhere to this format.
    A maximum of 11 arguments for the whois command are allowed, 
    including an option. Each argument's max length is 50 characters.
*/

#define MAX_ARG_LENGTH 50
#define MAX_CMD_ARGS 11

struct request {
    char command[MAX_ARG_LENGTH+1]; // +1 for null character termination
    uint32_t num_args;
    char arguments[MAX_CMD_ARGS+1][MAX_ARG_LENGTH+1]; // +1 for null character termination 
};