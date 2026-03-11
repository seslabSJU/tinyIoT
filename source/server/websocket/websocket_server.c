#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include "../cJSON.h"
#include "../onem2m.h"
#include "../logger.h"
#include "websocket_server.h"
#include "../util.h"
#include "../wolfmqtt/mqtt_client.h"
#include "../config.h"
#include "../mqttClient.h"

#ifdef ENABLE_WS
struct messageLink
{
    unsigned char buf[2048];
    size_t len;
    struct messageLink* next;
};

struct per_session_data
{
    struct messageLink first;
};


// Function prototypes
void send_websocket_message(struct lws *wsi, const char *message);
void response_create(oneM2MPrimitive *o2pt, cJSON *resource_obj, const char *resource_key);
int response_delete(oneM2MPrimitive *o2pt);
void response_update(oneM2MPrimitive *o2pt, cJSON *resource_obj, const char *resource_key);
void route(oneM2MPrimitive *o2pt);
char *find_x_m2m_origin(const char *data);
void websocket_respond_to_client(oneM2MPrimitive *o2pt, struct per_session_data *pss, struct lws *wsi);

#if MONO_THREAD == 0
#include <pthread.h>
extern pthread_mutex_t ws_lock;
#endif

#define LOG_TAG "WEBSOCKET"

#define MAX_SESSIONS 100

// 프로토콜별 연결 종료 함수 타입 정의 (구독 시스템의 핵심)

typedef struct
{
    RTNode *ae;                    // 유저 식별자
    struct lws *connection_handle; // 실제 소켓이나 세션 객체
    int is_active;
} Session;

// 전역 세션 테이블
Session session_table[MAX_SESSIONS];


// 세션 중복확인 및 종료 함수
void handle_duplicate_session(RTNode *ae)
{
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (session_table[i].is_active && session_table[i].ae == ae)
        {
            // 세션 종료

            char *reason = "WS connection already associated with originator. Closing first connection.";
            // 연결 차단
            lws_close_reason(session_table[i].connection_handle, LWS_CLOSE_STATUS_NORMAL, (unsigned char *)reason, strlen(reason));

            logger("WEBSOCKET", LOG_LEVEL_ERROR, "WS connection already associated with originator. Closing first connection.");

            session_table[i].is_active = 0;
            break;
        }
    }
}

/// @brief adding not admin, not anymouse ae to session table
/// @param role
/// @param wsi
/// @return success 0, fail -1
int add_session_connected(struct lws *wsi, RTNode *ae)
{
    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Check Session-Table");
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (session_table[i].is_active == 0)
        {
            session_table[i].is_active = 1;
            session_table[i].ae = ae;
            session_table[i].connection_handle = wsi;
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "add AE-Session to Session-Table [%s]", ae->uri);
            return 0;
        }
    }
    logger(LOG_TAG, LOG_LEVEL_ERROR, "Session Table is Full");
    return -1;
}

int remove_session_from_Session_Table(struct lws *wsi)
{
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (session_table[i].is_active == 1 && session_table[i].connection_handle == wsi)
        {
            session_table[i].is_active = 0;
            session_table[i].connection_handle = NULL;
            return 0;
        }
    }
    return -1;
}

struct lws *find_wsi_from_session_table(RTNode *rtnode)
{
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (session_table[i].is_active == 1 && session_table[i].ae == rtnode)
        {
            return session_table[i].connection_handle;
        }
    }
    return NULL;
}

int hendle_incomming_connection(struct lws *wsi, oneM2MPrimitive *o2pt)
{
    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Is Comming");

    if (o2pt->fr && !strcmp(o2pt->fr, ADMIN_AE_ID))
    {
        // 어드민이므로 연결 해줌
        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "ADMIN_AE_ID : %s", o2pt->fr);
        return 0;
    }
    else if (o2pt->fr == NULL || strlen(o2pt->fr) == 0 || strcmp(o2pt->fr, "C") == 0)
    {
        // 익명이므로 그냥 연결 해줌
        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "anonymity User");
        return 0;
    }
    else
    {

        RTNode *rtnode = find_rtnode(o2pt->fr);

        if(rtnode==NULL){
            // 틀린 권한 연결 차단
            // 차단 이유

            // 연결 차단
            logger("WEBSOCKET", LOG_LEVEL_ERROR, "등록되지 않은 사용자는 익명취급");
            // lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, "등록되지 않은 사용자", strlen("등록되지 않은 사용자"));
            return 0;
        }


        if (check_privilege(o2pt, rtnode, ACOP_CREATE) == 0)
        {
            // 연결 성공 세션 테이블에 등록
            handle_duplicate_session(rtnode);

            if (add_session_connected(wsi, rtnode) != 0)
            {
                // 세션테이블에 추가 실패 연결 차단
                // 차단 이유
                char *reason = "{ Fail to add AE-Session to Session-Table. }";
                logger("WEBSOCKET", LOG_LEVEL_ERROR, "Fail to add AE-Session to Session-Table");

                // 연결 차단
                lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, (unsigned char *)reason, strlen(reason));
                return -1;
            }
            return 0;
        }
        else
        {
            // 틀린 권한 연결 차단
            // 차단 이유

            // 연결 차단
            logger("WEBSOCKET", LOG_LEVEL_ERROR, "Privilege 없는 사용자 익명취급?");
            return 0;
        }
    }
}

// struct tuple_lws_and_wsi{
//     struct lws* wsi;
//     char* content;
// };
// struct wsi_list{
//     int count;
//     int max;

//     struct tuple_lws_and_wsi* tuple_list;
// };

// noti
static int interrupted = 0;
static char *noti_json = NULL;
static int rsc_result = 0;

void *websocket_server_thread(void *arg)
{
    initialize_websocket_server();
}

static char *get_latest_content_instance(const char *path)
{
    // TODO: 실제 oneM2M에서 cin 값 가져오기
    oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(1, sizeof(oneM2MPrimitive));
    o2pt->to = strdup(path);
    o2pt->ty = 4;
    o2pt->rvi = 3;
    o2pt->rqi = strdup("req12347");
    o2pt->op = 2;
    o2pt->rcn = RCN_ATTRIBUTES_AND_CHILD_RESOURCES;
    o2pt->fr = strdup("CAdmin");
    route(o2pt);
    cJSON *response = o2pt_to_json(o2pt);

    char *st_internal = cJSON_GetObjectItem(
                            cJSON_GetObjectItem(
                                cJSON_GetObjectItem(response, "pc"), "m2m:cin"),
                            "con")
                            ->valuestring;

    char *st = strdup(st_internal); // copy by strdup
    free(response);
    free_o2pt(o2pt); // o2pt memory free
    return st;
}

static int callback_websocket(struct lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len)
{
    // logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Websocket callback: %d", reason);
    struct per_session_data *pss = (struct per_session_data *)user;

    switch (reason)
    {

        case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
        {   
            const char *target_key = "x-m2m-origin:"; // 반드시 끝에 ':' 포함
            char aei[256];
            // 연결 승인 전 헤더에서 AEI 추출
            if (lws_hdr_custom_copy(wsi, aei, sizeof(aei), target_key, strlen(target_key)) == -1)
            {
                aei[0] = '\0';
            }
            oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(1, sizeof(oneM2MPrimitive));
            o2pt->fr = aei;
            
            int n = hendle_incomming_connection(wsi, o2pt);
            if(n!=0){
                //실패!
            } else {
                lws_callback_on_writable(wsi);
                break;
            }
            
        }
    case LWS_CALLBACK_SERVER_WRITEABLE:
    {

        struct messageLink* message = pss->first.next;
        while(message!=NULL){
            
            int n = lws_write(wsi, message->buf + LWS_PRE, message->len, LWS_WRITE_TEXT);

            if (n < 0)
            {
                // 전송 에러 발생 시 연결 종료
                return -1;
            }
            else if (n < (int)message->len)
            {
                // 'Partial Write' 발생 시: 다 못 보낸 만큼 버퍼를 밀어주고 다시 요청해야 함
                // (이 로직은 복잡해지므로 구현은 미루고 일단 로그를 남깁시다)
                logger("WEBSOCKET", LOG_LEVEL_ERROR, "Partial write occurred"); // 웹소켓 연결 수립
            }

            struct messageLink* tempMessage = message->next;
            free(message);
            message = tempMessage;
        }

        pss->first.next = NULL;
        break;
    }

    case LWS_CALLBACK_RECEIVE:
    {
        // 메시지 수신 시

        // 프리미티브 초기화
        oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(1, sizeof(oneM2MPrimitive));
        if (!o2pt)
        {
            logger("WEBSOCKET", LOG_LEVEL_ERROR, "Error: Memory allocation failed!");
            break;
        }

        char *received_data = in;
        *(received_data + len) = '\0';

        cJSON *root = cJSON_ParseWithLength(received_data, len);
        if (!root)
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "Invalid JSON format");
            websocket_respond_to_client(o2pt, pss, wsi); // 에러 발생 시 즉시 응답
            free_o2pt(o2pt);
            break;
        }

        // fr 파싱
        // fr이 없을 수 있는데 그렇다면 X-m2m-Origin 값 사용
        cJSON *fr = cJSON_GetObjectItem(root, "fr");
        if (fr && cJSON_IsString(fr))
        {
            o2pt->fr = strdup(fr->valuestring);
        }
        else
        {

            char origin[64];
            if (lws_hdr_custom_copy(wsi, origin, sizeof(origin), "x-m2m-origin:", 12) == -1)
            {
                origin[0] = '\0';
            }

            o2pt->fr = origin;
        }
        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "fr : %s ", o2pt->fr);

        cJSON *rqi = cJSON_GetObjectItem(root, "rqi");
        if (rqi && cJSON_IsNumber(rqi))
        {
            o2pt->rqi = strdup(rqi->valuestring);
        }

        cJSON *to = cJSON_GetObjectItem(root, "to");
        if (to && cJSON_IsString(to))
        {
            // o2pt->to = strdup(to->valuestring);

            // to 필드 처리
            const char *to_str = to->valuestring;
            if (to_str[0] == '/')
            {
                if (to_str[1] == '~')
                {
                    o2pt->to = strdup(to_str + 3);
                }
                else if (to_str[1] == '_')
                {
                    o2pt->to = strdup(to_str + 3);
                }
                else
                {
                    o2pt->to = strdup(to_str + 1);
                }
            }
            else
            {
                o2pt->to = strdup(to_str);
            }
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "to: %s", o2pt->to);
        }

        cJSON *op = cJSON_GetObjectItem(root, "op");
        
        if(!op){
            //노티 ws세션 응답을 수신했을때 op는 없지만 recive로 들어온다
            cJSON_Delete(root);
            free_o2pt(o2pt);
            return -1;
        }


        if (op && cJSON_IsNumber(op))
        {
            o2pt->op = (Operation)op->valueint;
        }

        cJSON *ty = cJSON_GetObjectItem(root, "ty");
        if (ty && cJSON_IsNumber(ty))
        {
            o2pt->ty = ty->valueint;
        }

        // 8. Primitive Content (pc) 파싱
        cJSON *pc = cJSON_GetObjectItem(root, "pc");
        if (pc)
        {
            // pc 내부의 실제 리소스 데이터를 o2pt->request_pc에 저장
            o2pt->request_pc = cJSON_Duplicate(pc, 1);
        }

        // 9. Release Version Indicator (rvi) 파싱
        cJSON *rvi = cJSON_GetObjectItem(root, "rvi");
        if (rvi && cJSON_IsString(rvi))
        {
            o2pt->rvi = to_rvi(rvi->valuestring);
        }
        else
        {
            o2pt->rvi = RVI_NONE;
        }

        o2pt->errFlag = false;

        // 요청 유형에 따라 rcn 필드 설정
        // switch (o2pt->op) {
        //    case 1: // create
        //    case 2: // retrieve
        //    case 3: // update
        //        o2pt->rcn = RCN_ATTRIBUTES;
        //        break;
        //    case 4: // delete
        //        o2pt->rcn = RCN_NOTHING;
        //        break;
        //}

        switch (o2pt->op)
        {
        case OP_CREATE:   // 1
        case OP_RETRIEVE: // 2
        case OP_UPDATE:   // 3
            // 클라이언트가 rcn을 지정했으면 그대로 사용, 없으면 기본값 적용
            o2pt->rcn = RCN_ATTRIBUTES; // 기본값: 생성 후 리소스 속성만 반환
            break;

            // o2pt->rcn = RCN_ATTRIBUTES_AND_CHILD_RESOURCES;
            // break;

            // o2pt->rcn = RCN_ATTRIBUTES; // 기본값: 수정된 속성만 반환
            // break;

        case OP_DELETE:              // 4
            o2pt->rcn = RCN_NOTHING; // 삭제 시 반환할 내용 없음
            break;

        case OP_DISCOVERY: // 5
            o2pt->rcn = RCN_DISCOVERY_RESULT_REFERENCES;
            break;

        default:
            handle_error(o2pt, RSC_BAD_REQUEST, "Unsupported operation");
            break;
        }

        route(o2pt);

        // cJSON * response = o2pt_to_json(o2pt);
        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "rsc : %d", o2pt->rsc);

        websocket_respond_to_client(o2pt, pss, wsi);
        // 응답 생성
        // if (o2pt->rsc == 2001) {  // 생성 요청
        //     const char *resource_key = get_resource_key(o2pt->ty);

        //     //신 생성 요청
        //     if (resource_key) {
        //         cJSON* resource = cJSON_GetObjectItem(o2pt->request_pc, resource_key);
        //         //        const char* rn = cJSON_GetObjectItem(resource, "rn")->valuestring;
        //         //        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Resource name: %s", rn);

        //         //        char created_ri[256];
        //         //        snprintf(created_ri, sizeof(created_ri), "%s/%s", o2pt->to, rn);
        //         //        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Created resource path: %s", created_ri);
        //         //        RTNode* created_node = find_rtnode(created_ri);

        //         RTNode* parent_node = find_rtnode(o2pt->to);
        //         create_onem2m_resource(o2pt, parent_node);
        //         response_create(o2pt, parent_node->obj, resource_key);
        //         /*if (created_node && created_node->obj) {
        //             response_create(o2pt, created_node->obj, resource_key);
        //         }
        //         else {
        //             logger("WEBSOCKET", LOG_LEVEL_ERROR, "Created node or resource key not found");
        //         }*/
        //     }
        //     //else if (ty == 3) { //cnt 생성 요청

        //     //    if (resource_key) {
        //     //        cJSON* resource = cJSON_GetObjectItem(o2pt->request_pc, resource_key);
        //     //        const char* rn = cJSON_GetObjectItem(resource, "rn")->valuestring;
        //     //        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Resource name: %s", rn);

        //     //        char created_ri[256];
        //     //        snprintf(created_ri, sizeof(created_ri), "%s/%s", o2pt->to, rn);
        //     //        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Created resource path: %s", created_ri);
        //     //        RTNode* created_node = find_rtnode(created_ri);
        //     //        if (created_node && created_node->obj) {
        //     //            response_create(o2pt, created_node->obj, resource_key);
        //     //        }
        //     //        else {
        //     //            logger("WEBSOCKET", LOG_LEVEL_ERROR, "Created node or resource key not found");
        //     //        }
        //     //    }
        //     //}

        // }

        // else if (o2pt->rsc == 2000) {  // 조회 요청
        //     const char *resource_key = get_resource_key(o2pt->ty);
        //     if (resource_key) {
        //         RTNode *retrieved_node = find_rtnode(o2pt->to);
        //         if (retrieved_node && retrieved_node->obj) {
        //             response_retrieve(o2pt, retrieved_node->obj, resource_key);
        //         } else {
        //          logger("WEBSOCKET", LOG_LEVEL_ERROR, "Retrieved node or resource key not found");
        //         }
        //     }
        // }

        // else if (o2pt->rsc == 2002) {  // 삭제 요청
        //     response_delete(o2pt);
        // }
        // else if (o2pt->rsc == 2004) {  // 업데이트 요청
        //     const char *resource_key = get_resource_key(o2pt->ty);
        //     if (resource_key) {
        //         RTNode *created_node = find_rtnode(o2pt->to);
        //         if (created_node && created_node->obj) {
        //             response_update(o2pt, created_node->obj, resource_key);
        //         } else {
        //             logger("WEBSOCKET", LOG_LEVEL_ERROR, "Created node or resource key not found");
        //         }
        //     }
        // }
        // else if (o2pt->rsc == 4105) {  // 충돌 상태
        //     char *error_message = cJSON_PrintUnformatted(response);
        //     send_websocket_message(wsi, error_message);
        //     free(error_message);
        // }

        // // 응답 메시지 생성 및 전송
        // if (o2pt->response_pc != NULL) {
        //     char *response_message = cJSON_PrintUnformatted(response);
        //     send_websocket_message(wsi, response_message);
        //     free(response_message);
        // } else {
        //     logger("WEBSOCKET", LOG_LEVEL_INFO, "response_pc is NULL");
        // }
        cJSON_Delete(root);
        free_o2pt(o2pt);
    }
    break;

    case LWS_CALLBACK_CLOSED:
        logger("WEBSOCKET", LOG_LEVEL_INFO, "Connection closed");
        remove_session_from_Session_Table(wsi);
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
        sizeof(struct per_session_data),
        2048,
    },
    {NULL, NULL, 0, 0} /* end */
};

void initialize_websocket_server()
{
    struct lws_context_creation_info info;
    // info.extensions = exts;
    memset(&info, 0, sizeof(info));
    info.port = WS_PORT;        // 일반 WS 포트
    info.protocols = protocols; // 미리 정의된 프로토콜 배열
    // info.extensions = lws_get_internal_extensions();  // permessage-deflate 포함
    /*info.options |= LWS_SERVER_OPTION_SKIP_SERVER_PERMESSAGE_DEFLATE;*/

    info.gid = -1;
    info.uid = -1;
    info.max_http_header_data = 1024;
    info.max_http_header_pool = 64;

    lws_set_log_level(LLL_ERR, NULL);

    // WS context 생성
    struct lws_context *context_ws = lws_create_context(&info);
    if (context_ws == NULL)
    {
        logger("WEBSOCKET", LOG_LEVEL_ERROR, "WebSocket context creation failed");
        return;
    }

    logger("WEBSOCKET", LOG_LEVEL_INFO, "WebSocket Server started on port %d", info.port);

    while (1)
    {
        lws_service(context_ws, 1000); // WS만 서비스
    }

    // 서버 종료 시 context destroy
    lws_context_destroy(context_ws);
}

void websocket_respond_to_client(oneM2MPrimitive *o2pt, struct per_session_data *pss, struct lws *wsi)
{

    struct messageLink* message = &(pss->first);
        while(message->next!=NULL){
            message = message->next;
        }
    message->next = (struct messageLink*)malloc(sizeof(struct messageLink));

    cJSON *response = o2pt_to_json(o2pt);
    // logger("WEBSOCKET", LOG_LEVEL_DEBUG, "3: %d", o2pt->rsc);

    if (o2pt->rsc == 2001)
    { // 생성 요청
        const char *resource_key = get_resource_key(o2pt->ty);
        if (resource_key)
        {
            cJSON *resource = cJSON_GetObjectItem(o2pt->request_pc, resource_key);
            const char *rn = cJSON_GetObjectItem(resource, "rn")->valuestring;
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Resource name: %s", rn);

            char created_ri[256];
            snprintf(created_ri, sizeof(created_ri), "%s/%s", o2pt->to, rn);
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Created resource path: %s", created_ri);
            RTNode *created_node = find_rtnode(created_ri);
            if (created_node && created_node->obj)
            {
                response_create(o2pt, created_node->obj, resource_key);
            }
            else
            {
                logger("WEBSOCKET", LOG_LEVEL_ERROR, "Created node or resource key not found");
            }
        }
        char* temp = cJSON_PrintUnformatted(o2pt->response_pc); 
        message->next->len = strlen(temp);
        strcpy(message->next->buf + LWS_PRE, temp);
        free(temp);
    }

    else if (o2pt->rsc == 2000)
    { // 조회 요청

        const char *resource_key = get_resource_key(o2pt->ty);
        if (resource_key)
        {
            RTNode *retrieved_node = find_rtnode(o2pt->to);
            if (retrieved_node && retrieved_node->obj)
            {
                response_retrieve(o2pt, retrieved_node->obj, resource_key);
            }
            else
            {
                logger("WEBSOCKET", LOG_LEVEL_ERROR, "Retrieved node or resource key not found");
            }
        }
        char* temp = cJSON_PrintUnformatted(o2pt->response_pc); 
        message->next->len = strlen(temp);
        strcpy(message->next->buf + LWS_PRE, temp);
        free(temp);
    }

    else if (o2pt->rsc == 2002)
    { // 삭제 요청
        // const char *resource_key = get_resource_key(o2pt->ty);
        // cJSON *resource = cJSON_GetObjectItem(o2pt->request_pc, resource_key);
        response_delete(o2pt);
        char* temp = cJSON_PrintUnformatted(o2pt->response_pc); 
        message->next->len = strlen(temp);
        strcpy(message->next->buf + LWS_PRE, temp);
        free(temp);
    }
    else if (o2pt->rsc == 2004)
    { // 업데이트 요청
        const char *resource_key = get_resource_key(o2pt->ty);
        if (resource_key)
        {
            RTNode *created_node = find_rtnode(o2pt->to);
            if (created_node && created_node->obj)
            {
                response_update(o2pt, created_node->obj, resource_key);
            }
            else
            {
                logger("WEBSOCKET", LOG_LEVEL_ERROR, "Created node or resource key not found");
            }
        }
        char* temp = cJSON_PrintUnformatted(o2pt->response_pc); 
        message->next->len = strlen(temp);
        strcpy(message->next->buf + LWS_PRE, temp);
        free(temp);
    }
    else if (o2pt->rsc == 4105)
    { // 충돌 상태
        char *error_message = cJSON_PrintUnformatted(response);

        
        message->next->len = strlen(error_message);
        strcpy(message->next->buf + LWS_PRE, error_message);

        free(error_message);
    }else if (o2pt != NULL && o2pt->response_pc != NULL)
    {
        char *responString = cJSON_PrintUnformatted(response);

        message->next->len = strlen(responString);
        strcpy(message->next->buf + LWS_PRE, responString);
        free(responString);
    }
    else
    {
        // 비표준 입력상태
        char *error_message = cJSON_PrintUnformatted(response);
        

        message->next->len = strlen(error_message);
        strcpy(message->next->buf + LWS_PRE, error_message);
        free(error_message);
    }
    printf("웹소켓 응답 o2pt->rsc[%d], o2pt->ty[%d]\n", o2pt->rsc, o2pt->ty);
    lws_callback_on_writable(wsi);

    if (response != NULL)
        cJSON_Delete(response);
}

void response_create(oneM2MPrimitive *o2pt, cJSON *resource_obj, const char *resource_key)
{

    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "rsc", o2pt->rsc);           // 응답 상태 코드
    cJSON_AddStringToObject(response, "rqi", o2pt->rqi);           // 요청 ID
    cJSON_AddStringToObject(response, "rvi", from_rvi(o2pt->rvi)); // 버전

    if (resource_obj != NULL)
    {
        cJSON *resource = cJSON_Duplicate(resource_obj, 1);
        cJSON *pc = cJSON_CreateObject();

        // const char *aei = cJSON_GetObjectItem(resource, "ri")->valuestring;
        // cJSON_AddStringToObject(resource, "aei", aei); // aei 필드 추가

        cJSON_AddItemToObject(pc, resource_key, resource);
        cJSON_AddItemToObject(response, "pc", pc);
    }
    else
    {
        logger("RESPONSE", LOG_LEVEL_ERROR, "Resource object is NULL");
    }

    o2pt->response_pc = response;
    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "create 응답 메세지 : %s", cJSON_PrintUnformatted(o2pt->response_pc));
}

void response_update(oneM2MPrimitive *o2pt, cJSON *resource_obj, const char *resource_key)
{

    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "rsc", o2pt->rsc);           // 응답 상태 코드
    cJSON_AddStringToObject(response, "rqi", o2pt->rqi);           // 요청 ID
    cJSON_AddStringToObject(response, "rvi", from_rvi(o2pt->rvi)); // 버전

    if (resource_obj != NULL)
    {
        cJSON *resource = cJSON_Duplicate(resource_obj, 1);
        cJSON *pc = cJSON_CreateObject();

        cJSON_AddItemToObject(pc, resource_key, resource);
        cJSON_AddItemToObject(response, "pc", pc);
    }
    else
    {
        logger("RESPONSE", LOG_LEVEL_ERROR, "Resource object is NULL");
    }

    o2pt->response_pc = response;
    // logger("WEBSOCKET", LOG_LEVEL_DEBUG, "update 응답 메세지 : %s", cJSON_PrintUnformatted(response));
}

int response_delete(oneM2MPrimitive *o2pt)
{
    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "rsc", o2pt->rsc);           // 응답 상태 코드
    cJSON_AddStringToObject(response, "rqi", o2pt->rqi);           // 요청 ID
    cJSON_AddStringToObject(response, "rvi", from_rvi(o2pt->rvi)); // 버전
    cJSON_AddNumberToObject(response, "op", 4); 
    cJSON_AddStringToObject(response, "fr", o2pt->fr);
    cJSON_AddStringToObject(response, "to", o2pt->to);
    o2pt->response_pc = response;
    // logger("WEBSOCKET", LOG_LEVEL_DEBUG, "delete 응답 메세지 : %s", cJSON_PrintUnformatted(response));
    return 0;
}

void response_retrieve(oneM2MPrimitive *o2pt, cJSON *resource_obj, const char *resource_key)
{
    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "rsc", o2pt->rsc);           // 응답 상태 코드
    cJSON_AddStringToObject(response, "rqi", o2pt->rqi);           // 요청 ID
    cJSON_AddStringToObject(response, "rvi", from_rvi(o2pt->rvi)); // 버전
    cJSON_AddStringToObject(response, "fr", o2pt->fr);
    cJSON_AddStringToObject(response, "to", o2pt->to);
    
    if (resource_obj != NULL)
    {
        cJSON *resource = cJSON_Duplicate(resource_obj, 1);
        cJSON *pc = cJSON_CreateObject();

        cJSON_AddItemToObject(pc, resource_key, resource);
        cJSON_AddItemToObject(response, "pc", pc);
    }
    else
    {
        logger("RESPONSE", LOG_LEVEL_ERROR, "Resource object is NULL");
    }

    o2pt->response_pc = response;
    logger("WEBSOCKET", LOG_LEVEL_DEBUG, "retrieve 응답 메세지 : %s", cJSON_PrintUnformatted(o2pt->response_pc));
}

#pragma region Websocket_NOTIFY

void websocket_check_and_forwarding_from_sessionTable(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{
    struct lws *connected_wsi = find_wsi_from_session_table(target_rtnode);
    printf("finded wsi : %d\n", connected_wsi);
    printf("o2pt : %d\n", o2pt);
    printf("o2pt->request_pc : %d\n", o2pt->request_pc);
    if (connected_wsi)
    {
        // 이미 연결된 세션이 있다면 즉시 전송 예약
        cJSON* json = cJSON_CreateObject();
        cJSON_AddNumberToObject(json, "op", 5);
        cJSON_AddStringToObject(json, "fr", o2pt->fr);
        cJSON_AddStringToObject(json, "to", o2pt->to);
        cJSON_AddStringToObject(json, "rqi", o2pt->rqi);
        cJSON_AddStringToObject(json, "rvi", from_rvi(o2pt->rvi));
        cJSON_AddItemToObject(json, "pc", cJSON_Duplicate(o2pt->request_pc, true));

        struct per_session_data *pss = lws_wsi_user(connected_wsi);
        
        char* buffer = cJSON_PrintUnformatted(json);

        struct messageLink* message = &(pss->first);
        while(message->next!=NULL){
            message = message->next;
        }
        
        message->next = (struct messageLink*)malloc(sizeof(struct messageLink));
        message->next->len = strlen(buffer);

        strcpy(message->next->buf + LWS_PRE, buffer);
        cJSON_free(json);
        free(buffer);
        lws_callback_on_writable(connected_wsi);
    }
    
}

static int callback_notify_verify(struct lws *wsi, enum lws_callback_reasons reason,
                                  void *user, void *in, size_t len)
{
    struct notify_result *res = (struct notify_result *)lws_get_opaque_user_data(wsi);

    switch (reason)
    {
    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
    {
        unsigned char **p = (unsigned char **)in, *end = (*p) + len;
        // 필수 헤더 주입
        lws_add_http_header_by_name(wsi, (unsigned char *)"X-M2M-Origin:", (unsigned char *)"/" CSE_BASE_RI, strlen(CSE_BASE_RI) + 1, p, end);
        lws_add_http_header_by_name(wsi, (unsigned char *)"X-M2M-RVI:", (unsigned char *)"3", 1, p, end);
        break;
    }

    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        // 연결 성공 시 전송 가능 상태로 설정
        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Established ");
        lws_callback_on_writable(wsi);
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        size_t msg_len = strlen(res->json_to_send);
        // LWS_PRE 만큼의 여유 공간이 있는 버퍼 준비
        unsigned char *buf = malloc(LWS_PRE + msg_len);
        memcpy(&buf[LWS_PRE], res->json_to_send, msg_len);

        // 데이터 전송 (TEXT 프레임)
        lws_set_timer_usecs(wsi, 200000);
        lws_write(wsi, &buf[LWS_PRE], msg_len, LWS_WRITE_TEXT);
        free(buf);
        
        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Notification body sent.");
        break;
    case LWS_CALLBACK_CLIENT_RECEIVE: {
            // 서버로부터 응답 프레임 수신 (보통 JSON 형태)
            logger("WEBSOCKET", LOG_LEVEL_DEBUG, "Response received: %s", (char *)in);

            cJSON *root = cJSON_Parse((char *)in);
            if (root) {
                // response_pc 내의 rsc 필드 혹은 루트의 rsc 필드 추출
                cJSON *rsc_item = cJSON_GetObjectItem(root, "rsc");
                if (!rsc_item) {
                    // m2m:rsp 형태인 경우 등 구조에 따라 재탐색
                    cJSON *rsp = cJSON_GetObjectItem(root, "m2m:rsp");
                    rsc_item = cJSON_GetObjectItem(rsp, "rsc");
                    res->rsc = rsc_item->valueint;
                } else {
                    res->rsc = rsc_item->valueint;
                }

                cJSON_Delete(root);
            }
            printf("res->rsc : %d\n", res->rsc);
            res->done = 1;
            return -1; // 응답 확인 후 종료
        }
    // 3. 응답이 안 와서 타이머가 터진 경우
    case LWS_CALLBACK_TIMER:
        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "NotiServer Response time out");
        // 여기서 필요한 에러 처리 (로그 기록 등) 수행 후 연결 종료
        res->done = 1;
        return -1;
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        logger("WEBSOCKET", LOG_LEVEL_DEBUG, "NotiServer not exist");
        res->rsc = 4000;
        res->done = 1;
        break;
    case LWS_CALLBACK_CLIENT_CLOSED:
        res->done = 1;
    default:
        break;
    }
    return 0;
}
// // 세션을 유지할 필요가 없으므로 패킷을 한번 보내고, 받고 세션 종료
// int ws_protocol_binding_notify_vertify(oneM2MPrimitive* o2pt, NotiTarget *nt)
// {
//     logger("WEBSOCKET", LOG_LEVEL_DEBUG, "websocket_notify 11");
//     struct lws_context_creation_info info;
//     struct lws_client_connect_info i;
//     // logger("WEBSOCKET", LOG_LEVEL_DEBUG, "websocket_notify fr %d", o2pt->fr);
//     // logger("WEBSOCKET", LOG_LEVEL_DEBUG, "websocket_notify nt->notijson %s", nt->noti_json);
//     // logger("WEBSOCKET", LOG_LEVEL_DEBUG, "websocket_notify to %s", o2pt->to);
//     cJSON* json = cJSON_CreateObject();
//     cJSON_AddNumberToObject(json, "op", 5);
//     cJSON_AddStringToObject(json, "fr", o2pt->fr);
//     cJSON_AddStringToObject(json, "to", o2pt->to);
//     cJSON_AddStringToObject(json, "rsc", o2pt->rsc);
//     cJSON* notiJson = cJSON_Parse(nt->noti_json);
//     cJSON_AddItemToObject(json, "pc", notiJson);
//     cJSON_free(notiJson);

//     struct notify_result res = {json, 1, 0, 0}; // 초기화
//     struct lws_context *context;

//     memset(&info, 0, sizeof(info));
//     info.port = CONTEXT_PORT_NO_LISTEN;
//     info.protocols = (struct lws_protocols[]){
//         {"oneM2M.json", callback_notify_verify, 0, 0},
//         {NULL, NULL, 0, 0}};

//     context = lws_create_context(&info);
    
//     if (!context)
//         return 4000;

//     memset(&i, 0, sizeof(i));
//     i.context = context;
//     i.address = nt->host;
//     i.port = nt->port;
//     i.path = "/";
//     i.host = i.address;
//     i.origin = i.address;
//     i.protocol = "oneM2M.json";
//     i.opaque_user_data = &res;

//     if (!lws_client_connect_via_info(&i))
//     {
//         lws_context_destroy(context);
//         return 4000;
//     }
//     // 서비스 루프 실행
//     while (!res.done)
//     {
//         if (lws_service(context, 100) < 0)
//             break;
//     }

//     lws_context_destroy(context);
//     cJSON_free(json);
//     return (res.rsc != 0 ? res.rsc : 4000);
// }

// 세션을 유지할 필요가 없으므로 패킷을 한번 보내고, 받고 세션 종료
int ws_protocol_binding_notify(oneM2MPrimitive *o2pt, NotiTarget *nt)
{
    struct lws_context_creation_info info;
    struct lws_client_connect_info i;
    // logger("WEBSOCKET", LOG_LEVEL_DEBUG, "websocket_notify fr %d", o2pt->fr);
    // logger("WEBSOCKET", LOG_LEVEL_DEBUG, "websocket_notify nt->notijson %s", nt->noti_json);
    // logger("WEBSOCKET", LOG_LEVEL_DEBUG, "websocket_notify to %s", o2pt->to);
    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "op", 5);
    cJSON_AddStringToObject(json, "fr", o2pt->fr);
    cJSON_AddStringToObject(json, "to", o2pt->to);
    cJSON_AddStringToObject(json, "rqi", o2pt->rqi);
    cJSON_AddStringToObject(json, "rvi", from_rvi(o2pt->rvi));
    // cJSON_AddStringToObject(json, "rsc", o2pt->rsc);
    cJSON* notiJson = cJSON_Duplicate(cJSON_Parse(nt->noti_json), true);
    cJSON_AddItemToObject(json, "pc", notiJson);

    struct notify_result res = {cJSON_PrintUnformatted(json), 0, 0}; // 초기화
    struct lws_context *context;
    // cJSON_free(notiJson);

    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.pt_serv_buf_size = 10 * 1024 * 1024;
    info.protocols = (struct lws_protocols[]){
        {"oneM2M.json", callback_notify_verify, 0, 0},
        {NULL, NULL, 0, 0}};

    context = lws_create_context(&info);
    if (!context)
        return 4000;

    memset(&i, 0, sizeof(i));
    i.context = context;
    i.address = strdup(nt->host);
    i.port = nt->port;
    i.path = "/";
    i.host = i.address;
    i.origin = i.address;
    i.protocol = "oneM2M.json";
    i.opaque_user_data = &res;

    if (!lws_client_connect_via_info(&i))
    {
        lws_context_destroy(context);
        return 4000;
    }
    // 서비스 루프 실행
    while (!res.done)
    {
        if (lws_service(context, 100) < 0)
            break;
    }

    lws_context_destroy(context);
    cJSON_free(json);
    return (res.rsc != 0 ? res.rsc : 4000);
}

#pragma endregion

#endif