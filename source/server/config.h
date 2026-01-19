#ifndef __CONFIG_H__
#define __CONFIG_H__
#include "logger.h"

// IN_CSE, MN_CSE
#define SERVER_TYPE IN_CSE

// #define NIC_NAME "eth0"
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT "3000"
#define CSE_BASE_NAME "TinyIoT"
#define CSE_BASE_RI "tinyiot"
#define CSE_BASE_SP_ID "tinyiot.example.com"
#define CSE_RVI RVI_2a

#if SERVER_TYPE == MN_CSE
#define REMOTE_CSE_ID ""
#define REMOTE_CSE_NAME ""
#define REMOTE_CSE_HOST ""
#define REMOTE_CSE_SP_ID ""
#define REMOTE_CSE_PORT 0
#endif

// Database Settings
#define DB_SQLITE 1
#define DB_POSTGRESQL 2

// Select Database Type: DB_SQLITE or DB_POSTGRESQL
#define DB_TYPE DB_SQLITE
// #define DB_TYPE DB_POSTGRESQL


#if DB_TYPE == DB_POSTGRESQL
// PostgreSQL connection settings
#define PG_HOST "localhost"
#define PG_PORT 5432
#define PG_USER "user"
#define PG_PASSWORD "password"
#define PG_DBNAME "tinydb"

// PostgreSQL Schema Types
#define PG_SCHEMA_VARCHAR 0  // Use VARCHAR for string fields
#define PG_SCHEMA_TEXT 1     // Use TEXT for string fields

// Select PostgreSQL Schema Type: PG_SCHEMA_VARCHAR or PG_SCHEMA_TEXT
// #define PG_SCHEMA_TYPE PG_SCHEMA_VARCHAR
#define PG_SCHEMA_TYPE PG_SCHEMA_TEXT
#endif

// Security

// To disable ADMIN_AE_ID, comment the following line
#define ADMIN_AE_ID "CAdmin"

// To enable multiple ACOP, add each ACOP
// Supported ACOP
// ACOP_CREATE | ACOP_RETRIEVE | ACOP_UPDATE | ACOP_DELETE | ACOP_DISCOVERY | ACOP_NOTIFY
#define DEFAULT_ACOP ACOP_CREATE

// Set allowed remote CSE ID
// To allow all remote CSE ID, set to "/*"
#define ALLOWED_REMOTE_CSE_ID "/id-in,/id-mn"

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
// #define ENABLE_MQTT_WEBSOCKET
#ifdef ENABLE_MQTT_WEBSOCKET
#define MQTT_OVER_WS_PORT 8888
#endif

#define MQTT_HOST "127.0.0.1"
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
#define MQTT_PORT 1885
#endif



#define MQTT_MAX_PACKET_SZ 16384
#define INVALID_SOCKET_FD -1
#define PRINT_BUFFER_SIZE 16384



#endif

#define LOG_LEVEL LOG_LEVEL_DEBUG
#define LOG_BUFFER_SIZE MAX_PAYLOAD_SIZE
// #define SAVE_LOG

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

// To enable WS, de-comment the following line
#define ENABLE_WS 1
#ifdef ENABLE_WS
#define WS_HOST "localhost"
#define WS_PORT 8081
#endif

// Additional Settings

// For browser access, CORS should be enabled
// To add CORS headers to HTTP response decomment following line.

// De-Register remoteCSE on shutdown
// To make server delete all remote remoteCSE resource decomment following line.
// #define DEREGISTER_AT_SHUTDOWN

// Default Discovery Result Sort
#define DEFAULT_DISCOVERY_SORT SORT_DESC

#endif
