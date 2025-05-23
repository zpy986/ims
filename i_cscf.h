#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#define I_CSCF_PORT 5001
#define HSS_PORT 5002
#define HSS_IP "127.0.0.1"
#define USER_ID_LEN 100
#define IP_LEN 50
#define MAX_MSG_SIZE 1024

char *handle_register_at_icscf(char *register_msg);