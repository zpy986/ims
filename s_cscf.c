
#include "hss.h"
#include "s_cscf.h"

#define BUFFER_SIZE 1024
UserRecord scscf_user_db[MAX_USERS];
pthread_mutex_t scscf_db_mutex;

int scscf_parse_register_msg(const char *msg, char *user_id, char *ip, int *expires)
{
    if (!msg || !user_id || !ip || !expires)
        return -1;

    char msg_copy[1024];
    strncpy(msg_copy, msg, sizeof(msg_copy) - 1);
    msg_copy[sizeof(msg_copy) - 1] = '\0';

    char *line = strtok(msg_copy, "\r\n");
    char from_line[256] = "";
    char contact_line[256] = "";
    char expires_line[64] = "";

    while (line != NULL)
    {
        if (strncmp(line, "From:", 5) == 0)
        {
            strncpy(from_line, line, sizeof(from_line) - 1);
        }
        else if (strncmp(line, "Contact:", 8) == 0)
        {
            strncpy(contact_line, line, sizeof(contact_line) - 1);
        }
        else if (strncmp(line, "Expires:", 8) == 0)
        {
            strncpy(expires_line, line, sizeof(expires_line) - 1);
        }
        line = strtok(NULL, "\r\n");
    }

    // Parse user_id from From line
    // Example: From: <sip:JohnMiller@university.edu>;tag=123456
    if (strlen(from_line) > 0)
    {
        char uri[128];
        if (sscanf(from_line, "From: <sip:%[^@]", uri) == 1)
        {
            strncpy(user_id, uri, 64);
        }
        else
        {
            return -2; // Failed to parse user ID
        }
    }
    else
    {
        return -3; // Missing From line
    }

    // Parse IP from Contact line
    // Example: Contact: <sip:JohnMiller@127.0.0.1>
    if (strlen(contact_line) > 0)
    {
        char uri[128];
        if (sscanf(contact_line, "Contact: <sip:%[^>]", uri) == 1)
        {
            char *at_ptr = strchr(uri, '@');
            if (at_ptr && strlen(at_ptr + 1) > 0)
                strncpy(ip, at_ptr + 1, 64);
            else
                return -4; // Invalid Contact format
        }
        else
        {
            return -5; // Could not parse Contact
        }
    }
    else
    {
        return -6; // Missing Contact line
    }

    // Parse Expires value
    if (strlen(expires_line) > 0)
    {
        if (sscanf(expires_line, "Expires: %d", expires) != 1)
        {
            return -7; // Could not parse Expires
        }
    }
    else
    {
        return -8; // Missing Expires line
    }

    return 0; // Success
}

char *handle_register_at_scscf(char *msg)
{
    char *response = malloc(MAX_MSG_SIZE);
    if (!response)
    {
        perror("Memory allocation failed");
        return strdup("SIP/2.0 500 Internal Server Error\r\n\r\n");
    }

    UserRecord *user_record = malloc(sizeof(UserRecord));
    if (!user_record)
    {
        perror("Memory allocation failed");
        free(response);
        return strdup("SIP/2.0 500 Internal Server Error\r\n\r\n");
    }

    // Parse the REGISTER message, return 0 if successful
    int ret = scscf_parse_register_msg(msg, user_record->user_id, user_record->ip, &user_record->expires);
    if (ret != 0)
    {
        printf("[ERROR] Failed to parse REGISTER message. Error code: %d\n", ret);
        free(user_record);
        free(response);
        return strdup("SIP/2.0 400 Bad Request\r\n\r\n");
    }

    printf("[S-CSCF] REGISTER user: %s, IP: %s, Expires: %d\n", user_record->user_id, user_record->ip, user_record->expires);

    pthread_mutex_lock(&scscf_db_mutex);
    int updated = 0;
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (scscf_user_db[i].active && strcmp(scscf_user_db[i].user_id, user_record->user_id) == 0)
        {
            if (user_record->expires == 0)
            {
                // Unregister the user
                scscf_user_db[i].active = 0;
                printf("[S-CSCF] User %s unregistered\n", user_record->user_id);
            }
            else
            {
                // Update the user's registration info
                strncpy(scscf_user_db[i].ip, user_record->ip, sizeof(scscf_user_db[i].ip) - 1);
                scscf_user_db[i].ip[sizeof(scscf_user_db[i].ip) - 1] = '\0';
                scscf_user_db[i].expires = user_record->expires;
                printf("[S-CSCF] User %s registration updated\n", user_record->user_id);
            }
            updated = 1;
            break;
        }
    }

    if (!updated && user_record->expires > 0)
    {
        // Register a new user
        for (int i = 0; i < MAX_USERS; i++)
        {
            if (!scscf_user_db[i].active)
            {
                strncpy(scscf_user_db[i].user_id, user_record->user_id, sizeof(scscf_user_db[i].user_id) - 1);
                scscf_user_db[i].user_id[sizeof(scscf_user_db[i].user_id) - 1] = '\0';
                strncpy(scscf_user_db[i].ip, user_record->ip, sizeof(scscf_user_db[i].ip) - 1);
                scscf_user_db[i].ip[sizeof(scscf_user_db[i].ip) - 1] = '\0';
                scscf_user_db[i].expires = user_record->expires;
                scscf_user_db[i].active = 1;
                printf("[S-CSCF] New user %s registered\n", user_record->user_id);
                updated = 1;
                break;
            }
        }
    }
    pthread_mutex_unlock(&scscf_db_mutex);

    if (updated)
    {
        // Note: It is recommended to dynamically fill Via, To, From headers from the request
        // For simplicity, a fixed format is used here. In production, parse the request to extract headers
        snprintf(response, MAX_MSG_SIZE,
                 "SIP/2.0 200 OK\r\n"
                 "Via: SIP/2.0/TCP 127.0.0.1:5001;branch=elelmloperg;received=127.0.0.1\r\n"
                 "To: <sip:%s>\r\n"
                 "From: <sip:%s>;tag=4445556\r\n"
                 "Call-ID: 23456678901223\r\n"
                 "CSeq: 1 REGISTER\r\n"
                 "Contact: <sip:%s>\r\n"
                 "Expires: %d\r\n"
                 "Content-Length: 0\r\n\r\n",
                 user_record->user_id, user_record->user_id, user_record->ip, user_record->expires);
    }
    else
    {
        snprintf(response, MAX_MSG_SIZE, "SIP/2.0 500 Server Error\r\n\r\n");
    }

    printf("[DEBUG] S-CSCF response:\n%s\n", response);

    free(user_record);
    return response;
}

char *forward_package_to_sipuser(char *msg, char *sipuser_ip, int sipuser_port)
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

    struct sockaddr_in sipuser_addr = {0};
    sipuser_addr.sin_family = AF_INET;
    sipuser_addr.sin_port = htons(sipuser_port);
    if (inet_pton(AF_INET, sipuser_ip, &sipuser_addr.sin_addr) <= 0)
    {
        perror("Invalid SIP User IP address");
        free(response);
        close(sockfd);
        return strdup("SIP/2.0 500 Invalid SIP User Address\r\n\r\n");
    }

    if (connect(sockfd, (struct sockaddr *)&sipuser_addr, sizeof(sipuser_addr)) < 0)
    {
        perror("Connection to SIP User failed");
        free(response);
        close(sockfd);
        return strdup("SIP/2.0 500 Cannot Connect to SIP User\r\n\r\n");
    }

    // Send all data
    size_t len_to_send = strlen(msg);
    size_t total_sent = 0;
    while (total_sent < len_to_send)
    {
        ssize_t sent = send(sockfd, msg + total_sent, len_to_send - total_sent, 0);
        if (sent <= 0)
        {
            perror("Sending to SIP User failed");
            free(response);
            close(sockfd);
            return strdup("SIP/2.0 500 Send Failed\r\n\r\n");
        }
        total_sent += sent;
    }

    int len = server_recv_sip_message(sockfd, response, MAX_MSG_SIZE);
    if (len <= 0)
    {
        perror("Receiving from SIP User failed");
        free(response);
        close(sockfd);
        return strdup("SIP/2.0 500 No Response from SIP User\r\n\r\n");
    }
    printf("S-CSCF: Received response from SIP User:\n%s\n", response);
    close(sockfd);
    return response;
}

char *handle_invite_at_scscf(char *invite_msg)
{
    char *sip_user_ip = SIP_USER_IP;
    int sip_user_port = SIP_USER_PORT;
    if (!invite_msg)
    {
        return strdup("SIP/2.0 400 Bad Request\r\n\r\n");
    }

    // Process the INVITE message
    printf("[S-CSCF] Processing INVITE message:\n%s\n", invite_msg);

    // Forward the INVITE message to the sip user
    char *response = forward_package_to_sipuser(invite_msg, sip_user_ip, sip_user_port);
    if (!response)
    {
        return strdup("SIP/2.0 500 Internal Server Error\r\n\r\n");
    }
    // In a real implementation, you would look up the user and forward the message
    return response;
}

char *handle_ack_at_scscf(char *ack_msg)
{
    char *sip_user_ip = SIP_USER_IP;
    int sip_user_port = SIP_USER_PORT;

    if (!ack_msg)
    {
        return strdup("SIP/2.0 400 Bad Request\r\n\r\n");
    }

    // Process the ACK message
    printf("[S-CSCF] Processing ACK message:\n%s\n", ack_msg);

    // Forward the ACK message to the appropriate user
    forward_package_to_sipuser(ack_msg, sip_user_ip, sip_user_port);
    // In a real implementation, you would look up the user and forward the message
    return NULL;
}

char *handle_bye_at_scscf(char *bye_msg)
{
    char *sip_user_ip = SIP_USER_IP;
    int sip_user_port = SIP_USER_PORT;
    if (!bye_msg)
    {
        return strdup("SIP/2.0 400 Bad Request\r\n\r\n");
    }

    // Process the BYE message
    printf("[S-CSCF] Processing BYE message:\n%s\n", bye_msg);
    char *response = forward_package_to_sipuser(bye_msg, sip_user_ip, sip_user_port);
    return response; // No response expected for BYE
}

char *handle_package_at_scscf(char *package_msg, int type)
{
    char *response = NULL;
    if (package_msg == NULL || strlen(package_msg) == 0)
    {
        return strdup("SIP/2.0 400 Bad Request\r\n\r\n");
    }

    switch (type)
    {
    case IMS_REGISTRATION:
        response = handle_register_at_scscf(package_msg);
        break;
    case IMS_INVITE:
        response = handle_invite_at_scscf(package_msg);
        break;
    case IMS_ACK:
        printf("[S-CSCF] Received ACK message:\n%s\n", package_msg);
        handle_ack_at_scscf(package_msg);
        break;
    case IMS_BYE:
        printf("[S-CSCF] Received BYE message:\n%s\n", package_msg);
        response = handle_bye_at_scscf(package_msg);
        break;
    default:
        break;
    }

    return response;
}
