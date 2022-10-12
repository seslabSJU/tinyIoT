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
#define RESPONSE_JSON_HEADERS "Accept: application/json\nContent-Type: application/json; charset=utf-8"

#define HTTP_200_CORS printf("%s 200 OK\n%s\n\n", RESPONSE_PROTOCOL, RESPONSE_CORS_HEADERS)
#define HTTP_200_JSON printf("%s 200 OK\n%s\n%s\n\n", RESPONSE_PROTOCOL, RESPONSE_CORS_HEADERS, RESPONSE_JSON_HEADERS)
#define HTTP_201_JSON printf("%s 201 Created\n%s\n%s\n\n", RESPONSE_PROTOCOL, RESPONSE_CORS_HEADERS, RESPONSE_JSON_HEADERS)
#define HTTP_209_JSON printf("%s 209 Conflict\n%s\n%s\n\n", RESPONSE_PROTOCOL, RESPONSE_CORS_HEADERS, RESPONSE_JSON_HEADERS)
#define HTTP_400 printf("%s 400 Bad Request\n%s\n\n", RESPONSE_PROTOCOL, RESPONSE_JSON_HEADERS)
#define HTTP_404 printf("%s 404 Not found\n%s\n\n", RESPONSE_PROTOCOL, RESPONSE_JSON_HEADERS)
#define HTTP_406 printf("%s 406 Not Acceptable\n%s\n\n", RESPONSE_PROTOCOL, RESPONSE_JSON_HEADERS)
#define HTTP_500 printf("%s 500 Internal Server Error\n%s\n\n", RESPONSE_PROTOCOL, RESPONSE_JSON_HEADERS)

// some interesting macro for `route()`
#define ROUTE_START() if (0) {
#define ROUTE(METHOD, URI)                                                     \
  }                                                                            \
  else if (strcmp(URI, uri) == 0 && strcmp(METHOD, method) == 0) {
#define GET(URI) ROUTE("GET", URI)
#define POST(URI) ROUTE("POST", URI)
#define ROUTE_END()                                                            \
  }                                                                            \
  else HTTP_500

#endif
