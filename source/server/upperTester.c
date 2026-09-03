#include "logger.h"
#include "onem2m.h"
#include "dbmanager.h"
#include "httpd.h"
#include "cJSON.h"
#include "util.h"
#include "config.h"
#include "onem2mTypes.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifdef UPPERTESTER

#define LOG_TAG "UT"

/**
 * @brief Handle the special Upper Tester "Reset" command (TS-0019 table 5.4.4.2.2-2).
 *
 * Brings the IUT back to its initial state by wiping every stored resource and
 * rebuilding the CSE. The UtTriggerAck may only carry a response status code,
 * restricted to 2000 (OK) or 4000 (BAD_REQUEST) - see TS-0019 table 5.4.4.2.2-3.
 */
static int handle_ut_reset(oneM2MPrimitive *o2pt)
{
    logger(LOG_TAG, LOG_LEVEL_INFO, "UT command: Reset");

    if (reset_cse() != 0)
    {
        return handle_error(o2pt, RSC_BAD_REQUEST, "reset failed");
    }

    o2pt->rsc = RSC_OK;
    if (o2pt->response_pc)
        cJSON_Delete(o2pt->response_pc);
    o2pt->response_pc = NULL;
    return 0;
}

int handle_uppertester_procedure(oneM2MPrimitive *o2pt)
{
    cJSON *pjson = NULL;
    logger(LOG_TAG, LOG_LEVEL_INFO, "handle_uppertester_procedure");

    // Special commands are carried in the X-M2M-UTCMD header, not the body.
    if (o2pt->utcmd)
    {
        if (strcasecmp(o2pt->utcmd, "Reset") == 0)
            return handle_ut_reset(o2pt);
        return handle_error(o2pt, RSC_BAD_REQUEST, "Unknown Upper Tester command");
    }

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

#endif /* UPPERTESTER */