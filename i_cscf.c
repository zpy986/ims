#include "i_cscf.h"
#include "s_cscf.h"
#include "hss.h"

int query_scscf_from_hss_tcp(UserRecord *user_record, char *scscf_ip, int *scscf_port)
{
    int hss_socketfd;
    struct sockaddr_in hss_addr;
    char msg[256];
    snprintf(msg, MAX_MSG_SIZE, "GETSCSCF %s %s %d \r\n\r\n", user_record->user_id, user_record->ip, user_record->expires);

    // 1. Create TCP socket
    hss_socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (hss_socketfd < 0)
    {
        perror("Socket creation failed");
        return 0;
    }

    // 2. Set up HSS address
    memset(&hss_addr, 0, sizeof(hss_addr));
    hss_addr.sin_family = AF_INET;
    hss_addr.sin_port = htons(HSS_PORT);
    if (inet_pton(AF_INET, HSS_IP, &hss_addr.sin_addr) <= 0)
    {
        perror("Invalid address");
        close(hss_socketfd);
        return 0;
    }

    // 3. Connect to HSS
    if (connect(hss_socketfd, (struct sockaddr *)&hss_addr, sizeof(hss_addr)) < 0)
    {
        perror("Connection to HSS failed");
        close(hss_socketfd);
        return 0;
    }

    // 4. Send request
    if (send(hss_socketfd, msg, strlen(msg), 0) < 0)
    {
        perror("Send to HSS failed");
        close(hss_socketfd);
        return 0;
    }
    printf("Sent user info to HSS: %s\n", msg);

    // 5. Receive response
    char response[256];
    int recv_len = server_recv_sip_message(hss_socketfd, response, 256);
    if (recv_len < 0)
    {
        perror("Receive from HSS failed");
        close(hss_socketfd);
        return 0;
    }
    response[recv_len] = '\0';
    printf("Received response from HSS: %s\n", response);

    close(hss_socketfd);

    // 6. Parse response
    if (strncmp(response, "200 SCSCF", 9) == 0)
    {
        if (sscanf(response, "200 SCSCF %[^:]:%d", scscf_ip, scscf_port) == 2)
        {
            return 1; // success
        }
    }

    printf("[I-CSCF] Failed to parse S-CSCF info from HSS response.\n");
    return 0;
}

void parse_register_msg(char *msg, char *user_id, char *ip, int *expires)
{
    char msg_copy[1024];
    char from_line[128], contact_line[128], expires_line[64];
    strncpy(msg_copy, msg, sizeof(msg_copy));
    msg_copy[sizeof(msg_copy) - 1] = '\0'; // Ensure null-termination
    char *line = strtok(msg_copy, "\r\n");

    while (line != NULL)
    {
        if (strncmp(line, "From:", 5) == 0)
        {
            strcpy(from_line, line);
        }
        else if (strncmp(line, "Contact:", 8) == 0)
        {
            strcpy(contact_line, line);
        }
        else if (strncmp(line, "Expires:", 8) == 0)
        {
            strcpy(expires_line, line);
        }
        line = strtok(NULL, "\n");
    }

    char *from_start = strstr(from_line, "sip:");
    if (from_start)
    {
        sscanf(from_start, "sip:%[^>]", user_id);
    }

    char *contact_start = strstr(contact_line, "sip:");
    if (contact_start)
    {
        char uri[64];
        sscanf(contact_start, "sip:%[^>]", uri);
        // uri = "JohnMiller@127.0.0.1"
        char *at_ptr = strchr(uri, '@');
        if (at_ptr)
        {
            strcpy(ip, at_ptr + 1); // ip = "127.0.0.1"
        }
        else
        {
            strcpy(ip, uri); // fallback
        }
    }

    sscanf(expires_line, "Expires: %d", expires);
}

char *forward_package_to_scscf(char *register_msg, char *scscf_ip, int scscf_port)
{
    char *response = malloc(MAX_MSG_SIZE);
    if (!response)
    {
        perror("Memory allocation failed");
        return strdup("SIP/2.0 500 Internal Server Error\r\n\r\n");
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        free(response);
        return strdup("SIP/2.0 500 Internal Socket Error\r\n\r\n");
    }

    struct sockaddr_in scscf_addr = {0};
    scscf_addr.sin_family = AF_INET;
    scscf_addr.sin_port = htons(scscf_port);
    if (inet_pton(AF_INET, scscf_ip, &scscf_addr.sin_addr) <= 0)
    {
        perror("Invalid S-CSCF IP address");
        free(response);
        close(sockfd);
        return strdup("SIP/2.0 500 Invalid S-CSCF Address\r\n\r\n");
    }

    if (connect(sockfd, (struct sockaddr *)&scscf_addr, sizeof(scscf_addr)) < 0)
    {
        perror("Connection to S-CSCF failed");
        free(response);
        close(sockfd);
        return strdup("SIP/2.0 500 Cannot Connect to S-CSCF\r\n\r\n");
    }

    // Send all data
    size_t len_to_send = strlen(register_msg);
    size_t total_sent = 0;
    while (total_sent < len_to_send)
    {
        ssize_t sent = send(sockfd, register_msg + total_sent, len_to_send - total_sent, 0);
        if (sent <= 0)
        {
            perror("Sending to S-CSCF failed");
            free(response);
            close(sockfd);
            return strdup("SIP/2.0 500 Send Failed\r\n\r\n");
        }
        total_sent += sent;
    }

    int len = server_recv_sip_message(sockfd, response, MAX_MSG_SIZE);
    if (len <= 0)
    {
        perror("Receiving from S-CSCF failed");
        free(response);
        close(sockfd);
        return strdup("SIP/2.0 500 No Response from S-CSCF\r\n\r\n");
    }
    printf("I-CSCF: Received response from S-CSCF:\n%s\n", response);
    close(sockfd);
    return response;
}

char *handle_register_at_icscf(char *register_msg)
{
    char scscf_ip[64];
    int scscf_port;
    UserRecord *user_record = malloc(sizeof(UserRecord));
    if (register_msg == NULL || strlen(register_msg) < 10)
    {
        printf("[I-CSCF] Received empty or invalid REGISTER message.\n");
        return strdup("SIP/2.0 400 Bad Request\r\n\r\n");
    }
    printf("I-CSCF: Received REGISTER message:\n%s\n", register_msg);
    if (user_record == NULL)
    {
        perror("Memory allocation failed");
        return strdup("SIP/2.0 500 Internal Server Error\r\n\r\n");
    }
    memset(user_record, 0, sizeof(UserRecord));

    // when the message is a response from S-CSCF
    if (strncmp(register_msg, "SIP/2.0", 7) == 0)
    {
        printf("[I-CSCF] Received response from S-CSCF:\n%s\n", register_msg);
        return register_msg;
    }

    parse_register_msg(register_msg, user_record->user_id, user_record->ip, &user_record->expires);

    printf("I-CSCF received REGISTER request for user: %s\n", user_record->user_id);
    printf("I-CSCF received REGISTER request for IP: %s\n", user_record->ip);
    // Simulate sending a message to HSS
    printf("I-CSCF sending message to HSS for user: %s\n", user_record->user_id);

    if (!query_scscf_from_hss_tcp(user_record, scscf_ip, &scscf_port))
    {
        free(user_record);
        return strdup("SIP/2.0 500 HSS Failed\r\n\r\n");
    }

    printf("I-CSCF: Forwarding REGISTER to S-CSCF at %s:%d\n", scscf_ip, scscf_port);
    char *response = forward_package_to_scscf(register_msg, scscf_ip, scscf_port);

    free(user_record);
    return response;
}

void parse_invite_msg(char *msg, char *user_id, char *ip)
{
    char msg_copy[1024];
    char from_line[128], contact_line[128];
    strncpy(msg_copy, msg, sizeof(msg_copy));
    msg_copy[sizeof(msg_copy) - 1] = '\0'; // Ensure null-termination
    char *line = strtok(msg_copy, "\r\n");

    while (line != NULL)
    {
        if (strncmp(line, "From:", 5) == 0)
        {
            strcpy(from_line, line);
        }
        else if (strncmp(line, "Contact:", 8) == 0)
        {
            strcpy(contact_line, line);
        }

        line = strtok(NULL, "\n");
    }

    char *from_start = strstr(from_line, "sip:");
    if (from_start)
    {
        sscanf(from_start, "sip:%[^>]", user_id);
    }

    char *contact_start = strstr(contact_line, "sip:");
    if (contact_start)
    {
        char uri[64];
        sscanf(contact_start, "sip:%[^>]", uri);
        // uri = "JohnMiller@127.0.0.1"
        char *at_ptr = strchr(uri, '@');
        if (at_ptr)
        {
            strcpy(ip, at_ptr + 1); // ip = "127.0.0.1"
        }
        else
        {
            strcpy(ip, uri); // fallback
        }
    }
}

char *handle_invite_at_icscf(char *invite_msg)
{
    char scscf_ip[64];
    int scscf_port;
    UserRecord *user_record = malloc(sizeof(UserRecord));

    if (invite_msg == NULL || strlen(invite_msg) < 10)
    {
        printf("[I-CSCF] Received empty or invalid INVITE message.\n");
        return strdup("SIP/2.0 400 Bad Request\r\n\r\n");
    }

    printf("[I-CSCF] Received INVITE message:\n%s\n", invite_msg);

    //  parse the INVITE message to extract the target user
    parse_invite_msg(invite_msg, user_record->user_id, user_record->ip);
    user_record->expires = 3600; // default expires value

    printf("[I-CSCF] INVITE target user: %s\n", user_record->user_id);

    // query S-CSCF from HSS
    if (!query_scscf_from_hss_tcp(user_record, scscf_ip, &scscf_port))
    {
        printf("[I-CSCF] HSS query failed, unable to get S-CSCF.\n");
        return strdup("SIP/2.0 500 HSS Lookup Failed\r\n\r\n");
    }

    printf("[I-CSCF] send invite to S-CSCF at %s:%d\n", scscf_ip, scscf_port);

    // Forward INVITE message to S-CSCF
    char *response = forward_package_to_scscf(invite_msg, scscf_ip, scscf_port);
    if (response == NULL)
    {
        return strdup("SIP/2.0 500 S-CSCF Unreachable\r\n\r\n");
    }

    return response;
}

char *handle_package_at_icscf(char *package_msg, int type)
{
    char *response = NULL;
    if (package_msg == NULL || strlen(package_msg) == 0)
    {
        return strdup("SIP/2.0 400 Bad Request\r\n\r\n");
    }

    switch (type)
    {
    case IMS_REGISTRATION:
        response = handle_register_at_icscf(package_msg);
        break;
    case IMS_INVITE:
        response = handle_invite_at_icscf(package_msg);
        break;
    default:
        break;
    }
    return response;
}