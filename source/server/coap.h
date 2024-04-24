#include "libcoap/coap.h"
// #include <coap3/coap.h>
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <dirent.h>
#include <syslog.h>

// Current CoAP attributes
#define COAP_VERSION 1
#define COAP_HEADER_SIZE 4
#define COAP_OPTION_HEADER_SIZE 1
#define COAP_PAYLOAD_MARKER 0xFF

#define LOG_TAG "COAP"
#define BUF_SIZE 65535

typedef struct
{
    uint8_t ver;
    coap_pdu_type_t type;
    uint8_t code;
    uint8_t *token;
    uint8_t token_len;
    uint16_t message_id;
    uint16_t option_cnt;
} coapPacket;

void *coap_serve();
void coap_notify(oneM2MPrimitive *o2pt, char *noti_json, NotiTarget *nt);
void coap_forwarding(oneM2MPrimitive *o2pt, char *host, int port);

extern void route(oneM2MPrimitive *o2pt);