#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "hss.h"
#include "s_cscf.h"

UserRecord user_db[MAX_USERS];
pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_user_db()
{
    for (int i = 0; i < MAX_USERS; i++)
    {
        user_db[i].active = 0;
    }
}
char *create_response(const char *msg)
{
    char *res = malloc(MAX_MSG_SIZE);
    if (!res)
    {
        perror("malloc");
        return strdup("SIP/2.0 500 Internal Server Error\r\n\r\n");
    }
    snprintf(res, MAX_MSG_SIZE, "%s", msg);
    return res;
}

int server_recv_sip_message(int sock, char *buffer, int max_len)
{
    printf(" server sock %d Receiving SIP message...\n", sock);
    int total = 0;
    while (total < max_len - 1)
    {
        int n = recv(sock, buffer + total, max_len - 1 - total, 0);
        if (n <= 0)
            break;
        total += n;
        buffer[total] = '\0';
        if (strstr(buffer, "\r\n\r\n"))
            break;
    }
    return total;
}
// Format: "GETSCSCF <user_id> <ip> <expires>"
char *handle_package_at_hss(char *msg)
{
    char *response = malloc(MAX_MSG_SIZE);
    if (!response)
    {
        fprintf(stderr, "Memory allocation failed for response buffer\n");
        return strdup("ERROR Internal Server Error");
    }

    char user_id[USER_ID_LEN];
    char ip[IP_LEN];
    int expires;

    printf("[HSS] Received message: %s\n", msg);

    if (sscanf(msg, "GETSCSCF %s %s %d", user_id, ip, &expires) != 3)
    {
        snprintf(response, MAX_MSG_SIZE, "ERROR Invalid format");
        fprintf(stderr, "[HSS] Invalid message format. Expecting: GETSCSCF <user_id> <ip> <expires>\n");
        return response;
    }

    printf("[HSS] Parsed user_id: %s, ip: %s, expires: %d\n", user_id, ip, expires);

    pthread_mutex_lock(&db_mutex);

    int found = 0;
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (user_db[i].active && strcmp(user_db[i].user_id, user_id) == 0)
        {
            printf("[HSS] Found existing user: %s. Updating IP and expires\n", user_id);
            strcpy(user_db[i].ip, ip);
            user_db[i].expires = expires;
            found = 1;
            break;
        }
    }

    if (!found)
    {
        for (int i = 0; i < MAX_USERS; i++)
        {
            if (!user_db[i].active)
            {
                printf("[HSS] Adding new user: %s\n", user_id);
                strcpy(user_db[i].user_id, user_id);
                strcpy(user_db[i].ip, ip);
                user_db[i].expires = expires;
                user_db[i].active = 1;
                found = 1;
                break;
            }
        }
    }

    pthread_mutex_unlock(&db_mutex);

    if (found)
    {
        snprintf(response, MAX_MSG_SIZE, "200 SCSCF 127.0.0.1:5003 \r\n\r\n");
        printf("[HSS] Registration success: %s -> 127.0.0.1:5003\n", user_id);
    }
    else
    {
        snprintf(response, MAX_MSG_SIZE, "ERROR User DB full \r\n\r\n");
        fprintf(stderr, "[HSS] User DB full. Cannot register: %s\n", user_id);
    }

    return response;
}

void *expiry_thread(void *arg)
{
    while (1)
    {
        sleep(1);
        pthread_mutex_lock(&db_mutex);
        for (int i = 0; i < MAX_USERS; i++)
        {
            if (user_db[i].active && user_db[i].expires > 0)
            {
                user_db[i].expires--;
                if (user_db[i].expires == 0)
                {
                    printf("[HSS] User %s expired, deactivating.\n", user_db[i].user_id);
                    user_db[i].active = 0;
                    memset(user_db[i].user_id, 0, sizeof(user_db[i].user_id));
                    memset(user_db[i].ip, 0, sizeof(user_db[i].ip));
                }
            }
        }
        pthread_mutex_unlock(&db_mutex);
    }
    return NULL;
}