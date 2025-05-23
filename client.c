#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <sys/select.h>
#define NUM_SERVERS 5
#define BUFFER_SIZE 1024
#define P_CSCF_PORT 5000
#define I_CSCF_PORT 5001
#define HSS_PORT 5002
#define SCSCF_PORT 5003
#define SIP_USER_PORT 5004
#define SERVER_PORT 5060

const int ports[NUM_SERVERS] = {5000, 5001, 5002, 5003, 5004};
int client_recv_sip_message(int sock, char *buffer, int max_len)
{
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

void send_register_request_with_retry(int sockfd, const char *auth)
{
    char send_buffer[BUFFER_SIZE];
    char recv_buffer[BUFFER_SIZE];
    int retry_count = 0;
    const int MAX_RETRIES = 3;

    snprintf(send_buffer, BUFFER_SIZE,
             "REGISTER sip:yahoo.com SIP/2.0\r\n"
             "Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKelelmloperg\r\n"
             "Max-Forwards: 70\r\n"
             "From: <sip:JohnMiller@university.edu>;tag=4445556\r\n"
             "To: <sip:JohnMiller@yahoo.com>\r\n"
             "Call-ID: 23456678901223\r\n"
             "CSeq: 1 REGISTER\r\n"
             "Contact: <sip:JohnMiller@127.0.0.1>\r\n"
             "Expires: 3600\r\n"
             "%s"
             "Content-Length: 0\r\n\r\n",
             auth ? auth : "");

    while (retry_count < MAX_RETRIES)
    {
        printf("Sending REGISTER request (attempt %d)...\n", retry_count + 1);
        send(sockfd, send_buffer, strlen(send_buffer), 0);

        // Set socket to non-blocking with timeout
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;

        int ready = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
        if (ready > 0 && FD_ISSET(sockfd, &readfds))
        {
            memset(recv_buffer, 0, sizeof(recv_buffer));
            client_recv_sip_message(sockfd, recv_buffer, BUFFER_SIZE);
            printf("Received SIP response: %s\n", recv_buffer);

            if (strstr(recv_buffer, "401 Unauthorized") != NULL)
            {
                printf("Authentication required. Sending authentication request...\n");
                // 可递归调用自身或另起一轮重试
                send_register_request_with_retry(sockfd,
                                                 "Authorization: Digest username=\"JohnMiller\", realm=\"yahoo.com\", nonce=\"xyz123\", uri=\"sip:yahoo.com\", response=\"abcd1234\"\r\n"); // 示例
                return;
            }
            else if (strstr(recv_buffer, "200 OK") != NULL)
            {
                printf("Registration successful.\n");
                return;
            }
            else
            {
                printf("Unexpected response: %s\n", recv_buffer);
                return;
            }
        }
        else
        {
            printf("No response received, retrying...\n");
            retry_count++;
        }
    }

    printf("No response after %d attempts. Registration failed.\n", MAX_RETRIES);
}

void *handle_client(void *arg)
{
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = client_recv_sip_message(client_socket, buffer, sizeof(buffer));
        if (bytes_received <= 0)
            break;
        printf("Server received: %s\n", buffer);
        send(client_socket, buffer, bytes_received, 0);
    }
    close(client_socket);
    return NULL;
}
int main()
{
    int sockets[NUM_SERVERS];
    pthread_t threads[NUM_SERVERS];
    // Connect to all servers
    for (int i = 0; i < NUM_SERVERS; i++)
    {
        sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(ports[i]);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP address used for this implementation
        if (connect(sockets[i], (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            perror("Connection failed");
            close(sockets[i]);
            continue;
        }
        printf("Connected to server on port %d\n", ports[i]); // Based on port No. display Serve No.Example port 5000 is Server - 0 pthread_create(&threads[i], NULL, receive_messages, &sockets[i]);
    }

    send_register_request_with_retry(sockets[0], NULL);

    return 0;
}
