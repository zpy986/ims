
#include "p_cscf.h"
#define MAX_MSG_SIZE 1024
#include "hss.h"

// Simulate forwarding SIP message to I-CSCF and receiving a response
char *forward_to_icscf(char *sip_msg)
{
    char *response_msg = malloc(MAX_MSG_SIZE);
    if (!response_msg)
    {
        perror("Memory allocation failed");
        return "SIP/2.0 500 Server Internal Error\r\n\r\n";
    }
    int icscf_socketfd;
    struct sockaddr_in icscf_addr;
    int sent_len, recv_len;

    // 1. Create socket
    icscf_socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (icscf_socketfd < 0)
    {
        perror("Socket creation failed");
        return "SIP/2.0 500 Server Internal Error\r\n\r\n";
    }
    // 2. Set up I-CSCF address
    memset(&icscf_addr, 0, sizeof(icscf_addr));
    icscf_addr.sin_family = AF_INET;
    icscf_addr.sin_port = htons(I_CSCF_PORT);
    if (inet_pton(AF_INET, I_CSCF_IP, &icscf_addr.sin_addr) <= 0)
    {
        perror("Invalid address");
        close(icscf_socketfd);
        return "SIP/2.0 500 Invalid Address\r\n\r\n";
    }

    // 3. connect to I-CSCF
    if (connect(icscf_socketfd, (struct sockaddr *)&icscf_addr, sizeof(icscf_addr)) < 0)
    {
        perror("Connection to I-CSCF failed");
        close(icscf_socketfd);
        return "SIP/2.0 500 Connection Failed\r\n\r\n";
    }
    // 4. Send SIP message to I-CSCF
    sent_len = send(icscf_socketfd, sip_msg, strlen(sip_msg), 0);
    if (sent_len < 0)
    {
        perror("Send to I-CSCF failed");
        close(icscf_socketfd);
        return "SIP/2.0 500 Send Failed\r\n\r\n";
    }
    // 5. Receive response from I-CSCF
    recv_len = server_recv_sip_message(icscf_socketfd, response_msg, MAX_MSG_SIZE);
    if (recv_len < 0)
    {
        perror("Receive from I-CSCF failed");
        close(icscf_socketfd);
        return strdup("SIP/2.0 500 Receive Failed\r\n\r\n");
    }

    printf("P-CSCF received response from I-CSCF:\n%s\n", response_msg);
    // 6. Close the socket
    close(icscf_socketfd);
    return response_msg;
}

char *forward_to_scscf(char *sip_msg, int message_type)
{
    char *response_msg = malloc(MAX_MSG_SIZE);
    if (!response_msg)
    {
        perror("Memory allocation failed");
        return strdup("SIP/2.0 500 No Response from S-CSCF\r\n\r\n");
    }
    int icscf_socketfd;
    struct sockaddr_in icscf_addr;

    icscf_socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (icscf_socketfd < 0)
    {
        perror("Socket creation failed");
        free(response_msg);
        return strdup("SIP/2.0 500 No Response from S-CSCF\r\n\r\n");
    }

    memset(&icscf_addr, 0, sizeof(icscf_addr));
    icscf_addr.sin_family = AF_INET;
    icscf_addr.sin_port = htons(S_CSCF_PORT);
    if (inet_pton(AF_INET, S_CSCF_IP, &icscf_addr.sin_addr) <= 0)
    {
        perror("Invalid address");
        free(response_msg);
        close(icscf_socketfd);
        return strdup("SIP/2.0 500 No Response from S-CSCF\r\n\r\n");
    }

    if (connect(icscf_socketfd, (struct sockaddr *)&icscf_addr, sizeof(icscf_addr)) < 0)
    {
        perror("Connection to S-CSCF failed");
        free(response_msg);
        close(icscf_socketfd);
        return strdup("SIP/2.0 500 No Response from S-CSCF\r\n\r\n");
    }

    int sent_len = send(icscf_socketfd, sip_msg, strlen(sip_msg), 0);
    if (sent_len < 0)
    {
        perror("Send to S-CSCF failed");
        free(response_msg);
        close(icscf_socketfd);
        return strdup("SIP/2.0 500 No Response from S-CSCF\r\n\r\n");
    }
    if (message_type == IMS_ACK)
    {
        printf("P-CSCF: Sent ACK to S-CSCF\n");
        close(icscf_socketfd);
        return NULL; // No response expected for ACK
    }
    else if (message_type == IMS_BYE)
    {
        printf("P-CSCF: Sent BYE to S-CSCF\n");
        int len = server_recv_sip_message(icscf_socketfd, response_msg, MAX_MSG_SIZE - 1);
        if (len <= 0)
        {
            perror("Receiving from S-CSCF failed");
            free(response_msg);
            close(icscf_socketfd);
            return strdup("SIP/2.0 500 No Response from S-CSCF\r\n\r\n");
        }

        printf("P-CSCF: Received response from S-CSCF:\n%s\n", response_msg);
        close(icscf_socketfd);
        return response_msg; // No response expected for BYE
    }

    return response_msg;
}

char *handle_invite_at_pcscf(char *invite_msg)
{
    printf("P-CSCF: Received INVITE\n");
    if (invite_msg == NULL || strlen(invite_msg) == 0)
    {
        printf("P-CSCF: Empty INVITE message\n");
        return strdup("SIP/2.0 400 Bad Request\r\n\r\n");
    }
    if (!strstr(invite_msg, "INVITE") || !strstr(invite_msg, "From:") || !strstr(invite_msg, "To:"))
    {
        printf("P-CSCF: Malformed INVITE\n");
        return strdup("SIP/2.0 400 Bad Request\r\n\r\n");
    }

    printf("P-CSCF: Forwarding INVITE to I-CSCF\n");

    char *response = forward_to_icscf(invite_msg);

    if (response == NULL)
    {
        printf("P-CSCF: No response from I-CSCF\n");
        return strdup("SIP/2.0 500 Internal Server Error\r\n\r\n");
    }

    return response;
}

int *handle_ack_at_pcscf(char *ack_msg)
{
    if (ack_msg == NULL || strlen(ack_msg) == 0)
    {
        return -1;
    }
    // For simplicity, we will just forward the ack message to S-CSCF
    forward_to_scscf(ack_msg, IMS_ACK);

    return NULL;
}

int *handle_bye_at_pcscf(char *bye_msg)
{
    char *response = NULL;
    if (bye_msg == NULL || strlen(bye_msg) == 0)
    {
        return -1;
    }
    // For simplicity, we will just forward the bye message to S-CSCF
    response = forward_to_scscf(bye_msg, IMS_BYE);

    return response;
}

// Handle REGISTER message received at the P-CSCF
char *handle_register_at_pcscf(char *register_msg)
{
    char *modified_msg = malloc(MAX_MSG_SIZE);
    if (!modified_msg)
    {
        return strdup("SIP/2.0 500 Internal Server Error\r\n\r\n");
    }

    printf("P-CSCF received REGISTER request:\n%s\n", register_msg);

    // step 1: Check for mandatory headers
    if (!strstr(register_msg, "From:") || !strstr(register_msg, "To:") || !strstr(register_msg, "Call-ID:") || !strstr(register_msg, "CSeq:"))
    {
        free(modified_msg);
        return strdup("SIP/2.0 400 Bad Request\r\nReason: Missing Required Headers\r\n\r\n");
    }

    if (strstr(modified_msg, "Contact:") == NULL)
    {
        // Simulate adding Contact header
        snprintf(modified_msg, MAX_MSG_SIZE,
                 "%s"
                 "Contact: <sip:JohnMiller@127.0.0.1>\r\n",
                 register_msg);
    }
    else
    {
        // If Contact header is present, just copy the message
        snprintf(modified_msg, MAX_MSG_SIZE, "%s", register_msg);
    }
    // Step 2: Check for Expires header (simplified logic)
    if (strstr(modified_msg, "Expires:") == NULL)
    {
        // Simulate adding Expires header
        snprintf(modified_msg, MAX_MSG_SIZE,
                 "%s"
                 "Expires: 3600\r\n",
                 register_msg);
    }
    else
    {
        // If Expires header is present, just copy the message
        snprintf(modified_msg, MAX_MSG_SIZE, "%s", register_msg);
    }
    // Step 3: Check for Content-Length header (simplified logic)
    if (strstr(modified_msg, "Content-Length:") == NULL)
    {
        // Simulate adding Content-Length header
        snprintf(modified_msg, MAX_MSG_SIZE,
                 "%s"
                 "Content-Length: 0\r\n",
                 register_msg);
    }
    else
    {
        // If Content-Length header is present, just copy the message
        snprintf(modified_msg, MAX_MSG_SIZE, "%s", register_msg);
    }
    // step 4: Add Via header and Path header (simplified logic)
    // Check if Via header is present
    if (strstr(modified_msg, "Via:") == NULL)
    {
        // Simulate adding Via header
        snprintf(modified_msg, MAX_MSG_SIZE,
                 "%s"
                 "Via: SIP/2.0/UDP pcscf.ims.example.com;branch=z9hG4bK1234\r\n"
                 "Path: <sip:pcscf.ims.example.com;lr>\r\n",
                 register_msg);
    }
    printf("P-CSCF modified REGISTER request:\n%s\n", modified_msg);

    // Step 5: Forward to I-CSCF and receive response
    char *response = forward_to_icscf(modified_msg);

    // Step 6: Return the response back to terminal
    printf("P-CSCF received upstream response:\n%s\n", response);
    return response;
}

char *handle_package_at_pcscf(char *package_msg, int type)
{
    char *response = NULL;
    if (package_msg == NULL || strlen(package_msg) == 0)
    {
        return strdup("SIP/2.0 400 Bad Request\r\n\r\n");
    }

    switch (type)
    {
    case IMS_REGISTRATION:
        response = handle_register_at_pcscf(package_msg);
        break;
    case IMS_INVITE:
        printf("P-CSCF: Received INVITE message\n");
        response = handle_invite_at_pcscf(package_msg);
        break;
    case IMS_ACK:
        printf("P-CSCF: Received ACK message\n");
        handle_ack_at_pcscf(package_msg);
        break;
    case IMS_BYE:
        printf("P-CSCF: Received BYE message\n");
        response = handle_bye_at_pcscf(package_msg);
        break;
    default:
        break;
    }

    return response;
}