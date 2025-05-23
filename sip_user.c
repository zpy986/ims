#include "sip_user.h"

char *handle_package_at_sipuser(char *package_msg, int type)
{
    char *sip_response = malloc(MAX_MSG_SIZE);
    if (sip_response == NULL)
    {
        perror("Memory allocation failed");
        return strdup("SIP/2.0 500 Internal Server Error\r\n\r\n");
    }

    if (package_msg == NULL || strlen(package_msg) == 0)
    {
        snprintf(sip_response, MAX_MSG_SIZE, "SIP/2.0 400 Bad Request\r\n\r\n");
        return sip_response;
    }

    switch (type)
    {
    case IMS_INVITE:
        printf("[SIP User] Received INVITE message:\n");
        snprintf(sip_response, MAX_MSG_SIZE, "SIP/2.0 200 OK\r\n\r\n");
        break;
    case IMS_ACK:
        printf("[SIP User] Received ACK message:\n");
        snprintf(sip_response, MAX_MSG_SIZE, "SIP/2.0 200 OK\r\n\r\n");
        break;
    case IMS_BYE:
        printf("[SIP User] Received BYE message:\n");
        snprintf(sip_response, MAX_MSG_SIZE, "SIP/2.0 200 OK\r\n\r\n");
        break;
    default:
        snprintf(sip_response, MAX_MSG_SIZE, "SIP/2.0 501 Not Implemented\r\n\r\n");
        break;
    }

    printf("[SIP User] Response: %s %d \n", sip_response, strlen(sip_response));
    return sip_response;
}
