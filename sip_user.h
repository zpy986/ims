
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#define MAX_MSG_SIZE 1024
#define MAX_USERS 100
#define USER_ID_LEN 100
#define IP_LEN 50
#define IMS_REGISTRATION 1
#define IMS_INVITE 2
#define IMS_ACK 3
#define IMS_BYE 4

char *handle_package_at_sipuser(char *package_msg, int type);
