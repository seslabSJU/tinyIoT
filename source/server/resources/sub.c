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

    // Missing data is only detected on a <timeSeries>, so net=8 is meaningless anywhere
    // else. validate_sub() has no access to the parent, so the check lives here.
    if (rsc == RSC_OK && parent_rtnode && parent_rtnode->ty != RT_TS)
    {
        cJSON *enc_chk = cJSON_GetObjectItem(sub, "enc");
        cJSON *net_chk = enc_chk ? cJSON_GetObjectItem(enc_chk, "net") : NULL;
        cJSON *net_it = NULL;
        cJSON_ArrayForEach(net_it, net_chk)
        {
            if (cJSON_IsNumber(net_it) && (int)cJSON_GetNumberValue(net_it) == NET_REPORT_ON_MISSING_DATA_POINTS)
            {
                handle_error(o2pt, RSC_BAD_REQUEST, "`net` 8 is only allowed under a <timeSeries> resource");
                cJSON_Delete(root);
                return o2pt->rsc;
            }
        }
    }

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

    // TS-0004 Table 6.3.5.13-1: notificationEvent(net+rep) is optional
    // (0..1) and verificationRequest(vrq) is an independent top-level
    // field. A verification request only checks reachability, so it
    // shall not carry net/rep (which describe an actual resource
    // change).
    cJSON *noti_cjson, *sgn, *nct;
    RTNode *nu_rtnode;
    noti_cjson = cJSON_CreateObject();
    cJSON_AddItemToObject(noti_cjson, "m2m:sgn", sgn = cJSON_CreateObject());
    cJSON_AddStringToObject(sgn, "cr", o2pt->fr);
    {
        cJSON *sub_ri_obj = cJSON_GetObjectItem(sub, "ri");
        char *sur = (sub_ri_obj && cJSON_IsString(sub_ri_obj))
                        ? make_subscription_reference(sub_ri_obj->valuestring) : NULL;
        cJSON_AddStringToObject(sgn, "sur", sur ? sur : ptr);
        free(sur);
    }
    cJSON_AddBoolToObject(sgn, "vrq", true);
    cJSON_ArrayForEach(pjson, cJSON_GetObjectItem(sub, "nu"))
    {

        nu_rtnode = find_rtnode(pjson->valuestring);
        if (nu_rtnode)
        {
            // TS-0001 9.6.8: a Resource-ID target may be an AE or a CSE;
            // TS-0001 10.2.11.2: verification is optional and skipped for the originator
            if (nu_rtnode->ty == RT_CSR)
            {
                continue;
            }
            if (nu_rtnode->ty != RT_AE)
            {
                handle_error(o2pt, RSC_SUBSCRIPTION_VERIFICATION_INITIATION_FAILED, "nu is invalid");
                cJSON_Delete(noti_cjson);
                cJSON_Delete(root);
                return RSC_SUBSCRIPTION_VERIFICATION_INITIATION_FAILED;
            }
            cJSON *aei = cJSON_GetObjectItem(nu_rtnode->obj, "aei");
            if (aei && aei->valuestring && strcmp(aei->valuestring, o2pt->fr) == 0)
            {
                continue;
            }
        }
        else if (!strcmp(pjson->valuestring, o2pt->fr))
        {
            continue;
        }
        // A URL-format notificationURI is verified like any other target: the
        // verification request is what proves the endpoint is reachable, and the
        // conformance tests expect it. send_verification_request() dispatches
        // PROTOCOL_BINDING targets over their own binding.
        result = send_verification_request(o2pt->to, pjson->valuestring, noti_cjson);
        

        if (result == RSC_SUBSCRIPTION_CREATOR_HAS_NO_PRIVILEGE ||
            result == RSC_SUBSCRIPTION_HOST_HAS_NO_PRIVILEGE)
        {
            cJSON_Delete(noti_cjson);
            cJSON_Delete(root);
            return handle_error(o2pt, result, "subscription verification error");
        }
        else if (result / 1000 == 4 || result / 1000 == 5)
        {
            cJSON_Delete(noti_cjson);
            cJSON_Delete(root);
            return handle_error(o2pt, RSC_SUBSCRIPTION_VERIFICATION_INITIATION_FAILED, "subscription verification error");
        }
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

    if (parent_rtnode->ty == RT_CIN) {
        handle_error(o2pt,
                     RSC_TARGET_NOT_SUBSCRIBABLE,
                     "TARGET_NOT_SUBSCRIPBABLE");
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
    char invalid_key[][8] = {"ty", "pi", "ri", "rn", "ct", "su"};
    cJSON *pjson = NULL;
    cJSON *m2m_sub = cJSON_GetObjectItem(o2pt->request_pc, "m2m:sub");
    int invalid_key_size = sizeof(invalid_key) / (8 * sizeof(char));

    for (int i = 0; i < invalid_key_size; i++)
    {
        if (cJSON_GetObjectItem(m2m_sub, invalid_key[i]))
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "{\"m2m:dbg\": \"unsupported attribute on update\"}");
            return RSC_BAD_REQUEST;
        }
    }

    cJSON *sub = target_rtnode->obj;

    if (validate_sub(o2pt, m2m_sub, o2pt->op) != RSC_OK)
    {
        return o2pt->rsc;
    }

    cJSON *new_nu = cJSON_GetObjectItem(m2m_sub, "nu");
    if (new_nu)
    {
        // TS-0004 Table 6.3.5.13-1: notificationEvent(net+rep) is optional
        // (0..1) and verificationRequest(vrq) is an independent top-level
        // field. A verification request only checks reachability, so it
        // shall not carry net/rep (which describe an actual resource
        // change).
        cJSON *noti_cjson, *sgn;
        RTNode *nu_rtnode = NULL;
        noti_cjson = cJSON_CreateObject();
        cJSON_AddItemToObject(noti_cjson, "m2m:sgn", sgn = cJSON_CreateObject());
        cJSON_AddStringToObject(sgn, "cr", o2pt->fr);
        {
            char *sur = make_subscription_reference(get_ri_rtnode(target_rtnode));
            cJSON_AddStringToObject(sgn, "sur", sur ? sur : target_rtnode->uri);
            free(sur);
        }
        cJSON_AddBoolToObject(sgn, "vrq", true);
        cJSON_ArrayForEach(pjson, new_nu)
        {
            if (!strcmp(pjson->valuestring, o2pt->fr))
            {
                continue;
            }
            nu_rtnode = find_rtnode(pjson->valuestring);
            if (nu_rtnode)
            {
                if (nu_rtnode->ty == RT_CSR)
                {
                    continue;
                }
                if (nu_rtnode->ty != RT_AE)
                {
                    cJSON_Delete(noti_cjson);
                    return handle_error(o2pt, RSC_SUBSCRIPTION_VERIFICATION_INITIATION_FAILED, "nu is invalid");
                }
                cJSON *aei = cJSON_GetObjectItem(nu_rtnode->obj, "aei");
                if (aei && aei->valuestring && !strcmp(aei->valuestring, o2pt->fr))
                {
                    continue;
                }
            }
            // TS-0001 10.2.11.2/10.2.11.4: URL-format targets shall not be verified
            if (checkResourceAddressingType(pjson->valuestring) == PROTOCOL_BINDING)
            {
                continue;
            }
            int result = send_verification_request(o2pt->to, pjson->valuestring, noti_cjson);
            if (result == RSC_SUBSCRIPTION_CREATOR_HAS_NO_PRIVILEGE ||
                result == RSC_SUBSCRIPTION_HOST_HAS_NO_PRIVILEGE)
            {
                cJSON_Delete(noti_cjson);
                return handle_error(o2pt, result, "subscription verification failed");
            }
            // TS-0001 10.2.11.2: verification is optional; only an explicit failed
            // verification response rejects the update
            if (result / 1000 == 4 || result / 1000 == 5)
            {
                logger("SUB", LOG_LEVEL_WARN, "verification not completed for %s (rsc=%d)", pjson->valuestring, result);
            }
        }
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

    make_response_body(o2pt, target_rtnode);
    o2pt->rsc = RSC_UPDATED;
    return RSC_UPDATED;
}

int validate_sub(oneM2MPrimitive *o2pt, cJSON *sub, Operation op)
{
    cJSON *pjson = NULL;
    cJSON *enc, *net, *nct;
    char *ptr = NULL;

    {
        char *ma_error = NULL;
        int ma_rsc = validate_mandatory_attrs(RT_SUB, sub, op, &ma_error);
        if (ma_rsc != RSC_OK)
        {
            return handle_error(o2pt, ma_rsc, ma_error);
        }
    }

    pjson = cJSON_GetObjectItem(sub, "acpi");
    if (pjson)
    {
        if (op == OP_UPDATE && cJSON_GetArraySize(sub) > 1)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `acpi` shall be the only attribute in an UPDATE request");
        }
        int result = validate_acpi(o2pt, pjson, op);
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
                // Missing-data notifications carry a timeSeriesNotification, so this is
                // the only notificationContentType that can represent them.
                if (nct_val != NCT_TIMESERIES_NOTIFICATION)
                {
                    return handle_error(o2pt, RSC_BAD_REQUEST,
                                        "`nct` must be 5 (timeSeries notification) for `net` 8");
                }
                // `enc.md.num` is what the Hosting CSE counts against; without it no
                // notification can ever be emitted.
                {
                    cJSON *md = cJSON_GetObjectItem(enc, "md");
                    cJSON *md_num = md ? cJSON_GetObjectItem(md, "num") : NULL;
                    if (!md || !md_num || !cJSON_IsNumber(md_num) || cJSON_GetNumberValue(md_num) <= 0)
                    {
                        return handle_error(o2pt, RSC_BAD_REQUEST,
                                            "`enc/md/num` is required for `net` 8");
                    }
                }
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
        cJSON *nu_item = NULL;
        cJSON_ArrayForEach(nu_item, pjson)
        {
            if (!cJSON_IsString(nu_item) || !nu_item->valuestring)
            {
                return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `nu` is invalid");
            }
        }
    }

    return RSC_OK;
}
