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

void send_invite_request_with_retry(int sockfd)
{
    char send_buffer[BUFFER_SIZE];
    char recv_buffer[BUFFER_SIZE];
    int retry_count = 0;
    const int MAX_RETRIES = 3;

    char sdp_body[] =
        "v=0\r\n"
        "o=JohnMiller 1073055600 1073055600 IN IP4 127.0.0.1\r\n"
        "s=Session SDP\r\n"
        "c=IN IP4 127.0.0.1\r\n"
        "t=0 0\r\n"
        "m=audio 6000 RTP/AVP 0\r\n"
        "a=rtpmap:0 PCMU/8000\r\n";

    int content_length = strlen(sdp_body);

    snprintf(send_buffer, BUFFER_SIZE,
             "INVITE sip:recipient@yahoo.com SIP/2.0\r\n"
             "Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bK123456\r\n"
             "Max-Forwards: 70\r\n"
             "From: <sip:JohnMiller@university.edu>;tag=4445556\r\n"
             "To: <sip:recipient@yahoo.com>\r\n"
             "Call-ID: 23456678901223\r\n"
             "CSeq: 1 INVITE\r\n"
             "Contact: <sip:JohnMiller@127.0.0.1>\r\n"
             "Content-Type: application/sdp\r\n"
             "Content-Length: %d\r\n"
             "\r\n"
             "%s",
             content_length, sdp_body);

    while (retry_count < MAX_RETRIES)
    {
        printf("Sending INVITE request (attempt %d)...\n", retry_count + 1);
        send(sockfd, send_buffer, strlen(send_buffer), 0);

        // 设置 select 等待超时，等待响应
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        timeout.tv_sec = 3; // 3秒超时
        timeout.tv_usec = 0;

        int ready = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
        if (ready > 0 && FD_ISSET(sockfd, &readfds))
        {
            memset(recv_buffer, 0, sizeof(recv_buffer));
            // 这里调用你自己的接收函数，注意它的返回值和实现
            int bytes = client_recv_sip_message(sockfd, recv_buffer, sizeof(recv_buffer) - 1);
            if (bytes > 0)
            {
                printf("Received SIP response: %s\n", recv_buffer);

                if (strstr(recv_buffer, "200 OK") != NULL)
                {
                    printf("INVITE succeeded.\n");
                    return;
                }
                else if (strstr(recv_buffer, "401 Unauthorized") != NULL)
                {
                    printf("Authentication required for INVITE. (未实现)\n");
                    // 如果你要支持认证，这里可以调用带认证的函数或处理
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
                printf("Failed to receive SIP response properly.\n");
            }
        }
        else
        {
            printf("No response received, retrying...\n");
            retry_count++;
        }
    }

    printf("No response after %d attempts. INVITE failed.\n", MAX_RETRIES);
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

void send_ack_request(int sockfd)
{
    char send_buffer[BUFFER_SIZE];

    snprintf(send_buffer, BUFFER_SIZE,
             "ACK sip:recipient@yahoo.com SIP/2.0\r\n"
             "Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKack123456\r\n"
             "Max-Forwards: 70\r\n"
             "From: <sip:JohnMiller@university.edu>;tag=4445556\r\n"
             "To: <sip:recipient@yahoo.com>\r\n"
             "Call-ID: 23456678901223\r\n"
             "CSeq: 1 ACK\r\n"
             "Contact: <sip:JohnMiller@127.0.0.1>\r\n"
             "Content-Length: 0\r\n\r\n");

    printf("Sending ACK request...\n");
    send(sockfd, send_buffer, strlen(send_buffer), 0);
}

void send_bye_request(int sockfd)
{
    char send_buffer[BUFFER_SIZE];
    char recv_buffer[BUFFER_SIZE];

    snprintf(send_buffer, BUFFER_SIZE,
             "BYE sip:recipient@yahoo.com SIP/2.0\r\n"
             "Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKbye123456\r\n"
             "Max-Forwards: 70\r\n"
             "From: <sip:JohnMiller@university.edu>;tag=4445556\r\n"
             "To: <sip:recipient@yahoo.com>\r\n"
             "Call-ID: 23456678901223\r\n"
             "CSeq: 1 BYE\r\n"
             "Contact: <sip:JohnMiller@127.0.0.1>\r\n"
             "Content-Length: 0\r\n\r\n");

    printf("Sending BYE request...\n");
    int sent = send(sockfd, send_buffer, strlen(send_buffer), 0);
    printf("Sent %d bytes for BYE request.\n", sent);

    // 可选：阻塞等待一次 200 OK 响应
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
        int bytes = client_recv_sip_message(sockfd, recv_buffer, sizeof(recv_buffer) - 1);
        if (bytes > 0 && strstr(recv_buffer, "200 OK") != NULL)
        {
            printf("Received 200 OK for BYE, session terminated.\n");
        }
        else
        {
            printf("Unexpected response or no 200 OK after BYE.\n");
        }
    }
    else
    {
        printf("No response after BYE sent.\n");
    }
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
    send_invite_request_with_retry(sockets[0]);
    send_ack_request(sockets[0]);
    sleep(1);
    send_bye_request(sockets[0]);

    return 0;
}
