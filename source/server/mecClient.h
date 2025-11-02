#ifndef MEC_CLIENT_H
#define MEC_CLIENT_H
#ifdef ENABLE_MEC
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <pthread.h>
#include <curl/curl.h>

#include "cJSON.h"
#include "onem2m.h"
// MEC Client configuration defines
#define MAX_URL_SIZE 512
#define MAX_RESPONSE_SIZE 32768
#define MAX_JSON_SIZE 2048
#define MAX_HOST_SIZE 128
#define MAX_ID_SIZE 64
#define MAX_RETRIES 5
#define RETRY_DELAY 10
#define MEC_THREAD_STACK_SIZE 8192

// HTTP methods
#define HTTP_POST "POST"
#define HTTP_DELETE "DELETE"
#define HTTP_GET "GET"

// Response status codes
typedef enum {
    MEC_RSC_OK = 200,
    MEC_RSC_CREATED = 201,
    MEC_RSC_BAD_REQUEST = 400,
    MEC_RSC_NOT_FOUND = 404,
    MEC_RSC_DELETE = 204,
    MEC_RSC_INTERNAL_SERVER_ERROR = 500
} MECResponseStatusCode;

// MEC Client state
typedef enum {
    MEC_STATE_STOPPED = 0,
    MEC_STATE_RUNNING = 1,
    MEC_STATE_PAUSED = 2,
    MEC_STATE_REGISTERED = 3
} MECClientState;

// MEC Result structure
typedef struct {
    int status_code;
    char* uuid;
    char* response_data;
    size_t data_length;
    char* error_message;
} MECResult;

// MEC Client structure
typedef struct {
    // Client configuration
    char cse_id[MAX_ID_SIZE];
    char http_address[MAX_HOST_SIZE];
    int http_port;
    
    // MEC platform configuration
    char mec_host[MAX_HOST_SIZE];
    int mec_port;
    char mec_protocol[16];
    char mec_platform[64];
    char mec_sandbox_id[64];
    int mec_enable;
    
    // Registration state
    char iot_platform_id[MAX_ID_SIZE];
    MECClientState state;
    
    // Threading
    pthread_t mec_thread;
    pthread_mutex_t state_mutex;
    
    // Statistics
    int registration_attempts;
    time_t last_registration_time;
    
} MECClient;

// Global MEC client instance
extern MECClient g_mec_client;

// Function prototypes
int mec_client_init(void);
void mec_client_destroy(void);
int mec_client_start(void);
int mec_client_stop(void);
void mec_client_pause(void);
void mec_client_resume(void);

// Registration functions
MECResult* mec_client_register(void);
MECResult* mec_client_deregister(void);
int mec_client_is_registered(void);

// Utility functions
void mec_generate_uuid(char* uuid_str);
int mec_send_http_request(const char* method, const char* url, struct curl_slist* headers, const char* body, char* response, size_t response_size);

void mec_result_destroy(MECResult* result);
void mec_log_info(const char* message);
void mec_log_error(const char* message);

// libcurl initialization
int mec_curl_init(void);
void mec_curl_cleanup(void);

// Thread function
void* mec_client_thread(void* arg);

// Integration with oneM2M
int mec_notify_resource_created(oneM2MPrimitive *o2pt);
int mec_notify_resource_updated(oneM2MPrimitive *o2pt);
int mec_notify_resource_deleted(oneM2MPrimitive *o2pt);

#endif
#endif