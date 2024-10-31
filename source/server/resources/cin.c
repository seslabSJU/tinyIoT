#include <stdlib.h>
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
    cJSON *final_at = cJSON_CreateArray();
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
        cJSON_Delete(final_at);
    }
#endif

    RTNode *cin_rtnode = create_rtnode(cin, RT_CIN);
    update_cnt_cin(parent_rtnode, cin_rtnode, 1);

    make_response_body(o2pt, cin_rtnode);
    o2pt->rsc = RSC_CREATED;

    // Add uri attribute
    char ptr[1024] = {0};
    rn = cJSON_GetObjectItem(cin, "rn");
    sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);

    // Store to DB
    int result = db_store_resource(cin, ptr);
    if (result != 1)
    {
        handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail");
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
        // check cnf
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