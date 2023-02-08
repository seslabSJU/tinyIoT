#ifndef __LOGGER_H__
#define __LOGGER_H__

typedef enum{
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_FATAL = 4
}LOGLEVEL;

typedef enum{
BLUE = 94,
GREEN = 32,
YELLOW = 93,
BR_RED = 91,
RED = 31
}COLOR;

int logger(const char* tag,  LOGLEVEL level, const char *msg, ...);

#endif