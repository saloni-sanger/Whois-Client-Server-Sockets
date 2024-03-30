#define MAX_ARG_LENGTH 50
#define MAX_CMD_ARGS 11 //includes option (whois [option] argument-list) 10+1 = 11

struct request {
    char command[MAX_ARG_LENGTH+1];//+1 for null character termination 
    //told we can assume max 10 arguments, each max 50 characters 
    uint32_t num_args;
    char arguments[MAX_CMD_ARGS][MAX_ARG_LENGTH+1]; //+1 for null character termination 
};

// struct response {
//     int bytes;

// };