#ifndef HSS_H
#define HSS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define HSS_PORT 5002
#define SCSCF_PORT 5003
#define SIP_USER_PORT 5004
#define MAX_USERS 100
#define USER_ID_LEN 100
#define IP_LEN 50
#define MAX_MSG_SIZE 1024

typedef struct
{
    char user_id[USER_ID_LEN];
    char ip[IP_LEN];
    int expires;
    int active; // 1: active, 0: expired
} UserRecord;

void init_user_db();
void *expiry_thread(void *arg);
char *handle_package_at_hss(char *msg);
int server_recv_sip_message(int sock, char *buffer, int max_len);
char *create_response(const char *msg);

#endif // HSS_H