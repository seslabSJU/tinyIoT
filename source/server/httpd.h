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

typedef struct {
  char *name, *value;
} header_t;
static header_t reqhdr[17] = {{"\0", "\0"}};
header_t *request_headers(void);

// user shall implement this function

void route();
void bindfd(int slot);

// Response
#define RESPONSE_PROTOCOL "HTTP/1.1"
#define RESPONSE_CORS_HEADERS "Access-Control-Allow-Origin: *\nAccess-Control-Allow-Headers: Accept, Accept-Language, Content-Language, Content-Type\nAccess-Control-Allow-Methods: GET, PUT, POST, DELETE, OPTIONS\nAccess-Control-Request-Methods: GET, PUT, POST, DELETE, OPTIONS"
#define RESPONSE_JSON_HEADERS "Connection: close\nAccept: application/json\nContent-Type: application/json; charset=utf-8"

#define HTTP_200_CORS printf("%s 200 OK\n%s%s\n\n", RESPONSE_PROTOCOL, RESPONSE_CORS_HEADERS,response_header)
#define HTTP_200_JSON printf("%s 200 OK\n%s\n%s%s\n\n", RESPONSE_PROTOCOL, RESPONSE_CORS_HEADERS, RESPONSE_JSON_HEADERS, response_header)
#define HTTP_201_JSON printf("%s 201 Created\n%s\n%s%s\n\n", RESPONSE_PROTOCOL, RESPONSE_CORS_HEADERS, RESPONSE_JSON_HEADERS, response_header)
#define HTTP_209_JSON printf("%s 209 Conflict\n%s\n%s%s\n\n", RESPONSE_PROTOCOL, RESPONSE_CORS_HEADERS, RESPONSE_JSON_HEADERS, response_header)
#define HTTP_400 printf("%s 400 Bad Request\n%s%s\n\n", RESPONSE_PROTOCOL, RESPONSE_JSON_HEADERS, response_header)
#define HTTP_403 printf("%s 403 Forbidden\n%s%s\n\n", RESPONSE_PROTOCOL, RESPONSE_JSON_HEADERS, response_header)
#define HTTP_404 printf("%s 404 Not found\n%s%s\n\n", RESPONSE_PROTOCOL, RESPONSE_JSON_HEADERS, response_header)
#define HTTP_406 printf("%s 406 Not Acceptable\n%s%s\n\n", RESPONSE_PROTOCOL, RESPONSE_JSON_HEADERS, response_header)
#define HTTP_413 printf("%s 413 Payload Too Large\n%s%s\n\n", RESPONSE_PROTOCOL, RESPONSE_JSON_HEADERS, response_header)
#define HTTP_500 printf("%s 500 Internal Server Error\n%s%s\n\n", RESPONSE_PROTOCOL, RESPONSE_JSON_HEADERS, response_header)

#endif
