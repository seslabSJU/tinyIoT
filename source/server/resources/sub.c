#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../config.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;

int create_sub(oneM2MPrimitive *o2pt, RTNode *parent_rtnode)
{
    if (parent_rtnode->ty == RT_CIN || parent_rtnode->ty == RT_SUB)
    {
        return handle_error(o2pt, RSC_TARGET_NOT_SUBSCRIBABLE, "target not subscribable");
    }

    cJSON *root = cJSON_Duplicate(o2pt->cjson_pc, 1);
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

    // Store to DB
    int result = db_store_resource(sub, ptr);
    if (result != 1)
    {
        handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail");
        cJSON_Delete(root);
        return o2pt->rsc;
    }

    RTNode *child_rtnode = create_rtnode(sub, RT_SUB);
    add_child_resource_tree(parent_rtnode, child_rtnode);

    cJSON *noti_cjson, *sgn, *nev, *rep, *nct;
    noti_cjson = cJSON_CreateObject();
    cJSON_AddItemToObject(noti_cjson, "m2m:sgn", sgn = cJSON_CreateObject());
    cJSON_AddItemToObject(sgn, "nev", nev = cJSON_CreateObject());
    cJSON_AddNumberToObject(nev, "net", NET_CREATE_OF_DIRECT_CHILD_RESOURCE);
    cJSON_AddItemToObject(nev, "rep", rep = cJSON_CreateObject());
    cJSON_AddItemToObject(rep, "m2m:sub", cJSON_Duplicate(sub, true));
    cJSON_AddItemToObject(sgn, "vrq", cJSON_CreateBool(true));

    cJSON_AddStringToObject(sgn, "sur", ptr);
    if (notify_to_nu(child_rtnode, noti_cjson, NET_CREATE_OF_DIRECT_CHILD_RESOURCE) != RSC_OK)
    {
        cJSON_Delete(noti_cjson);
        cJSON_DetachItemFromObject(root, "m2m:sub");
        cJSON_Delete(root);
        delete_onem2m_resource(o2pt, child_rtnode);
        return handle_error(o2pt, RSC_SUBSCRIPTION_VERIFICATION_INITIATION_FAILED, "notification error");
    }

    make_response_body(o2pt, child_rtnode, sub);
    o2pt->rsc = RSC_CREATED;

    cJSON_DetachItemFromObject(root, "m2m:sub");
    cJSON_Delete(root);

    return RSC_CREATED;
}

int validate_sub(oneM2MPrimitive *o2pt, cJSON *sub, Operation op)
{
    cJSON *pjson = NULL;
    cJSON *net, *nct;
    char *ptr = NULL;

    pjson = cJSON_GetObjectItem(sub, "acpi");
    if (pjson)
    {
        int result = validate_acpi(o2pt, pjson, op);
        if (result != RSC_OK)
            return result;
    }

    if ((net = cJSON_GetObjectItem(cJSON_GetObjectItem(sub, "enc"), "net")))
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
            if (nct_val == NCT_MODIFIED_ATTRIBUTES)
            {
                return handle_error(o2pt, RSC_BAD_REQUEST, "Notification Content Type `Modified Attributes` is not supported");
            }
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
                break;
            }
        }
    }

    return RSC_OK;
}