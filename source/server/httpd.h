#ifndef _HTTPD_H___
#define _HTTPD_H___

#include <stdio.h>
#include <string.h>

// Client request
extern char *method, // "GET" or "POST"
    *uri,            // "/index.html" things before '?'
    *qs,             // "a=1&b=2" things after  '?'
    *prot,           // "HTTP/1.1"
    *payload;        // for POST

extern int payload_size;

// Server control functions
void serve_forever(const char *PORT);

char *request_header(const char *name);

void set_response_header(char *key, char *value);

void respond_to_client(int status, char *json);

typedef struct {
  char *name, *value;
} header_t;
static header_t reqhdr[17] = {{"\0", "\0"}};
header_t *request_headers(void);

// user shall implement this function

void route();

// Response
#define RESPONSE_PROTOCOL "HTTP/1.1"
#define DEFAULT_RESPONSE_HEADERS "Connection: Close\nAccept: application/json\nContent-Type: application/json; charset=utf-8\nAccess-Control-Allow-Origin: *\nAccess-Control-Allow-Headers: Accept, Accept-Language, Content-Language, Content-Type\nAccess-Control-Allow-Methods: GET, PUT, POST, DELETE, OPTIONS\nAccess-Control-Request-Methods: GET, PUT, POST, DELETE, OPTIONS\n"

#define HTTP_200 printf("%s 200 OK\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define HTTP_201 printf("%s 201 Created\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define HTTP_209 printf("%s 209 Conflict\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define HTTP_400 printf("%s 400 Bad Request\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define HTTP_403 printf("%s 403 Forbidden\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define HTTP_404 printf("%s 404 Not found\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define HTTP_406 printf("%s 406 Not Acceptable\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define HTTP_413 printf("%s 413 Payload Too Large\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define HTTP_500 printf("%s 500 Internal Server Error\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)

#endif
