#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "p_cscf.h"
#include "i_cscf.h"
#include "hss.h"
#include "s_cscf.h"
#define P_CSCF_PORT 5000
#define BUFFER_SIZE 1024

typedef struct
{
    int client_socket;
    int server_id;
} client_info;

void *handle_client(void *arg)
{
    client_info *info = (client_info *)arg;
    int client_socket = info->client_socket;
    int server_id = info->server_id;
    free(info);
    char *response = NULL;

    char buffer[BUFFER_SIZE];
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));

        int bytes_received = server_recv_sip_message(client_socket, buffer, sizeof(buffer));
        if (bytes_received <= 0)
        {
            printf("Client disconnected or recv error.\n");
            break;
        }

        if (strlen(buffer) == 0)
        {
            // empty message, continue
            printf("Received empty message, ignoring...\n");
            continue;
        }
        switch (server_id)
        {
        case 0:
            // Handle P-CSCF
            response = handle_register_at_pcscf(buffer);
            break;
        case 1:
            // Handle I-CSCF
            printf("Handling I-CSCF request\n");
            response = handle_register_at_icscf(buffer);
            break;
        case 2:
            // Handle HSS
            printf("Handling HSS request\n");
            response = handle_register_at_hss(buffer);
            break;
        case 3:
            // Handle S-CSCF
            printf("Handling S-CSCF request\n");
            if (strncmp(buffer, "REGISTER", 8) == 0)
            {
                response = handle_register_at_scscf(buffer);
            }
            else
            {
                response = strdup("SIP/2.0 400 Bad Request\r\n\r\n");
                printf("S-CSCF received invalid request: %s\n", buffer);
            }
            break;
        case 4:
            // Handle SIP User
            printf("Handling SIP User request\n");
            break;

        default:
            break;
        }
        if (response)
        {
            printf(" server Sending response: %s\n", response);
            int sent = send(client_socket, response, strlen(response), 0);
            free(response);
        }
        else
        {
            const char *error_response = "SIP/2.0 500 Internal Server Error\r\n\r\n";
            int sent = send(client_socket, error_response, strlen(error_response), 0);
            printf("[S-CSCF] Sent %d bytes: \n%s\n", sent, error_response);
        }
    }

    // send(client_socket, buffer, bytes_received, 0);

    printf("Client disconnected\n");
    close(client_socket);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    int server_id = port - P_CSCF_PORT;
    if (server_id == 3)
    {
        init_user_db();
        pthread_t expiry_tid;
        pthread_create(&expiry_tid, NULL, expiry_thread, NULL);
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("Socket creation failed");
        return 1;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        return 1;
    }
    listen(server_socket, 5);
    printf("Server (ID %d) listening on port %d\n", server_id, port);

    while (1)
    {
        struct sockaddr_in client_addr;
        client_info *info = malloc(sizeof(client_info));
        socklen_t addr_size = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
        if (client_socket < 0)
        {
            perror("Accept failed");
            continue;
        }
        int *client_ptr = malloc(sizeof(int));
        *client_ptr = client_socket;
        pthread_t thread_id;
        info->client_socket = client_socket;
        info->server_id = server_id;
        pthread_create(&thread_id, NULL, handle_client, info);
        pthread_detach(thread_id);
    }
    close(server_socket);
    return 0;
}