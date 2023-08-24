/* mqttsimple.c
 *
 * Copyright (C) 2006-2022 wolfSSL Inc.
 *
 * This file is part of wolfMQTT.
 *
 * wolfMQTT is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfMQTT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/* Standalone Example */

//#define WOLFMQTT_MULTITHREAD true
#include "config.h"
#include <signal.h>

#ifdef ENABLE_MQTT

#include <pthread.h>

#include "mqttClient.h"
#include "onem2mTypes.h"
#include "time.h"
#include "logger.h"
#include "util.h"

#define LOG_TAG "MQTT"

/* thread signal */
extern int terminate;

/* Local Variables */
static MqttClient mClient;
static MqttNet mNetwork;
static int mSockFd = INVALID_SOCKET_FD;
static byte mSendBuf[MQTT_MAX_PACKET_SZ];
static byte mReadBuf[MQTT_MAX_PACKET_SZ];
static volatile word16 mPacketIdLast;

extern pthread_mutex_t mutex_lock;

/* Local Functions */

/* msg_new on first data callback */
/* msg_done on last data callback */
/* msg->total_len: Payload total length */
/* msg->buffer: Payload buffer */
/* msg->buffer_len: Payload buffer length */
/* msg->buffer_pos: Payload buffer position */

static word16 mqtt_get_packetid(void);
int invalidRequest();

static int mqtt_message_cb(MqttClient *client, MqttMessage *msg,
    byte msg_new, byte msg_done)
{
    byte buf[PRINT_BUFFER_SIZE+1];
    word32 len;

    cJSON *json = NULL, *pjson = NULL;
    char *puri, *req_type, *originator, *reciever, *contentType;
    oneM2MPrimitive *o2pt;
    
    o2pt = (oneM2MPrimitive *) calloc(1, sizeof(oneM2MPrimitive));

    (void)client;

    if (msg_new) {
        /* check reciever*/
        puri = (char *) malloc(msg->topic_name_len + 1);
        memcpy(puri, msg->topic_name, msg->topic_name_len);
        puri[msg->topic_name_len] = '\0';
        strtok(puri, "/");
        req_type = strtok(NULL, "/");
        
        originator = strtok(NULL,"/");
        reciever = strtok(NULL,"/");
        contentType = strtok(NULL,"/");
        
        if(!strcmp(req_type, "resp") || !strcmp(req_type, "reg_resp")){
            return MQTT_CODE_SUCCESS;
        }

        if(strcmp(reciever, CSE_BASE_RI)){
            logger(LOG_TAG, LOG_LEVEL_DEBUG, "recieved msg not for %s\n", CSE_BASE_RI);
            return MQTT_CODE_SUCCESS;
        }
        
        /* Determine min size to dump */

        len = msg->topic_name_len;
        if (len > PRINT_BUFFER_SIZE) {
            len = PRINT_BUFFER_SIZE;
        }
        XMEMCPY(buf, msg->topic_name, len);
        buf[len] = '\0'; /* Make sure its null terminated */

        /* Print incoming message */
        logger(LOG_TAG, LOG_LEVEL_DEBUG, "%s, Len %u",
            buf, msg->total_len);
        
    }

    /* Print message payload */
    len = msg->buffer_len;
    if (len > PRINT_BUFFER_SIZE) {
        len = PRINT_BUFFER_SIZE;
    }
    XMEMCPY(buf, msg->buffer, len);
    buf[len] = '\0'; /* Make sure its null terminated */

    logger(LOG_TAG, LOG_LEVEL_DEBUG, "%s",buf);

    logger(LOG_TAG, LOG_LEVEL_DEBUG, "request type: %s, originator : %s, reciever : %s, contentType: %s\n", req_type, originator, reciever, contentType);

    json = cJSON_Parse(buf);

    if(!json){
        logger(LOG_TAG, LOG_LEVEL_WARN, "Invalid request\n");
        logger(LOG_TAG, LOG_LEVEL_DEBUG, "ERROR before %10s\n", cJSON_GetErrorPtr());
        //cJSON_Delete(json);
        return MQTT_CODE_SUCCESS;
    }

    /* type mqtt */
    o2pt->origin = strdup(originator);
    MqttClientIdToId(o2pt->origin);

    /* fill primitives */
    pjson = cJSON_GetObjectItem(json, "op");
    if(!pjson) return invalidRequest();

    if(pjson->valueint) o2pt->op = pjson->valueint;
    else o2pt->op = atoi(pjson->valuestring);

    pjson = cJSON_GetObjectItem(json, "to");
    if(!pjson) return invalidRequest();
    o2pt->to = strdup(pjson->valuestring);

    pjson = cJSON_GetObjectItem(json, "fr");
    if(!pjson) return invalidRequest();
    if(pjson->valuestring) o2pt->fr = strdup(pjson->valuestring);


    pjson = cJSON_GetObjectItem(json, "pc");
    if(pjson){
        o2pt->pc = cJSON_PrintUnformatted(pjson);
        o2pt->cjson_pc = cJSON_Parse(o2pt->pc);
    }

    pjson = cJSON_GetObjectItem(json, "rvi");
    if(pjson) o2pt->rvi = strdup(pjson->valuestring);

    pjson = cJSON_GetObjectItem(json, "rqi");
    if(pjson) o2pt->rqi = strdup(pjson->valuestring);
    

    pjson = cJSON_GetObjectItem(json, "ty");
    if(pjson){
        if(pjson->valueint) o2pt->ty = pjson->valueint;
        else o2pt->ty = atoi(pjson->valuestring);
    }
    int rsc = 0;
    pjson = cJSON_GetObjectItem(json, "fc");
    if(pjson){
        o2pt->fc = pjson;
        if(rsc = validate_filter_criteria(o2pt) > 4000){
            handle_error(o2pt, rsc, "Invalid FilterCriteria");
            mqtt_respond_to_client(o2pt, req_type);
            goto exit;
        }
    }

    /* initialize */
    o2pt->fopt = NULL;
    o2pt->isFopt = false;

    // if(!strncmp(o2pt->to, "/~/", 3)){
    //     if(!strncmp(o2pt->to + 3, CSE_BASE_RI, strlen(CSE_BASE_RI))){
    //         char *temp = strdup(o2pt->to + 3 + strlen(CSE_BASE_RI) + 1);
    //         free(o2pt->to);
    //         o2pt->to = temp;
    //     }else{
    //         o2pt->isForwarding = true;
    //     }
    // }

    /* supported content type : json*/
    if(strcmp(contentType, "json")){
        logger(LOG_TAG, LOG_LEVEL_DEBUG, "only json supported\n");

        o2pt->rsc = RSC_UNSUPPORTED_MEDIATYPE;
    
        o2pt->pc = "{\"m2m:dbg\": \"Unsupported media type for content-type: 5\"}";
        mqtt_respond_to_client(o2pt, req_type);
    }else{
        pthread_mutex_trylock(&mutex_lock);
        route(o2pt);
        pthread_mutex_unlock(&mutex_lock);
        mqtt_respond_to_client(o2pt, req_type);
    }

    /* Free allocated memories */
    //cJSON_Delete(json);
    exit:
    free_o2pt(o2pt);
    if(puri)
        free(puri);
    
    puri = NULL;
    pjson = NULL;
    if(json) cJSON_Delete(json);
    json = NULL;
    
    return MQTT_CODE_SUCCESS;
}

int mqtt_respond_to_client(oneM2MPrimitive *o2pt, char* req_type){
    MqttPublish mqttPub;
    char *respTopic;
    cJSON *json;
    char *pl;
    int rc = 0;

    respTopic =(char *) malloc(512);

    logger(LOG_TAG, LOG_LEVEL_DEBUG, "publish mqtt response");

    idToMqttClientId(o2pt->origin);

    if( !strcmp(req_type, "req") ){
        sprintf(respTopic, "%s/oneM2M/resp/%s/%s/json", topicPrefix, o2pt->origin, CSE_BASE_RI);
    }else{
        sprintf(respTopic, "%s/oneM2M/reg_resp/%s/%s/json", topicPrefix, o2pt->origin, CSE_BASE_RI);
    }

    logger(LOG_TAG, LOG_LEVEL_DEBUG, "Topic : %s", respTopic);
    json = o2pt_to_json(o2pt);

    pl = cJSON_PrintUnformatted(json);
    XMEMSET(&mqttPub, 0, sizeof(MqttPublish));
    mqttPub.retain = 0;
    mqttPub.qos = MQTT_QOS;
    mqttPub.topic_name = respTopic;
    mqttPub.packet_id = mqtt_get_packetid();
    mqttPub.buffer = pl;
    mqttPub.total_len = XSTRLEN(pl);

    logger(LOG_TAG, LOG_LEVEL_DEBUG, "MQTT Publish: Topic %s, Qos %d\n%s\n",
        mqttPub.topic_name, mqttPub.qos, mqttPub.buffer);

    do{
        rc = MqttClient_Publish(&mClient, &mqttPub);
    } while(rc == MQTT_CODE_PUB_CONTINUE);

    if(rc != MQTT_CODE_SUCCESS){
        return rc;
    }


    cJSON_Delete(json);

    free(respTopic);
    free(pl);
    respTopic = NULL;
    
    return rc;
}

int invalidRequest(){
    logger(LOG_TAG, LOG_LEVEL_WARN, "Invalid Request");
    return MQTT_CODE_SUCCESS;
}

static void setup_timeout(struct timeval* tv, int timeout_ms)
{
    tv->tv_sec = timeout_ms / 1000;
    tv->tv_usec = (timeout_ms % 1000) * 1000;

    /* Make sure there is a minimum value specified */
    if (tv->tv_sec < 0 || (tv->tv_sec == 0 && tv->tv_usec <= 0)) {
        tv->tv_sec = 0;
        tv->tv_usec = 100;
    }
}

static int socket_get_error(int sockFd)
{
    int so_error = 0;
    socklen_t len = sizeof(so_error);
    getsockopt(sockFd, SOL_SOCKET, SO_ERROR, &so_error, &len);
    return so_error;
}

static int mqtt_net_connect(void *context, const char* host, word16 port,
    int timeout_ms)
{
    int rc;
    int sockFd, *pSockFd = (int*)context;
    struct sockaddr_in addr;
    struct addrinfo *result = NULL;
    struct addrinfo hints;
    struct timeval tv;

    if (pSockFd == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    setup_timeout(&tv, 1000);

    /* get address */
    XMEMSET(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    XMEMSET(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    rc = getaddrinfo(host, NULL, &hints, &result);
    if (rc >= 0 && result != NULL) {
        struct addrinfo* res = result;

        /* prefer ip4 addresses */
        while (res) {
            if (res->ai_family == AF_INET) {
                result = res;
                break;
            }
            res = res->ai_next;
        }
        if (result->ai_family == AF_INET) {
            addr.sin_port = htons(port);
            addr.sin_family = AF_INET;
            addr.sin_addr =
                ((struct sockaddr_in*)(result->ai_addr))->sin_addr;
        }
        else {
            rc = -1;
        }
        freeaddrinfo(result);
    }
    if (rc < 0) {
        return MQTT_CODE_ERROR_NETWORK;
    }

    sockFd = socket(addr.sin_family, SOCK_STREAM, 0);
    if (sockFd < 0) {
        return MQTT_CODE_ERROR_NETWORK;
    }
    setsockopt(sockFd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
    /* Start connect */
    rc = connect(sockFd, (struct sockaddr*)&addr, sizeof(addr));
    if (rc < 0) {
        PRINTF("NetConnect: Error %d (Sock Err %d)",
            rc, socket_get_error(*pSockFd));
        close(sockFd);
        return MQTT_CODE_ERROR_NETWORK;
    }

    /* save socket number to context */
    *pSockFd = sockFd;

    return MQTT_CODE_SUCCESS;
}

static int mqtt_net_read(void *context, byte* buf, int buf_len, int timeout_ms)
{
    int rc;
    int *pSockFd = (int*)context;
    int bytes = 0;
    struct timeval tv;

    if (pSockFd == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    /* Setup timeout */
    setup_timeout(&tv, timeout_ms);
    setsockopt(*pSockFd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));

    /* Loop until buf_len has been read, error or timeout */
    while (bytes < buf_len) {
        rc = (int)recv(*pSockFd, &buf[bytes], buf_len - bytes, 0);
        if (rc < 0) {
            rc = socket_get_error(*pSockFd);
            if (rc == 0)
                break; /* timeout */
            PRINTF("NetRead: Error %d", rc);
            return MQTT_CODE_ERROR_NETWORK;
        }
        bytes += rc; /* Data */
    }

    if (bytes == 0) {
        return MQTT_CODE_ERROR_TIMEOUT;
    }

    return bytes;
}

static int mqtt_net_write(void *context, const byte* buf, int buf_len,
    int timeout_ms)
{
    int rc;
    int *pSockFd = (int*)context;
    struct timeval tv;

    if (pSockFd == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    /* Setup timeout */
    setup_timeout(&tv, timeout_ms);
    setsockopt(*pSockFd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));

    rc = (int)send(*pSockFd, buf, buf_len, 0);
    if (rc < 0) {
        PRINTF("NetWrite: Error %d (Sock Err %d)",
            rc, socket_get_error(*pSockFd));
        return MQTT_CODE_ERROR_NETWORK;
    }

    return rc;
}

static int mqtt_net_disconnect(void *context)
{
    int *pSockFd = (int*)context;

    if (pSockFd == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    close(*pSockFd);
    *pSockFd = INVALID_SOCKET_FD;

    return MQTT_CODE_SUCCESS;
}

#ifdef ENABLE_MQTT_TLS
static int mqtt_tls_verify_cb(int preverify, WOLFSSL_X509_STORE_CTX* store)
{
    char buffer[WOLFSSL_MAX_ERROR_SZ];
    PRINTF("MQTT TLS Verify Callback: PreVerify %d, Error %d (%s)",
        preverify, store->error, store->error != 0 ?
            wolfSSL_ERR_error_string(store->error, buffer) : "none");
    PRINTF("  Subject's domain name is %s", store->domain);

    if (store->error != 0) {
        /* Allowing to continue */
        /* Should check certificate and return 0 if not okay */
        PRINTF("  Allowing cert anyways");
    }

    return 1;
}

/* Use this callback to setup TLS certificates and verify callbacks */
static int mqtt_tls_cb(MqttClient* client)
{
    int rc = WOLFSSL_FAILURE;

    /* Use highest available and allow downgrade. If wolfSSL is built with
     * old TLS support, it is possible for a server to force a downgrade to
     * an insecure version. */
    client->tls.ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
    if (client->tls.ctx) {
        wolfSSL_CTX_set_verify(client->tls.ctx, WOLFSSL_VERIFY_PEER,
                               mqtt_tls_verify_cb);

        /* default to success */
        rc = WOLFSSL_SUCCESS;

    #if !defined(NO_CERT)
    #if 0
        /* Load CA certificate buffer */
        rc = wolfSSL_CTX_load_verify_buffer(client->tls.ctx, caCertBuf,
                                          caCertSize, WOLFSSL_FILETYPE_PEM);
    #endif
    #if 0
        /* If using a client certificate it can be loaded using: */
        rc = wolfSSL_CTX_use_certificate_buffer(client->tls.ctx,
                           clientCertBuf, clientCertSize, WOLFSSL_FILETYPE_PEM);
    #endif
    #endif /* !NO_CERT */
    }

    PRINTF("MQTT TLS Setup (%d)", rc);

    return rc;
}
#else
static int mqtt_tls_cb(MqttClient* client)
{
    (void)client;
    return 0;
}
#endif /* ENABLE_MQTT_TLS */

static word16 mqtt_get_packetid(void)
{
    /* Check rollover */
    if (mPacketIdLast >= MAX_PACKET_ID) {
        mPacketIdLast = 0;
    }

    return ++mPacketIdLast;
}

int mqtt_notify(oneM2MPrimitive *o2pt, char* noti_json, NotiTarget *nt){
    int rc = 0;
    int sfd = 0;
    MqttObject mqttObj;
    MqttNet mNet;
    MqttPublish mqttPub;
    MqttClient notiClient;

    char buf[1024] = {'\0'};
    char topic[1024] = {'\0'};

    char notiSbuf[1024], notiRbuf[1024];
    
    sprintf(topic, "/oneM2M/req/%s/%s", CSE_BASE_RI, nt->target + 1);
    logger("MQTT", LOG_LEVEL_DEBUG, "topic : %s", topic);

    if(!strcmp(nt->host, MQTT_HOST)){
        memcpy(&notiClient, &mClient, sizeof(MqttClient));
        memcpy(&mNet, &mNetwork, sizeof(MqttNet));
    }else{
        memset(&mNet, 0, sizeof(mNet));
        mNet.connect = mqtt_net_connect;
        mNet.read = mqtt_net_read;
        mNet.write = mqtt_net_write;
        mNet.disconnect = mqtt_net_disconnect;
        mNet.context = &sfd;
        
        rc = MqttClient_Init(&notiClient, &mNet, mqtt_message_cb,
            notiSbuf, sizeof(notiSbuf), notiRbuf, sizeof(notiRbuf),
            MQTT_CON_TIMEOUT_MS);
        if (rc != MQTT_CODE_SUCCESS) {
            goto exit;
        }
        logger(LOG_TAG, LOG_LEVEL_INFO, "MQTT Noti Init Success");

        rc = MqttClient_NetConnect(&notiClient, nt->host, nt->port,
            MQTT_CON_TIMEOUT_MS, MQTT_USE_TLS, mqtt_tls_cb);
        if (rc != MQTT_CODE_SUCCESS) {
            goto exit;
        }
        logger(LOG_TAG, LOG_LEVEL_INFO, "MQTT Network Connect Success: Host %s, Port %d, UseTLS %d",
            MQTT_HOST, MQTT_PORT, MQTT_USE_TLS);

        sprintf(buf, "%s_notify",MQTT_CLIENT_ID); // new client ID for notification
        /* Send Connect and wait for Ack */
        XMEMSET(&mqttObj, 0, sizeof(mqttObj));
        mqttObj.connect.keep_alive_sec = MQTT_KEEP_ALIVE_SEC;
        mqttObj.connect.client_id = strdup(buf);
        mqttObj.connect.username = MQTT_USERNAME;
        mqttObj.connect.password = MQTT_PASSWORD;
        rc = MqttClient_Connect(&notiClient, &mqttObj.connect);
        if (rc != MQTT_CODE_SUCCESS) {
            goto exit;
        }
        logger(LOG_TAG, LOG_LEVEL_INFO, "MQTT Broker Connect Success: ClientID %s, Username %s, Password %s",
            MQTT_CLIENT_ID,
            (MQTT_USERNAME == NULL) ? "Null" : MQTT_USERNAME,
            (MQTT_PASSWORD == NULL) ? "Null" : MQTT_PASSWORD);

    }

    
    
    XMEMSET(&mqttPub, 0, sizeof(MqttPublish));
    mqttPub.retain = 0;
    mqttPub.qos = MQTT_QOS;
    mqttPub.topic_name = topic;
    mqttPub.packet_id = mqtt_get_packetid();
    mqttPub.buffer = strdup(noti_json);
    mqttPub.total_len = XSTRLEN(noti_json);

    logger(LOG_TAG, LOG_LEVEL_DEBUG, "MQTT Publish: Topic %s, Qos %d\n%s\n",
        mqttPub.topic_name, mqttPub.qos, mqttPub.buffer);

    do{
        rc = MqttClient_Publish(&notiClient, &mqttPub);
    } while(rc == MQTT_CODE_PUB_CONTINUE);

    if(rc != MQTT_CODE_SUCCESS){
        return rc;
    }


    exit:
    if (rc != MQTT_CODE_SUCCESS) {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "MQTT Error %d: %s", rc, MqttClient_ReturnCodeToString(rc));
    }
    if(sfd > 0){
        MqttClient_Disconnect(&notiClient);
        MqttClient_DeInit(&notiClient);
    }
    return NULL;
}

int mqtt_forwarding(oneM2MPrimitive *o2pt, char *host, char *port, cJSON *csr){
    int rc = 0;
    int sfd = 0;
    MqttObject mqttObj;
    MqttNet mNet;
    MqttPublish mqttPub;
    MqttClient notiClient;
    MqttTopic topics[2];

    char buf[1024] = {'\0'};
    char topic[1024] = {'\0'};

    char notiSbuf[1024], notiRbuf[1024];
    
    sprintf(topic, "/oneM2M/req/%s/%s/json", CSE_BASE_RI, csr->csi);
    logger("MQTT", LOG_LEVEL_DEBUG, "topic : %s", topic);

    if(!strcmp(host, MQTT_HOST)){
        memcpy(&notiClient, &mClient, sizeof(MqttClient));
        memcpy(&mNet, &mNetwork, sizeof(MqttNet));
    }else{
        memset(&mNet, 0, sizeof(mNet));
        mNet.connect = mqtt_net_connect;
        mNet.read = mqtt_net_read;
        mNet.write = mqtt_net_write;
        mNet.disconnect = mqtt_net_disconnect;
        mNet.context = &sfd;
        
        rc = MqttClient_Init(&notiClient, &mNet, NULL,
            notiSbuf, sizeof(notiSbuf), notiRbuf, sizeof(notiRbuf),
            MQTT_CON_TIMEOUT_MS);
        if (rc != MQTT_CODE_SUCCESS) {
            goto exit;
        }
        logger(LOG_TAG, LOG_LEVEL_INFO, "MQTT Noti Init Success");

        rc = MqttClient_NetConnect(&notiClient, host, atoi(port),
            MQTT_CON_TIMEOUT_MS, MQTT_USE_TLS, mqtt_tls_cb);
        if (rc != MQTT_CODE_SUCCESS) {
            goto exit;
        }
        logger(LOG_TAG, LOG_LEVEL_INFO, "MQTT Network Connect Success: Host %s, Port %d, UseTLS %d",
            MQTT_HOST, MQTT_PORT, MQTT_USE_TLS);

        sprintf(buf, "%s_forwarding",MQTT_CLIENT_ID); // new client ID for notification
        /* Send Connect and wait for Ack */
        XMEMSET(&mqttObj, 0, sizeof(mqttObj));
        mqttObj.connect.keep_alive_sec = MQTT_KEEP_ALIVE_SEC;
        mqttObj.connect.client_id = strdup(buf);
        mqttObj.connect.username = MQTT_USERNAME;
        mqttObj.connect.password = MQTT_PASSWORD;
        rc = MqttClient_Connect(&notiClient, &mqttObj.connect);
        if (rc != MQTT_CODE_SUCCESS) {
            goto exit;
        }
        logger(LOG_TAG, LOG_LEVEL_INFO, "MQTT Broker Connect Success: ClientID %s, Username %s, Password %s",
            MQTT_CLIENT_ID,
            (MQTT_USERNAME == NULL) ? "Null" : MQTT_USERNAME,
            (MQTT_PASSWORD == NULL) ? "Null" : MQTT_PASSWORD);

    }

    char respTopic[256] = {0};

    sprintf(respTopic, "/oneM2M/resp/%s/%s/#", CSE_BASE_RI, csr->csi);
    topics[0].topic_filter = respTopic;
    topics[0].qos = MQTT_QOS;

    mqttObj.subscribe.packet_id = mqtt_get_packetid();
    mqttObj.subscribe.topic_count = sizeof(topics) / sizeof(MqttTopic);
    mqttObj.subscribe.topics = topics;
    rc = MqttClient_Subscribe(&mClient, &mqttObj.subscribe);
    if (rc != MQTT_CODE_SUCCESS) {
        goto exit;
    }
    
    XMEMSET(&mqttPub, 0, sizeof(MqttPublish));
    mqttPub.retain = 0;
    mqttPub.qos = MQTT_QOS;
    mqttPub.topic_name = topic;
    mqttPub.packet_id = mqtt_get_packetid();
    mqttPub.buffer = strdup(o2pt->pc);
    mqttPub.total_len = XSTRLEN(o2pt->pc);

    logger(LOG_TAG, LOG_LEVEL_DEBUG, "MQTT Publish: Topic %s, Qos %d\n%s\n",
        mqttPub.topic_name, mqttPub.qos, mqttPub.buffer);

    do{
        rc = MqttClient_Publish(&notiClient, &mqttPub);
    } while(rc == MQTT_CODE_PUB_CONTINUE);

    if(rc != MQTT_CODE_SUCCESS){
        return rc;
    }

    rc = MqttClient_WaitMessage_ex(&mClient, &mqttObj, 5000);
    if (rc == MQTT_CODE_ERROR_TIMEOUT) {
        /* send keep-alive ping */
        rc = MqttClient_Ping_ex(&mClient, &mqttObj.ping);
        if (rc != MQTT_CODE_SUCCESS) {
            goto exit;
        }
        
    }
    else if (rc != MQTT_CODE_SUCCESS) {
        goto exit;
    }else{
        logger(LOG_TAG, LOG_LEVEL_INFO, "MQTT Recv Success");

        logger(LOG_TAG, LOG_LEVEL_DEBUG, "MQTT Recv: Topic %s, Qos %d\n%s\n",
            mqttObj.publish.topic_name, mqttObj.publish.qos, mqttObj.publish.buffer);
    }

    exit:
    if (rc != MQTT_CODE_SUCCESS) {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "MQTT Error %d: %s", rc, MqttClient_ReturnCodeToString(rc));
    }
    if(sfd > 0){
        MqttClient_Disconnect(&notiClient);
        MqttClient_DeInit(&notiClient);
    }
    return NULL;
}

/* Public Function */
void *mqtt_serve(void)
{
    int pingc = 0;
    int rc = 0;
    MqttObject mqttObj;
    MqttTopic topics[4];
    char *reqTopic, *respTopic, *reg_reqTopic, *reg_respTopic;

    /* Initialize MQTT client */
    XMEMSET(&mNetwork, 0, sizeof(mNetwork));
    mNetwork.connect = mqtt_net_connect;
    mNetwork.read = mqtt_net_read;
    mNetwork.write = mqtt_net_write;
    mNetwork.disconnect = mqtt_net_disconnect;
    mNetwork.context = &mSockFd;
    rc = MqttClient_Init(&mClient, &mNetwork, mqtt_message_cb,
        mSendBuf, sizeof(mSendBuf), mReadBuf, sizeof(mReadBuf),
        MQTT_CON_TIMEOUT_MS);
    if (rc != MQTT_CODE_SUCCESS) {
        goto exit;
    }
    logger(LOG_TAG, LOG_LEVEL_INFO, "MQTT Init Success");

    /* Connect to broker */
    rc = MqttClient_NetConnect(&mClient, MQTT_HOST, MQTT_PORT,
        MQTT_CON_TIMEOUT_MS, MQTT_USE_TLS, mqtt_tls_cb);
    if (rc != MQTT_CODE_SUCCESS) {
        goto exit;
    }
    logger(LOG_TAG, LOG_LEVEL_INFO, "MQTT Network Connect Success: Host %s, Port %d, UseTLS %d",
        MQTT_HOST, MQTT_PORT, MQTT_USE_TLS);

    /* Send Connect and wait for Ack */
    XMEMSET(&mqttObj, 0, sizeof(mqttObj));
    mqttObj.connect.keep_alive_sec = MQTT_KEEP_ALIVE_SEC;
    mqttObj.connect.client_id = MQTT_CLIENT_ID;
    mqttObj.connect.username = MQTT_USERNAME;
    mqttObj.connect.password = MQTT_PASSWORD;
    rc = MqttClient_Connect(&mClient, &mqttObj.connect);
    if (rc != MQTT_CODE_SUCCESS) {
        goto exit;
    }
    logger(LOG_TAG, LOG_LEVEL_INFO, "MQTT Broker Connect Success: ClientID %s, Username %s, Password %s",
        MQTT_CLIENT_ID,
        (MQTT_USERNAME == NULL) ? "Null" : MQTT_USERNAME,
        (MQTT_PASSWORD == NULL) ? "Null" : MQTT_PASSWORD);


    reqTopic = (char *) malloc(128);
    respTopic = (char *) malloc(128);
    reg_reqTopic = (char *) malloc(128);
    reg_respTopic = (char *) malloc(128);

    sprintf(reqTopic, "%s%s%s%s", topicPrefix, "/oneM2M/req/+/", CSE_BASE_RI, "/#");
    sprintf(respTopic, "%s%s%s%s", topicPrefix, "/oneM2M/resp/", CSE_BASE_RI, "/+/#");
    sprintf(reg_reqTopic, "%s%s%s%s", topicPrefix, "/oneM2M/reg_req/+/", CSE_BASE_RI, "/#");
    sprintf(reg_respTopic, "%s%s%s%s", topicPrefix, "/oneM2M/reg_resp/", CSE_BASE_RI, "/+/#");

    /* Subscribe and wait for Ack */
    XMEMSET(&mqttObj, 0, sizeof(mqttObj));
    topics[0].topic_filter = reqTopic;
    topics[0].qos = MQTT_QOS;
    topics[1].topic_filter = respTopic;
    topics[1].qos = MQTT_QOS;
    topics[2].topic_filter = reg_reqTopic;
    topics[2].qos = MQTT_QOS;
    topics[3].topic_filter = reg_respTopic;
    topics[3].qos = MQTT_QOS;

    mqttObj.subscribe.packet_id = mqtt_get_packetid();
    mqttObj.subscribe.topic_count = sizeof(topics) / sizeof(MqttTopic);
    mqttObj.subscribe.topics = topics;
    rc = MqttClient_Subscribe(&mClient, &mqttObj.subscribe);
    if (rc != MQTT_CODE_SUCCESS) {
        goto exit;
    }

    logger(LOG_TAG, LOG_LEVEL_INFO, "MQTT Subscribe Success: Topic %s, QoS %d", reqTopic, MQTT_QOS);
    logger(LOG_TAG, LOG_LEVEL_INFO, "MQTT Subscribe Success: Topic %s, QoS %d", respTopic, MQTT_QOS);
    logger(LOG_TAG, LOG_LEVEL_INFO, "MQTT Subscribe Success: Topic %s, QoS %d", reg_reqTopic, MQTT_QOS);
    logger(LOG_TAG, LOG_LEVEL_INFO, "MQTT Subscribe Success: Topic %s, QoS %d", reg_respTopic, MQTT_QOS);

    /* Wait for messages */
    while (!terminate) {
        rc = MqttClient_WaitMessage_ex(&mClient, &mqttObj, 3000);
        pingc += 3000;
        if (rc == MQTT_CODE_ERROR_TIMEOUT) {
            /* send keep-alive ping */
            if(pingc >= MQTT_CMD_TIMEOUT_MS){
                rc = MqttClient_Ping_ex(&mClient, &mqttObj.ping);
                pingc = 0;
                if (rc != MQTT_CODE_SUCCESS) {
                break;
                }
            }  
        }
        else if (rc != MQTT_CODE_SUCCESS) {
            break;
        }
    }

exit:
    if (!terminate && rc != MQTT_CODE_SUCCESS) {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "MQTT Error %d: %s", rc, MqttClient_ReturnCodeToString(rc));
    }

    terminate = 1;
    free(reqTopic);
    free(respTopic);
    free(reg_reqTopic);
    free(reg_respTopic);
    //MqttClient_Disconnect(&mClient);
    raise(SIGINT);
    return NULL;
}
//#endif /* HAVE_SOCKET */


void idToMqttClientId(char *cseid){
    int len = strlen(cseid);
    if(cseid[0] == '/') {
        for(int i = 0 ; i < len ; i++) cseid[i] = cseid[i+1];
        len--;
    }

    for(int i = 0 ; i < len ; i++){
        if(cseid[i] == '/') cseid[i] = ':';
    }
}

void MqttClientIdToId(char *cseid){
    int len = strlen(cseid);
    for(int i = 0 ; i < len ; i++){
        if(cseid[i] == ':') cseid[i] = '/';
    }
}

#endif