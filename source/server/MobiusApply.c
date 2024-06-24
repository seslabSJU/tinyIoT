#include <stdlib.h>
#include <string.h>
#include "onem2m.h"
#include "logger.h"
#include "dbmanager.h"
#include "util.h"
#include "config.h"


#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <sys/timeb.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "httpd.h"
#include "onem2mTypes.h"
#include "jsonparser.h"
#include "mqttClient.h"
#include "coap.h"
#include "MobiusApply.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;

char* Mobius_func(const char* addr) {
    if (addr == NULL) {
        return NULL;
    }

    size_t len = strlen(addr);

    size_t first_slash_index = 0;
    while (first_slash_index < len && addr[first_slash_index] != '/') {
        first_slash_index++;
    }

    if (first_slash_index == len) {
        char* empty_str = (char*)malloc(1 * sizeof(char));
        if (empty_str == NULL) {
            return NULL; // 메모리 할당 실패 시 NULL 반환
        }
        empty_str[0] = '\0';
        return empty_str;
    }

    size_t new_len = len - first_slash_index;

    char* new_str = (char*)malloc((new_len) * sizeof(char));
    if (new_str == NULL) {
        return NULL; 
    }

    for (size_t i = first_slash_index + 1; i < len; i++) {
        new_str[i - first_slash_index - 1] = addr[i];
    }

    new_str[new_len - 1] = '\0';

    printf("looking for %s\n", new_str);
    return new_str;
}

int Mobius_deRegister_csr() //From util.c
{
	RTNode *rtnode = rt->cb->child;
	RTNode *next_rt = NULL;
	Protocol prot = PROT_HTTP;
	char *path = NULL;
	char *host = NULL;
	int port = 0;

	while (rtnode)
	{
		next_rt = rtnode->sibling_right;
		if (rtnode->ty == RT_CSR)
		{
			char buf[1024] = {0};
			cJSON *csr = rtnode->obj;
			cJSON *csi = cJSON_GetObjectItem(csr, "csi");
			cJSON *cb = cJSON_GetObjectItem(csr, "cb");

			cJSON *poa = cJSON_GetObjectItem(csr, "poa");
			cJSON *pjson = NULL;
			oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(sizeof(oneM2MPrimitive), 1);
			o2pt->op = OP_DELETE;
			o2pt->fr = strdup("/" CSE_BASE_RI);
			sprintf(buf, "%s/%s", REMOTE_CSE_NAME, CSE_BASE_RI);
			o2pt->to = strdup(buf);
			o2pt->rqi = strdup("delete-csr");
			o2pt->rvi = strdup("2a");

			forwarding_onem2m_resource(o2pt, rtnode);
			free_o2pt(o2pt);

			db_delete_onem2m_resource(rtnode);
		}

		rtnode = next_rt;
	}
	return 0;
}

void Mobius_http_forwarding(oneM2MPrimitive *o2pt, char *host, int port) //From httpd.c
{
    logger("HTTP", LOG_LEVEL_DEBUG, "http_forwarding");
    struct sockaddr_in serv_addr;

    int rcvd = 0;
    char buffer[BUF_SIZE];
    HTTPRequest *req = (HTTPRequest *)calloc(sizeof(HTTPRequest), 1);
    HTTPResponse *res = (HTTPResponse *)calloc(sizeof(HTTPResponse), 1);

    ResourceAddressingType rat = checkResourceAddressingType(o2pt->to);

    req->method = op_to_method(o2pt->op);
    req->uri = calloc(1, sizeof(char) * (strlen(o2pt->to) + 4));
    req->uri[0] = '/';
    // req->uri[1] = '/';
    // if (rat == SP_RELATIVE)
    //     req->uri[1] = '~';
    // else if (rat == ABSOLUTE)
    //     req->uri[1] = '_';
    strcat(req->uri, o2pt->to);
    req->headers = (header_t *)calloc(sizeof(header_t), 1);
    add_header("X-M2M-Origin", o2pt->fr, req->headers);
    add_header("X-M2M-RVI", "2a", req->headers);
    add_header("X-M2M-RI", o2pt->rqi, req->headers);

    if (o2pt->fc)
    {
        req->qs = fc_to_qs(o2pt->fc);
    }

    if (o2pt->ty > 0)
    {
        sprintf(buffer, "application/json;ty=%d", o2pt->ty);
        add_header("Content-Type", buffer, req->headers);
    }
    else
    {
        add_header("Content-Type", "application/json", req->headers);
    }
    if (o2pt->request_pc)
    {
        req->payload = cJSON_PrintUnformatted(o2pt->request_pc);
        req->payload_size = strlen(req->payload);
    }
    else
    {
        req->payload = NULL;
        req->payload_size = 0;
    }
    send_http_request(host, port, req, res);
    char *rsc = search_header(res->headers, "x-m2m-rsc");
    if (rsc)
        o2pt->rsc = atoi(rsc);
    else
        o2pt->rsc = RSC_TARGET_NOT_REACHABLE;

    if (res->payload)
        o2pt->response_pc = cJSON_Parse(res->payload);

    free_HTTPRequest(req);
    free_HTTPResponse(res);
    return;
}

RTNode *Mobius_find_rtnode(char *addr) //From rtManager.c
{
    char *new_addr = Mobius_func(addr);
    // char *new_addr = addr;
    char *last_word;
    int found_slash = 0;
    if (!new_addr)
        return NULL;
    RTNode *rtnode = NULL;
    if (strcmp(new_addr, CSE_BASE_NAME) == 0 || strcmp(new_addr, "-") == 0)
    {
        return rt->cb;
    }
    logger("UTIL", LOG_LEVEL_DEBUG, "addr : %s", new_addr);
    printf("name check :%s\n", CSE_BASE_NAME);
    if ((strncmp(new_addr, CSE_BASE_NAME, strlen(CSE_BASE_NAME)) == 0 && new_addr[strlen(CSE_BASE_NAME)] == '/') || (new_addr[0] == '-' && new_addr[1] == '/'))
    {
        logger("UTIL", LOG_LEVEL_DEBUG, "Hierarchical Addressing");
        rtnode = find_rtnode_by_uri(new_addr);
    }
    else
    {
        logger("UTIL", LOG_LEVEL_DEBUG, "Non-Hierarchical Addressing");
        for (int i = strlen(new_addr) - 1; i >= 0; i--)
            {
                if (new_addr[i] == '/') 
                {
                    last_word = &new_addr[i + 1];
                    found_slash = 1;
                    break;
                }
            }

            // '/'가 없는 경우 그대로 출력
            if (!found_slash) 
            {
                last_word = new_addr;
            }
    
        printf("last word : %s\n", last_word);
        rtnode = find_rtnode_by_ri(last_word);
    }
    return rtnode;
}