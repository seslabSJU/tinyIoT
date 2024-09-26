#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include "cJSON.h"
#include "onem2m.h"
#include "logger.h"
#include "websocket_server.h"
#include "rtManager.h"
#include "util.h"

// Function prototypes
void send_websocket_message(struct lws *wsi, const char *message);
void response_create(oneM2MPrimitive *o2pt, cJSON *resource_obj, const char *resource_key);
int response_delete(oneM2MPrimitive *o2pt);
void response_update(oneM2MPrimitive *o2pt, cJSON *resource_obj, const char *resource_key);
void route(oneM2MPrimitive *o2pt);


char *find_x_m2m_origin(const char *data) { // 요청헤더에서 X-M2M-Origin 추출
    const char *origin_start = strstr(data, "X-M2M-Origin:");
    if (origin_start) {
        origin_start += strlen("X-M2M-Origin:");
        while (*origin_start == ' ') origin_start++;  // 공백 스킵
        const char *origin_end = strstr(origin_start, "\r\n");
        if (!origin_end) origin_end = origin_start + strlen(origin_start);  // 마지막 줄일 경우
        size_t origin_len = origin_end - origin_start;
        char *origin = (char *)malloc(origin_len + 1);
        if (origin) {
            strncpy(origin, origin_start, origin_len);
            origin[origin_len] = '\0';
            return origin; // 오리진 반환
        }
    }
    return strdup(""); // 못 찾았을 경우 빈값을 반환하도록 설정
}

static int callback_websocket(struct lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len) {

    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Connection established"); // 웹소켓 연결 수립
            break;
        }

        case LWS_CALLBACK_RECEIVE: {
            // 메시지 수신 시
            char *received_data = (char *)in;
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Received raw data: \n--%s--\n", received_data); // 여기서 헤더의 정보가 함께 들어옴

            // X-M2M-Origin 헤더 추출
            char *origin = find_x_m2m_origin(received_data);
            if (origin) {
                logger("WEBSOCKET", LOG_LEVEL_INFO, "X-M2M-Origin: %s", origin);
            } else {
                logger("WEBSOCKET", LOG_LEVEL_WARN, "X-M2M-Origin 헤더 없음");
            }

            cJSON *json = cJSON_Parse(received_data);
            if (json == NULL) { 
                logger("WEBSOCKET", LOG_LEVEL_ERROR, "Invalid JSON format");
                return -1;
            }

            // JSON 구조체를 문자열로 변환하여 출력 -> 여기서 헤더 사라짐
            char *json_string = cJSON_Print(json); 
            if (json_string != NULL) {
                logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Parsed JSON data: %s", json_string);
                free(json_string); // 메모리 해제 필요
            } else {
                logger("WEBSOCKET", LOG_LEVEL_ERROR, "Failed to print JSON");
            }

            // 프리미티브 초기화
            oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(1, sizeof(oneM2MPrimitive));

            // JSON 데이터에서 필요한 필드 추출
            cJSON *op = cJSON_GetObjectItem(json, "op");
            cJSON *to = cJSON_GetObjectItem(json, "to");
            cJSON *rqi = cJSON_GetObjectItem(json, "rqi");
            cJSON *fr = cJSON_GetObjectItem(json, "fr");
            cJSON *ty = cJSON_GetObjectItem(json, "ty");
            cJSON *pc = cJSON_GetObjectItem(json, "pc");
            cJSON *rvi = cJSON_GetObjectItem(json, "rvi");


            // Payload 먼저 처리    
            if (cJSON_IsObject(pc)) {
                o2pt->request_pc = cJSON_Duplicate(pc, 1);
                logger("WEBSOCKET", LOG_LEVEL_DEBUG, "pc: %s", cJSON_Print(o2pt->request_pc));
            } 

            // fr 필드가 없으면 WebSocket 헤더에서 가져온 X-M2M-Origin 값을 사용
            if (fr) {
                o2pt->fr = strdup(fr->valuestring);
                logger("WEBSOCKET", LOG_LEVEL_DEBUG, "fr 필드가 있을때 : %s", o2pt->fr);
            } else {
                o2pt->fr = strdup(origin);
                logger("WEBSOCKET", LOG_LEVEL_DEBUG, "fr 필드가 없을때 : %s ", o2pt->fr);
            }

            // 나머지 필드들 처리
            o2pt->op = op->valueint;
            logger("WEBSOCKET", LOG_LEVEL_INFO, "op: %d", o2pt->op);

        
            // 요청 유형에 따라 rcn 필드 설정
            switch (o2pt->op) {
                case 1: // create
                case 2: // retrieve
                case 3: // update
                    o2pt->rcn = RCN_ATTRIBUTES;
                    break;
                case 4: // delete
                    o2pt->rcn = RCN_NOTHING;
                    break;
            }

            // to 필드 처리
            if (to) {
                const char *to_str = to->valuestring;
                if (to_str[0] == '/') {
                    if (to_str[1] == '~') {
                        o2pt->to = strdup(to_str + 3);
                    } else if (to_str[1] == '_') {
                        o2pt->to = strdup(to_str + 3);
                    } else {
                        o2pt->to = strdup(to_str + 1);
                    }
                } else {
                    o2pt->to = strdup(to_str);
                }
                logger("WEBSOCKET", LOG_LEVEL_DEBUG, "to: %s", o2pt->to);
            }

        
            if (rqi) o2pt->rqi = strdup(rqi->valuestring);
            if (ty) o2pt->ty = ty->valueint;
            if (rvi) o2pt->rvi = strdup(rvi->valuestring); // rvi는 문자임
            if (pc) o2pt->pc = cJSON_Duplicate(pc, 1);

            // 요청 로깅
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Parsed request: op=%d, to=%s, rqi=%s, ty=%d, rvi=%s",
                    o2pt->op, o2pt->to, o2pt->rqi, o2pt->ty, o2pt->rvi);
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Parsed content: %s", cJSON_Print(o2pt->request_pc));

            // 요청 라우팅
            route(o2pt);
            cJSON * response = o2pt_to_json(o2pt);
            
            // 응답 생성
            if (o2pt->rsc == 2001) {  // 생성 요청
                const char *resource_key = get_resource_key(o2pt->ty);
                if (resource_key) {
                    cJSON *resource = cJSON_GetObjectItem(o2pt->request_pc, resource_key);
                    const char *rn = cJSON_GetObjectItem(resource, "rn")->valuestring;
                    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Resource name: %s", rn);
                    
                    char created_ri[256];
                    snprintf(created_ri, sizeof(created_ri), "%s/%s", o2pt->to, rn);
                    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Created resource path: %s", created_ri);
                    RTNode *created_node = find_rtnode(created_ri);
                    if (created_node && created_node->obj) {
                        response_create(o2pt, created_node->obj, resource_key);
                    } else {
                        logger("WEBSOCKET", LOG_LEVEL_ERROR, "Created node or resource key not found");
                    }
                }
            } 
            else if (o2pt->rsc == 2002) {  // 삭제 요청
                response_delete(o2pt);
            } 
            else if (o2pt->rsc == 2004) {  // 업데이트 요청
                const char *resource_key = get_resource_key(o2pt->ty);
                if (resource_key) {
                    RTNode *created_node = find_rtnode(o2pt->to);
                    if (created_node && created_node->obj) {
                        response_update(o2pt, created_node->obj, resource_key);
                    } else {
                        logger("WEBSOCKET", LOG_LEVEL_ERROR, "Created node or resource key not found");
                    }
                }
            } 
            else if (o2pt->rsc == 4105) {  // 충돌 상태
                char *error_message = cJSON_PrintUnformatted(response);
                send_websocket_message(wsi, error_message);
                free(error_message);
            }
            

            // 응답 메시지 생성 및 전송
            if (o2pt->response_pc != NULL) {
                char *response_message = cJSON_PrintUnformatted(response);
                send_websocket_message(wsi, response_message);
                free(response_message);
            } else {
                logger("WEBSOCKET", LOG_LEVEL_INFO, "response_pc is NULL");
            }
        

            cJSON_Delete(json);
            free(o2pt);  // 프리미티브 메모리 해제
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

    lws_set_log_level(LLL_ERR, NULL);

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
    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Sending message: %s", &buffer[LWS_PRE]);

    lws_write(wsi, &buffer[LWS_PRE], message_length, LWS_WRITE_TEXT);
    free(buffer);
}

void response_create(oneM2MPrimitive *o2pt, cJSON *resource_obj, const char *resource_key) {
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "rsc", o2pt->rsc);  // 응답 상태 코드
    cJSON_AddStringToObject(response, "rqi", o2pt->rqi);  // 요청 ID
    cJSON_AddStringToObject(response, "rvi", o2pt->rvi);  // 버전

    
    if (resource_obj != NULL) {
        cJSON *resource = cJSON_Duplicate(resource_obj, 1);
        cJSON *pc = cJSON_CreateObject();

        //const char *aei = cJSON_GetObjectItem(resource, "ri")->valuestring;
        //cJSON_AddStringToObject(resource, "aei", aei); // aei 필드 추가


        cJSON_AddItemToObject(pc, resource_key, resource);
        cJSON_AddItemToObject(response, "pc", pc);
    } else {
        logger("RESPONSE", LOG_LEVEL_ERROR, "Resource object is NULL");
    }

    o2pt->response_pc = response;
    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "create 응답 메세지 : %s", cJSON_PrintUnformatted(o2pt->response_pc));
}

void response_update(oneM2MPrimitive *o2pt, cJSON *resource_obj, const char *resource_key) {
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "rsc", o2pt->rsc);  // 응답 상태 코드
    cJSON_AddStringToObject(response, "rqi", o2pt->rqi);  // 요청 ID
    cJSON_AddStringToObject(response, "rvi", o2pt->rvi);  // 버전

    
    if (resource_obj != NULL) {
        cJSON *resource = cJSON_Duplicate(resource_obj, 1);
        cJSON *pc = cJSON_CreateObject();

        cJSON_AddItemToObject(pc, resource_key, resource);
        cJSON_AddItemToObject(response, "pc", pc);
    } else {
        logger("RESPONSE", LOG_LEVEL_ERROR, "Resource object is NULL");
    }

    o2pt->response_pc = response;
    //logger("WEBSOCKET", LOG_LEVEL_DEBUG, "update 응답 메세지 : %s", cJSON_PrintUnformatted(response));
}


int response_delete(oneM2MPrimitive *o2pt) {
    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "rsc", o2pt->rsc);  // 응답 상태 코드
    cJSON_AddStringToObject(response, "rqi", o2pt->rqi);  // 요청 ID
    cJSON_AddStringToObject(response, "rvi", o2pt->rvi);  // 버전
    cJSON_AddStringToObject(response, "pc", o2pt->request_pc);  // 요청 ID

    o2pt->response_pc = response;
    //logger("WEBSOCKET", LOG_LEVEL_DEBUG, "delete 응답 메세지 : %s", cJSON_PrintUnformatted(response));
    return 0;
}

