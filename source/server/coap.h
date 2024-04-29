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

#ifdef ENABLE_COAP_DTLS
#define MAX_KEY 64 /* Maximum length of a pre-shared key in bytes. */

typedef struct psk_sni_def_t {
  char *sni_match;
  coap_bin_const_t *new_key;
  coap_bin_const_t *new_hint;
} psk_sni_def_t;

typedef struct valid_psk_snis_t {
  size_t count;
  psk_sni_def_t *psk_sni_list;
} valid_psk_snis_t;

static valid_psk_snis_t valid_psk_snis = {0, NULL};

typedef struct id_def_t {
  char *hint_match;
  coap_bin_const_t *identity_match;
  coap_bin_const_t *new_key;
} id_def_t;

typedef struct valid_ids_t {
  size_t count;
  id_def_t *id_list;
} valid_ids_t;

static valid_ids_t valid_ids = {0, NULL};
typedef struct pki_sni_def_t {
  char *sni_match;
  char *new_cert;
  char *new_ca;
} pki_sni_def_t;

typedef struct valid_pki_snis_t {
  size_t count;
  pki_sni_def_t *pki_sni_list;
} valid_pki_snis_t;

static valid_pki_snis_t valid_pki_snis = {0, NULL};

static int verify_cn_callback(const char *cn, const uint8_t *asn1_public_cert COAP_UNUSED, size_t asn1_length COAP_UNUSED, coap_session_t *session COAP_UNUSED, unsigned depth, int validated COAP_UNUSED, void *arg);
static void update_pki_key(coap_dtls_key_t *dtls_key, const char *key_name, const char *cert_name, const char *ca_name);
static coap_dtls_key_t *verify_pki_sni_callback(const char *sni, void *arg COAP_UNUSED);
static const coap_dtls_spsk_info_t *verify_psk_sni_callback(const char *sni, coap_session_t *c_session COAP_UNUSED, void *arg COAP_UNUSED);
static const coap_bin_const_t *verify_id_callback(coap_bin_const_t *identity, coap_session_t *c_session, void *arg COAP_UNUSED);
static coap_dtls_pki_t *setup_pki(coap_context_t *ctx, coap_dtls_role_t role, char *client_sni);
static coap_dtls_spsk_t *setup_spsk(void);
#endif