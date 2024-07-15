#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include "cJSON.h"
#include "onem2m.h"
#include "logger.h"
#include "websocket_server.h" // 헤더 파일 포함

static int callback_websocket(struct lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            logger("WEBSOCKET", LOG_LEVEL_INFO, "Connection established");
            break;

        case LWS_CALLBACK_RECEIVE: {
            logger("WEBSOCKET", LOG_LEVEL_INFO, "Received data");

            char *received_data = (char *)in;
            cJSON *json = cJSON_Parse(received_data);

            if (json == NULL) {
                logger("WEBSOCKET", LOG_LEVEL_ERROR, "Invalid JSON format");
                return -1;
            }

            oneM2MPrimitive o2pt;
            memset(&o2pt, 0, sizeof(oneM2MPrimitive)); // 초기화 추가
            if (process_onem2m_request(json, &o2pt) == -1) {
                logger("WEBSOCKET", LOG_LEVEL_ERROR, "Invalid request format");
                cJSON_Delete(json);
                return -1;
            }

            // 여기에 요청 처리를 추가하세요
            logger("WEBSOCKET", LOG_LEVEL_INFO, "Processed request successfully");

            // 응답 메시지 생성
            cJSON *response = cJSON_CreateObject();
            cJSON_AddNumberToObject(response, "rsc", o2pt.rsc);
            cJSON_AddStringToObject(response, "rqi", o2pt.rqi);
            cJSON_AddItemToObject(response, "pc", cJSON_Duplicate(o2pt.response_pc, 1));

            char *response_message = cJSON_PrintUnformatted(response);
            send_websocket_message(wsi, response_message);

            cJSON_Delete(response);
            free(response_message);

            cJSON_Delete(json);
            break;
        }

        case LWS_CALLBACK_CLOSED:
            logger("WEBSOCKET", LOG_LEVEL_INFO, "Connection closed");
            break;

        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "oneM2M.json",
        callback_websocket,
        0,
        65536,
    },
    {NULL, NULL, 0, 0} /* end */
};

void initialize_websocket_server() {
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof info);
    info.port = 8081;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

    struct lws_context *context = lws_create_context(&info);
    if (context == NULL) {
        logger("WEBSOCKET", LOG_LEVEL_ERROR, "WebSocket context creation failed");
        return;
    }

    logger("WEBSOCKET", LOG_LEVEL_INFO, "WebSocket Server started");

    while (1) {
        lws_service(context, 1000);
    }

    lws_context_destroy(context);
}

// WebSocket 메시지를 보내는 함수
void send_websocket_message(struct lws *wsi, const char *message) {
    size_t message_length = strlen(message);
    unsigned char *buf = (unsigned char *)malloc(LWS_SEND_BUFFER_PRE_PADDING + message_length + LWS_SEND_BUFFER_POST_PADDING);
    if (buf == NULL) {
        logger("WEBSOCKET", LOG_LEVEL_ERROR, "Memory allocation failed");
        return;
    }
    memcpy(&buf[LWS_SEND_BUFFER_PRE_PADDING], message, message_length);
    lws_write(wsi, &buf[LWS_SEND_BUFFER_PRE_PADDING], message_length, LWS_WRITE_TEXT);
    free(buf);
}
