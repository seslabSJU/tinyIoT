#ifndef __HTTPD_H__
#define __HTTPD_H__

#include <stdio.h>
#include <string.h>
#include "onem2m.h"
#include "mqttClient.h"

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
void set_response_header(char *key, char *value, char *response_headers);
void normalize_payload();
Operation http_parse_operation();
void http_respond_to_client(oneM2MPrimitive *o2pt, int status);

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
#define LOG_HTTP_200 fprintf(stderr,"%s 200 OK\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define LOG_HTTP_201 fprintf(stderr,"%s 201 Created\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define LOG_HTTP_209 fprintf(stderr,"%s 209 Conflict\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define LOG_HTTP_400 fprintf(stderr,"%s 400 Bad Request\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define LOG_HTTP_403 fprintf(stderr,"%s 403 Forbidden\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define LOG_HTTP_404 fprintf(stderr,"%s 404 Not found\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define LOG_HTTP_406 fprintf(stderr,"%s 406 Not Acceptable\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define LOG_HTTP_413 fprintf(stderr,"%s 413 Payload Too Large\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)
#define LOG_HTTP_500 fprintf(stderr,"%s 500 Internal Server Error\n%s%s\n", RESPONSE_PROTOCOL, DEFAULT_RESPONSE_HEADERS, response_headers)

#endif
