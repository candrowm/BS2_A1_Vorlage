//
// Created by andi on 09.11.17.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include "tcp_socket.h"
#include <unistd.h>
#include <fcntl.h>

#define TRUE             1
#define FALSE            0

typedef enum Eventtypes{
    NEW_CONNECTION,
    MSG_RECEIVED,
    MSG_TO_SEND,
    DISCONNECT,
    KEYPRESS
}e_type;

const char* getEventName(enum Eventtypes eventtype)
{
    switch (eventtype)
    {
        case NEW_CONNECTION: return "NEW_CONNECTION";
        case MSG_RECEIVED: return "MSG_RECEIVED";
        case MSG_TO_SEND: return "MSG_TO_SEND";
        case DISCONNECT: return "DISCONNECT";
        case KEYPRESS: return "KEYPRESS";
        default: return "Unknown!";
    }
}

struct event {
    e_type type;
    //struct timeval t; // the timestamp
    int fd;
    char* message;
    ssize_t msglen;
};

// callback: type for event handlers
typedef void (*callback) (struct event *);

struct subscription {
    e_type type;
    callback cb;
};
struct subscription* subscriptions[100];
int subscrCounter = 0;

struct socket_info* listenInfo;
ssize_t  dataSize = 1;
int    rc = 1;
int    end_server = FALSE;
int    listen_sd = -1;
int    timeout;
struct pollfd fds[200];
nfds_t nfds = 2;
int    current_size = 0, j, i;



/*******************************************************/
/* Event Creation                                      */
/*******************************************************/
struct event* createEvent(e_type type, int fd, char* message, ssize_t msglen){
    struct event* evp = malloc(sizeof(struct event));
    evp->type = type;
    evp->fd = fd;
    evp->message = message;
    evp->msglen = msglen;
    return evp;
}

void destroyEvent(struct event* eventPointer){
    free(eventPointer);
}

void subscribe(e_type type, callback cb){
    struct subscription* s;
    s = (struct subscription *) malloc(sizeof(struct subscription));
    s->type=type;
    s->cb=cb;
    subscriptions[subscrCounter] = s;
    subscrCounter++;
}


/*******************************************************/
/* Event Queue                                      */
/*******************************************************/
const int qmax = 1000000;
struct event* evtQ[1000000];
int qfront = 0;
int qrear = -1;
int qCount = 0;

int qIsFull(){
    return qCount == qmax;
}

void qInsert(struct event* val){
    if(!qIsFull()){
        if(qrear == qmax-1){
            qrear = -1;
        }

        evtQ[++qrear] = val;
        qCount++;
    }
}

int qIsEmpty(){
    return qCount == 0;
}

void qRemove(struct event** val){
    if(qIsEmpty()){
        perror("  qRemove failed: queue is empty!");
        return;
    }

    /* We have an element. Pop it normally and return it in val_r. */
    *val = evtQ[qfront++];

    if(qfront == qmax){
        qfront = 0;
    }

    qCount--;
}



void writeToConsole(char* msg){
    /*struct sockaddr_in address;
    int addrlen;
    // Get details
    getpeername(fromFd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
    printf("%s:%d - %s\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port), msg);*/
    printf("%s\n", msg);
}


/*******************************************************/
/* Event Handlers                                      */
/*******************************************************/
void handleConnect(struct event* evp) {
    //printf("handleConnect\n");
    int new_sd = accept_connection(&listenInfo);

    if (new_sd < 0) {
        if (errno != EWOULDBLOCK) {
            end_server = TRUE;
        }
        /*****************************************************/
        /* If accept fails with EWOULDBLOCK, then we         */
        /* have accepted all new connections.
        /*****************************************************/
        return;
    }

    /*****************************************************/
    /* Add the new incoming connection to the            */
    /* pollfd structure                                  */
    /*****************************************************/
    fds[nfds].fd = new_sd;
    fds[nfds].events = POLLIN;
    nfds++;

    char* msg = malloc(1024*sizeof(char));
    sprintf(msg, "FD %d has entered the chat room.\n", new_sd);
    printf("%s", msg);

    for(int j=0; j<nfds; j++){
        int fd = fds[j].fd;
        if(fd != new_sd && fd != listen_sd && fd != 0){
            char* mymsg = malloc(1024*sizeof(char));
            strcpy(mymsg, msg);
            qInsert(createEvent(MSG_TO_SEND, fd, mymsg, strlen(mymsg)));
        }
    }

    /*****************************************************/
    /* Check if there are more new connections           */
    /*****************************************************/
    qInsert(createEvent(NEW_CONNECTION, listen_sd, "", 0));

    destroyEvent(evp);
}


void handleReceive(struct event* evp){
    /*******************************************************/
    /* Receive all incoming data on this socket            */
    /* before we loop back and call poll again.            */
    /*******************************************************/
    int close_conn = FALSE;
    char buffer[1024];
    do
    {
        // Clear buffer
        memset(&buffer[0],0,sizeof(buffer));

        /*****************************************************/
        /* Receive data on this connection until the         */
        /* recv fails with EWOULDBLOCK. If any other        */
        /* failure occurs, we will close the                 */
        /* connection.                                       */
        /*****************************************************/
        dataSize = recv(evp->fd, buffer, sizeof(buffer), 0);
        if (dataSize < 0)
        {
            if (errno != EWOULDBLOCK)
            {
                //perror("  recv() failed");
                close_conn = TRUE;
            }
            break;
        }

        /*****************************************************/
        /* Check to see if the connection has been           */
        /* closed by the client                              */
        /*****************************************************/
        if (dataSize == 0)
        {
            printf("  Connection closed\n");
            close_conn = TRUE;
            break;
        }

        /*****************************************************/
        /* Data was received                                 */
        /*****************************************************/
        struct sockaddr_in address;
        int addrlen;
        // Get details
        getpeername(evp->fd , (struct sockaddr*)&address , (socklen_t*)&addrlen);

        char* msg = malloc(1024*sizeof(char));
        sprintf(msg, "%s:%d:%d - %s\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port), evp->fd, buffer);

        /*****************************************************/
        /* Write message to terminal                         */
        /*****************************************************/
        //writeToConsole(evp->fd, buffer);
        writeToConsole(msg);

        /*****************************************************/
        /* Create Write Event                                */
        /*****************************************************/
        for(int j=0; j<nfds; j++){
            int fd = fds[j].fd;
            if(fd != evp->fd && fd != listen_sd && fd != 0){
                //printf("Adding to queue for fd %d", fd);
                char* mymsg = malloc(1024*sizeof(char));
                strcpy(mymsg, msg);
                qInsert(createEvent(MSG_TO_SEND, fd, mymsg, strlen(mymsg)));
            }
        }
    } while(TRUE);

    /*******************************************************/
    /* If the close_conn flag was turned on, we need       */
    /* to clean up this active connection. This           */
    /* clean up process includes removing the              */
    /* descriptor.                                         */
    /*******************************************************/
    if (close_conn){
        qInsert(createEvent(DISCONNECT, evp->fd, "", 0));
    }
    destroyEvent(evp);
}


void handleSend(struct event* evp){
    dataSize = send(evp->fd, evp->message, (size_t) evp->msglen, 0);
    if (dataSize < 0)
    {
        perror("  send() failed");
        qInsert(createEvent(DISCONNECT, evp->fd, "", 0));
        free(evp->message);
    }
    destroyEvent(evp);
}


void handleDisconnect(struct event* evp){
    close(evp->fd);

    char* msg = malloc(1024*sizeof(char));
    sprintf(msg, "FD %d has left the chat room.\n", evp->fd);
    printf("%s", msg);

    for(int j=0; j<nfds; j++){
        int fd = fds[j].fd;
        if(fd != evp->fd && fd != listen_sd && fd != 0){
            char* mymsg = malloc(1024*sizeof(char));
            strcpy(mymsg, msg);
            qInsert(createEvent(MSG_TO_SEND, fd, mymsg, strlen(mymsg)));
        }
    }

    for(int i=0; i<nfds; i++){
        if(evp->fd == fds[i].fd){
            fds[i].fd = -1;

            for(j = i; j < nfds; j++)
            {
                fds[j].fd = fds[j+1].fd;
            }
            nfds--;
            break;
        }
    }

    destroyEvent(evp);
}


void handleKeypress(struct event* evp){
    char* c = malloc(1024*sizeof(char));
    read (0, c, 1024);

    for(int j=0; j<nfds; j++){
        int fd = fds[j].fd;
        if(fd != 0 && fd != listen_sd){
            char* mymsg = malloc(1024*sizeof(char));
            strcpy(mymsg, "Server: ");
            strcat(mymsg, c);
            qInsert(createEvent(MSG_TO_SEND, fd, mymsg, strlen(mymsg)));
        }
    }

    destroyEvent(evp);
}



void chat_server_event(int port)
{
    printf("Starting event server \n");
    if( (create_passive_socket(&listenInfo, (uint16_t ) port)) != 0)
    {
        printf("Couldn't create passive socket \n");
        exit(EXIT_FAILURE);
    }
    listen_sd = listenInfo->socket_fd;
    /*************************************************************/
    /* pollfd structure init                                      */
    /*************************************************************/
    memset(fds, 0 , sizeof(fds));

    /*************************************************************/
    /* Set up listening socket and terminal fd                   */
    /*************************************************************/
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fcntl (0, F_SETFL, O_NONBLOCK);

    fds[1].fd = listen_sd;
    fds[1].events = POLLIN;
    timeout = 0; // Now 0, because we want the event queue to be processed even if poll is not ready and we're not multithreading.... (10 * 60 * 1000);

    /*************************************************************/
    /* Register Event Handlers                                  */
    /*************************************************************/
    //printf("Adding subscriptions\n");
    subscribe(NEW_CONNECTION, handleConnect);
    subscribe(MSG_RECEIVED, handleReceive);
    subscribe(MSG_TO_SEND, handleSend);
    subscribe(DISCONNECT, handleDisconnect);
    subscribe(KEYPRESS, handleKeypress);

    /*************************************************************/
    /* Event Loop   */
    /*************************************************************/
    do
    {
        //printf("Polling...\n");
        rc = poll(fds, nfds, timeout);

        if (rc < 0) {
            perror("  poll() failed");
            break;
        }
        if (rc == 0)
        {
            /* Poll time out (0 s) - nothing to read */
            //printf("  poll() timed out\n");
        }else{
            /***********************************************************/
            /* Find and handle the active FDs                           */
            /***********************************************************/
            current_size = nfds;
            for (i = 0; i < current_size; i++)
            {
                if(fds[i].revents == 0){
                    //printf("FD not active\n");
                    continue;
                }


                /*********************************************************/
                /* If revents is not POLLIN, it's an unexpected result,  */
                /* log and end the server.                               */
                /*********************************************************/
                if(fds[i].revents != POLLIN){
                    printf("  Error! revents = %d\n", fds[i].revents);
                    end_server = TRUE;
                    break;

                }
                if (fds[i].fd == listen_sd){
                    //printf("  Listening socket is readable\n");
                    qInsert(createEvent(NEW_CONNECTION, listen_sd, "", 0));
                }
                else if(fds[i].fd == 0){
                    // Writing on terminal
                    qInsert(createEvent(KEYPRESS, 0, "", 0));
                }
                else{
                    //printf("  Descriptor %d is readable\n", fds[i].fd);
                    qInsert(createEvent(MSG_RECEIVED, fds[i].fd,"", 0));
                }  /* End of existing connection is readable             */
            } /* End of loop through pollable descriptors              */

        }


        /*********************************************************/
        /* Event Handling in same Thread:                        */
        /* But only run through the event queue once because
         * we will create blocking behavior if we keep looping
         * when we always create a new write event if we cant write
        /*********************************************************/
        for(int i=0; i<qCount; i++){
            struct event* event;
            qRemove(&event);
            if(event != NULL){
                //printf("Handling event %s - overall %d subscriptions\n", getEventName(event->type), subscrCounter);
                for(int j=0; j<subscrCounter; j++){
                    if(subscriptions[j]->type == event->type){
                        //printf("Found subscription\n");
                        subscriptions[j]->cb(event);
                        //printf("Subscription handled\n");
                    }
                }
            }
        }

    } while (end_server == FALSE); /* End of serving running.    */

    /*************************************************************/
    /* Clean up all of the sockets that are open                  */
    /*************************************************************/
    for (i = 0; i < nfds; i++)
    {
        if(fds[i].fd >= 0)
            close(fds[i].fd);
    }
}

