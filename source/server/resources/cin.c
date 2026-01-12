#include <stdlib.h>
#include <regex.h>
#include <time.h>
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;

int create_cin(oneM2MPrimitive *o2pt, RTNode *parent_rtnode)
{
    cJSON *root = cJSON_Duplicate(o2pt->request_pc, 1);
    cJSON *cin = cJSON_GetObjectItem(root, "m2m:cin");
    cJSON *rn = NULL;
#if !ALLOW_CIN_RN
    // check if rn exists
    rn = cJSON_GetObjectItem(cin, "rn");
    if (rn != NULL)
    {
        handle_error(o2pt, RSC_BAD_REQUEST, "rn attribute for cin is assigned by CSE");
        cJSON_Delete(root);
        return o2pt->rsc;
    }
#endif

    add_general_attribute(cin, parent_rtnode, RT_CIN);

    // add cs attribute
    cJSON *con = cJSON_GetObjectItem(cin, "con");
    if (cJSON_IsString(con))
        cJSON_AddNumberToObject(cin, "cs", strlen(cJSON_GetStringValue(con)));

    // Add st attribute
    cJSON *st = cJSON_GetObjectItem(parent_rtnode->obj, "st");
    cJSON_AddNumberToObject(cin, "st", st->valueint);

    int rsc = validate_cin(o2pt, parent_rtnode->obj, cin, OP_CREATE);
    if (rsc != RSC_OK)
    {
        cJSON_Delete(root);
        return rsc;
    }
    cJSON *pjson = NULL;
    if ((pjson = cJSON_GetObjectItem(cin, "cr")))
    {
        if (pjson->type == cJSON_NULL)
        {
            cJSON_DeleteItemFromObject(cin, "cr");
            cJSON_AddStringToObject(cin, "cr", o2pt->fr);
        }
        else
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "creator attribute with arbitary value is not allowed");
            cJSON_Delete(root);
            return o2pt->rsc;
        }
    }
#if CSE_RVI >= RVI_3
    bool parent_was_announced = false; 
    cJSON *final_at = cJSON_CreateArray();
    cJSON *parent_at = cJSON_GetObjectItem(parent_rtnode->obj, "at");
    if (parent_at && cJSON_GetArraySize(parent_at) > 0)
    {
        parent_was_announced = true;
    }

    if (parent_was_announced)
    {
        if (handle_annc_create(parent_rtnode, cin, cJSON_GetObjectItem(cin, "at"), final_at) == -1)
        {
            cJSON_Delete(root);
            cJSON_Delete(final_at);
            return handle_error(o2pt, RSC_BAD_REQUEST, "invalid attribute in `aa`");
        }

        if (cJSON_GetArraySize(final_at) > 0)
        {
            cJSON_DeleteItemFromObject(cin, "at");
            cJSON_AddItemToObject(cin, "at", final_at);
        }
        else
        {
            cJSON_DeleteItemFromObject(cin, "at");
+           cJSON_AddItemToObject(cin, "at", final_at);
        }
    }
    else //cin can't be announced with out cnt
    {
        cJSON *at = cJSON_GetObjectItem(cin, "at");
        if (at && cJSON_GetArraySize(at) > 0) {
            handle_error(o2pt, RSC_BAD_REQUEST, "cinA can't be announced alone");
        }
    }
#endif

    RTNode *cin_rtnode = create_rtnode(cin, RT_CIN);
    struct timespec cnt_start, cnt_end;
    if (!db_begin_tx())
    {
        handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB begin fail");
        free_rtnode(cin_rtnode);
        cJSON_Delete(root);
        return o2pt->rsc;
    }
    clock_gettime(CLOCK_MONOTONIC, &cnt_start);
    if (update_cnt_cin(parent_rtnode, cin_rtnode, 1) != 0)
    {
        db_rollback_tx();
        handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "CNT update fail");
        free_rtnode(cin_rtnode);
        cJSON_Delete(root);
        return o2pt->rsc;
    }
    clock_gettime(CLOCK_MONOTONIC, &cnt_end);
    long update_ms = (cnt_end.tv_sec - cnt_start.tv_sec) * 1000 +
                     (cnt_end.tv_nsec - cnt_start.tv_nsec) / 1000000;
    logger("CIN", LOG_LEVEL_INFO, "update_cnt_cin time : %ld ms", update_ms);

    make_response_body(o2pt, cin_rtnode);
    o2pt->rsc = RSC_CREATED;

    // Add uri attribute
    char ptr[1024] = {0};
    rn = cJSON_GetObjectItem(cin, "rn");
    sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);

    // Store to DB
    struct timespec db_start, db_end;
    clock_gettime(CLOCK_MONOTONIC, &db_start);
    int result = db_store_resource(cin, ptr);
    clock_gettime(CLOCK_MONOTONIC, &db_end);
    long store_ms = (db_end.tv_sec - db_start.tv_sec) * 1000 +
                    (db_end.tv_nsec - db_start.tv_nsec) / 1000000;
    logger("CIN", LOG_LEVEL_INFO, "db_store_resource time : %ld ms", store_ms);
    if (result != 1)
    {
        db_rollback_tx();
        handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail");
        free_rtnode(cin_rtnode);
        cJSON_Delete(root);
        return o2pt->rsc;
    }
    if (!db_commit_tx())
    {
        handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB commit fail");
        free_rtnode(cin_rtnode);
        cJSON_Delete(root);
        return o2pt->rsc;
    }

    RTNode *rtnode = parent_rtnode->child;
    if (!rtnode)
    {
        add_child_resource_tree(parent_rtnode, cin_rtnode);
        if (cin_rtnode->rn)
            free(cin_rtnode->rn);
        cin_rtnode->rn = strdup("la");
    }
    else
    {
        while (rtnode && strcmp(rtnode->rn, "la") != 0)
        {
            rtnode = rtnode->sibling_right;
        }
        if (!rtnode)
        {
            add_child_resource_tree(parent_rtnode, cin_rtnode);
            if (cin_rtnode->rn)
                free(cin_rtnode->rn);
            cin_rtnode->rn = strdup("la");
        }
        else
        {
            cJSON_Delete(rtnode->obj);
            rtnode->obj = cJSON_Duplicate(cin, 1);
            cJSON_DetachItemFromObject(root, "m2m:cin");
            free_rtnode(cin_rtnode);
        }
    }
    cJSON_DetachItemFromObject(root, "m2m:cin");
    cJSON_Delete(root);

    return RSC_CREATED;
}

bool validate_cnf(const char *cnf) 
{
    regex_t regex;
    int reti;
    const char *pattern = "^[^:/]+/[^:/]+:[0-2](:[0-5])?$";
    reti = regcomp(&regex, pattern, REG_EXTENDED);
    if (reti) {
        return false;
    }
    reti = regexec(&regex, cnf, 0, NULL, 0);
    regfree(&regex);
    return (reti == 0);
}

int validate_cin(oneM2MPrimitive *o2pt, cJSON *parent_cnt, cJSON *cin, Operation op)
{
    cJSON *pjson = NULL, *pjson2 = NULL;
    char *ptr = NULL;

    cJSON *mbs = NULL;
    cJSON *cs = NULL;

    if ((pjson = cJSON_GetObjectItem(cin, "rn")))
    {
        if (!strcmp(pjson->valuestring, "la") || !strcmp(pjson->valuestring, "latest"))
        {
            handle_error(o2pt, RSC_NOT_ACCEPTABLE, "attribute `rn` is invalid");
            return RSC_BAD_REQUEST;
        }
        if (!strcmp(pjson->valuestring, "ol") || !strcmp(pjson->valuestring, "oldest"))
        {
            handle_error(o2pt, RSC_NOT_ACCEPTABLE, "attribute `rn` is invalid");
            return RSC_BAD_REQUEST;
        }
    }

    if ((pjson = cJSON_GetObjectItem(cin, "acpi")))
    {
        return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `acpi` for `cin` is not supported");
    }

    if ((mbs = cJSON_GetObjectItem(parent_cnt, "mbs")))
    {
        logger("CIN", LOG_LEVEL_DEBUG, "mbs %d", mbs->valueint);
        if ((cs = cJSON_GetObjectItem(cin, "cs")))
        {
            logger("CIN", LOG_LEVEL_DEBUG, "cs %d", cs->valueint);
            if (mbs->valueint >= 0 && cs->valueint > mbs->valueint)
            {
                return handle_error(o2pt, RSC_NOT_ACCEPTABLE, "contentInstance size exceed `mbs`");
            }
        }
    }

    if ((pjson = cJSON_GetObjectItem(cin, "cnf")))
    {
        // cnf check
        if (!validate_cnf(pjson->valuestring)) {
            return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `cnf` is invalid");
        }
    }
    cJSON *aa = cJSON_GetObjectItem(cin, "aa");
    if (aa && CSE_RVI < RVI_3)
    {
        return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `aa` is not supported");
    }
    cJSON *attr = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(RT_CIN));
    cJSON_ArrayForEach(pjson, aa)
    {
        if (strcmp(pjson->valuestring, "lbl") == 0)
            continue;
        if (strcmp(pjson->valuestring, "ast") == 0)
            continue;
        if (strcmp(pjson->valuestring, "lnk") == 0)
            continue;
        if (!cJSON_GetObjectItem(attr, pjson->valuestring))
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "invalid attribute in `aa`");
        }
    }

    return RSC_OK;
}
