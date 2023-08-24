#ifndef __HTTPD_H__
#define __HTTPD_H__

#include <stdio.h>
#include <string.h>
#include "onem2m.h"

// Client request
// extern char *method, // "GET" or "POST"
//     *uri,            // "/index.html" things before '?'
//     *qs,             // "a=1&b=2" things after  '?'
//     *prot,           // "HTTP/1.1"
//     *payload;        // for POST

// extern int payload_size;

typedef struct {
  char *name; 
  char *value;
} header_t;
typedef struct _request {
    char *method;
    char *uri;
    char *qs;
    char *prot;
    char *payload;
    int payload_size;
    header_t *headers;
    int header_count;
} HTTPRequest;


//static header_t reqhdr[17] = {{"\0", "\0"}};

// Server control functions
void serve_forever(const char *PORT);
char *request_header(header_t *h, int cnt, const char *name);
void set_response_header(char *key, char *value, char *response_headers);
void normalize_payload(char *body);
Operation http_parse_operation(char *method);
void http_respond_to_client(oneM2MPrimitive *o2pt, int slotno);
void http_notify(oneM2MPrimitive *o2pt, char *noti_json, NotiTarget *nt);
void http_forwarding(oneM2MPrimitive *o2pt, char *host, char *port);

// user shall implement this function
void handle_http_request(HTTPRequest *req, int slotno);

void http_send_get_request(char *host, int port, char *uri, char *header, char *qs, char *data);

// Response
#define HTTP_PROTOCOL_VERSION "HTTP/1.1"
#define DEFAULT_RESPONSE_HEADERS "Connection: Close\nAccept: application/json\nContent-Type: application/json; charset=utf-8\nAccess-Control-Allow-Origin: *\nAccess-Control-Allow-Headers: Accept, Accept-Language, Content-Language, Content-Type, X-M2M-Origin, X-M2M-RI, X-M2M-RVI\nAccess-Control-Allow-Methods: GET, PUT, POST, DELETE, OPTIONS\nAccess-Control-Request-Methods: GET, PUT, POST, DELETE, OPTIONS\n"
#define DEFAULT_REQUEST_HEADERS "Content-Type: application/json\r\n"

#endif
