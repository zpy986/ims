#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_MSG_SIZE 1024
#define P_CSCF_PORT 5000
#define I_CSCF_PORT 5001
#define I_CSCF_IP "127.0.0.1"
char *handle_register_at_pcscf(char *register_msg);