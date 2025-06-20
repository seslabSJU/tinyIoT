#include "mecClient.h"
#include "config.h"
#include "httpd.h"

// Global MEC client instance
MECClient g_mec_client;

// HTTP response callback for libcurl
// size_t mec_http_callback(void *contents, size_t size, size_t nmemb, HTTPResponse *response) {
//     size_t realsize = size * nmemb;
//     char *ptr = realloc(response->memory, response->size + realsize + 1);
    
//     if (!ptr) {
//         mec_log_error("Not enough memory (realloc returned NULL)");
//         return 0;
//     }
    
//     response->memory = ptr;
//     memcpy(&(response->memory[response->size]), contents, realsize);
//     response->size += realsize;
//     response->memory[response->size] = 0;
    
//     return realsize;
// }

//HTTP function for payload
size_t body_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    HTTPResponse *resp = (HTTPResponse *)userp;

    char *ptr = realloc(resp->payload, resp->payload_size + realsize + 1);
    if (!ptr) {
        mec_log_error("Not enough memory for payload");
        return 0;
    }

    resp->payload = ptr;
    memcpy(&(resp->payload[resp->payload_size]), contents, realsize);
    resp->payload_size += realsize;
    resp->payload[resp->payload_size] = '\0';

    return realsize;
}

// Http function for headers
size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    size_t realsize = size * nitems;
    HTTPResponse *resp = (HTTPResponse *)userdata;
    // You can parse the header line here or just store raw header strings
    printf("Header line: %.*s", (int)realsize, buffer);
    // Optional: store headers in a linked list or custom header_t type
    // You will need to define what `header_t` is and how to store headers
    return realsize;
}

// Initialize libcurl
int mec_curl_init(void) {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        mec_log_error("Failed to initialize libcurl");
        return 0;
    }
    return 1;
}

// Cleanup libcurl
void mec_curl_cleanup(void) {
    curl_global_cleanup();
}

// Initialize MEC Client
int mec_client_init(void) {
    memset(&g_mec_client, 0, sizeof(MECClient));
    
    // Initialize libcurl
    if (!mec_curl_init()) {
        return 0;
    }
    
    // Load configuration from config.h or config file
    strncpy(g_mec_client.cse_id, "i dont know that", MAX_ID_SIZE - 1);
    strncpy(g_mec_client.http_address, "localhost", MAX_HOST_SIZE - 1);
    g_mec_client.http_port = 8080;
    
    // MEC platform configuration (should be configurable)
    strncpy(g_mec_client.mec_host, MEC_HOST, MAX_HOST_SIZE - 1);
    g_mec_client.mec_port = 8080;
    strncpy(g_mec_client.mec_protocol, MEC_PROTOCOL, 15);
    strncpy(g_mec_client.mec_platform, MEC_PLATFORM, 63);
    strncpy(g_mec_client.mec_sandbox_id, MEC_SANDBOX_ID, 63);
    g_mec_client.mec_enable = MEC_ENABLE;
    
    g_mec_client.state = MEC_STATE_STOPPED;
    g_mec_client.registration_attempts = 0;
    
    // Initialize mutex
    if (pthread_mutex_init(&g_mec_client.state_mutex, NULL) != 0) {
        mec_log_error("MEC Client: Failed to initialize mutex");
        mec_curl_cleanup();
        return 0;
    }
    
    mec_log_info("MEC Client initialized");
    return 1;
}

// Destroy MEC Client
void mec_client_destroy(void) {
    mec_client_stop();
    
    pthread_mutex_destroy(&g_mec_client.state_mutex);
    mec_curl_cleanup();
    
    mec_log_info("MEC Client destroyed");
}

// Start MEC Client
int mec_client_start(void) {
    if (!g_mec_client.mec_enable) {
        mec_log_info("MEC Client: NOT enabled");
        return 1;
    }
    
    pthread_mutex_lock(&g_mec_client.state_mutex);
    
    if (g_mec_client.state != MEC_STATE_STOPPED) {
        pthread_mutex_unlock(&g_mec_client.state_mutex);
        return 1; // Already running
    }
    
    g_mec_client.state = MEC_STATE_RUNNING;
    
    // Create MEC client thread
    if (pthread_create(&g_mec_client.mec_thread, NULL, mec_client_thread, NULL) != 0) {
        mec_log_error("MEC Client: Failed to create thread");
        g_mec_client.state = MEC_STATE_STOPPED;
        pthread_mutex_unlock(&g_mec_client.state_mutex);
        return 0;
    }
    
    pthread_mutex_unlock(&g_mec_client.state_mutex);
    
    mec_log_info("MEC Client started");
    return 1;
}

// Stop MEC Client
int mec_client_stop(void) {
    pthread_mutex_lock(&g_mec_client.state_mutex);
    
    if (g_mec_client.state == MEC_STATE_STOPPED) {
        pthread_mutex_unlock(&g_mec_client.state_mutex);
        return 1;
    }
    
    // Deregister if registered
    if (g_mec_client.state == MEC_STATE_REGISTERED) {
        pthread_mutex_unlock(&g_mec_client.state_mutex);
        MECResult* result = mec_client_deregister();
        if (result) {
            mec_result_destroy(result);
        }
        pthread_mutex_lock(&g_mec_client.state_mutex);
    }
    
    g_mec_client.state = MEC_STATE_STOPPED;
    pthread_mutex_unlock(&g_mec_client.state_mutex);
    
    // Wait for thread to finish
    pthread_join(g_mec_client.mec_thread, NULL);
    
    mec_log_info("MEC Client stopped");
    return 1;
}

// Pause MEC Client
void mec_client_pause(void) {
    pthread_mutex_lock(&g_mec_client.state_mutex);
    if (g_mec_client.state == MEC_STATE_RUNNING || g_mec_client.state == MEC_STATE_REGISTERED) {
        g_mec_client.state = MEC_STATE_PAUSED;
        mec_log_info("MEC Client paused");
    }
    pthread_mutex_unlock(&g_mec_client.state_mutex);
}

// Resume MEC Client
void mec_client_resume(void) {
    pthread_mutex_lock(&g_mec_client.state_mutex);
    if (g_mec_client.state == MEC_STATE_PAUSED) {
        g_mec_client.state = MEC_STATE_RUNNING;
        mec_log_info("MEC Client resumed");
    }
    pthread_mutex_unlock(&g_mec_client.state_mutex);
}

// MEC Client thread function
void* mec_client_thread(void* arg) {
    mec_log_info("MEC Client thread started");
    
    while (1) {
        pthread_mutex_lock(&g_mec_client.state_mutex);
        MECClientState current_state = g_mec_client.state;
        pthread_mutex_unlock(&g_mec_client.state_mutex);
        
        if (current_state == MEC_STATE_STOPPED) {
            break;
        }
        
        if (current_state == MEC_STATE_RUNNING) {
            // Try to register if not registered
            MECResult* result = mec_client_register();
            if (result && result->status_code == MEC_RSC_CREATED) {
                pthread_mutex_lock(&g_mec_client.state_mutex);
                g_mec_client.state = MEC_STATE_REGISTERED;
                pthread_mutex_unlock(&g_mec_client.state_mutex);
                mec_log_info("MEC Client registered successfully");
            }
            if (result) {
                mec_result_destroy(result);
            }
        }
        
        // Sleep for a while before next iteration
        sleep(30); // Check every 30 seconds
    }
    
    mec_log_info("MEC Client thread stopped");
    return NULL;
}

// Register to MEC platform
MECResult* mec_client_register(void) {
    if (g_mec_client.state == MEC_STATE_PAUSED) {
        MECResult* result = (MECResult*)malloc(sizeof(MECResult));
        result->status_code = MEC_RSC_INTERNAL_SERVER_ERROR;
        result->response_data = NULL;
        result->data_length = 0;
        result->error_message = strdup("MEC client is paused");
        return result;
    }
    
    char url[MAX_URL_SIZE];
    char* json_body = NULL;
    char response[MAX_RESPONSE_SIZE];
    char uuid_str1[37], uuid_str2[37], uuid_str3[37];
    
    // Generate UUIDs
    mec_generate_uuid(uuid_str1); // iotPlatformId
    mec_generate_uuid(uuid_str2); // userTransportInfo id
    mec_generate_uuid(uuid_str3); // customServicesTransportInfo id
    
    // Store the platform ID for deregistration
    strncpy(g_mec_client.iot_platform_id, uuid_str1, MAX_ID_SIZE - 1);
    
    // Build URL
    snprintf(url, MAX_URL_SIZE, "%s://%s/%s/%s/iots/v1/registered_iot_platforms",
             g_mec_client.mec_protocol, g_mec_client.mec_host,
             g_mec_client.mec_sandbox_id, g_mec_client.mec_platform);
    
    // Build JSON using cJSON
    cJSON *json = cJSON_CreateObject();
    cJSON *iot_platform_id = cJSON_CreateString(uuid_str1);
    cJSON_AddItemToObject(json, "iotPlatformId", iot_platform_id);
    
    // User Transport Info
    cJSON *user_transport_info_array = cJSON_CreateArray();
    cJSON *user_transport_info = cJSON_CreateObject();
    cJSON_AddStringToObject(user_transport_info, "id", uuid_str2);
    cJSON_AddStringToObject(user_transport_info, "name", g_mec_client.cse_id);
    cJSON_AddStringToObject(user_transport_info, "description", "MQTT");
    cJSON_AddStringToObject(user_transport_info, "type", "MB_TOPIC_BASED");
    cJSON_AddStringToObject(user_transport_info, "protocol", "MQTT");
    cJSON_AddStringToObject(user_transport_info, "version", "2");
    
    cJSON *endpoint = cJSON_CreateObject();
    cJSON *addresses = cJSON_CreateArray();
    cJSON *address = cJSON_CreateObject();
    cJSON_AddStringToObject(address, "host", "meep-mosquitto");
    cJSON_AddNumberToObject(address, "port", 1883);
    cJSON_AddItemToArray(addresses, address);
    cJSON_AddItemToObject(endpoint, "addresses", addresses);
    cJSON_AddItemToObject(user_transport_info, "endpoint", endpoint);
    
    cJSON_AddItemToObject(user_transport_info, "security", cJSON_CreateObject());
    cJSON_AddItemToObject(user_transport_info, "implSpecificInfo", cJSON_CreateObject());
    cJSON_AddItemToArray(user_transport_info_array, user_transport_info);
    cJSON_AddItemToObject(json, "userTransportInfo", user_transport_info_array);
    
    // Custom Services Transport Info
    cJSON *custom_services_array = cJSON_CreateArray();
    cJSON *custom_service = cJSON_CreateObject();
    cJSON_AddStringToObject(custom_service, "id", uuid_str3);
    cJSON_AddStringToObject(custom_service, "name", g_mec_client.cse_id);
    cJSON_AddStringToObject(custom_service, "description", "TinyIOT oneM2M CSE");
    cJSON_AddStringToObject(custom_service, "type", "REST_HTTP");
    cJSON_AddStringToObject(custom_service, "protocol", "REST_HTTP");
    cJSON_AddStringToObject(custom_service, "version", "4");
    
    cJSON *custom_endpoint = cJSON_CreateObject();
    cJSON *custom_addresses = cJSON_CreateArray();
    cJSON *custom_address = cJSON_CreateObject();
    cJSON_AddStringToObject(custom_address, "host", g_mec_client.http_address);
    cJSON_AddNumberToObject(custom_address, "port", g_mec_client.http_port);
    cJSON_AddItemToArray(custom_addresses, custom_address);
    cJSON_AddItemToObject(custom_endpoint, "addresses", custom_addresses);
    cJSON_AddItemToObject(custom_service, "endpoint", custom_endpoint);
    
    cJSON_AddItemToObject(custom_service, "security", cJSON_CreateObject());
    cJSON_AddItemToArray(custom_services_array, custom_service);
    cJSON_AddItemToObject(json, "customServicesTransportInfo", custom_services_array);
    
    cJSON_AddBoolToObject(json, "enabled", 1);
    
    json_body = cJSON_Print(json);
    cJSON_Delete(json);
    
    struct curl_slist *header_list = NULL;
    header_list = curl_slist_append(header_list, "Content-Type: application/json; charset=UTF-8");
    
    // Try registration with retries
    MECResult* result = NULL;
    for (int tries = 0; tries < MAX_RETRIES; tries++) {
        g_mec_client.registration_attempts++;

        int status = mec_send_http_request(HTTP_POST, url, header_list, json_body, response, MAX_RESPONSE_SIZE);
        
        if (status == MEC_RSC_CREATED) {
            g_mec_client.last_registration_time = time(NULL);
            result = (MECResult*)malloc(sizeof(MECResult));
            result->status_code = status;
            result->response_data = strdup(response);
            result->data_length = strlen(response);
            result->error_message = NULL;
            
            mec_log_info("MEC Client registration successful");
            break;
        } else {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "MEC Client registration failed: %d", status);
            mec_log_error(error_msg);
            
            if (tries < MAX_RETRIES - 1) {
                sleep(RETRY_DELAY);
            }
        }
    }
    
    if (json_body) {
        free(json_body);
    }
    
    return result;
}

// Deregister from MEC platform
MECResult* mec_client_deregister(void) {
    if (strlen(g_mec_client.iot_platform_id) == 0) {
        MECResult* result = (MECResult*)malloc(sizeof(MECResult));
        result->status_code = MEC_RSC_INTERNAL_SERVER_ERROR;
        result->response_data = NULL;
        result->data_length = 0;
        result->error_message = strdup("Not registered to MEC platform");
        return result;
    }
    
    char url[MAX_URL_SIZE];
    char response[MAX_RESPONSE_SIZE];
    
    // Build URL with platform ID
    snprintf(url, MAX_URL_SIZE, "%s://%s:%d/%s/%s/iots/v1/registered_iot_platforms/%s",
             g_mec_client.mec_protocol, g_mec_client.mec_host, g_mec_client.mec_port,
             g_mec_client.mec_sandbox_id, g_mec_client.mec_platform, g_mec_client.iot_platform_id);
    
    //const char* headers = "Content-Type: application/json; charset=UTF-8\r\n";
    struct curl_slist *header_list = NULL;
    header_list = curl_slist_append(header_list, "Content-Type: application/json; charset=UTF-8");
    int status = mec_send_http_request(HTTP_DELETE, url, header_list, NULL, response, MAX_RESPONSE_SIZE);
    
    // Clear registration data
    memset(g_mec_client.iot_platform_id, 0, MAX_ID_SIZE);
    
    MECResult* result = (MECResult*)malloc(sizeof(MECResult));
    result->status_code = status;
    result->response_data = strdup(response);
    result->data_length = strlen(response);
    result->error_message = NULL;
    
    if (status == MEC_RSC_OK) {
        mec_log_info("MEC Client deregistration successful");
    } else {
        mec_log_error("MEC Client deregistration failed");
    }
    
    return result;
}

// Check if client is registered
int mec_client_is_registered(void) {
    pthread_mutex_lock(&g_mec_client.state_mutex);
    int registered = (g_mec_client.state == MEC_STATE_REGISTERED);
    pthread_mutex_unlock(&g_mec_client.state_mutex);
    return registered;
}

// Generate simple UUID-like string
void mec_generate_uuid(char* uuid_str) {
    static int counter = 0;
    time_t now = time(NULL);
    
    srand(now + counter++);
    
    snprintf(uuid_str, 37, "%08x-%04x-4%03x-%04x-%08x%04x",
             (unsigned int)rand(),
             (unsigned int)(rand() & 0xFFFF),
             (unsigned int)(rand() & 0x0FFF),
             (unsigned int)((rand() & 0x3FFF) | 0x8000),
             (unsigned int)rand(),
             (unsigned int)(rand() & 0xFFFF));
}

// Real HTTP request implementation using libcurl
int mec_send_http_request(const char* method, const char* url, struct curl_slist* headers, const char* body, char* response, size_t response_size) {
    CURL *curl;
    CURLcode res;
    HTTPResponse http_response = {0};  // Using httpd.h's definition
    long response_code = 0;

    printf("Sending HTTP %s request to %s\n", method, url);
    curl = curl_easy_init();
    if (!curl) {
        mec_log_error("Failed to initialize curl handle");
        return MEC_RSC_INTERNAL_SERVER_ERROR;
    }

    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Set response body and header callbacks
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, body_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &http_response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &http_response);

    // Timeouts
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 300L);

    // SSL options (insecure for development)
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Set HTTP method and body
    if (strcmp(method, HTTP_POST) == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (body) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(body));
        }
    } else if (strcmp(method, HTTP_DELETE) == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    // Set headers
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    // Perform the request
    printf("Performing HTTP %s request...\n", method);
    res = curl_easy_perform(curl);
    printf("curl_easy_perform() returned %d\n", res);
    if (res != CURLE_OK) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
                 "curl_easy_perform() failed: %s", curl_easy_strerror(res));
        mec_log_error(error_msg);

        if (res == CURLE_PEER_FAILED_VERIFICATION) {
            mec_log_error("SSL certificate verification failed.");
        }

        response_code = MEC_RSC_INTERNAL_SERVER_ERROR;
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        // Copy payload to provided response buffer
        if (http_response.payload && response) {
            size_t copy_size = (http_response.payload_size < response_size - 1) ? http_response.payload_size : response_size - 1;
            memcpy(response, http_response.payload, copy_size);
            response[copy_size] = '\0';
        }

        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "HTTP %s %s -> %ld", method, url, response_code);
        mec_log_info(log_msg);

        if (http_response.payload && http_response.payload_size > 0) {
            char response_log[256];
            snprintf(response_log, sizeof(response_log), "Response: %.200s%s",
                     http_response.payload,
                     (http_response.payload_size > 200) ? "..." : "");
            mec_log_info(response_log);
        }
    }

    // Cleanup
    if (http_response.payload) {
        free(http_response.payload);
    }

    curl_easy_cleanup(curl);
    return (int)response_code;
}




// Destroy result structure
void mec_result_destroy(MECResult* result) {
    if (result) {
        if (result->response_data) {
            free(result->response_data);
        }
        if (result->error_message) {
            free(result->error_message);
        }
        free(result);
    }
}

// Logging functions (integrate with TinyIOT logging)
void mec_log_info(const char* message) {
    printf("[MEC INFO] %s\n", message);
}

void mec_log_error(const char* message) {
    fprintf(stderr, "[MEC ERROR] %s\n", message);
}

// Integration functions with oneM2M
int mec_notify_resource_created(oneM2MPrimitive *o2pt) {
    if (!mec_client_is_registered()) {
        return 0; // Not registered, skip notification
    }
    
    // Implement MEC notification for resource creation
    mec_log_info("MEC: Resource created notification");
    return 1;
}

int mec_notify_resource_updated(oneM2MPrimitive *o2pt) {
    if (!mec_client_is_registered()) {
        return 0;
    }
    
    // Implement MEC notification for resource update
    mec_log_info("MEC: Resource updated notification");
    return 1;
}

int mec_notify_resource_deleted(oneM2MPrimitive *o2pt) {
    if (!mec_client_is_registered()) {
        return 0;
    }
    
    // Implement MEC notification for resource deletion
    mec_log_info("MEC: Resource deleted notification");
    return 1;
}