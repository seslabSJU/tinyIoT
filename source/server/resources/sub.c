#include <stdlib.h>
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;

int create_sub(oneM2MPrimitive *o2pt, RTNode *parent_rtnode)
{
    int result = 0;

    cJSON *root = cJSON_Duplicate(o2pt->request_pc, 1);
    cJSON *sub = cJSON_GetObjectItem(root, "m2m:sub");

    add_general_attribute(sub, parent_rtnode, RT_SUB);

    int rsc = validate_sub(o2pt, sub, OP_CREATE);
    if (cJSON_GetObjectItem(sub, "nct") == NULL)
        cJSON_AddNumberToObject(sub, "nct", NCT_ALL_ATTRIBUTES);

    cJSON *pjson = NULL;

    if ((pjson = cJSON_GetObjectItem(sub, "cr")))
    {
        if (pjson->type == cJSON_NULL)
        {
            cJSON_DeleteItemFromObject(sub, "cr");
            cJSON_AddStringToObject(sub, "cr", o2pt->fr);
        }
        else
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "creator attribute with arbitary value is not allowed");
            cJSON_Delete(root);
            return o2pt->rsc;
        }
    }

    if (rsc != RSC_OK)
    {
        cJSON_Delete(root);
        return rsc;
    }

    // Add uri attribute
    char ptr[1024];
    cJSON *rn = cJSON_GetObjectItem(sub, "rn");
    sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);

    cJSON *noti_cjson, *sgn, *nev, *rep, *nct;
    RTNode *nu_rtnode;
    noti_cjson = cJSON_CreateObject();
    cJSON_AddItemToObject(noti_cjson, "m2m:sgn", sgn = cJSON_CreateObject());
    cJSON_AddItemToObject(sgn, "nev", nev = cJSON_CreateObject());
    cJSON_AddNumberToObject(nev, "net", NET_CREATE_OF_DIRECT_CHILD_RESOURCE);
    cJSON_AddItemToObject(nev, "rep", rep = cJSON_CreateObject());
    cJSON_AddStringToObject(sgn, "cr", o2pt->fr);
    cJSON_AddItemReferenceToObject(rep, "m2m:sub", sub);
    cJSON_AddStringToObject(sgn, "sur", ptr);
    cJSON_AddBoolToObject(sgn, "vrq", true);
    cJSON_ArrayForEach(pjson, cJSON_GetObjectItem(sub, "nu"))
    {

        nu_rtnode = find_rtnode(pjson->valuestring);
        if (nu_rtnode)
        {
            if (nu_rtnode->ty != RT_AE)
            {
                handle_error(o2pt, RSC_SUBSCRIPTION_VERIFICATION_INITIATION_FAILED, "nu is invalid");
                cJSON_Delete(noti_cjson);
                cJSON_DetachItemFromObject(root, "m2m:sub");
                cJSON_Delete(root);
                return RSC_SUBSCRIPTION_VERIFICATION_INITIATION_FAILED;
            }
            if (strcmp(cJSON_GetObjectItem(nu_rtnode->obj, "aei")->valuestring, o2pt->fr) == 0)
            {
                continue;
            }
        }
        else if (!strcmp(pjson->valuestring, o2pt->fr))
        {
            continue;
        }

        result = send_verification_request(pjson->valuestring, noti_cjson);
        logger("SUB", LOG_LEVEL_INFO, "vrq result : %d", result);

        if (result == RSC_SUBSCRIPTION_CREATOR_HAS_NO_PRIVILEGE)
        {
            cJSON_Delete(noti_cjson);
            cJSON_Delete(root);
            return handle_error(o2pt, RSC_SUBSCRIPTION_CREATOR_HAS_NO_PRIVILEGE, "subscription verification error");
        }
        else if (result / 1000 == 4 || result / 1000 == 5)
        {
            cJSON_Delete(noti_cjson);
            cJSON_Delete(root);
            return handle_error(o2pt, RSC_SUBSCRIPTION_VERIFICATION_INITIATION_FAILED, "subscription verification error");
        }

        cJSON_DeleteItemFromObject(sgn, "vrq");
    }
    cJSON_Delete(noti_cjson);
    // Store to DB
    result = db_store_resource(sub, ptr);
    if (result != 1)
    {
        handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail");
        cJSON_Delete(root);
        return o2pt->rsc;
    }

    RTNode *child_rtnode = create_rtnode(sub, RT_SUB);
    add_child_resource_tree(parent_rtnode, child_rtnode);
    make_response_body(o2pt, child_rtnode);
    o2pt->rsc = RSC_CREATED;

    cJSON_DetachItemFromObject(root, "m2m:sub");
    cJSON_Delete(root);

    return RSC_CREATED;
}

int update_sub(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{
    char invalid_key[][8] = {"ty", "pi", "ri", "rn", "ct"};
    cJSON *pjson = NULL;
    cJSON *m2m_sub = cJSON_GetObjectItem(o2pt->request_pc, "m2m:sub");
    int invalid_key_size = sizeof(invalid_key) / (8 * sizeof(char));
    int updateAttrCnt = cJSON_GetArraySize(m2m_sub);

    for (int i = 0; i < invalid_key_size; i++)
    {
        if (cJSON_GetObjectItem(m2m_sub, invalid_key[i]))
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "{\"m2m:dbg\": \"unsupported attribute on update\"}");
            return RSC_BAD_REQUEST;
        }
    }

    cJSON *sub = target_rtnode->obj;
    int result;

    if (validate_sub(o2pt, m2m_sub, o2pt->op) != RSC_OK)
    {
        return o2pt->rsc;
    }

    cJSON *old_nu = cJSON_GetObjectItem(sub, "nu");
    cJSON *new_nu = cJSON_GetObjectItem(m2m_sub, "nu");
    if (new_nu)
    {
        cJSON_DetachItemFromObject(sub, "nu");
        cJSON_AddItemToObject(sub, "nu", new_nu);
        cJSON *noti_cjson, *sgn, *nev, *rep, *nct;
        noti_cjson = cJSON_CreateObject();
        cJSON_AddItemToObject(noti_cjson, "m2m:sgn", sgn = cJSON_CreateObject());
        cJSON_AddItemToObject(sgn, "nev", nev = cJSON_CreateObject());
        cJSON_AddStringToObject(sgn, "cr", o2pt->fr);
        cJSON_AddNumberToObject(nev, "net", NET_CREATE_OF_DIRECT_CHILD_RESOURCE);
        cJSON_AddItemToObject(nev, "rep", rep = cJSON_CreateObject());
        cJSON_AddItemToObject(rep, "m2m:sub", cJSON_Duplicate(sub, true));
        cJSON_AddStringToObject(sgn, "sur", target_rtnode->uri);
        cJSON_AddBoolToObject(sgn, "vrq", true);
        cJSON_ArrayForEach(pjson, new_nu)
        {
            if (!strcmp(pjson->valuestring, o2pt->fr))
            {
                continue;
            }
            if (send_verification_request(pjson->valuestring, noti_cjson) != RSC_OK)
            {
                cJSON_Delete(noti_cjson);

                cJSON_DetachItemFromObject(sub, "nu");
                cJSON_AddItemToObject(sub, "nu", old_nu);
                return handle_error(o2pt, RSC_SUBSCRIPTION_VERIFICATION_INITIATION_FAILED, "notification error");
            }
        }

        cJSON_DetachItemFromObject(sub, "nu");
        cJSON_AddItemToObject(sub, "nu", old_nu);
        cJSON_Delete(noti_cjson);
    }
    pjson = cJSON_GetObjectItem(m2m_sub, "enc");
    if (pjson && pjson->type == cJSON_NULL)
    {
        cJSON *pjson2;
        pjson = cJSON_CreateObject();
        cJSON_AddItemToObject(pjson, "net", pjson2 = cJSON_CreateArray());
        cJSON_AddItemToArray(pjson2, cJSON_CreateNumber(NET_UPDATE_OF_RESOURCE));
        cJSON_ReplaceItemInObject(m2m_sub, "enc", pjson);
    }
    cJSON_AddItemToObject(m2m_sub, "lt", cJSON_CreateString(get_local_time(0)));

    update_resource(sub, m2m_sub);

    db_update_resource(m2m_sub, cJSON_GetObjectItem(sub, "ri")->valuestring, RT_SUB);

    for (int i = 0; i < updateAttrCnt; i++)
    {
        cJSON_DeleteItemFromArray(m2m_sub, 0);
    }

    make_response_body(o2pt, target_rtnode);
    o2pt->rsc = RSC_UPDATED;
    return RSC_UPDATED;
}

int validate_sub(oneM2MPrimitive *o2pt, cJSON *sub, Operation op)
{
    cJSON *pjson = NULL;
    cJSON *enc, *net, *nct;
    char *ptr = NULL;

    pjson = cJSON_GetObjectItem(sub, "acpi");
    if (pjson)
    {
        int result = validate_acpi(o2pt, pjson, op_to_acop(op));
        if (result != RSC_OK)
            return result;
    }
    enc = cJSON_GetObjectItem(sub, "enc");
    if ((net = cJSON_GetObjectItem(enc, "net")))
    {
        nct = cJSON_GetObjectItem(sub, "nct");
        cJSON_ArrayForEach(pjson, net)
        {
            if (pjson->valueint < 0)
            {
                return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `net` is invalid");
            }
            if (!nct)
                continue;
            int nct_val = nct->valueint;
            switch (pjson->valueint)
            {
            case NET_NONE:
                break;
            case NET_UPDATE_OF_RESOURCE:
                if (nct_val == NCT_TRIGGER_PAYLOAD ||
                    nct_val == NCT_TIMESERIES_NOTIFICATION)
                {
                    return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `nct` is invalid");
                }
                break;
            case NET_DELETE_OF_RESOURCE:
                if (nct_val == NCT_TRIGGER_PAYLOAD ||
                    nct_val == NCT_TIMESERIES_NOTIFICATION)
                {
                    return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `nct` is invalid");
                }
                break;
            case NET_CREATE_OF_DIRECT_CHILD_RESOURCE:
                if (nct_val == NCT_MODIFIED_ATTRIBUTES ||
                    nct_val == NCT_TRIGGER_PAYLOAD ||
                    nct_val == NCT_TIMESERIES_NOTIFICATION)
                {
                    return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `nct` is invalid");
                }
                break;
            case NET_DELETE_OF_DIRECT_CHILD_RESOURCE:
                if (nct_val == NCT_MODIFIED_ATTRIBUTES ||
                    nct_val == NCT_TRIGGER_PAYLOAD ||
                    nct_val == NCT_TIMESERIES_NOTIFICATION)
                {
                    return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `nct` is invalid");
                }
                break;
            case NET_RETRIEVE_OF_CONTAINER_RESOURCE_WITH_NO_CHILD_RESOURCE:
                if (nct_val == NCT_MODIFIED_ATTRIBUTES ||
                    nct_val == NCT_TRIGGER_PAYLOAD ||
                    nct_val == NCT_TIMESERIES_NOTIFICATION)
                {
                    return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `nct` is invalid");
                }
                break;
            case NET_TRIGGER_RECIEVED_FOR_AE_RESOURCE:
                if (nct_val == NCT_MODIFIED_ATTRIBUTES ||
                    nct_val == NCT_ALL_ATTRIBUTES ||
                    nct_val == NCT_RESOURCE_ID ||
                    nct_val == NCT_TIMESERIES_NOTIFICATION)
                {
                    return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `nct` is invalid");
                }
                break;
            case NET_BLOCKING_UPDATE:
                if (nct_val == NCT_ALL_ATTRIBUTES ||
                    nct_val == NCT_TRIGGER_PAYLOAD ||
                    nct_val == NCT_RESOURCE_ID ||
                    nct_val == NCT_TIMESERIES_NOTIFICATION)
                {
                    return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `nct` is invalid");
                }
                break;
            case NET_REPORT_ON_MISSING_DATA_POINTS:
                return handle_error(o2pt, RSC_BAD_REQUEST, "`net` NET_REPORT_ON_MISSING_DATA_POINTS is not supported");
                break;
            }
        }
    }

    if (pjson = cJSON_GetObjectItem(sub, "nu"))
    {
        if (pjson->type != cJSON_Array)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `nu` is invalid");
        }
        if (cJSON_GetArraySize(pjson) == 0)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `nu` is invalid");
        }
    }

    return RSC_OK;
}