#ifndef EVENT_VS_THREAD_CHAT_SOCKET_H
#define EVENT_VS_THREAD_CHAT_SOCKET_H

#include <arpa/inet.h>

typedef struct socket_info socket_info;

struct socket_info
{
    int socket_fd;
    struct sockaddr_in address;
};

int create_passive_socket(struct socket_info** listener_socket, uint16_t port);
int create_active_socket(struct socket_info** active_socket, char* ip_address, uint16_t port);
int accept_connection(struct socket_info** socket);
int destroy_socket(struct socket_info** socket);


#endif //EVENT_VS_THREAD_CHAT_SOCKET_H
