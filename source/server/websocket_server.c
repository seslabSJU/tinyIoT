#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include "cJSON.h"
#include "onem2m.h"
#include "logger.h"
#include "websocket_server.h"
#include "rtManager.h"

void send_websocket_message(struct lws *wsi, const char *message);
void response_create(oneM2MPrimitive *o2pt, cJSON *resource_obj, const char *resource_key);
RTNode* find_created_rtnode(const char *parent_ri, const char *child_ri);
int response_delete(oneM2MPrimitive *o2pt);

static int callback_websocket(struct lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            logger("WEBSOCKET", LOG_LEVEL_INFO, "Connection established");
            break;

        case LWS_CALLBACK_RECEIVE: {
            // logger("WEBSOCKET", LOG_LEVEL_INFO, "Received data");

            char *received_data = (char *)in;
            //logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Received raw data: %s\n", received_data);

            cJSON *json = cJSON_Parse(received_data);
            if (json == NULL) { 
                logger("WEBSOCKET", LOG_LEVEL_ERROR, "Invalid JSON format");
                return -1;
            }

            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Received raw data: %s\n", received_data);

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
            //if (!op || !to || !rqi || !ty || !pc) {
                //logger("WEBSOCKET", LOG_LEVEL_ERROR, "Mandatory fields missing");
                //cJSON_Delete(json);
                //return -1;
            //}

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

            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "to: %s", o2pt.to);

            o2pt.rqi = strdup(rqi->valuestring);
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "rqi: %s", o2pt.rqi);

            // ty 필드 처리 : 존재할 경우 값 설정
            if (ty){
                o2pt.ty = ty->valueint;
                logger("WEBSOCKET", LOG_LEVEL_DEBUG, "ty: %d", o2pt.ty);
            }

        
            // 'pc' 필드 처리 : 존재할 경우 값 설정
            if(pc){
                o2pt.request_pc = cJSON_Duplicate(pc, 1); // 'pc' 필드 복사
                logger("WEBSOCKET", LOG_LEVEL_DEBUG, "pc: %s", cJSON_Print(o2pt.request_pc));
            }

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

            // 기존 함수로 생성 요청 처리
            RTNode *target_rtnode = find_rtnode(o2pt.to);

            if (!target_rtnode) {
                logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Target resource node not found: %s", o2pt.to);
                cJSON_Delete(json);
                return -1;
            }

            int flag = handle_onem2m_request(&o2pt, target_rtnode);
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Flag: %d", flag);


            // rsc가 2001인 경우 응답 필드를 조합
            if (o2pt.rsc == 2001) {
                const char *resource_key = NULL;
                switch (o2pt.ty) {
                    case 1:
                        resource_key = "m2m:acp";
                        break;
                    case 2:
                        resource_key = "m2m:ae";
                        break;
                    case 3:
                        resource_key = "m2m:cnt";
                        break;
                    case 4:
                        resource_key = "m2m:cin";
                        break;
                    case 5:
                        resource_key = "m2m:cse";
                        break;
                    case 9:
                        resource_key = "m2m:grp";
                        break;
                    case 16:
                        resource_key = "m2m:csr";
                        break;
                    case 23:
                        resource_key = "m2m:sub";
                        break;
                }

                if (resource_key) {
                    cJSON *resource = cJSON_GetObjectItem(o2pt.request_pc, resource_key);
                    const char *rn = cJSON_GetObjectItem(resource, "rn")->valuestring;
                    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Resource name: %s", rn);
                    
                    char created_ri[256];
                    snprintf(created_ri, sizeof(created_ri), "%s/%s", o2pt.to, rn);
                    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Created resource path: %s", created_ri);


                    RTNode *created_node = find_rtnode(created_ri);
                    if (created_node && created_node->obj) {
                        response_create(&o2pt, created_node->obj, resource_key);
                    } else {
                        logger("WEBSOCKET", LOG_LEVEL_ERROR, "Created node or resource key not found");
                    }
                }
            }


            // rsc가 2002인 경우 응답 필드를 조합
            if (o2pt.rsc == 2002) {
                response_delete(&o2pt);
            }

            if (o2pt.rsc == 2004){
                const char *resource_key = NULL;
                switch (o2pt.ty) {
                    case 0:
                        resource_key = "m2m:ae";
                        break;
                    case 1:
                        resource_key = "m2m:acp";
                        break;
                    case 2:
                        resource_key = "m2m:ae";
                        break;
                    case 3:
                        resource_key = "m2m:cnt";
                        break;
                    case 4:
                        resource_key = "m2m:cin";
                        break;
                    case 5:
                        resource_key = "m2m:cse";
                        break;
                    case 9:
                        resource_key = "m2m:grp";
                        break;
                    case 16:
                        resource_key = "m2m:csr";
                        break;
                    case 23:
                        resource_key = "m2m:sub";
                        break;
                }


                logger("WEBSOCKET", LOG_LEVEL_DEBUG, "resource_key: %s", resource_key);
                logger("WEBSOCKET", LOG_LEVEL_DEBUG, "to: %s",  o2pt.to);

                if (resource_key) {
                    //logger("WEBSOCKET", LOG_LEVEL_DEBUG, "체크");
                    //cJSON *resource = cJSON_GetObjectItem(o2pt.request_pc, resource_key);
                    //const char *rn = cJSON_GetObjectItem(resource, "rn")->valuestring;
                    //logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Resource name: %s", rn);
                    
                    //char created_ri[256];
                    //snprintf(created_ri, sizeof(created_ri), "%s/%s", o2pt.to, rn);
                    //logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Created resource path: %s", created_ri);


                    RTNode *created_node = find_rtnode(o2pt.to);
                    if (created_node && created_node->obj) {
                        response_update(&o2pt, created_node->obj, resource_key);
                    } else {
                        logger("WEBSOCKET", LOG_LEVEL_ERROR, "Created node or resource key not found");
                    }
                }
            }



            // 응답 메시지 생성 및 전송
            if (o2pt.response_pc != NULL) {
                //logger("WEBSOCKET", LOG_LEVEL_DEBUG, "응답 메세지 : Response message: %s", cJSON_PrintUnformatted(o2pt.response_pc));
                char *response_message = cJSON_PrintUnformatted(o2pt.response_pc);
                send_websocket_message(wsi, response_message);
                free(response_message);
            } else {
                logger("WEBSOCKET", LOG_LEVEL_INFO, "response_pc is NULL");
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

        cJSON_AddItemToObject(pc, resource_key, resource);
        cJSON_AddItemToObject(response, "pc", pc);
    } else {
        logger("RESPONSE", LOG_LEVEL_ERROR, "Resource object is NULL");
    }

    o2pt->response_pc = response;
    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "create 응답 메세지 : %s", cJSON_PrintUnformatted(response));
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
    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "update 응답 메세지 : %s", cJSON_PrintUnformatted(response));
}


int response_delete(oneM2MPrimitive *o2pt) {
    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "rsc", o2pt->rsc);  // 응답 상태 코드
    cJSON_AddStringToObject(response, "rqi", o2pt->rqi);  // 요청 ID
    cJSON_AddStringToObject(response, "rvi", o2pt->rvi);  // 버전
    cJSON_AddStringToObject(response, "pc", o2pt->request_pc);  // 요청 ID

    o2pt->response_pc = response;
    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "delete 응답 메세지 : %s", cJSON_PrintUnformatted(response));
    return 0;
}
