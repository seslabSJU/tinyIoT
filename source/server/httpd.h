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

typedef struct _header{
  char *name; 
  char *value;
  struct _header *next;
} header_t;
typedef struct _request {
    char *method;   // METHOD : GET, POST, PUT, DELETE, OPTIONS (use static string)
    char *uri;      // 
    char *qs;       // query string
    char *prot;     // protocol (HTTP/1.1)
    char *payload;  // body for POST
    int payload_size; //  content-size
    header_t *headers; // header list
} HTTPRequest;

typedef struct _response {
    int status_code;
    char *protocol;
    char *status_msg;
    header_t *headers;
    char *payload;
    int payload_size;
} HTTPResponse;

typedef struct _host {
    char *host;
    int port;
} Host;


// Server control functions
void serve_forever(const char *PORT);
char *search_header(header_t *h, const char *name);
void set_header(char *key, char *value, char *response_headers);
void normalize_payload(char *body);
Operation http_parse_operation(char *method);
void http_respond_to_client(oneM2MPrimitive *o2pt, int slotno);
void http_notify(oneM2MPrimitive *o2pt, char *host, int port);
void http_forwarding(oneM2MPrimitive *o2pt, char *host, int port);



// user shall implement this function
void handle_http_request(HTTPRequest *req, int slotno);
char *op_to_method(Operation op);
int send_http_request(char *host, int port,  HTTPRequest *req, HTTPResponse *res);
void add_header(char *key, char *value, header_t *header);

void parse_http_request(HTTPRequest *req, char *packet);

void free_HTTPRequest(HTTPRequest *req);
void free_HTTPResponse(HTTPResponse *res);


// Response
#define HTTP_PROTOCOL_VERSION "HTTP/1.1"
#define DEFAULT_RESPONSE_HEADERS "Connection: Close\nAccept: application/json\nContent-Type: application/json; charset=utf-8\nAccess-Control-Allow-Origin: *\nAccess-Control-Allow-Headers: Accept, Accept-Language, Content-Language, Content-Type, X-M2M-Origin, X-M2M-RI, X-M2M-RVI\nAccess-Control-Allow-Methods: GET, PUT, POST, DELETE, OPTIONS\nAccess-Control-Request-Methods: GET, PUT, POST, DELETE, OPTIONS\n"
#define DEFAULT_REQUEST_HEADERS "Content-Type: application/json\r\n"

#endif
