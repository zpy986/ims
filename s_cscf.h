// scscf.h
#ifndef SCSCF_H
#define SCSCF_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#define MAX_MSG_SIZE 1024
#define MAX_USERS 100
#define USER_ID_LEN 100
#define IP_LEN 50
#define IMS_REGISTRATION 1
#define IMS_INVITE 2
#define IMS_ACK 3
#define IMS_BYE 4
#define IMS_GET_SCSCF 5
#define SIP_USER_IP "127.0.0.1"
#define SIP_USER_PORT 5004

char *handle_register_at_scscf(char *msg);
char *handle_invite_at_scscf(char *msg);
char *handle_package_at_scscf(char *package_msg, int type);

#endif