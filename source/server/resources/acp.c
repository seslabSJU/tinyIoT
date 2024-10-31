#include <stdlib.h>
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;

int create_acp(oneM2MPrimitive *o2pt, RTNode *parent_rtnode)
{
    if (parent_rtnode->ty != RT_CSE && parent_rtnode->ty != RT_AE)
    {
        handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
        return o2pt->rsc;
    }

    cJSON *root = cJSON_Duplicate(o2pt->request_pc, 1);
    cJSON *acp = cJSON_GetObjectItem(root, "m2m:acp");

    add_general_attribute(acp, parent_rtnode, RT_ACP);

    int rsc = validate_acp(o2pt, acp, OP_CREATE);

    if (rsc != RSC_OK)
    {
        cJSON_Delete(root);
        return rsc;
    }

#if CSE_RVI >= RVI_3
    cJSON *final_at = cJSON_CreateArray();
    cJSON *at = NULL;
    char *at_str = NULL;
    cJSON_ArrayForEach(at, cJSON_GetObjectItem(acp, "at"))
    {
        at_str = create_remote_annc(parent_rtnode, acp, at->valuestring, false);
        if (!at_str)
        {
            continue;
        }
        logger("UTIL", LOG_LEVEL_DEBUG, "add at %s", at_str);
        cJSON_AddItemToArray(final_at, cJSON_CreateString(at_str));
    }

    if (cJSON_GetArraySize(final_at) > 0)
    {
        cJSON_DeleteItemFromObject(acp, "at");
        cJSON_AddItemToObject(acp, "at", final_at);
    }
    else
    {
        cJSON_Delete(final_at);
        cJSON_DeleteItemFromObject(acp, "at");
    }
#endif
    // Add uri attribute
    char *ptr = malloc(1024);
    cJSON *rn = cJSON_GetObjectItem(acp, "rn");
    sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);
    // Save to DB
    int result = db_store_resource(acp, ptr);
    if (result != 1)
    {
        handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail");
        cJSON_Delete(root);
        free(ptr);
        ptr = NULL;
        return RSC_INTERNAL_SERVER_ERROR;
    }

    free(ptr);
    ptr = NULL;

    // Add to resource tree
    RTNode *child_rtnode = create_rtnode(acp, RT_ACP);
    add_child_resource_tree(parent_rtnode, child_rtnode);
    make_response_body(o2pt, child_rtnode);

    cJSON_DetachItemFromObject(root, "m2m:acp");
    cJSON_Delete(root);

    return o2pt->rsc = RSC_CREATED;
}

int update_acp(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{
    char invalid_key[][8] = {"ty", "pi", "ri", "rn", "ct"};
    cJSON *m2m_acp = cJSON_GetObjectItem(o2pt->request_pc, "m2m:acp");
    int invalid_key_size = sizeof(invalid_key) / (8 * sizeof(char));

    int updateAttrCnt = cJSON_GetArraySize(m2m_acp);

    for (int i = 0; i < invalid_key_size; i++)
    {
        if (cJSON_GetObjectItem(m2m_acp, invalid_key[i]))
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "unsupported attribute on update");
            return RSC_BAD_REQUEST;
        }
    }

    int result = validate_acp(o2pt, m2m_acp, OP_UPDATE);
    if (result != RSC_OK)
        return result;

    cJSON *acp = target_rtnode->obj;
    cJSON *pjson = NULL;

    cJSON *at = NULL;
    if ((at = cJSON_GetObjectItem(m2m_acp, "at")))
    {
        cJSON *final_at = cJSON_CreateArray();
        handle_annc_update(target_rtnode, at, final_at);
        cJSON_DeleteItemFromObject(m2m_acp, "at");
        cJSON_AddItemToObject(m2m_acp, "at", final_at);
    }
    cJSON_AddItemToObject(m2m_acp, "lt", cJSON_CreateString(get_local_time(0)));

    update_resource(target_rtnode->obj, m2m_acp);

    result = db_update_resource(m2m_acp, cJSON_GetObjectItem(target_rtnode->obj, "ri")->valuestring, RT_ACP);

    if (result != 1)
    {
        return handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB update fail");
        ;
    }

    for (int i = 0; i < updateAttrCnt; i++)
    {
        cJSON_DeleteItemFromArray(m2m_acp, 0);
    }

    make_response_body(o2pt, target_rtnode);

    o2pt->rsc = RSC_UPDATED;

    // cJSON_DetachItemFromObject(root, "m2m:acp");
    // cJSON_Delete(root);
    return RSC_UPDATED;
}

/**
 * @brief validate acp resource
 * @param o2pt oneM2M request primitive
 * @param acp acp attribute cJSON object
 * @param op operation type
 * @return 0 if valid, -1 if invalid
 */
int validate_acp(oneM2MPrimitive *o2pt, cJSON *acp, Operation op)
{
    cJSON *pjson = NULL;
    cJSON *pjson2 = NULL;
    char *ptr = NULL;
    if (!acp)
    {
        return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
    }

    if (op == OP_CREATE)
    {
        pjson = cJSON_GetObjectItem(acp, "pv");
        if (!pjson)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
        }
        else if (pjson->type == cJSON_NULL)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "null `pv` is not allowed");
        }
        else
        {
            if (validate_acr(o2pt, cJSON_GetObjectItem(pjson, "acr")) != RSC_OK)
                return o2pt->rsc;
        }

        pjson = cJSON_GetObjectItem(acp, "pvs");
        if (!pjson)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
        }
        else if (cJSON_IsNull(pjson))
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "null `pvs` is not allowed");
        }
        else if ((pjson2 = cJSON_GetObjectItem(pjson, "acr")))
        {
            if (cJSON_GetArraySize(pjson2) == 0)
            {
                return handle_error(o2pt, RSC_BAD_REQUEST, "empty `acr` is not allowed");
            }
        }
        else if (pjson2 == NULL)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
        }
        else
        {
            if (validate_acr(o2pt, cJSON_GetObjectItem(pjson, "acr")) != RSC_OK)
            {
                return o2pt->rsc;
            }
        }
    }
    if (op == OP_UPDATE)
    {
        pjson = cJSON_GetObjectItem(acp, "pv");
        if (pjson)
        {
            if (cJSON_IsNull(pjson))
            {
                return handle_error(o2pt, RSC_BAD_REQUEST, "empty `pv` is not allowed");
            }
        }
        else
        {
            if (validate_acr(o2pt, cJSON_GetObjectItem(pjson, "acr")) != RSC_OK)
            {
                return o2pt->rsc;
            }
        }

        pjson = cJSON_GetObjectItem(acp, "pvs");
        if (pjson)
        {
            if (cJSON_IsNull(pjson) || !cJSON_GetObjectItem(pjson, "acr"))
            {
                return handle_error(o2pt, RSC_BAD_REQUEST, "empty `pvs` is not allowed");
            }
        }
        else
        {
            if (validate_acr(o2pt, cJSON_GetObjectItem(pjson, "acr")) != RSC_OK)
            {
                return o2pt->rsc;
            }
        }
    }

    cJSON *aa = cJSON_GetObjectItem(acp, "aa");
    cJSON *attr = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(RT_ACP));
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
