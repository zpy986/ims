#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_MSG_SIZE 1024
#define P_CSCF_PORT 5000
#define I_CSCF_PORT 5001
#define S_CSCF_PORT 5003
#define I_CSCF_IP "127.0.0.1"
#define S_CSCF_IP "127.0.0.1"
#define IMS_REGISTRATION 1
#define IMS_INVITE 2
#define IMS_ACK 3
#define IMS_BYE 4

char *handle_package_at_pcscf(char *package_msg, int type);