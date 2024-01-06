#include "httpd.h"
#include "config.h"
#include "onem2m.h"
#include "util.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#define MAX_CONNECTIONS 1024
#define BUF_SIZE 65535
#define QUEUE_SIZE 128

pthread_mutex_t mutex_lock;
int listenfd;
int clients[MAX_CONNECTIONS];
static void start_server(const char *);
static void respond(int);
extern void route(oneM2MPrimitive *o2pt);

static char *buf[MAX_CONNECTIONS];

// Client request


void *respond_thread(void *ps) {
    int slot = *((int*)ps);
    respond(slot);
    close(clients[slot]);
    clients[slot] = -1;
    return NULL;
}

void serve_forever(const char *PORT) {
    pthread_mutex_init(&mutex_lock, NULL);
    struct sockaddr_in clientaddr;
    socklen_t addrlen = 0;

    int slot = 0;
    int slots[MAX_CONNECTIONS];
    for(int i=0; i<MAX_CONNECTIONS; i++) {
        slots[i] = i;
        clients[i] = -1;
    }   

    start_server(PORT); 
    logger("HTTP", LOG_LEVEL_INFO, "Server started %shttp://127.0.0.1:%s%s", "\033[92m", PORT, "\033[0m");  
    // Ignore SIGCHLD to avoid zombie threads
    signal(SIGCHLD, SIG_IGN); 
    // ACCEPT connections
    int on = 1;
    struct timeval tv;
    tv.tv_sec  = SOCKET_TIMEOUT;
    tv.tv_usec = 0;
    while (1) {
        clients[slot] = accept(listenfd, (struct sockaddr *)&clientaddr, &addrlen);
        setsockopt(clients[slot], IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));    
        // set timeout for socket
        setsockopt(clients[slot], SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv)); 
        if (clients[slot] < 0) {
            perror("accept() error");
            //exit(1);
            clients[slot] = -1;
        } else {
            pthread_t thread_id;
            if(MONO_THREAD) {
                respond(slot);
                close(clients[slot]);
                clients[slot] = -1;
            }else{
                pthread_create(&thread_id, NULL, respond_thread, (void*)&slots[slot]);
            }
        }
        while (clients[slot] != -1)
            slot = (slot + 1) % MAX_CONNECTIONS;
    }
}

// start server
void start_server(const char *port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    // create socket
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0", &(addr.sin_addr.s_addr));
    addr.sin_port = htons(atoi(port));    
    listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int option = 1;
    struct linger {
        int l_onoff;
        int l_linger;
    };
    struct linger _linger;
    _linger.l_onoff = 0;
    _linger.l_linger = 0;

    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, &option, sizeof(option));
    #ifdef TCP_KEEPALIVE
    setsockopt(listenfd, IPPROTO_TCP, TCP_KEEPALIVE, &option, sizeof(option));
    #endif
    if(listenfd == -1) {
        perror("socket() error");
        exit(1);
    }
    // bind socket to address
    if(bind(listenfd, &addr, sizeof(addr)) == -1) {
        perror("bind() error");
        exit(1);
    }
    
    if (listen(listenfd, QUEUE_SIZE) != 0) {
        perror("listen() error");
        exit(1);
    }
}

// get header by name
char *search_header(header_t *h, const char *name) {
    if(h == NULL) return NULL;
    header_t *ptr = h;
    while(ptr){
        if(strcmp(ptr->name, name) == 0) return ptr->value;
        ptr = ptr->next;
    }
  return NULL;
}

// Handle escape characters (%xx)
static void uri_unescape(char *uri) {
    char chr = 0;
    char *src = uri;
    char *dst = uri;

    // Skip initial non encoded character
    while (*src && !isspace((int)(*src)) && (*src != '%')) 
      src++;    
    // Replace encoded characters with corresponding code.
    dst = src;
    while (*src && !isspace((int)(*src))) {
        if (*src == '+')
            chr = ' ';
        else if ((*src == '%') && src[1] && src[2]) {
            src++;
            chr = ((*src & 0x0F) + 9 * (*src > '9')) * 16;
            src++;
            chr += ((*src & 0x0F) + 9 * (*src > '9'));
        } else
            chr = *src;

        *dst++ = chr;
        src++;
    }

    *dst = '\0';
}

// client connection
void respond(int slot) {
    HTTPRequest *req = calloc(1, sizeof(HTTPRequest));
    int rcvd;
    bool flag = false;
    char buffer[BUF_SIZE] = {0};
    buf[slot] = malloc(BUF_SIZE*sizeof(char));
    rcvd = recv(clients[slot], buf[slot], BUF_SIZE, 0); 

    if (rcvd < 0){ // receive error
        logger("HTTP", LOG_LEVEL_ERROR, "recv() error");
        return;
    } else if (rcvd == 0) { // receive socket closed
        logger("HTTP", LOG_LEVEL_ERROR, "Client disconnected upexpectedly");
        return;
    }
    // message received
    pthread_mutex_lock(&mutex_lock);  
    buf[slot][rcvd] = '\0';
    logger("HTTP", LOG_LEVEL_DEBUG, "\n\n%s\n",buf[slot]);
    memcpy(buffer, buf[slot], rcvd);

    parse_http_request(req, buffer);
    if(req->payload_size > 0 && ((req->payload == NULL) || strlen(req->payload) == 0) ) {   // if there is payload but it is not in the buffer
        flag = true;
        req->payload = (char *)calloc(MAX_PAYLOAD_SIZE, sizeof(char));
        recv(clients[slot], req->payload, MAX_PAYLOAD_SIZE, 0); // receive payload
    }

    if(req->payload) normalize_payload(req->payload);

    // call router
    handle_http_request(req, slot);    
    free(buf[slot]);
    free_HTTPRequest(req);
    pthread_mutex_unlock(&mutex_lock);
}

Operation http_parse_operation(char *method){
	Operation op;

	if(strcmp(method, "POST") == 0) op = OP_CREATE;
	else if(strcmp(method, "GET") == 0) op = OP_RETRIEVE;
	else if (strcmp(method, "PUT") == 0) op = OP_UPDATE;
	else if (strcmp(method, "DELETE") == 0) op = OP_DELETE;
    else if (strcmp(method, "OPTIONS") == 0) op = OP_OPTIONS;

	return op;
}

void handle_http_request(HTTPRequest *req, int slotno) {
	oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(1, sizeof(oneM2MPrimitive));
    cJSON *fcjson = NULL;
	char *header = NULL;    
    
	if(req->payload) {
        o2pt->pc = strdup(req->payload);
		o2pt->cjson_pc = cJSON_Parse(o2pt->pc);
	} 
    

	if((header = search_header(req->headers, "X-M2M-Origin"))) {
        logger("HTTP", LOG_LEVEL_DEBUG, "fr: %s", header);
		o2pt->fr = strdup(header);
	} 

	if((header = search_header(req->headers, "X-M2M-RI")) && header) {
		o2pt->rqi = strdup(header);
	} 

	if((header = search_header(req->headers, "X-M2M-RVI")) && header) {
		o2pt->rvi = strdup(header);
	} 
    
	if(req->uri) {
        if(req->uri[1] == '~'){ // SP relative
            o2pt->to = (char *) malloc(sizeof(char) * (strlen(req->uri+1)));
            o2pt->to[0] = '/';
            strcat(o2pt->to, req->uri+3);
        }else if(req->uri[1] == '_'){ // absolute
            o2pt->to = (char *) malloc(sizeof(char) * (strlen(req->uri+1) + 1));
            o2pt->to[0] = '/';
            o2pt->to[1] = '/';
            strcat(o2pt->to, req->uri+3);
        }
        else{
            o2pt->to = strdup(req->uri+1);
        }

        logger("HTTP", LOG_LEVEL_DEBUG, "to: %s", o2pt->to);
	} 
    
    o2pt->op = http_parse_operation(req->method);    
    logger("HTTP", LOG_LEVEL_INFO, "Request : %s", req->method);

    /* get client ip address */
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    getpeername(clients[slotno], (struct sockaddr *)&client_addr, &client_addr_len);
    o2pt->ip = (char *)malloc((INET_ADDRSTRLEN + 1) * sizeof(char));
    logger("HTTP", LOG_LEVEL_INFO, "Client connected %d.%d.%d.%d\n",
         (int)(client_addr.sin_addr.s_addr&0xFF), (int)((client_addr.sin_addr.s_addr&0xFF00)>>8), (int)((client_addr.sin_addr.s_addr&0xFF0000)>>16), (int)((client_addr.sin_addr.s_addr&0xFF000000)>>24));

    inet_ntop(AF_INET, &(client_addr.sin_addr.s_addr), o2pt->ip, INET_ADDRSTRLEN);
    logger("HTTP", LOG_LEVEL_DEBUG, "ip: %s", o2pt->ip);

	if(o2pt->op == OP_CREATE) o2pt->ty = http_parse_object_type(req->headers);
    else if(o2pt->op == OP_OPTIONS){
        o2pt->rsc = RSC_OK;
        http_respond_to_client(o2pt, slotno);
        free_o2pt(o2pt);
        return;
    }
    // else if(o2pt->op == OP_FORWARDING){
    //     o2pt->pc = strndup(buf[slotno], BUF_SIZE);
    // }
    o2pt->errFlag = false;

    

    if(req->qs && strlen(req->qs) > 0){
        o2pt->fc = qs_to_json(req->qs);
        parse_filter_criteria(o2pt->fc);
        if(cJSON_GetNumberValue(cJSON_GetObjectItem(o2pt->fc, "fu")) == FU_DISCOVERY){
            o2pt->op = OP_DISCOVERY;
        }
    }
    
    pthread_mutex_trylock(&mutex_lock);
	route(o2pt);
    pthread_mutex_unlock(&mutex_lock);
    
    http_respond_to_client(o2pt, slotno);
    free_o2pt(o2pt);
}

void set_header(char *key, char *value, char *response_headers) {
    if(!value) return;
    if(!response_headers) return;
    char header[128];
    sprintf(header, "%s: %s\r\n", key, value);
    strcat(response_headers, header);

    return;
}

void normalize_payload(char *body) {
	int index = 0;

	for(int i=0; i<strlen(body); i++) {
		if(is_json_valid_char(body[i])) {
			body[index++] =  body[i];
		}
	}

	body[index] = '\0';
}

void http_respond_to_client(oneM2MPrimitive *o2pt, int slotno) {
    char *status_msg = NULL;
    int status_code = rsc_to_http_status(o2pt->rsc, &status_msg);
    char content_length[64];
    char rsc[64];
    char cnst[32], ot[32];
    char response_headers[2048] = {'\0'};
    char buf[BUF_SIZE] = {'\0'};

    sprintf(content_length, "%ld", o2pt->pc ? strlen(o2pt->pc) : 0);
    sprintf(rsc, "%d", o2pt->rsc);
    set_header("Content-Length", content_length, response_headers);
    set_header("X-M2M-RSC", rsc, response_headers);
    set_header("X-M2M-RVI", o2pt->rvi, response_headers);
    set_header("X-M2M-RI", o2pt->rqi, response_headers);

    if(o2pt->cnot > 0){
        sprintf(cnst, "%d", o2pt->cnst);
        set_header("X-M2M-CNST", cnst, response_headers);
        sprintf(ot, "%d", o2pt->cnot);
        set_header("X-M2M-CNOT", ot, response_headers);
    }

    sprintf(buf, "%s %d %s\r\n%s%s\r\n", HTTP_PROTOCOL_VERSION, status_code, status_msg, DEFAULT_RESPONSE_HEADERS, response_headers);
    if(o2pt->pc){
        strcat(buf, o2pt->pc);
    } 
    write(clients[slotno], buf, strlen(buf)); 
    logger("HTTP", LOG_LEVEL_DEBUG, "\n\n%s\n",buf);
}

void http_notify(oneM2MPrimitive *o2pt, char *host, int port) {
    logger("HTTP", LOG_LEVEL_DEBUG, "http_notify %s:%d", host, port);
    HTTPRequest *req = (HTTPRequest *) calloc(1, sizeof(HTTPRequest));
    HTTPResponse *res =  (HTTPResponse *) calloc(1, sizeof(HTTPResponse));
    req->method = op_to_method(o2pt->op);
    req->payload = strdup(o2pt->pc);
    req->payload_size = strlen(req->payload);
    req->uri = strdup(o2pt->to);
    req->headers = malloc(sizeof(header_t));
    add_header("X-M2M-Origin", "/"CSE_BASE_RI, req->headers);
    add_header("X-M2M-RVI", "2a", req->headers);
    add_header("Content-Type", "application/json", req->headers);

    send_http_request(host, port, req, res);
    logger("HTTP", LOG_LEVEL_DEBUG, "http_notify response: %d", res->status_code);
    free_HTTPRequest(req);
    free_HTTPResponse(res);
}

void http_forwarding(oneM2MPrimitive *o2pt, char *host, int port){
    logger("HTTP", LOG_LEVEL_DEBUG, "http_forwarding");
    struct sockaddr_in serv_addr;
        
    int rcvd = 0;
    char buffer[BUF_SIZE];
    HTTPRequest *req = (HTTPRequest *) calloc(sizeof(HTTPRequest), 1);
    HTTPResponse *res =  (HTTPResponse *) calloc(sizeof(HTTPResponse), 1);

    ResourceAddressingType rat = checkResourceAddressingType(o2pt->to);

    req->method = op_to_method(o2pt->op);
    req->uri = calloc(1, sizeof(char) * (strlen(o2pt->to) + 4));
    req->uri[0] = '/';
    if(rat == SP_RELATIVE) req->uri[1] = '~';
    else if(rat == ABSOLUTE) req->uri[1] = '_';
    strcat(req->uri, o2pt->to);
    req->headers = (header_t *) calloc(sizeof(header_t), 1);
    add_header("X-M2M-Origin", o2pt->fr, req->headers);
    add_header("X-M2M-RVI", "2a", req->headers);
    add_header("X-M2M-RI", o2pt->rqi, req->headers);
    if(o2pt->ty > 0){
        sprintf(buffer, "application/json;ty=%d", o2pt->ty);
        add_header("Content-Type", buffer, req->headers);
    }else{
        add_header("Content-Type", "application/json", req->headers);
    }
    if(o2pt->pc){
        req->payload = strdup(o2pt->pc);
        req->payload_size = strlen(o2pt->pc);
    }else{
        req->payload = NULL;
        req->payload_size = 0;
    }
    logger("HTTP", LOG_LEVEL_DEBUG, "uri: %s", req->uri);
    send_http_request(host, port, req, res);
    char *rsc = search_header(res->headers, "X-M2M-RSC");
    if(rsc) o2pt->rsc = atoi(rsc);
    if(o2pt->pc) {
        free(o2pt->pc);
        o2pt->pc = NULL;
    }
    if(res->payload)
        o2pt->pc = strdup(res->payload);

    free_HTTPRequest(req);
    free_HTTPResponse(res);
    return;
}

void parse_http_request(HTTPRequest *req, char *packet){
    char *ptr = NULL;
    
    req->method = op_to_method(http_parse_operation(strtok(packet, " \t\r\n")));
    req->uri = strdup(strtok(NULL, " \t"));
    req->prot = strdup(strtok(NULL, " \t\r\n"));   
    if(!req->uri) {
        logger("HTTP", LOG_LEVEL_ERROR, "URI is NULL");
        return;
    } 
    uri_unescape(req->uri);

    logger("HTTP", LOG_LEVEL_DEBUG, "\x1b[36m[%s] %s\x1b[0m",req->method, req->uri); 
    if( (ptr = strchr(req->uri, '?')) ) {
        req->qs = strdup(ptr+1);
        *ptr = '\0';
    }else{
        req->qs = NULL;
    }

    req->headers = (header_t *) calloc(sizeof(header_t),1 );
    header_t *h = req->headers;
    char *t, *t2;
    while (true) {
        char *key, *val;  
        key = strtok(NULL, "\r\n: \t");
        if (!key)
            break;    
        val = strtok(NULL, "\r\n");
        while (*val && *val == ' ')
            val++; 

        h->name = strdup(key);
        h->value = strdup(val);
        
        t = val + 1 + strlen(val);
        if (t[1] == '\r' && t[2] == '\n')
            break;
        h->next = (header_t *) calloc(sizeof(header_t), 1);
        h = h->next;
    }
    //t = strtok(NULL, "\r\n");
    t = strtok(NULL, "\n");
    if(t && t[0] == '\r') t+=2; // now the *t shall be the beginning of user payload
    t2 = search_header(req->headers, "Content-Length"); // and the related header if there is
    if(t) req->payload = strdup(t);
    req->payload_size = t2 ? atol(t2) : 0;
}

void parse_http_response(HTTPResponse *res, char *packet){
    char *ptr = NULL;
    res->protocol = strdup(strtok(packet, " "));
    res->status_code = atoi(strtok(NULL, " "));
    res->status_msg = strdup(strtok(NULL, "\r\n"));

    res->headers = (header_t *) calloc(sizeof(header_t),1 );
    header_t *h = res->headers;
    char *t, *t2;
    while (true) {
        char *key, *val;  
        key = strtok(NULL, "\r\n: \t");
        if (!key)
            break;    
        val = strtok(NULL, "\r\n");
        while (*val && *val == ' ')
            val++; 

        h->name = strdup(key);
        h->value = strdup(val);
  
        
        t = val + 1 + strlen(val);
        if (t[1] == '\r' && t[2] == '\n')
            break;
        h->next = (header_t *) calloc(sizeof(header_t), 1);
        h = h->next;
    }
    //t = strtok(NULL, "\r\n");
    t = strtok(NULL, "\n");
    if(t && t[0] == '\r') t+=2; // now the *t shall be the beginning of user payload
    t2 = search_header(res->headers, "Content-Length"); // and the related header if there is
    if(t) res->payload = strdup(t);
    res->payload_size = t2 ? atol(t2) : 0;  
}

void send_http_request(char *host, int port,  HTTPRequest *req, HTTPResponse *res){
    struct sockaddr_in serv_addr;
    char *bs = "";
    memset(&serv_addr, 0, sizeof(serv_addr));
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    int rcvd = 0;

    if(sock == -1) {
        logger("HTTP", LOG_LEVEL_ERROR, "socket error");
        if(res)
            res->status_code = 500;
        return;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if(strcmp(host, "localhost") == 0) {
        serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    }else{
        serv_addr.sin_addr.s_addr = inet_addr(host);
    }
    if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        logger("HTTP", LOG_LEVEL_ERROR, "connect error");
        if(res)
            res->status_code = 999;
        close(sock);
        return;
    }

    struct timeval tv;
    tv.tv_sec  = SOCKET_TIMEOUT;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));

    char header[2048] = {'\0'};
    char buffer[BUF_SIZE] = {0};
    
    sprintf(buffer, "%s:%d", host, port);
    set_header("Host", buffer, header);
    header_t *h = req->headers;
    while(h)
    {
        set_header(h->name, h->value, header);
        h = h->next;
    }
    char contentSize[32] = {0};
    if(req->payload_size > 0) {
        sprintf(contentSize, "%d", req->payload_size);
        set_header("Content-Length", contentSize, header);
    }
    if(req->qs == NULL) req->qs = bs;
    sprintf(buffer, "%s %s%s %s\r\n%s\r\n", req->method, req->uri, req->qs, HTTP_PROTOCOL_VERSION, header);
    if(req->payload) {
        strcat(buffer, req->payload);
        strcat(buffer, "\r\n");
    }

    if(req->qs == bs) req->qs = NULL;
    send(sock, buffer, sizeof(buffer), 0);
    
    if(res == NULL){
        close(sock);
        return;
    }

    memset(buffer, 0, BUF_SIZE);
    rcvd = recv(sock, buffer, BUF_SIZE, 0); 

    if (rcvd < 0){ // receive error
        logger("HTTP", LOG_LEVEL_ERROR, "recv() error");
        res->status_code = 500;
    } else if (rcvd == 0) { // receive socket closed
        logger("HTTP", LOG_LEVEL_ERROR, "Client disconnected upexpectedly");
        res->status_code = 500;
    } else { // message received
        parse_http_response(res, buffer);
        if(res->payload_size > 0 && ((res->payload == NULL) || strlen(res->payload) == 0) ) {   // if there is payload but it is not in the buffer
            res->payload = (char *)calloc(MAX_PAYLOAD_SIZE, sizeof(char));
            recv(sock, res->payload, MAX_PAYLOAD_SIZE, 0); // receive payload
        }
        if(res->payload) normalize_payload(res->payload );

    }
    close(sock);
    return;
}

char *op_to_method(Operation op){
    char *method = NULL;
    switch (op){
        case OP_CREATE:
        case OP_NOTIFY:
            method = "POST";
            break;
        case OP_UPDATE:
            method = "PUT";
            break;
        case OP_DELETE:
            method = "DELETE";
            break;
        case OP_RETRIEVE:
        case OP_DISCOVERY:
            method = "GET";
            break;
        case OP_OPTIONS:
            method = "OPTIONS";
            break;
    }
    return method;
}

void add_header(char *key, char *value, header_t *header){
    if(!value) return;
    if(!header) return;
    header_t *h = header;
    while(h->next) h = h->next;
    h->next = (header_t *) calloc(sizeof(header_t), 1);
    h->next->name = strdup(key);
    h->next->value = strdup(value);
    h->next->next = NULL;
}

void free_HTTPRequest(HTTPRequest *req){
    header_t *h = req->headers;
    header_t *tmp;
    if(req->uri) free(req->uri);
    if(req->prot) free(req->prot);
    if(req->payload) free(req->payload);
    if(req->qs) free(req->qs);
    
    while(h){
        tmp = h;
        h = h->next;
        free(tmp->name);
        free(tmp->value);
        free(tmp);
    }
    free(req);
}

void free_HTTPResponse(HTTPResponse *res){
    header_t *h = res->headers;
    header_t *tmp;
    if(res->status_msg) free(res->status_msg);
    if(res->payload) free(res->payload);
    
    while(h){
        tmp = h;
        h = h->next;
        free(tmp->name);
        free(tmp->value);
        free(tmp);
    }
    free(res);
}