
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
