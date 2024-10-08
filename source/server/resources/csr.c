#include <stdlib.h>
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;

int create_csr(oneM2MPrimitive *o2pt, RTNode *parent_rtnode)
{
    int e = check_rn_invalid(o2pt, RT_CSR);
    if (e == -1)
        return o2pt->rsc;

    if (SERVER_TYPE == ASN_CSE)
    {
        return handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "operation not allowed");
    }

    cJSON *root = cJSON_Duplicate(o2pt->request_pc, 1);
    cJSON *csr = cJSON_GetObjectItem(root, "m2m:csr");

    int rsc = validate_csr(o2pt, parent_rtnode, csr, OP_CREATE);
    if (rsc != 0)
    {
        cJSON_Delete(root);
        return rsc;
    }

    add_general_attribute(csr, parent_rtnode, RT_CSR);
    cJSON_AddStringToObject(csr, "csi", o2pt->fr);
    cJSON_ReplaceItemInObject(csr, "rn", cJSON_CreateString(o2pt->fr[0] == '/' ? o2pt->fr + 1 : o2pt->fr));
    cJSON_ReplaceItemInObject(csr, "ri", cJSON_Duplicate(cJSON_GetObjectItem(csr, "rn"), 1));

    o2pt->rsc = RSC_CREATED;

    // Add uri attribute
    char *ptr = malloc(1024);
    cJSON *rn = cJSON_GetObjectItem(csr, "rn");
    sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);

    // Save to DB
    int result = db_store_resource(csr, ptr);
    if (result == -1)
    {
        cJSON_Delete(root);
        handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "database error");
        free(ptr);
        ptr = NULL;
        return RSC_INTERNAL_SERVER_ERROR;
    }
    free(ptr);
    ptr = NULL;

    RTNode *rtnode = create_rtnode(csr, RT_CSR);
    add_child_resource_tree(parent_rtnode, rtnode);
    make_response_body(o2pt, rtnode);

    // update descendent cse
    cJSON *dcse = cJSON_GetObjectItem(rt->cb->obj, "dcse");
    if (dcse == NULL)
    {
        dcse = cJSON_CreateArray();
        cJSON_AddItemToObject(rt->cb->obj, "dcse", dcse);
    }
    logger("UTIL", LOG_LEVEL_INFO, "add dcse %s", cJSON_GetObjectItem(csr, "csi")->valuestring);
    cJSON_AddItemToArray(dcse, cJSON_CreateString(cJSON_GetObjectItem(csr, "csi")->valuestring));
    update_remote_csr_dcse(rtnode);

    cJSON_DetachItemFromObject(root, "m2m:csr");
    cJSON_Delete(root);
    return RSC_CREATED;
}

int update_csr(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{
    char invalid_key[][8] = {"ty", "pi", "ri", "rn", "ct"};
    cJSON *m2m_csr = cJSON_GetObjectItem(o2pt->request_pc, "m2m:csr");
    int invalid_key_size = sizeof(invalid_key) / (8 * sizeof(char));

    int updateAttrCnt = cJSON_GetArraySize(m2m_csr);

    for (int i = 0; i < invalid_key_size; i++)
    {
        if (cJSON_GetObjectItem(m2m_csr, invalid_key[i]))
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "unsupported attribute on update");
            return RSC_BAD_REQUEST;
        }
    }
    cJSON *csr = target_rtnode->obj;
    int result;

    result = validate_csr(o2pt, target_rtnode->parent, m2m_csr, OP_UPDATE);
    if (result != 0)
    {
        return result;
    }

    cJSON_AddItemToObject(m2m_csr, "lt", cJSON_CreateString(get_local_time(0)));

    update_resource(target_rtnode->obj, m2m_csr);

    result = db_update_resource(m2m_csr, cJSON_GetStringValue(cJSON_GetObjectItem(csr, "ri")), RT_CSR);

    update_remote_csr_dcse(target_rtnode);

    for (int i = 0; i < updateAttrCnt; i++)
    {
        cJSON_DeleteItemFromArray(m2m_csr, 0);
    }

    make_response_body(o2pt, target_rtnode);
    o2pt->rsc = RSC_UPDATED;
    return RSC_UPDATED;
}

int validate_csr(oneM2MPrimitive *o2pt, RTNode *parent_rtnode, cJSON *csr, Operation op)
{
    cJSON *pjson = NULL;
    char *ptr = NULL;

    char *csi = NULL;
    if (op == OP_CREATE)
    {
        // check mandatory attribute
        if (cJSON_GetObjectItem(csr, "cb") == NULL)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
        }

        if (cJSON_GetObjectItem(csr, "srv") == NULL)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
        }

        if (cJSON_GetObjectItem(csr, "rr") == NULL)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
        }
    }

    // check np attribute
    if (cJSON_GetObjectItem(csr, "ri"))
    {
        return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `ri` is not allowed");
    }
    if (cJSON_GetObjectItem(csr, "pi"))
    {
        return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `pi` is not allowed");
    }

    if (op == OP_UPDATE)
    {
        if (cJSON_GetObjectItem(csr, "csi"))
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `csi` is not allowed");
        }
        if (cJSON_GetObjectItem(csr, "cb"))
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `cb` is not allowed");
        }
        if (cJSON_GetObjectItem(csr, "ct"))
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `ct` is not allowed");
        }
    }

    if ((pjson = cJSON_GetObjectItem(csr, "csi")))
    {
        csi = pjson->valuestring;
        if (check_csi_duplicate(csi, parent_rtnode) == -1)
        {
            handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "originator has already registered");
            return o2pt->rsc;
        }
    }

    return 0;
}