#include "libcoap/coap.h"
#include "onem2m.h"

// Current CoAP attributes
#define COAP_PORT 5683
#define COAP_VERSION 1
#define COAP_HEADER_SIZE 4
#define COAP_OPTION_HEADER_SIZE 1
#define COAP_PAYLOAD_MARKER 0xFF

typedef struct {
    uint8_t ver;
    coap_pdu_type_t type;
    uint8_t code;
    uint8_t* token;
    uint8_t token_len;
    uint8_t* payload;
    uint8_t payload_len;
    uint16_t message_id;
    uint16_t option_cnt;
} coapPacket;

void* coap_serve(void);
void coap_notify(oneM2MPrimitive *o2pt, char *noti_json, NotiTarget *nt);
void coap_forwarding(oneM2MPrimitive *o2pt, char *host, int port);

extern void route(oneM2MPrimitive *o2pt);