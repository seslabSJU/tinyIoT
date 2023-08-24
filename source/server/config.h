#ifndef __CONFIG_H__
#define __CONFIG_H__
#include "logger.h"

#define SERVER_TYPE IN_CSE

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT "3000"
#define CSE_BASE_NAME "TinyIoT"
#define CSE_BASE_RI "tinyiot"

#if SERVER_TYPE == MN_CSE
#define REMOTE_CSE_ID "in-cse"
#define REMOTE_CSE_HOST ""
#define REMOTE_CSE_PORT ""
#endif

#define MONO_THREAD 1 // 0 → multi-thread, 1 → mono-thread

#define MAX_PAYLOAD_SIZE 65536 
#define MAX_URI_SIZE 1024
#define MAX_ATTRIBUTE_SIZE 4096

#define MAX_TREE_VIEWER_SIZE 65536
#define DEFAULT_EXPIRE_TIME -3600*24*365*2


// AE Settings
// #define ALLOW_AE_ORIGIN "C*,S*" , no blankspace allowed
#define ALLOW_AE_ORIGIN "C*,S*"

// CNT Settings
#define DEFAULT_MAX_NR_INSTANCES 10
#define DEFAULT_MAX_BYTE_SIZE 1024

// Group Settings
#define DEFAULT_CONSISTENCY_POLICY CSY_SET_MIXED


// MQTT Settings
//#define ENABLE_MQTT

#ifdef ENABLE_MQTT
#define MQTT_HOST            "127.0.0.1"
#define MQTT_QOS             MQTT_QOS_0
#define MQTT_KEEP_ALIVE_SEC  60
#define MQTT_CMD_TIMEOUT_MS  30000
#define MQTT_CON_TIMEOUT_MS  5000
#define MQTT_CLIENT_ID       "TinyIoT"
#define MQTT_USERNAME        "test"
#define MQTT_PASSWORD        "mqtt"

#define topicPrefix ""

// MQTT TLS Setting
#ifdef ENABLE_MQTT_TLS
    #define MQTT_USE_TLS     1
    #define MQTT_PORT        8883
#else
    #define MQTT_USE_TLS     0
    #define MQTT_PORT        1883
#endif

#define MQTT_MAX_PACKET_SZ   16384
#define INVALID_SOCKET_FD    -1
#define PRINT_BUFFER_SIZE    16384

#endif

#define LOG_LEVEL LOG_LEVEL_INFO
#define LOG_BUFFER_SIZE MAX_PAYLOAD_SIZE

#endif