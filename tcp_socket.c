#define _GNU_SOURCE
#include "tcp_socket.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>

/// Creates a passive/listener socket for the server. The socket is non-blocking.
/// The caller must free allocated memory for the listener_socket.
/// \param listener_socket - Pointer to a structure holding socket information
/// \param port - Port the socket will listen on
/// \return 0 - success; -1 - failure
int create_passive_socket(socket_info** listener_socket, uint16_t port)
{
    *listener_socket = malloc(sizeof(socket_info));

    if(*listener_socket == NULL)
    {
        perror("create_passive_socket(): Could not allocate memory.");
        return -1;
    }

    (*listener_socket)->address.sin_family = AF_INET;
    (*listener_socket)->address.sin_port   = htons(port);
    (*listener_socket)->address.sin_addr.s_addr = INADDR_ANY;

    (*listener_socket)->socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if((*listener_socket)->socket_fd == -1)
    {
        perror("create_passive_socket(): Could not create socket.");
        goto on_error;
    }

    int option = 1;
    if(setsockopt((*listener_socket)->socket_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)))
    {
        perror("create_active_socket(): Could not set socket options.");
        goto on_error;
    }

    socklen_t socket_address_size  = sizeof(struct sockaddr);
    struct sockaddr *socket_address = (struct sockaddr *) &((*listener_socket)->address);

    if(bind((*listener_socket)->socket_fd, socket_address, socket_address_size) == -1)
    {
        perror("create_passive_socket(): Could not bind socket.");
        goto on_error;
    }

    if(listen((*listener_socket)->socket_fd, SOMAXCONN) == -1)
    {
        perror("create_passive_socket(): Could not listen on socket.");
        goto on_error;
    }

    return 0;

    on_error:
        free(*listener_socket);
        return -1;
}

/// Creates an active socket for the client. The socket is blocking.
/// The caller must free allocated memory for the active_socket.
/// \param active_socket - Pointer to a structure holding socket information
/// \param ip_address - String that represents an IPv4 address that the socket connects to
/// \param port - Port the sockets connects to
/// \return 0 - success; -1 - failure
int create_active_socket(socket_info** active_socket, char* ip_address, uint16_t port)
{
    *active_socket = malloc(sizeof(socket_info));

    if(*active_socket == NULL)
    {
        perror("create_active_socket(): Could not allocate memory.");
        return -1;
    }

    (*active_socket)->address.sin_family = AF_INET;
    (*active_socket)->address.sin_port   = htons(port);

    if(inet_pton(AF_INET, ip_address, &(*active_socket)->address.sin_addr) != 1)
    {
        perror("create_active_socket(): Could not parse ip address.");
        return -1;
    }

    (*active_socket)->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if((*active_socket)->socket_fd == -1)
    {
        perror("create_active_socket(): Could not create socket.");
        goto on_error;
    }

    int option = 1;
    if(setsockopt((*active_socket)->socket_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)))
    {
        perror("create_active_socket(): Could not set socket options.");
        goto on_error;
    }

    socklen_t socket_address_size  = sizeof(struct sockaddr);
    struct sockaddr *socket_address = (struct sockaddr *) &((*active_socket)->address);

    if(connect((*active_socket)->socket_fd, socket_address, socket_address_size) == -1)
    {
        perror("create_active_socket(): Could not connect to server.");
        goto on_error;
    }

    return 0;

    on_error:
        free(*active_socket);
        return -1;
}

/// Accepts an incoming connection on a passive/listening socket.
/// \param socket - Pointer to a structure holding socket information
/// \return file descriptor for the socket - success; -1 - failure
int accept_connection(socket_info** socket)
{

    int socket_fd = accept4((*socket)->socket_fd, NULL, NULL, SOCK_NONBLOCK);
    if(socket_fd == -1)
    {
        if(errno != EAGAIN && errno != EWOULDBLOCK)
        {
            perror("accept_connection(): Could not accept connection.");
        }
        return -1;
    }

    return socket_fd;
}

/// Closes and frees socket
/// \param socket - Pointer to a structure holding socket information
/// \return 0 - success; -1 - failure
int destroy_socket(socket_info** socket)
{
    if(close((*socket)->socket_fd) == -1)
    {
        perror("destroy_socket(): Could not close socket.");
        return -1;
    }

    free(*socket);
    *socket = NULL;

    return 0;
}
