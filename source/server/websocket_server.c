#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include "cJSON.h"
#include "onem2m.h" 
#include "logger.h"
#include "websocket_server.h"
#include "util.h"

// Function prototypes
void send_websocket_message(struct lws *wsi, const char *message);
void response_create(oneM2MPrimitive *o2pt, cJSON *resource_obj, const char *resource_key);
int response_delete(oneM2MPrimitive *o2pt);
void response_update(oneM2MPrimitive *o2pt, cJSON *resource_obj, const char *resource_key);
void route(oneM2MPrimitive *o2pt);
char *find_x_m2m_origin(const char *data);


#define MAX_CLIENTS 64

static struct lws* clients[MAX_CLIENTS];
static int client_count = 0;

//전방 클라이언트들에게 json 발사!!
static void broadcast_message(const char* msg)
{
    for (int i = 0; i < client_count; i++) {
        if (clients[i]) {
            size_t len = strlen(msg);
            unsigned char buf[LWS_PRE + 1024];
            unsigned char* p = &buf[LWS_PRE];
            memcpy(p, msg, len);
            lws_write(clients[i], p, len, LWS_WRITE_TEXT);
        }
    }
}

static char* get_latest_content_instance(const char* path)
{
    // TODO: 실제 oneM2M에서 cin 값 가져오기
    oneM2MPrimitive* o2pt = (oneM2MPrimitive*)calloc(1, sizeof(oneM2MPrimitive));
    o2pt->to = strdup(path);
    o2pt->ty = 4;
    o2pt->rvi = 3;
    o2pt->rqi = strdup("req12347");
    o2pt->op = 2;
    o2pt->rcn = RCN_ATTRIBUTES_AND_CHILD_RESOURCES;
    o2pt->fr = strdup("CAdmin");
    route(o2pt);
    cJSON* response = o2pt_to_json(o2pt);
    
    
    char* st_internal = cJSON_GetObjectItem(
        cJSON_GetObjectItem(
            cJSON_GetObjectItem(response, "pc"), "m2m:cin"),
        "con")->valuestring;

    char* st = strdup(st_internal); // strdup로 복사
    free(response);
    free_o2pt(o2pt);                // o2pt 메모리 해제
    return st;
}

// JSON 조합
static char* make_sensor_status_json()
{
    const char* paths[] = {
        "TinyIoT/TinyFarm/Sensors/Temperature/la",
        "TinyIoT/TinyFarm/Sensors/CO2/la"
    };

    /*"TinyIot/TinyFarm/Actuator/LED/la"*/
    const char* keys[] = { "Temperature", "CO2"};
    int count = sizeof(paths) / sizeof(paths[0]);

    cJSON* root = cJSON_CreateObject();

    for (int i = 0; i < count; i++) {
        char* value = get_latest_content_instance(paths[i]);
        cJSON_AddStringToObject(root, keys[i], value);
        free(value);
    }

    char* json_str = cJSON_PrintUnformatted(root); // 메모리 할당됨
    cJSON_Delete(root);
    return json_str; // 호출 후 free() 필요
}


static void PullingRoutine() {
    char* s = make_sensor_status_json();
    broadcast_message(s);
    free(s);
}


static int callback_websocket(struct lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len) {

    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Connection established"); // 웹소켓 연결 수립
            clients[client_count++] = wsi;
            //PullingRoutine();
            break;
        }
        case LWS_CALLBACK_CLOSED:
            logger("WEBSOCKET", LOG_LEVEL_INFO, "Connection closed");
            break;
            for (int i = 0; i < client_count; i++) {
                if (clients[i] == wsi) {
                    clients[i] = clients[--client_count];
                    clients[client_count] = NULL;
                    printf("Client disconnected. Total: %d\n", client_count);
                    break;
                }
            }

        case LWS_CALLBACK_RECEIVE: {
            // 메시지 수신 시
            char *received_data = in;
            *(received_data + len) = '\0';
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Received raw data: \n%s\n", received_data, len); // 여기서 헤더의 정보가 함께 들어옴
            
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
                return -1; //json 아닌 입력 차단
            }

            // // JSON 구조체를 문자열로 변환하여 출력 -> 여기서 헤더 사라짐
            // char *json_string = cJSON_Print(json); 
            // if (json_string != NULL) {
            //     logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Parsed JSON data: %s", json_string);
            //     free(json_string); // 메모리 해제 필요
            // } else {
            //     logger("WEBSOCKET", LOG_LEVEL_ERROR, "Failed to print JSON");
            // }

            // 프리미티브 초기화
            oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(1, sizeof(oneM2MPrimitive));

            // JSON 데이터에서 필요한 필드 추출
            cJSON *op = cJSON_GetObjectItem(json, "op");
            cJSON *to = cJSON_GetObjectItem(json, "to");
            cJSON *rqi = cJSON_GetObjectItem(json, "rqi");
            cJSON *fr = cJSON_GetObjectItem(json, "fr");
            cJSON *ty = cJSON_GetObjectItem(json, "ty");
            
            cJSON *rvi = cJSON_GetObjectItem(json, "rvi");

            cJSON* pc = cJSON_GetObjectItem(json, "pc"); //html에서 body에 해당하는 부분
            // Payload json 파싱
            if (!cJSON_IsObject(pc) && (op->valueint == 1 || op->valueint == 3 || op->valueint == 5)) {
                logger("WEBSOCKET", LOG_LEVEL_ERROR, "Invalid pc format, pc is not json.");
            }

            o2pt->request_pc = cJSON_Duplicate(pc, 1);
            /*logger("WEBSOCKET", LOG_LEVEL_DEBUG, "pc: %s", cJSON_Print(o2pt->request_pc));*/


            // fr 필드가 없으면 WebSocket 헤더에서 가져온 X-M2M-Origin 값을 사용
            if (fr) {
                o2pt->fr = strdup(fr->valuestring);
                logger("WEBSOCKET", LOG_LEVEL_DEBUG, "fr 필드가 있을때 X-M2M-Origin : %s", o2pt->fr);
            } else {
                if (origin) {
                    o2pt->fr = strdup(origin);
                    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "fr 필드가 없을때 X-M2M-Origin : %s ", o2pt->fr);
                }
                else {
                    logger("WEBSOCKET", LOG_LEVEL_WARN, "fr 필드도 origin도 없음");
                    return -1;
                }
                
            }

            // 나머지 필드들 처리
            o2pt->op = op->valueint;
            logger("WEBSOCKET", LOG_LEVEL_INFO, "op: %d", o2pt->op);

        
            // 요청 유형에 따라 rcn 필드 설정
            //switch (o2pt->op) {
            //    case 1: // create
            //    case 2: // retrieve
            //    case 3: // update
            //        o2pt->rcn = RCN_ATTRIBUTES;
            //        break;
            //    case 4: // delete
            //        o2pt->rcn = RCN_NOTHING;
            //        break;
            //}

            switch (o2pt->op) {
                case OP_CREATE:   // 1
                    // 클라이언트가 rcn을 지정했으면 그대로 사용, 없으면 기본값 적용
                    o2pt->rcn = RCN_ATTRIBUTES; // 기본값: 생성 후 리소스 속성만 반환
                    break;

                case OP_RETRIEVE: // 2
                    o2pt->rcn = RCN_ATTRIBUTES_AND_CHILD_RESOURCES;
                    break;

                case OP_UPDATE:   // 3
                    o2pt->rcn = RCN_ATTRIBUTES; // 기본값: 수정된 속성만 반환
                    break;

                case OP_DELETE:   // 4
                    o2pt->rcn = RCN_NOTHING; // 삭제 시 반환할 내용 없음
                    break;

                case OP_DISCOVERY: // 5
                    o2pt->rcn = RCN_DISCOVERY_RESULT_REFERENCES;
                    break;

                default:
                    handle_error(o2pt, RSC_BAD_REQUEST, "Unsupported operation");
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
            if (rvi) o2pt->rvi = to_rvi(rvi->valuestring); // rvi는 문자임
            // if (pc) o2pt->pc = cJSON_Duplicate(pc, 1);

            // 요청 로깅
            /*logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Parsed request: op=%d, to=%s, rqi=%s, ty=%d, rvi=%d",
                    o2pt->op, o2pt->to, o2pt->rqi, o2pt->ty, o2pt->rvi);
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Parsed content: %s", cJSON_Print(o2pt->request_pc));*/

            // 요청 라우팅
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "route 시작전 ty : = %d", o2pt->ty);

            route(o2pt);
            
            cJSON * response = o2pt_to_json(o2pt);
            
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "rsc : %d", o2pt->rsc);
            // 응답 생성
            if (o2pt->rsc == 2001) {  // 생성 요청
                const char *resource_key = get_resource_key(o2pt->ty);

                //신 생성 요청
                if (resource_key) {
                    cJSON* resource = cJSON_GetObjectItem(o2pt->request_pc, resource_key);
                    //        const char* rn = cJSON_GetObjectItem(resource, "rn")->valuestring;
                    //        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Resource name: %s", rn);

                    //        char created_ri[256];
                    //        snprintf(created_ri, sizeof(created_ri), "%s/%s", o2pt->to, rn);
                    //        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Created resource path: %s", created_ri);
                    //        RTNode* created_node = find_rtnode(created_ri);


                    RTNode* parent_node = find_rtnode(o2pt->to);
                    create_onem2m_resource(o2pt, parent_node);
                    response_create(o2pt, parent_node->obj, resource_key);
                    /*if (created_node && created_node->obj) {
                        response_create(o2pt, created_node->obj, resource_key);
                    }
                    else {
                        logger("WEBSOCKET", LOG_LEVEL_ERROR, "Created node or resource key not found");
                    }*/
                }
                //else if (ty == 3) { //cnt 생성 요청

                //    if (resource_key) {
                //        cJSON* resource = cJSON_GetObjectItem(o2pt->request_pc, resource_key);
                //        const char* rn = cJSON_GetObjectItem(resource, "rn")->valuestring;
                //        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Resource name: %s", rn);

                //        char created_ri[256];
                //        snprintf(created_ri, sizeof(created_ri), "%s/%s", o2pt->to, rn);
                //        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Created resource path: %s", created_ri);
                //        RTNode* created_node = find_rtnode(created_ri);
                //        if (created_node && created_node->obj) {
                //            response_create(o2pt, created_node->obj, resource_key);
                //        }
                //        else {
                //            logger("WEBSOCKET", LOG_LEVEL_ERROR, "Created node or resource key not found");
                //        }
                //    }
                //}


                
            } 

            else if (o2pt->rsc == 2000) {  // 조회 요청
                const char *resource_key = get_resource_key(o2pt->ty);
                if (resource_key) {
                    RTNode *retrieved_node = find_rtnode(o2pt->to);
                    if (retrieved_node && retrieved_node->obj) {
                        response_retrieve(o2pt, retrieved_node->obj, resource_key);
                    } else {
                     logger("WEBSOCKET", LOG_LEVEL_ERROR, "Retrieved node or resource key not found");
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
        

            // cJSON_Delete(json);
            free_o2pt(o2pt);  // 프리미티브 메모리 해제
            break;
        }

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

//void initialize_websocket_server() {
//    struct lws_context_creation_info info;
//    memset(&info, 0, sizeof info);
//    info.port = 8081;
//    info.protocols = protocols;
//    info.gid = -1;
//    info.uid = -1;
//
//    lws_set_log_level(LLL_ERR, NULL);
//
//    struct lws_context *context_ws = lws_create_context(&info);
//    if (context_ws == NULL) {
//        logger("WEBSOCKET", LOG_LEVEL_ERROR, "WebSocket(ws) context creation failed");
//        return;
//    }
//
//    logger("WEBSOCKET", LOG_LEVEL_INFO, "WebSocket Server(ws) started on port 8081");
//
//    //wss 설정
//    struct lws_context_creation_info info_wss;
//    memset(&info_wss, 0, sizeof(info_wss));
//    info_wss.port = 8443;
//    info_wss.protocols = protocols;
//    info_wss.gid = -1;
//    info_wss.uid = -1;
//
//    // SSL/TLS 설정 추가
//    info_wss.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT; 
//    info_wss.ssl_cert_filepath = "/root/tinyIoT/source/server/cert.pem"; 
//    info_wss.ssl_private_key_filepath = "/root/tinyIoT/source/server/key.pem"; 
//
//    struct lws_context *context_wss = lws_create_context(&info_wss);
//    if (context_wss == NULL) {
//        logger("WEBSOCKET", LOG_LEVEL_ERROR, "WebSocket (wss) context creation failed");
//        return;
//    }
//
//    logger("WEBSOCKET", LOG_LEVEL_INFO, "WebSocket Server (wss) started on port 8443");
//    
//
//    while (1) {
//        lws_service(context_ws, 1000); //ws
//        lws_service(context_wss, 1000); // wss
//    }
//
//    lws_context_destroy(context_ws);
//    lws_context_destroy(context_wss);
//}

void initialize_websocket_server() {
    struct lws_context_creation_info info;
    //info.extensions = exts;
    memset(&info, 0, sizeof(info));
    info.port = 8081;          // 일반 WS 포트
    info.protocols = protocols; // 미리 정의된 프로토콜 배열
    //info.extensions = lws_get_internal_extensions();  // permessage-deflate 포함
    /*info.options |= LWS_SERVER_OPTION_SKIP_SERVER_PERMESSAGE_DEFLATE;*/
    info.gid = -1;
    info.uid = -1;

    lws_set_log_level(LLL_ERR, NULL);

    // WS context 생성
    struct lws_context* context_ws = lws_create_context(&info);
    if (context_ws == NULL) {
        logger("WEBSOCKET", LOG_LEVEL_ERROR, "WebSocket context creation failed");
        return;
    }

    logger("WEBSOCKET", LOG_LEVEL_INFO, "WebSocket Server started on port 8081");

    // 테스트용으로 WSS/SSL 관련 코드는 제거

    while (1) {
        lws_service(context_ws, 1000); // WS만 서비스
    }

    // 서버 종료 시 context destroy
    lws_context_destroy(context_ws);
}


void send_websocket_message(struct lws *wsi, const char *message) {
    size_t message_length = strlen(message);
    unsigned char *buffer = (unsigned char *)malloc(LWS_PRE + message_length);
    memcpy(&buffer[LWS_PRE], message, message_length);
    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Sending message: %s", &buffer[LWS_PRE]);

    lws_write(wsi, &buffer[LWS_PRE], message_length, LWS_WRITE_TEXT);
    free(buffer);

    usleep(100 * 1000);
}

void response_create(oneM2MPrimitive *o2pt, cJSON *resource_obj, const char *resource_key) {
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "rsc", o2pt->rsc);  // 응답 상태 코드
    
    cJSON_AddStringToObject(response, "rqi", o2pt->rqi);  // 요청 ID
    cJSON_AddStringToObject(response, "rvi", from_rvi(o2pt->rvi));  // 버전

    
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
    cJSON_AddStringToObject(response, "rvi", from_rvi(o2pt->rvi));  // 버전

    
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
    cJSON_AddStringToObject(response, "rvi", from_rvi(o2pt->rvi));  // 버전

    cJSON_AddStringToObject(response, "pc", cJSON_GetStringValue(o2pt->request_pc));

    //cJSON_AddStringToObject(response, "pc", o2pt->request_pc);  // 요청 ID

    o2pt->response_pc = response;
    //logger("WEBSOCKET", LOG_LEVEL_DEBUG, "delete 응답 메세지 : %s", cJSON_PrintUnformatted(response));
    return 0;
}

void response_retrieve(oneM2MPrimitive *o2pt, cJSON *resource_obj, const char *resource_key) {
    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "rsc", o2pt->rsc);  // 응답 상태 코드
    cJSON_AddStringToObject(response, "rqi", o2pt->rqi);  // 요청 ID
    cJSON_AddStringToObject(response, "rvi", from_rvi(o2pt->rvi));  // 버전

    if (resource_obj != NULL) {
        cJSON *resource = cJSON_Duplicate(resource_obj, 1);
        cJSON *pc = cJSON_CreateObject();

        cJSON_AddItemToObject(pc, resource_key, resource);
        cJSON_AddItemToObject(response, "pc", pc);
    } else {
        logger("RESPONSE", LOG_LEVEL_ERROR, "Resource object is NULL");
    }

    o2pt->response_pc = response;
    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "retrieve 응답 메세지 : %s", cJSON_PrintUnformatted(o2pt->response_pc));
}


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
    return NULL; // 못 찾았을 경우 널값을 반환하도록 설정
}
