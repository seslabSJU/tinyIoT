#ifndef __HTTPD_H__
#define __HTTPD_H__

#include <stdio.h>
#include <string.h>
#include "onem2m.h"

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
void http_respond_to_client(oneM2MPrimitive *o2pt, int slotno);
void http_notify(oneM2MPrimitive *o2pt, char *noti_json, NotiTarget *nt);

typedef struct {
  char *name, *value;
} header_t;
static header_t reqhdr[17] = {{"\0", "\0"}};
header_t *request_headers(void);

// user shall implement this function
void handle_http_request(int slotno);

// Response
#define HTTP_PROTOCOL_VERSION "HTTP/1.1"
#define DEFAULT_RESPONSE_HEADERS "Connection: Close\nAccept: application/json\nContent-Type: application/json; charset=utf-8\nAccess-Control-Allow-Origin: *\nAccess-Control-Allow-Headers: Accept, Accept-Language, Content-Language, Content-Type, X-M2M-Origin, X-M2M-RI, X-M2M-RVI\nAccess-Control-Allow-Methods: GET, PUT, POST, DELETE, OPTIONS\nAccess-Control-Request-Methods: GET, PUT, POST, DELETE, OPTIONS\n"
#define DEFAULT_REQUEST_HEADERS "Content-Type: application/json\r\n"

#endif
