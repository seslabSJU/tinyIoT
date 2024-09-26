#include <stdlib.h>
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;

int create_cnt(oneM2MPrimitive *o2pt, RTNode *parent_rtnode)
{
    cJSON *root = cJSON_Duplicate(o2pt->request_pc, 1);
    cJSON *pjson = NULL;

    cJSON *cnt = cJSON_GetObjectItem(root, "m2m:cnt");

    add_general_attribute(cnt, parent_rtnode, RT_CNT);

    int rsc = validate_cnt(o2pt, cnt, OP_CREATE);
    if (rsc != RSC_OK)
    {
        cJSON_Delete(root);
        return rsc;
    }

    // Add cr attribute
    if ((pjson = cJSON_GetObjectItem(cnt, "cr")))
    {
        if (pjson->type == cJSON_NULL)
        {
            cJSON_AddStringToObject(cnt, "cr", o2pt->fr);
        }
        else
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "creator attribute with arbitary value is not allowed");
            cJSON_Delete(root);
            return o2pt->rsc;
        }
    }

    // Add st, cni, cbs, mni, mbs attribute
    cJSON_AddNumberToObject(cnt, "st", 0);
    cJSON_AddNumberToObject(cnt, "cni", 0);
    cJSON_AddNumberToObject(cnt, "cbs", 0);
#if CSE_RVI >= RVI_3
    cJSON *final_at = cJSON_CreateArray();
    if (handle_annc_create(parent_rtnode, cnt, cJSON_GetObjectItem(cnt, "at"), final_at) == -1)
    {
        cJSON_Delete(root);
        cJSON_Delete(final_at);
        return handle_error(o2pt, RSC_BAD_REQUEST, "invalid attribute in `aa`");
    }

    if (cJSON_GetArraySize(final_at) > 0)
    {
        cJSON_DeleteItemFromObject(cnt, "at");
        cJSON_AddItemToObject(cnt, "at", final_at);
    }
    else
    {
        cJSON_Delete(final_at);
    }
#endif
    if (rsc != RSC_OK)
    {
        cJSON_Delete(root);
        return rsc;
    }
    // add default mni if not set
    if (!cJSON_GetObjectItem(cnt, "mni"))
    {
        cJSON_AddNumberToObject(cnt, "mni", DEFAULT_MAX_NR_INSTANCES);
    }

    o2pt->rsc = RSC_CREATED;

    // Add uri attribute
    char *ptr = malloc(1024);
    cJSON *rn = cJSON_GetObjectItem(cnt, "rn");
    sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);

    // Store to DB
    int result = db_store_resource(cnt, ptr);
    if (result != 1)
    {
        handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail");
        cJSON_Delete(root);
        free(ptr);
        ptr = NULL;
        return o2pt->rsc;
    }
    free(ptr);
    ptr = NULL;

    RTNode *child_rtnode = create_rtnode(cnt, RT_CNT);
    add_child_resource_tree(parent_rtnode, child_rtnode);
    make_response_body(o2pt, child_rtnode);

    cJSON_DetachItemFromObject(root, "m2m:cnt");
    cJSON_Delete(root);

    return RSC_CREATED;
}

int update_cnt(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{
    char invalid_key[][9] = {"ty", "pi", "ri", "rn", "ct", "cr"};
    cJSON *m2m_cnt = cJSON_GetObjectItem(o2pt->request_pc, "m2m:cnt");
    int invalid_key_size = sizeof(invalid_key) / (9 * sizeof(char));

    int updateAttrCnt = cJSON_GetArraySize(m2m_cnt);

    for (int i = 0; i < invalid_key_size; i++)
    {
        if (cJSON_GetObjectItem(m2m_cnt, invalid_key[i]))
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "unsupported attribute on update");
            return RSC_BAD_REQUEST;
        }
    }

    cJSON *cnt = target_rtnode->obj;
    int result;
    cJSON *pjson = NULL;
    cJSON *acpi_obj = NULL;
    bool acpi_flag = false;

    result = validate_cnt(o2pt, m2m_cnt, OP_UPDATE); // TODO - add UPDATE validation

    // update acpi
    if (cJSON_GetObjectItem(m2m_cnt, "acpi"))
    {

        // delete removed acpi
        cJSON_ArrayForEach(acpi_obj, cJSON_GetObjectItem(cnt, "acpi"))
        {
            acpi_flag = false;
            cJSON_ArrayForEach(pjson, cJSON_GetObjectItem(m2m_cnt, "acpi"))
            {
                if (strcmp(acpi_obj->valuestring, pjson->valuestring) != 0)
                {
                    acpi_flag = true;
                    break;
                }
            }
            if (!acpi_flag)
            {
                logger("UTIL", LOG_LEVEL_INFO, "acpi %s", acpi_obj->valuestring);
                if (!has_acpi_update_privilege(o2pt, acpi_obj->valuestring))
                {
                    return handle_error(o2pt, RSC_ORIGINATOR_HAS_NO_PRIVILEGE, "no privilege to update acpi");
                }
            }
        }

        // validate new acpi
        if (cJSON_GetArraySize(cJSON_GetObjectItem(m2m_cnt, "acpi")) > 0)
        {
            if (validate_acpi(o2pt, cJSON_GetObjectItem(m2m_cnt, "acpi"), ACOP_UPDATE) != RSC_OK)
            {
                return handle_error(o2pt, RSC_BAD_REQUEST, "no privilege to update acpi");
            }
        }
    }

    if (result != RSC_OK)
        return result;

    cJSON_AddNumberToObject(m2m_cnt, "st", cJSON_GetObjectItem(cnt, "st")->valueint + 1);

    cJSON *at = NULL;
    if ((at = cJSON_GetObjectItem(m2m_cnt, "at")))
    {
        cJSON *final_at = cJSON_CreateArray();
        handle_annc_update(target_rtnode, at, final_at);
        cJSON_DeleteItemFromObject(m2m_cnt, "at");
        cJSON_AddItemToObject(m2m_cnt, "at", final_at);
    }

    cJSON_AddItemToObject(m2m_cnt, "lt", cJSON_CreateString(get_local_time(0)));

    update_resource(target_rtnode->obj, m2m_cnt);

    delete_cin_under_cnt_mni_mbs(target_rtnode);

    result = db_update_resource(m2m_cnt, cJSON_GetObjectItem(target_rtnode->obj, "ri")->valuestring, RT_CNT);

    for (int i = 0; i < updateAttrCnt; i++)
    {
        cJSON_DeleteItemFromArray(m2m_cnt, 0);
    }

    make_response_body(o2pt, target_rtnode);
    o2pt->rsc = RSC_UPDATED;
    return RSC_UPDATED;
}

int validate_cnt(oneM2MPrimitive *o2pt, cJSON *cnt, Operation op)
{
    cJSON *pjson = NULL;
    char *ptr = NULL;
    if (!cnt)
    {
        if (o2pt->rvi >= 3)
            return handle_error(o2pt, RSC_CONTENTS_UNACCEPTABLE, "insufficient mandatory attribute(s)");
        else
            return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
    }

    pjson = cJSON_GetObjectItem(cnt, "rn");
    if (pjson)
    {
        if (!strcmp(pjson->valuestring, "la") || !strcmp(pjson->valuestring, "ol"))
        {
            handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "attribute `rn` is invalid");
            return RSC_BAD_REQUEST;
        }

        if (!strcmp(pjson->valuestring, "latest") || !strcmp(pjson->valuestring, "oldest"))
        {
            handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "attribute `rn` is invalid");
            return RSC_BAD_REQUEST;
        }
    }

    if (op == OP_CREATE)
    {
        pjson = cJSON_GetObjectItem(cnt, "acpi");
        if (pjson && cJSON_GetArraySize(pjson) > 0)
        {
            int result = validate_acpi(o2pt, pjson, ACOP_CREATE);
            if (result != RSC_OK)
                return result;
        }
    }

    if (op == OP_UPDATE)
    {
        pjson = cJSON_GetObjectItem(cnt, "acpi");
        if (pjson && cJSON_GetArraySize(cnt) > 1)
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "only attribute `acpi` is allowed when updating `acpi`");
            return RSC_BAD_REQUEST;
        }
    }

    pjson = cJSON_GetObjectItem(cnt, "mni");
    if (pjson && pjson->valueint < 0)
    {
        handle_error(o2pt, RSC_BAD_REQUEST, "attribute `mni` is invalid");
        return RSC_BAD_REQUEST;
    }

    pjson = cJSON_GetObjectItem(cnt, "mbs");
    if (pjson && pjson->valueint < 0)
    {
        handle_error(o2pt, RSC_BAD_REQUEST, "attribute `mbs` is invalid");
        return RSC_BAD_REQUEST;
    }

    cJSON *aa = cJSON_GetObjectItem(cnt, "aa");
    cJSON *attr = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(RT_CNT));
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