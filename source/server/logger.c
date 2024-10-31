#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

#include "util.h"
#include "logger.h"
#include "config.h"

char *log_buffer;
FILE *log_file;

/**
 * @brief Initialize logger
 * @return void
 */
void logger_init()
{
    char logFileName[100];
    log_buffer = malloc(sizeof(char) * LOG_BUFFER_SIZE);
#ifdef SAVE_LOG
    time_t now;
    time(&now);
    char *t = get_local_time(0);
    sprintf(logFileName, "%s.log", t);
    log_file = fopen(logFileName, "w");
    free(t);
#endif
}

/**
 * @brief Free logger
 * @return void
 */
void logger_free()
{
    free(log_buffer);
#ifdef SAVE_LOG
    fclose(log_file);
#endif
}

#if MULTI_THREAD == 0
bool writing = false;
#endif

/**
 * @brief Print log message to stderr
 * @param tag Tag of the log message
 * @param level Log level (DEBUG, INFO, WARN, ERROR, FATAL)
 * @param msg Message to print includes format strings
 * @param ... Arguments for format strings
 * @return Number of characters printed
 */
int logger(const char *tag, LOGLEVEL level, const char *msg, ...)
{

    va_list ap;
    char *t = NULL;
    char *llChar;
    int charsCnt = 0, fcolor = 0;
#if MULTI_THREAD == 0
    while (writing)
    {
        usleep(20);
    }
#endif
    writing = true;
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

    if (level >= LOG_LEVEL)
    {
        time_t now;
        time(&now);
        t = ctime(&now);
        t[24] = '\0';
        fprintf(stderr, "\r%s \033[0;%dm%-5s\033[0m [%s]: ", t, fcolor, llChar, tag);

        va_start(ap, msg);
        charsCnt = vsnprintf(log_buffer, LOG_BUFFER_SIZE, msg, ap);
        va_end(ap);
        fprintf(stderr, "%s\n", log_buffer);

#ifdef SAVE_LOG
        fprintf(log_file, "\r%s %-5s [%s]: %s\n", t, llChar, tag, log_buffer);
#endif
    }
    writing = false;
    return charsCnt;
}