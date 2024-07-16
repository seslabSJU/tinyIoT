#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include "cJSON.h"
#include "onem2m.h"
#include "logger.h"
#include "websocket_server.h"
#include "rtManager.h"

void send_websocket_message(struct lws *wsi, const char *message);

static int callback_websocket(struct lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            logger("WEBSOCKET", LOG_LEVEL_INFO, "Connection established");
            break;

        case LWS_CALLBACK_RECEIVE: {
            logger("WEBSOCKET", LOG_LEVEL_INFO, "Received data");

            char *received_data = (char *)in;
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Received raw data: %s", received_data);

            cJSON *json = cJSON_Parse(received_data);
            if (json == NULL) {
                logger("WEBSOCKET", LOG_LEVEL_ERROR, "Invalid JSON format");
                return -1;
            }

            // oneM2MPrimitive 초기화
            oneM2MPrimitive o2pt;
            memset(&o2pt, 0, sizeof(oneM2MPrimitive));

            // JSON 데이터에서 필요한 필드 추출
            cJSON *op = cJSON_GetObjectItem(json, "op");
            cJSON *to = cJSON_GetObjectItem(json, "to");
            cJSON *rqi = cJSON_GetObjectItem(json, "rqi");
            cJSON *fr = cJSON_GetObjectItem(json, "fr");
            cJSON *ty = cJSON_GetObjectItem(json, "ty");
            cJSON *pc = cJSON_GetObjectItem(json, "pc");
            cJSON *rvi = cJSON_GetObjectItem(json, "rvi");

            // 필수 필드 체크
            if (!op || !to || !rqi || !ty || !pc) {
                logger("WEBSOCKET", LOG_LEVEL_ERROR, "Mandatory fields missing");
                cJSON_Delete(json);
                return -1;
            }

            o2pt.op = op->valueint;

            // 'to' 필드 처리
            const char *to_str = to->valuestring;
            if (to_str[0] == '/') {
                if (to_str[1] == '~') {
                    o2pt.to = (char *)calloc((strlen(to_str)), sizeof(char));
                    o2pt.to[0] = '/';
                    strcat(o2pt.to, to_str + 3);
                } else if (to_str[1] == '_') {
                    o2pt.to = (char *)calloc((strlen(to_str)), sizeof(char));
                    o2pt.to[0] = '/';
                    o2pt.to[1] = '/';
                    strcat(o2pt.to, to_str + 3);
                } else {
                    o2pt.to = strdup(to_str + 1);
                }
            } else {
                o2pt.to = strdup(to_str);
            }

            o2pt.rqi = strdup(rqi->valuestring);
            o2pt.ty = ty->valueint;
            o2pt.request_pc = cJSON_Duplicate(pc, 1); // 'pc' 필드의 깊은 복사

            // 'rvi' 필드 처리: 존재하지 않을 경우 기본값 설정
            if (rvi) {
            o2pt.rvi = strdup(rvi->valuestring);
            } else {
            o2pt.rvi = strdup("3"); // 기본값을 "3"으로 설정
            }

            // 'fr' 필드 처리
            if (fr && cJSON_IsString(fr)) {
                o2pt.fr = strdup(fr->valuestring);
            } else {
                o2pt.fr = strdup("CAdmin");
            }

            // 파싱된 값들 로그 출력
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Parsed request: op=%d, to=%s, rqi=%s, ty=%d ", o2pt.op, o2pt.to, o2pt.rqi, o2pt.ty);
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Parsed content: \n%s", cJSON_Print(o2pt.request_pc));

            // 기존 함수로 요청 처리
            RTNode *target_rtnode = find_rtnode(o2pt.to);
            if (!target_rtnode) {
                logger("WEBSOCKET", LOG_LEVEL_ERROR, "Target resource node not found: %s", o2pt.to);
                cJSON_Delete(json);
                return -1;
            }

            int flag = handle_onem2m_request(&o2pt, target_rtnode);
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Flag: %d", flag);
           //if (handle_onem2m_request(&o2pt, target_rtnode) != "20001") {
                //logger("WEBSOCKET", LOG_LEVEL_ERROR, "Failed to handle request");
                //cJSON_Delete(json);
                //return -1;
            //}

            // 응답 메시지 생성 및 전송
            //char *response_message = cJSON_Print(o2pt.response_pc);
            //send_websocket_message(wsi, response_message);
            //free(response_message);

            if (o2pt.response_pc != NULL) {
                char *response_message = cJSON_Print(o2pt.response_pc);
                send_websocket_message(wsi, response_message);
                free(response_message);
            } else {
                logger("WEBSOCKET", LOG_LEVEL_ERROR, "response_pc is NULL");
            }

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

void send_websocket_message(struct lws *wsi, const char *message) {
    size_t message_length = strlen(message);
    unsigned char *buffer = (unsigned char *)malloc(LWS_PRE + message_length);
    memcpy(&buffer[LWS_PRE], message, message_length);

    lws_write(wsi, &buffer[LWS_PRE], message_length, LWS_WRITE_TEXT);
    free(buffer);
}
