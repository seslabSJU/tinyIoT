#include <stdlib.h>
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"
#include "../jsonparser.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;
extern RTNode *registrar_csr;

int create_ae(oneM2MPrimitive *o2pt, RTNode *parent_rtnode)
{
    int e = check_aei_duplicate(o2pt, parent_rtnode);
    if (e != -1)
        e = check_aei_invalid(o2pt);
    if (e != -1)
        e = check_rn_invalid(o2pt, RT_AE);
    if (e == -1)
        return o2pt->rsc;

    cJSON *root = cJSON_Duplicate(o2pt->request_pc, 1);
    cJSON *ae = cJSON_GetObjectItem(root, "m2m:ae");

    add_general_attribute(ae, parent_rtnode, RT_AE);
    int rsc = validate_ae(o2pt, ae, OP_CREATE);

    if (o2pt->fr && o2pt->fr[0] == 'C')
    {
        if (strlen(o2pt->fr) > 1)
        {
            cJSON *ri = cJSON_GetObjectItem(ae, "ri");
            cJSON_SetValuestring(ri, o2pt->fr);
        }
    }
    else if (o2pt->fr && o2pt->fr[0] == 'S')
    {
        if (strlen(o2pt->fr) > 1)
        {
            cJSON_ReplaceItemInObject(ae, "ri", cJSON_CreateString(o2pt->fr));
        }
        else
        {
            cJSON *ri = cJSON_GetObjectItem(ae, "ri");
            ri->valuestring[0] = 'S';
        }
    }
    else
    {
        o2pt->fr = strdup(cJSON_GetObjectItem(ae, "ri")->valuestring);
    }
    cJSON_AddStringToObject(ae, "aei", cJSON_GetObjectItem(ae, "ri")->valuestring);

    if (rsc != RSC_OK)
    {
        cJSON_Delete(root);
        return rsc;
    }
    if (SERVER_TYPE == MN_CSE || SERVER_TYPE == ASN_CSE)
    {
        if (!strcmp(o2pt->fr, "/S"))
        {
            cJSON_AddItemToArray(cJSON_GetObjectItem(ae, "at"), cJSON_CreateString(cJSON_GetObjectItem(registrar_csr->obj, "csi")->valuestring));
        }
    }
#if CSE_RVI >= RVI_3
    cJSON *final_at = cJSON_CreateArray();
    if (handle_annc_create(parent_rtnode, ae, cJSON_GetObjectItem(ae, "at"), final_at) == -1)
    {
        cJSON_Delete(root);
        cJSON_Delete(final_at);
        return handle_error(o2pt, RSC_BAD_REQUEST, "invalid attribute in `aa`");
    }

    if (cJSON_GetArraySize(final_at) > 0)
    {
        cJSON_DeleteItemFromObject(ae, "at");
        cJSON_AddItemToObject(ae, "at", final_at);
    }
    else
    {
        cJSON_Delete(final_at);
        cJSON_DeleteItemFromObject(ae, "at");
    }
#endif

    // Add uri attribute
    char *ptr = malloc(1024);
    cJSON *rn = cJSON_GetObjectItem(ae, "rn");
    sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);
    // Save to DB
    int result = db_store_resource(ae, ptr);

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
    RTNode *child_rtnode = create_rtnode(ae, RT_AE);
    add_child_resource_tree(parent_rtnode, child_rtnode);

    make_response_body(o2pt, child_rtnode);

    cJSON_DetachItemFromObject(root, "m2m:ae");
    cJSON_Delete(root);

    return o2pt->rsc = RSC_CREATED;
}

int update_ae(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{ // TODO deannounce when at removed
    char invalid_key[][8] = {"ty", "pi", "ri", "rn", "ct"};
    cJSON *m2m_ae = cJSON_GetObjectItem(o2pt->request_pc, "m2m:ae");
    cJSON *pjson = NULL;
    int invalid_key_size = sizeof(invalid_key) / (8 * sizeof(char));

    int updateAttrCnt = cJSON_GetArraySize(m2m_ae);

    for (int i = 0; i < invalid_key_size; i++)
    {
        if (cJSON_GetObjectItem(m2m_ae, invalid_key[i]))
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "unsupported attribute on update");
            return RSC_BAD_REQUEST;
        }
    }

    if (cJSON_GetObjectItem(m2m_ae, "acpi") && cJSON_GetArraySize(m2m_ae) > 1)
    {
        handle_error(o2pt, RSC_BAD_REQUEST, "only `acpi` should be updated");
        return RSC_BAD_REQUEST;
    }

    int result = validate_ae(o2pt, m2m_ae, OP_UPDATE);
    if (result != RSC_OK)
    {
        logger("O2", LOG_LEVEL_ERROR, "validation failed");
        return result;
    }
    cJSON *orig_acpi_obj = NULL;
    bool acpi_flag = false;
    if (cJSON_GetObjectItem(m2m_ae, "acpi"))
    {
        cJSON_ArrayForEach(orig_acpi_obj, cJSON_GetObjectItem(target_rtnode->obj, "acpi"))
        {
            if (cJSON_getArrayIdx(cJSON_GetObjectItem(m2m_ae, "acpi"), orig_acpi_obj->valuestring) == -1)
            {
                logger("UTIL", LOG_LEVEL_INFO, "acpi deleted : %s", orig_acpi_obj->valuestring);
                if (!has_acpi_update_privilege(o2pt, orig_acpi_obj->valuestring))
                {
                    return handle_error(o2pt, RSC_ORIGINATOR_HAS_NO_PRIVILEGE, "no privilege to update acpi");
                }
            }
        }
    }

    cJSON *at = NULL;
    if ((at = cJSON_GetObjectItem(m2m_ae, "at")))
    {
        cJSON *final_at = cJSON_CreateArray();
        handle_annc_update(target_rtnode, at, final_at);
        cJSON_DeleteItemFromObject(m2m_ae, "at");
        cJSON_AddItemToObject(m2m_ae, "at", final_at);
    }
    cJSON_AddItemToObject(m2m_ae, "lt", cJSON_CreateString(get_local_time(0)));

    // merge update resource
    update_resource(target_rtnode->obj, m2m_ae);

    // remove acpi if acpi is null
    if ((pjson = cJSON_GetObjectItem(target_rtnode->obj, "acpi")))
    {
        if (pjson->type == cJSON_NULL)
            cJSON_DeleteItemFromObject(target_rtnode->obj, "acpi");
        pjson = NULL;
    }

    result = db_update_resource(m2m_ae, cJSON_GetObjectItem(target_rtnode->obj, "ri")->valuestring, RT_AE);
    for (int i = 0; i < updateAttrCnt; i++)
    {
        cJSON_DeleteItemFromArray(m2m_ae, 0);
    }

    make_response_body(o2pt, target_rtnode);
    o2pt->rsc = RSC_UPDATED;

    return RSC_UPDATED;
}

int validate_ae(oneM2MPrimitive *o2pt, cJSON *ae, Operation op)
{
    extern cJSON *ATTRIBUTES;
    cJSON *pjson = NULL;

    char *ptr = NULL;
    if (!ae)
    {
        if (o2pt->rvi >= RVI_3)
            return handle_error(o2pt, RSC_CONTENTS_UNACCEPTABLE, "insufficient mandatory attribute(s)");
        else
            return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
    }
    if (cJSON_GetObjectItem(ae, "aei"))
    {
        return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `aei` is not allowed");
    }

    if (op == OP_CREATE)
    {

        pjson = cJSON_GetObjectItem(ae, "api");
        if (!pjson)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
        }
        ptr = pjson->valuestring;
        if (o2pt->rvi <= RVI_3)
        {
            if (ptr[0] != 'R' && ptr[0] != 'N' && ptr[0] != 'r')
            {
                handle_error(o2pt, RSC_BAD_REQUEST, "attribute `api` prefix is invalid");
                return RSC_BAD_REQUEST;
            }
        }
        else if (o2pt->rvi >= RVI_4)
        {
            if (ptr[0] != 'R' && ptr[0] != 'N')
            {
                return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `api` prefix is invalid");
            }
        }
        pjson = cJSON_GetObjectItem(ae, "rr");
        if (!pjson)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
        }
    }

    if ((pjson = cJSON_GetObjectItem(ae, "rn")))
    {
        if (strstr(pjson->valuestring, "/"))
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `rn` is invalid");
        }
    }
    cJSON *attrs = cJSON_GetObjectItem(ATTRIBUTES, "m2m:ae");
    cJSON *general = cJSON_GetObjectItem(ATTRIBUTES, "general");

    if (cJSON_GetObjectItem(ae, "aa"))
    {
        if (CSE_RVI < RVI_3)
            return handle_error(o2pt, RSC_BAD_REQUEST, "`aa` attribute is not supported");
        cJSON *aa_final = cJSON_CreateArray();
        cJSON_ArrayForEach(pjson, cJSON_GetObjectItem(ae, "aa"))
        {
            if (strcmp(pjson->valuestring, "ri") == 0)
                continue;
            if (strcmp(pjson->valuestring, "pi") == 0)
                continue;
            if (strcmp(pjson->valuestring, "rn") == 0)
                continue;
            if (strcmp(pjson->valuestring, "ct") == 0)
                continue;
            if (strcmp(pjson->valuestring, "lt") == 0)
                continue;
            if (strcmp(pjson->valuestring, "et") == 0)
                continue;
            if (strcmp(pjson->valuestring, "acpi") == 0)
                continue;

            if (cJSON_GetObjectItem(general, pjson->valuestring) || cJSON_GetObjectItem(attrs, pjson->valuestring))
            {
                cJSON_AddItemToArray(aa_final, cJSON_CreateString(pjson->valuestring));
            }
            else
            {
                return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `aa` is invalid");
            }
        }
        if (cJSON_GetArraySize(aa_final) > 0)
        {
            cJSON_ReplaceItemInObject(ae, "aa", aa_final);
        }
        else
        {
            cJSON_DeleteItemFromObject(ae, "aa");
            cJSON_AddItemToObject(ae, "aa", cJSON_CreateNull());
        }
    }

    if ((pjson = cJSON_GetObjectItem(ae, "at")))
    {
        if (CSE_RVI < RVI_3)
            return handle_error(o2pt, RSC_BAD_REQUEST, "`at` attribute is not supported");
    }

    pjson = cJSON_GetObjectItem(ae, "acpi");
    if (pjson)
    {
        if (op == OP_CREATE)
        {
            if (pjson)
            {
                int result = validate_acpi(o2pt, pjson, op_to_acop(op));
                if (result != RSC_OK)
                    return result;
            }
        }
        else if (op == OP_UPDATE)
        {
            if (pjson && cJSON_GetArraySize(pjson) > 1)
            {
                handle_error(o2pt, RSC_BAD_REQUEST, "only attribute `acpi` is allowed when updating `acpi`");
                return RSC_BAD_REQUEST;
            }

            validate_acpi(o2pt, pjson, op_to_acop(op));
        }

        if (!cJSON_IsNull(pjson) && cJSON_GetArraySize(pjson) == 0)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "empty `acpi` is not allowed");
        }
    }

    return RSC_OK;
}

int check_aei_invalid(oneM2MPrimitive *o2pt)
{
    char *aei = o2pt->fr;
    if (!aei || strlen(aei) == 0)
        return 0;
    cJSON *cjson = string_to_cjson_string_list_item(ALLOW_AE_ORIGIN);

    int size = cJSON_GetArraySize(cjson);
    for (int i = 0; i < size; i++)
    {
        cJSON *item = cJSON_GetArrayItem(cjson, i);
        char *origin = strdup(item->valuestring);
        if (origin[strlen(origin) - 1] == '*')
        {
            if (!strncmp(aei, origin, strlen(origin) - 1))
            {
                cJSON_Delete(cjson);
                free(origin);
                return 0;
            }
        }
        else if (!strcmp(origin, aei))
        {
            cJSON_Delete(cjson);
            free(origin);
            return 0;
        }

        free(origin);
        origin = NULL;
    }
    handle_error(o2pt, RSC_APP_RULE_VALIDATION_FAILED, "originator is invalid");
    return -1;
}
