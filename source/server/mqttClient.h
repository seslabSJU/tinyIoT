#ifndef ENABLE_MQTT_TLS
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include <string.h>

#include "wolfmqtt/mqtt_client.h"
#include "config.h"
#include "onem2m.h"
#include "cJSON.h"
#include "config.h"

static int mqtt_tls_cb(MqttClient *client);

int mqtt_message_cb(MqttClient *client, MqttMessage *msg, byte msg_new, byte msg_done);

int mqtt_respond_to_client(oneM2MPrimitive *o2pt, char *req_type);
int mqtt_notify(oneM2MPrimitive *o2pt, cJSON *noti_json, NotiTarget *nt);
int mqtt_forwarding(oneM2MPrimitive *o2pt, char *host, int port, cJSON *csr);
void setup_timeout(struct timeval *tv, int timeout_ms);
void idToMqttClientId(char *id);
void MqttClientIdToId(char *id);
void *mqtt_serve(void);