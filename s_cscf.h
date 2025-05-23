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

char *handle_register_at_scscf(char *msg);
char *handle_invite_at_scscf(char *msg);

#endif