#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

#include "logger.h"
#include "config.h"

char *log_buffer;

/**
 * @brief Initialize logger
 * @return void
*/
void logger_init(){
    log_buffer = malloc(sizeof(char) * LOG_BUFFER_SIZE);
}

/**
 * @brief Free logger
 * @return void
*/
void logger_free(){
    free(log_buffer);
}

/**
 * @brief Print log message to stderr
 * @param tag Tag of the log message
 * @param level Log level (DEBUG, INFO, WARN, ERROR, FATAL)
 * @param msg Message to print includes format strings
 * @param ... Arguments for format strings
 * @return Number of characters printed
*/
int logger(const char* tag,  LOGLEVEL level, const char *msg, ...){

    va_list ap;
    char *t = NULL;
    char *llChar;
    int charsCnt = 0, fcolor = 0;

    switch (level)
        {
        case LOG_LEVEL_DEBUG:
            llChar = "DEBUG";
            fcolor = BLUE;
            break;

        case LOG_LEVEL_INFO:
            llChar = "INFO";
            fcolor = GREEN;
            break;

        case LOG_LEVEL_WARN:
            llChar = "WARN";
            fcolor = YELLOW;
            break;

        case LOG_LEVEL_ERROR:
            llChar = "ERROR";
            fcolor = BR_RED;
            break;
        
        case LOG_LEVEL_FATAL:
            llChar = "FATAL";
            fcolor = RED;
            break;

        default:
            return 0;
    }

    if(level >= LOG_LEVEL){
        time_t now;
        time(&now);
        t = ctime(&now);
        t[24] = '\0';
        fprintf(stderr, "\r%s \033[0;%dm%-5s\033[0m [%s]: ", t, fcolor, llChar, tag);

        va_start(ap, msg);
        charsCnt = vsnprintf(log_buffer, LOG_BUFFER_SIZE, msg, ap);
        va_end(ap);
        fprintf(stderr, "%s\n", log_buffer);
    }

    return charsCnt;
}