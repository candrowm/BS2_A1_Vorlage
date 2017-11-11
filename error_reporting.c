//
// Created by robert on 29.09.17.
//

#include "error_reporting.h"
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdarg.h>

/// Flag for error reporting; 1 - print, 0 - no print
#define REPORT_ERROR 1

/// Mutex securing stdout/stderr
pthread_mutex_t mutex_print = PTHREAD_MUTEX_INITIALIZER;

/// Prints the current time to stderr
void print_time_error(void)
{
    char buffer[30];
    time_t current_time;
    struct tm *time_struct;

    current_time = time(NULL);
    time_struct = localtime(&current_time);

    if (time_struct == NULL)
    {
        return;
    }

    strftime(buffer, sizeof(buffer), "%F %T", time_struct);
    fprintf(stderr, "[%s] ", buffer);
}

/// Prints the message to stderr, including errno information if available.
/// \param message - String to print
void print_error(const char* message)
{
    if(!REPORT_ERROR)
    {
        return;
    }

    pthread_mutex_lock(&mutex_print);

    print_time_error();

    if(errno != 0)
    {
        perror(message);
        errno = 0;
    }
    else
    {
        fprintf(stderr, "%s\n", message);
    }

    pthread_mutex_unlock(&mutex_print);
}

/// Formatted print for a variable number of arguments
/// to stderr.
/// \param format - Format string
/// \param ... - variadic arguments
void fprintf_error(const char *format, ...)
{
    if(!REPORT_ERROR)
    {
        return;
    }

    pthread_mutex_lock(&mutex_print);

    print_time_error();

    va_list arguments;

    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);

    pthread_mutex_unlock(&mutex_print);
}