#ifndef __CONFIG_H__
#define __CONFIG_H__
#include "logger.h"

// IN_CSE, MN_CSE
#define SERVER_TYPE IN_CSE

// #define NIC_NAME "eth0"
#define SERVER_IP "${MEC_HOST_URL}"
#define SERVER_PORT "${SERVER_PORT}"
#define CSE_BASE_NAME "TinyIoT-IN-CSE"
#define CSE_BASE_RI "tinyiot-in-cse"
#define CSE_BASE_SP_ID ""
#define CSE_RVI RVI_2a

#if SERVER_TYPE == MN_CSE
#define REMOTE_CSE_ID ""
#define REMOTE_CSE_NAME ""
#define REMOTE_CSE_HOST ""
#define REMOTE_CSE_SP_ID ""
#define REMOTE_CSE_PORT 0
#endif

// Security

// To disable ADMIN_AE_ID, comment the following line
#define ADMIN_AE_ID "CAdmin"

// To enable multiple ACOP, add each ACOP
// Supported ACOP
// ACOP_CREATE | ACOP_RETRIEVE | ACOP_UPDATE | ACOP_DELETE | ACOP_DISCOVERY | ACOP_NOTIFY
#define DEFAULT_ACOP ACOP_CREATE| ACOP_RETRIEVE | ACOP_UPDATE | ACOP_DELETE | ACOP_DISCOVERY | ACOP_NOTIFY

// Set allowed remote CSE ID
// To allow all remote CSE ID, set to "/*"
#define ALLOWED_REMOTE_CSE_ID "/*"

// Run server in mono Thread
// 0 → multi-thread, 1 → mono-thread
#define MONO_THREAD 0

// Upper Tester : under development
// #define UPPERTESTER

#ifdef UPPERTESTER
#define UPPERTESTER_URI "__ut__"
#endif

#define MAX_PAYLOAD_SIZE 65536
#define MAX_URI_SIZE 1024
#define MAX_ATTRIBUTE_SIZE 4096

#define DEFAULT_EXPIRE_TIME -3600 * 24 * 365 * 2

#define SOCKET_TIMEOUT 3 // seconds
#define MAX_URI_LENGTH 1024

// AE Settings
// According to oneM2M standard, AE originator ID should be freely settable
// #define ALLOW_AE_ORIGIN "C*,S*,/*"

// CNT Settings
#define DEFAULT_MAX_NR_INSTANCES 1000
#define DEFAULT_MAX_BYTE_SIZE 65536

// CIN Settings
#define ALLOW_CIN_RN true // true → allow, false → not allow

// Group Settings
#define DEFAULT_CONSISTENCY_POLICY CSY_ABANDON_MEMBER

// Discovery Settings
// Limitation of discovery result
#define DEFAULT_DISCOVERY_LIMIT 150

// MQTT Settings

// To enable MQTT, de-comment the following line
#define ENABLE_MQTT

#ifdef ENABLE_MQTT
#define MQTT_HOST "${MQTT_HOST}"
#define MQTT_QOS MQTT_QOS_0
#define MQTT_KEEP_ALIVE_SEC 60
#define MQTT_CMD_TIMEOUT_MS 30000
#define MQTT_CON_TIMEOUT_MS 5000
#define MQTT_CLIENT_ID "TinyIoT"
#define MQTT_USERNAME "test"
#define MQTT_PASSWORD "mqtt"

#define topicPrefix ""

// MQTT TLS Setting
#ifdef ENABLE_MQTT_TLS
#define MQTT_USE_TLS 1
#define MQTT_PORT 8883
#else
#define MQTT_USE_TLS 0
#define MQTT_PORT ${MQTT_PORT}
#endif

#define MQTT_MAX_PACKET_SZ 16384
#define INVALID_SOCKET_FD -1
#define PRINT_BUFFER_SIZE 16384

#endif

#define LOG_LEVEL LOG_LEVEL_DEBUG
#define LOG_BUFFER_SIZE MAX_PAYLOAD_SIZE
#define SAVE_LOG

// CoAP Settings

// To enable CoAP, de-comment the following line
// #define ENABLE_COAP
#ifdef ENABLE_COAP

// #define ENABLE_COAP_DTLS
#ifdef ENABLE_COAP_DTLS
#define COAP_PORT 5684
#else
#define COAP_PORT 5683
#endif

#endif

// Additional Settings

// For browser access, CORS should be enabled
// To add CORS headers to HTTP response decomment following line.
// #define CORS

// De-Register remoteCSE on shutdown
// To make server delete all remote remoteCSE resource decomment following line.
// #define DEREGISTER_AT_SHUTDOWN

// #define DEREGISTER_AT_SHUTDOWN
#define DEFAULT_DISCOVERY_SORT SORT_DESC

// #define ENABLE_MEC
// #define MEC_ENABLE 1
#ifdef ENABLE_MEC
#define MEC_HOST "${MEC_HOST_URL}"
#define MEC_PROTOCOL "https"
#define MEC_PLATFORM "${MEC_PLATFORM}"
#define MEC_SANDBOX_ID "${MEC_SANDBOX_ID}"
#endif

#endif