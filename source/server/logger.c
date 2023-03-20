#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

#include "logger.h"
#include "config.h"


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
        fprintf(stderr, "%s \033[0;%dm%-5s\033[0m [%s]: ", t, fcolor, llChar, tag);

        t = malloc(sizeof(char) * LOG_BUFFER_SIZE);

        va_start(ap, msg);
        charsCnt = vsnprintf(t, LOG_BUFFER_SIZE, msg, ap);
        va_end(ap);
        fprintf(stderr, "%s\n", t);
        free(t);
    }

    return charsCnt;
}