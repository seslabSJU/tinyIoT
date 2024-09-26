#include "logger.h"
#include "onem2m.h"
#include "dbmanager.h"
#include "httpd.h"
#include "cJSON.h"
#include "util.h"
#include "config.h"
#include "onem2mTypes.h"
#include "stdlib.h"

#define LOG_TAG "UT"

int handle_uppertester_procedure(oneM2MPrimitive *o2pt)
{
    cJSON *pjson = NULL;
    logger(LOG_TAG, LOG_LEVEL_INFO, "handle_uppertester_procedure");

    cJSON *pc = cJSON_GetObjectItem(o2pt->request_pc, "m2m:rqp");
    if (!pc)
    {
        return handle_error(o2pt, RSC_BAD_REQUEST, "Bad Request");
    }
    oneM2MPrimitive *req = calloc(1, sizeof(oneM2MPrimitive));

    if ((pjson = cJSON_GetObjectItem(pc, "op")))
    {
        req->op = pjson->valueint;
    }
    else
    {
        free_o2pt(req);
        return handle_error(o2pt, RSC_BAD_REQUEST, "Bad Request");
    }

    if ((pjson = cJSON_GetObjectItem(pc, "to")))
    {
        if (cJSON_IsString(pjson))
            req->to = strdup(pjson->valuestring);
    }
    else
    {
        free_o2pt(req);
        return handle_error(o2pt, RSC_BAD_REQUEST, "Bad Request");
    }

    if ((pjson = cJSON_GetObjectItem(pc, "fr")))
    {
        req->fr = strdup(pjson->valuestring);
    }

    if ((pjson = cJSON_GetObjectItem(pc, "rqi")))
    {
        req->rqi = strdup(pjson->valuestring);
    }

    if ((pjson = cJSON_GetObjectItem(pc, "pc")))
    {
        req->request_pc = cJSON_Duplicate(pjson, true);
    }

    if ((pjson = cJSON_GetObjectItem(pc, "ty")))
    {
        req->ty = pjson->valueint;
    }

    if ((pjson = cJSON_GetObjectItem(pc, "rvi")))
    {
        req->rvi = to_rvi(pjson->valuestring);
    }
    else
    {
        free_o2pt(req);
        return handle_error(o2pt, RSC_BAD_REQUEST, "Bad Request");
    }

    route(req);

    o2pt->response_pc = cJSON_Duplicate(req->response_pc, true);
    if (req->rsc >= RSC_BAD_REQUEST)
    {
        o2pt->rsc = req->rsc;
    }
    else
    {
        o2pt->rsc = RSC_OK;
    }

    free_o2pt(req);

    return 0;
}