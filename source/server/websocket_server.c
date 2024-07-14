#include "websocket_server.h"
#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_MESSAGE_SIZE 2048

typedef struct {
    unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + MAX_MESSAGE_SIZE + LWS_SEND_BUFFER_POST_PADDING];
    size_t len;
} per_session_data_t;

static int callback_websocket(struct lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len) {
    per_session_data_t *pss = (per_session_data_t *)user;

    switch (reason) {
        case LWS_CALLBACK_RECEIVE:
            if (pss->len + len > MAX_MESSAGE_SIZE) {
                printf("Buffer overflow\n");
                return -1;
            }
            memcpy(&pss->buf[LWS_SEND_BUFFER_PRE_PADDING + pss->len], in, len);
            pss->len += len;

            if (lws_is_final_fragment(wsi)) {
                printf("Received data: %.*s\n", (int)pss->len, &pss->buf[LWS_SEND_BUFFER_PRE_PADDING]);

                // 수신한 데이터를 파일에 저장
                FILE *file = fopen("resource.json", "w");
                if (file != NULL) {
                    fprintf(file, "%.*s", (int)pss->len, &pss->buf[LWS_SEND_BUFFER_PRE_PADDING]);
                    fclose(file);
                }

                // 저장된 데이터를 읽어서 응답으로 전송
                file = fopen("resource.json", "r");
                if (file != NULL) {
                    char response[MAX_MESSAGE_SIZE];
                    size_t response_len = fread(response, 1, MAX_MESSAGE_SIZE, file);
                    fclose(file);
                    unsigned char *p = &pss->buf[LWS_SEND_BUFFER_PRE_PADDING];
                    memcpy(p, response, response_len);
                    lws_write(wsi, p, response_len, LWS_WRITE_TEXT);
                }

                pss->len = 0;
            }
            break;
        case LWS_CALLBACK_CLOSED:
            printf("Connection closed\n");
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
        sizeof(per_session_data_t),
        MAX_MESSAGE_SIZE,
    },
    { NULL, NULL, 0, 0 }
};

int initialize_websocket_server(void) {
    struct lws_context_creation_info info;
    struct lws_context *context;

    memset(&info, 0, sizeof info);
    info.port = 3000;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

    context = lws_create_context(&info);
    if (context == NULL) {
        fprintf(stderr, "lws init failed\n");
        return -1;
    }

    printf("Starting WebSocket server\n");
    while (1) {
        lws_service(context, 1000);
    }

    lws_context_destroy(context);
    return 0;
}
