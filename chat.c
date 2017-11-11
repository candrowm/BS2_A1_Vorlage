/*
 * Entry point of the chat program that parses command line arguments and
 * starts the corresponding server/client.
 */

#define _GNU_SOURCE
#include "chat.h"
//#include "chat_server_event.h"
#include "chat_server_threads.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include "software_information.h"

/// Prints the name, copyright info and version of the program.
void version(void)
{
    printf("Generic Chat Program - v%s (c) YOUR NAME\n", VERSION);
}


/// Prints instructions on how to start the program and which
/// command line arguments are supported.
void help(void)
{
    version();
    printf("This program starts either a chat client or server, depending\n"\
    "on the supplied arguments.\n\n"\
    "The following options are supported:\n"\
    "\t-s, --server \tchat server is started, waiting for connections\n"\
    "\t\t\tARGUMENT needs to be the port the server is listening on\n\n"\
    "\t-c, --client  \tchat client is started, connects to chat server\n"\
    "\t\t\tARGUMENT needs to be ip:port of the chat server (IPv4)\n\n"\
    "If --s or --server are used the following options are also available:\n"\
    "\t-t, --thread \tuse multithreading\n"\
    "\t\t\tARGUMENT needs to be the number of threads to start [1-n]\n\n"\
    "\t-e, --event  \tuse event loop (default when omitted)\n\n"
    "Example calls:\n"\
    "\tchat -s 8080\n"\
    "\tchat --thread  -s 8080\n"\
    "\tchat -c 127.0.0.1:8080\n\n");

}


/// Print message to stdout and exit
/// \param error_message - String that is printed to stdout
void argument_error(const char* error_message)
{
    help();
    printf("------\n");
    printf("ERROR: %s\n", error_message);
    printf("------\n");
    exit(EXIT_FAILURE);
}


/// Converts a string representing an integer into an integer
/// \param str - String to convert
/// \param num - Pointer to integer where result is stored
/// \return 0 - success; -1 - failure
int string_to_int(const char* str, int* num)
{
    *num = (int) strtol(str, (char** ) NULL, 10);

    if(errno == ERANGE)
    {
        return -1;
    }

    return 0;
}


/// Validates if given string is a valid IPv4 address
/// \param ip - String to validate
/// \return 1 - success; 0 - failure
int is_valid_ip(char *ip)
{
    struct sockaddr_in sa;
    int retVal = inet_pton(AF_INET, ip, &(sa.sin_addr));
    if(retVal != 1)
    {
        return 0;
    }
    return 1;
}


/// Splits a given string of the form "ip:port", into "ip"
/// and "port" and stores the respective values at the addresses
/// of the supplied pointers.
/// Caller needs to free allocated memory for ip, unless this function
/// returns -1
/// \param str - String to split
/// \param ip - Pointer to string, where the resulting ip will be stored
/// \param port - Pointer to integer, where the resulting port will be stored
/// \return 0 - success; -1 - failure
int string_to_ip_port(const char* str, char** ip, int* port)
{
    char *str_copy;
    char *ip_str_copy;
    char *port_str_copy;
    char *safe_ptr;

    if(asprintf(&str_copy, "%s", str) < 0)
    {
        return -1;
    }

    char *ip_ptr = strtok_r(str_copy, ":", &safe_ptr);

    if (ip_ptr == NULL && str_copy != NULL)
    {
        goto on_error_1;
    }

    if (asprintf(&ip_str_copy, "%s", ip_ptr) == -1)
    {
        goto on_error_1;
    }

    char *port_ptr = strtok_r(NULL, ":", &safe_ptr);

    if (port_ptr == NULL && str_copy != NULL)
    {
        goto on_error_2;
    }

    if (asprintf(&port_str_copy, "%s", port_ptr) == -1)
    {
        goto on_error_2;
    }

    if(string_to_int(port_str_copy, port))
    {
        goto on_error_3;
    }

    if(!is_valid_ip(ip_str_copy))
    {
        goto on_error_3;
    }

    *ip = ip_str_copy;
    free(port_str_copy);
    free(str_copy);

    return 0;

    on_error_3:
        free(port_str_copy);
    on_error_2:
        free(ip_str_copy);
    on_error_1:
        free(str_copy);
        return -1;
}

/// main of the chat program. Parses arguments and starts calls the corresponding
/// functions to start either a client, an event-loop server or a multithreaded
/// server.
/// \param argc - Integer, number of arguments
/// \param argv - Pointer to char array, contains arguments
/// \return 0 - success; -1 - failure
int main(int argc, char *argv[])
{
    int multithread_flag = -1;
    int server_flag = -1;

    static struct option long_options[] =
        {
            {"server", required_argument, NULL, 's'},
            {"client", required_argument, NULL, 'c'},
            {"thread", no_argument, NULL, 't'},
            {"event", no_argument, NULL, 'e'},
            {"help", no_argument, NULL, 'h'},
            {"version", no_argument, NULL, 'v'}
        };

    int option_index = 0;
    int opt = 0;

    int port = 0;
    char* ip = NULL;
    int number_of_threads = 1;

    while ((opt = getopt_long(argc, argv, "s:c:t:ehv", long_options, &option_index)) != -1)
    {

        if((opt == 's' || opt == 'c') && server_flag != -1)
        {
            free(ip);
            argument_error("There may only be one occurence of either -s, --server or -c, --client");
        }
        else if((opt == 't' || opt == 'e') && multithread_flag != -1)
        {
            free(ip);
            argument_error("There may only be one occurence of either -t, --thread or -e, --event");
        }

        switch (opt)
        {
            case 's':
                if(string_to_int(optarg, &port))
                {
                    free(ip);
                    argument_error("Argument after -s or --server is not an integer.");
                }
                server_flag = 1;
                break;
            case 'c':
                if(string_to_ip_port(optarg, &ip, &port))
                {
                    free(ip);
                    argument_error("Argument after -c or --client is not ip:port, e.g. '127.0.0.1:8080'.\n"\
                                   "IP address needs to be in IPv4 format ddd.ddd.ddd.ddd with ddd in range 0 to 255.\n"\
                                   "The port has to be an integer.");
                }
                server_flag = 0;
                break;
            case 't':
                if(string_to_int(optarg, &number_of_threads))
                {
                    free(ip);
                    argument_error("Argument after -t or --thread is not an integer.");
                }
                multithread_flag = 1;
                break;
            case 'e':
                multithread_flag = 0;
                break;
            case 'h':
                help();
            case 'v':
                version();
            default:
                free(ip);
                argument_error("Unsupported argument used.");
        }
    }

    if (server_flag == -1)
    {
        free(ip);
        argument_error("Either -c, --client or -s, --server have to be used.");
    }
    else if(server_flag == 0 && multithread_flag != -1)
    {
        free(ip);
        argument_error("The client cannot be start with options -t, --thread and -e, --event.");
    }

    printf("Starting ");

    //When arguments are ok
    if(!server_flag)
    {
        printf("Chat Client: Not implemented in this version\n");
    }
    else if(server_flag && multithread_flag)
    {
        printf("Chat Server (multithreaded)\n");

        chat_server_threads(number_of_threads, port);
    }
    else if(server_flag && !multithread_flag)
    {
        printf("Chat Server (event loop)\n");
       // chat_server_event(port);
    }

    free(ip);

    return 0;
}
