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
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#define MAX_CONNECTIONS 1024
#define BUF_SIZE 65535
#define QUEUE_SIZE 64

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
    while (1) {
        clients[slot] = accept(listenfd, (struct sockaddr *)&clientaddr, &addrlen);
        if (clients[slot] < 0) {
            perror("accept() error");
            //exit(1);
            clients[slot] = -1;
        } else {
            pthread_t thread_id;
            pthread_create(&thread_id, NULL, respond_thread, (void*)&slots[slot]);
            if(MONO_THREAD) pthread_join(thread_id, NULL);
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
    inet_pton(AF_INET, SERVER_HOST, &(addr.sin_addr.s_addr));
    addr.sin_port = htons(atoi(port));    
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int option = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
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

// get request header by name
char *request_header(header_t *h, int cnt, const char *name) {
    if(h == NULL) return NULL;
    for(int i = 0 ; i < cnt ; i++) {
        if (strcmp((h+i)->name, name) == 0)
            return (h+i)->value;
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
    } else { // message received
        pthread_mutex_lock(&mutex_lock);  
        buf[slot][rcvd] = '\0';
        logger("HTTP", LOG_LEVEL_DEBUG, "\n\n%s\n",buf[slot]);
        memcpy(buffer, buf[slot], rcvd);

        req->method = strtok(buffer, " \t\r\n");
        req->uri = strtok(NULL, " \t");
        req->prot = strtok(NULL, " \t\r\n");   
        if(!req->uri) {
            logger("HTTP", LOG_LEVEL_ERROR, "URI is NULL");
            return;
        } 
        uri_unescape(req->uri);
    
        logger("HTTP", LOG_LEVEL_DEBUG, "\x1b[36m[%s] %s\x1b[0m",req->method, req->uri); 
        req->qs = strchr(req->uri, '?');    
        if (req->qs)
            *req->qs++ = '\0'; // split URI
        else
            req->qs = req->uri - 1; // use an empty string  

        req->headers = (header_t *) malloc(sizeof(header_t) * 20);
        header_t *h = req->headers;
        char *t, *t2;
        while (h < req->headers + 16) {
            char *key, *val;  
            key = strtok(NULL, "\r\n: \t");
            if (!key)
                break;    
            val = strtok(NULL, "\r\n");
            while (*val && *val == ' ')
                val++;    
            h->name = key;
            h->value = val;
            h++;
            req->header_count++;
            t = val + 1 + strlen(val);
            if (t[1] == '\r' && t[2] == '\n')
                break;
            //fprintf(stderr, "[H] %s: %s, %x\n", key, val, *t); // print request headers 
        }
        //t = strtok(NULL, "\r\n");
        t = strtok(NULL, "\n");
        if(t && t[0] == '\r') t+=2; // now the *t shall be the beginning of user payload
        
        logger("HTTP", LOG_LEVEL_DEBUG, "Payload: %s", t); // print user payload
        t2 = request_header(req->headers, req->header_count, "Content-Length"); // and the related header if there is
        req->payload = t;
        req->payload_size = t2 ? atol(t2) : 0;

        if(req->payload_size > 0 && (req->payload == NULL)) {
            flag = true;
            req->payload = (char *)calloc(MAX_PAYLOAD_SIZE, sizeof(char));
            recv(clients[slot], req->payload, MAX_PAYLOAD_SIZE, 0);
        }

        if(req->payload) normalize_payload(req->payload);
        // bind clientfd to stdout, making it easier to write
        
        //dup2(clientfd, STDOUT_FILENO);
        // call router
        handle_http_request(req, slot);    
        // tidy up
        fflush(stdout);
        shutdown(STDOUT_FILENO, SHUT_WR);
        //close(STDOUT_FILENO);
    }
    if(flag) free(req->payload);
    free(req->headers);
    free(buf[slot]);
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
		o2pt->pc = (char *)malloc((req->payload_size + 1) * sizeof(char));
		strcpy(o2pt->pc, req->payload);
		o2pt->cjson_pc = cJSON_Parse(o2pt->pc);
	} 
    

	if((header = request_header(req->headers, req->header_count, "X-M2M-Origin"))) {
		o2pt->fr = (char *)malloc((strlen(header) + 1) * sizeof(char));
		strcpy(o2pt->fr, header);
	} 

	if((header = request_header(req->headers, req->header_count, "X-M2M-RI")) && header) {
		o2pt->rqi = (char *)malloc((strlen(header) + 1) * sizeof(char));
		strcpy(o2pt->rqi, header);
	} 

	if((header = request_header(req->headers, req->header_count, "X-M2M-RVI")) && header) {
		o2pt->rvi = (char *)malloc((strlen(header) + 1) * sizeof(char));
		strcpy(o2pt->rvi, header);
	} 
    
	if(req->uri) {
        o2pt->to = (char *)malloc((strlen(req->uri) + 1) * sizeof(char));
        strcpy(o2pt->to, req->uri+1);
       logger("HTTP", LOG_LEVEL_DEBUG, "to: %s", o2pt->to);
	} 
    
    o2pt->op = http_parse_operation(req->method);    
    logger("HTTP", LOG_LEVEL_INFO, "Request : %s", req->method);

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    getsockname(clients[slotno], (struct sockaddr *)&client_addr, &client_addr_len);
    o2pt->ip = (char *)malloc((INET_ADDRSTRLEN + 1) * sizeof(char));
     logger("HTTP", LOG_LEVEL_INFO, "Client connected %d.%d.%d.%d\n",
         (int)(client_addr.sin_addr.s_addr&0xFF), (int)((client_addr.sin_addr.s_addr&0xFF00)>>8), (int)((client_addr.sin_addr.s_addr&0xFF0000)>>16), (int)((client_addr.sin_addr.s_addr&0xFF000000)>>24));

    inet_ntop(AF_INET, &(client_addr.sin_addr.s_addr), o2pt->ip, INET_ADDRSTRLEN);
    logger("HTTP", LOG_LEVEL_DEBUG, "ip: %s", o2pt->ip);

	if(o2pt->op == OP_CREATE) o2pt->ty = http_parse_object_type(req->headers, req->header_count);
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

void set_response_header(char *key, char *value, char *response_headers) {
    if(!value) return;
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
    int status_code = rsc_to_http_status(o2pt->rsc);
    char content_length[64];
    char rsc[64];
    char cnst[32], ot[32];
    char response_headers[2048] = {'\0'};
    char buf[BUF_SIZE] = {'\0'};
    char *status;

    sprintf(content_length, "%ld", o2pt->pc ? strlen(o2pt->pc) : 0);
    sprintf(rsc, "%d", o2pt->rsc);
    set_response_header("Content-Length", content_length, response_headers);
    set_response_header("X-M2M-RSC", rsc, response_headers);
    set_response_header("X-M2M-RVI", o2pt->rvi, response_headers);
    set_response_header("X-M2M-RI", o2pt->rqi, response_headers);

    if(o2pt->cnot > 0){
        sprintf(cnst, "%d", o2pt->cnst);
        set_response_header("X-M2M-CNST", cnst, response_headers);
        sprintf(ot, "%d", o2pt->cnot);
        set_response_header("X-M2M-CNOT", ot, response_headers);
    }

   

    switch(status_code) {
        case 200: status = "200 OK"; break;
        case 201: status = "201 Created"; break;
        case 209: status = "209 Conflict"; break;
        case 400: status = "400 Bad Request"; break;
        case 403: status = "403 Forbidden"; break;
        case 404: status = "404 Not found"; break;
        case 406: status = "406 Not Acceptable"; break;
        case 413: status = "413 Payload Too Large"; break;
        case 500: status = "500 Internal Server Error"; break;
    }
    sprintf(buf, "%s %s\r\n%s%s\r\n", HTTP_PROTOCOL_VERSION, status, DEFAULT_RESPONSE_HEADERS, response_headers);
    if(o2pt->pc){
        strcat(buf, o2pt->pc);
    } 
    write(clients[slotno], buf, strlen(buf)); 
    logger("HTTP", LOG_LEVEL_DEBUG, "\n\n%s\n",buf);
}

void http_notify(oneM2MPrimitive *o2pt, char *noti_json, NotiTarget *nt) {

    struct sockaddr_in serv_addr;
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        logger("HTTP", LOG_LEVEL_ERROR, "socket error");
        return;
    }

    http_send_get_request(nt->host, nt->port, nt->target, DEFAULT_REQUEST_HEADERS, "", noti_json);
}

void http_send_get_request(char *host, int port, char *uri, char *header, char *qs, char *data){
    struct sockaddr_in serv_addr;
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        logger("HTTP", LOG_LEVEL_ERROR, "socket error");
        return;
    }
    char buffer[BUF_SIZE];
    sprintf(buffer, "GET %s%s %s\r\n%s\r\n%s\r\n", uri, qs, HTTP_PROTOCOL_VERSION, header, data);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(host);
    serv_addr.sin_port = htons(port);

    if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        logger("HTTP", LOG_LEVEL_ERROR, "connect error");
        return;
    }
    logger("http", LOG_LEVEL_DEBUG, "%s", buffer);

    send(sock, buffer, sizeof(buffer), 0);
    close(sock);
}

void http_forwarding(oneM2MPrimitive *o2pt, char *host, char *port){
    struct sockaddr_in serv_addr;
    char *method = NULL;
    logger("HTTP", LOG_LEVEL_DEBUG, "o2pt->op : %d", o2pt->op);
    switch (o2pt->op){
        case OP_CREATE:
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
    
    int rcvd = 0;
    char *response = (char *)malloc(sizeof(char) * BUF_SIZE);
    char buffer[BUF_SIZE];

    char *headers[2048] = {0};
    set_response_header("Host", host, headers);
    if(o2pt->ty > 0){
        sprintf(buffer, "application/json;ty=%d", o2pt->ty);
    }else{
        sprintf(buffer, "application/json");
    }
    set_response_header("Content-Type", buffer, headers);
    sprintf(buffer, "%d", strlen(o2pt->pc));
    set_response_header("Content-Length", buffer, headers);
    set_response_header("X-M2M-Origin", o2pt->fr, headers);
    set_response_header("X-M2M-RVI", o2pt->rvi, headers);
    set_response_header("X-M2M-RI", o2pt->rqi, headers);

    sprintf(buffer, "%s %s %s\r\n%s\r\n", 
    method, o2pt->to, HTTP_PROTOCOL_VERSION, headers);
    if(o2pt->pc && strlen(o2pt->pc) > 0){
        strcat(buffer, o2pt->pc);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(host); 
    serv_addr.sin_port = htons(atoi(port));
    logger("http", LOG_LEVEL_DEBUG, "forwarding to %s, %s", host, port);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        logger("HTTP", LOG_LEVEL_ERROR, "socket error");
        return;
    }

    if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        logger("HTTP", LOG_LEVEL_ERROR, "connect error");
        o2pt->rsc = RSC_TARGET_NOT_REACHABLE;
        free(o2pt->pc);
        o2pt->pc = strdup("{\"dbg\": \"target not rechable\"}");
        close(sock);
        return;
    }

    // setting timeout
    struct timeval tv;
    tv.tv_sec  = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));


    int sent = 0;
    logger("http", LOG_LEVEL_DEBUG, "%s", buffer);
    send(sock, buffer, strlen(buffer), 0);
    //sent = write(sock, buffer, strlen(buffer));
    

    memset(buffer, 0, BUF_SIZE);
    rcvd = recv(sock, buffer, BUF_SIZE, 0); 

    if (rcvd < 0){ // receive error
        logger("HTTP", LOG_LEVEL_ERROR, "recv() error");
        o2pt->rsc = RSC_TARGET_NOT_REACHABLE;
        set_o2pt_pc(o2pt, "{\"dbg\": \"target not rechable\"}");
    } else if (rcvd == 0) { // receive socket closed
        logger("HTTP", LOG_LEVEL_ERROR, "Client disconnected upexpectedly");
        o2pt->rsc = RSC_TARGET_NOT_REACHABLE;
        set_o2pt_pc(o2pt, "{\"dbg\": \"target not rechable\"}");
    } else { // message received
        char *protocol = strtok(buffer, " \t");
        char *statuscode = strtok(NULL, " \t");
        char *statusword = strtok(NULL, " \r\n");   
        
        header_t *h = malloc(sizeof(header_t) * 20);
        header_t *ptr = h;
        int h_cnt = 0;
        char *t, *t2;
        while (ptr < h + 16) {
            char *key, *val;  
            key = strtok(NULL, "\r\n: \t");
            if (!key)
                break;    
            val = strtok(NULL, "\r\n");
            while (*val && *val == ' ')
                val++;    
            ptr->name = key;
            ptr->value = val;
            ptr++;
            h_cnt++;
            t = val + 1 + strlen(val);
            if (t[1] == '\r' && t[2] == '\n')
                break;
            fprintf(stderr, "[H] %s: %s\n", key, val); // print request headers 
        }
        t = strtok(NULL, "\n");
        if(t && t[0] == '\r') t+=2; // now the *t shall be the beginning of user payload
        char *body = t;
        
        t2 = request_header(h, h_cnt, "Content-Length");
        size_t body_size = t2 ? atol(t2) : 0;

        if(body_size > 0 && (body_size == NULL)) {
            body = (char *)calloc(MAX_PAYLOAD_SIZE, sizeof(char));
            recv(sock, body, MAX_PAYLOAD_SIZE, 0);
        }
        if(body) normalize_payload(body);

        char *rsc = request_header(h, h_cnt, "X-M2M-RSC");
        if(rsc) o2pt->rsc = atoi(rsc);
        if(o2pt->pc) {
            free(o2pt->pc);
            o2pt->pc = NULL;
        }
        if(body)
            o2pt->pc = strdup(body);
        else

        if(h) free(h);
    }
    close(sock);
    return;
}
