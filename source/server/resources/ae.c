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
    logger("O2M", LOG_LEVEL_DEBUG, "create_ae 시작");

    if (!o2pt || !parent_rtnode) {
        logger("O2M", LOG_LEVEL_ERROR, "o2pt 또는 parent_rtnode가 NULL입니다.");
        return RSC_INTERNAL_SERVER_ERROR;
    }

    int e = check_aei_duplicate(o2pt, parent_rtnode);
    logger("O2M", LOG_LEVEL_DEBUG, "eeee%d", e);
    if (e != -1)
        e = check_aei_invalid(o2pt);
    if (e != -1)
        e = check_rn_invalid(o2pt, RT_AE);
    if (e == -1)
        return o2pt->rsc;

    logger("O2M", LOG_LEVEL_DEBUG, "%d", parent_rtnode->ty);
    if (parent_rtnode->ty != RT_CSE)
    {
        handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
        return o2pt->rsc;
    }

    logger("O2M", LOG_LEVEL_DEBUG, "parent_rtnode: %s", o2pt->request_pc);
    cJSON *root = cJSON_Duplicate(o2pt->request_pc, 1);
    logger("O2M", LOG_LEVEL_DEBUG, "root: %s", cJSON_Print(root));
    if (!root) {
        logger("O2M", LOG_LEVEL_ERROR, "cJSON_Duplicate 실패");
        return RSC_INTERNAL_SERVER_ERROR;
    }

    cJSON *ae = cJSON_GetObjectItem(root, "m2m:ae");
    if (!ae) {
        logger("O2M", LOG_LEVEL_ERROR, "cJSON_GetObjectItem 실패: m2m:ae");
        cJSON_Delete(root);
        return RSC_BAD_REQUEST;
    }

    add_general_attribute(ae, parent_rtnode, RT_AE);
    int rsc = validate_ae(o2pt, ae, OP_CREATE);

    if (o2pt->fr && o2pt->fr[0] == 'C')
    {
        if (strlen(o2pt->fr) > 1)
        {
            cJSON *ri = cJSON_GetObjectItem(ae, "ri");
            if (ri) {
                cJSON_SetValuestring(ri, o2pt->fr);
            } else {
                logger("O2M", LOG_LEVEL_ERROR, "cJSON_GetObjectItem 실패: ri");
                cJSON_Delete(root);
                return RSC_BAD_REQUEST;
            }
        }
    }
    else
    {
        cJSON *ri = cJSON_GetObjectItem(ae, "ri");
        if (ri) {
            o2pt->fr = strdup(ri->valuestring);
        } else {
            logger("O2M", LOG_LEVEL_ERROR, "cJSON_GetObjectItem 실패: ri");
            cJSON_Delete(root);
            return RSC_BAD_REQUEST;
        }
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
            cJSON *csi = cJSON_GetObjectItem(registrar_csr->obj, "csi");
            if (csi) {
                cJSON_AddItemToArray(cJSON_GetObjectItem(ae, "at"), cJSON_CreateString(csi->valuestring));
            } else {
                logger("O2M", LOG_LEVEL_ERROR, "cJSON_GetObjectItem 실패: csi");
                cJSON_Delete(root);
                return RSC_BAD_REQUEST;
            }
        }
    }

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

    // Add uri attribute
    char *ptr = malloc(1024);
    if (!ptr) {
        logger("O2M", LOG_LEVEL_ERROR, "메모리 할당 실패");
        cJSON_Delete(root);
        return RSC_INTERNAL_SERVER_ERROR;
    }

    cJSON *rn = cJSON_GetObjectItem(ae, "rn");
    if (!rn) {
        logger("O2M", LOG_LEVEL_ERROR, "Resource Name (rn) not found in AE");
        free(ptr);
        cJSON_Delete(root);
        return RSC_BAD_REQUEST;
    }

    sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);
    // Save to DB
    //여기로 들어가야함
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
    if (!child_rtnode) {
        logger("O2M", LOG_LEVEL_ERROR, "Failed to create RTNode for AE");
        cJSON_Delete(root);
        return RSC_INTERNAL_SERVER_ERROR;
    }
    add_child_resource_tree(parent_rtnode, child_rtnode);

    make_response_body(o2pt, child_rtnode);

    cJSON_DetachItemFromObject(root, "m2m:ae");
    cJSON_Delete(root);

    logger("O2M", LOG_LEVEL_DEBUG, "create_ae 완료");
    return o2pt->rsc = RSC_CREATED;
}


int update_ae(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{ // TODO deannounce when at removed
    char invalid_key[][8] = {"ty", "pi", "ri", "rn", "ct"};
    cJSON *m2m_ae = cJSON_GetObjectItem(o2pt->request_pc, "m2m:ae");
    cJSON *pjson = NULL;
    int invalid_key_size = sizeof(invalid_key) / (8 * sizeof(char));
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
    cJSON *acpi_obj = NULL;
    bool acpi_flag = false;
    if (cJSON_GetObjectItem(m2m_ae, "acpi"))
    {
        cJSON_ArrayForEach(acpi_obj, cJSON_GetObjectItem(target_rtnode->obj, "acpi"))
        {
            acpi_flag = false;
            cJSON_ArrayForEach(pjson, cJSON_GetObjectItem(m2m_ae, "acpi"))
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
                    return handle_error(o2pt, RSC_BAD_REQUEST, "no privilege to update acpi");
                }
            }
        }

        if (validate_acpi(o2pt, pjson, OP_UPDATE) != RSC_OK)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "no privilege to update acpi");
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

    // announce_to_annc(target_rtnode);

    result = db_update_resource(m2m_ae, cJSON_GetObjectItem(target_rtnode->obj, "ri")->valuestring, RT_AE);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "m2m:ae", target_rtnode->obj);

    make_response_body(o2pt, target_rtnode);
    o2pt->rsc = RSC_UPDATED;

    cJSON_DetachItemFromObject(root, "m2m:ae");
    cJSON_Delete(root);
    return RSC_UPDATED;
}

int validate_ae(oneM2MPrimitive *o2pt, cJSON *ae, Operation op)
{
    logger("O2M", LOG_LEVEL_DEBUG, "validate_ae 시작");

    extern cJSON *ATTRIBUTES;
    cJSON *pjson = NULL;

    char *ptr = NULL;
    if (!ae)
    {
        logger("O2M", LOG_LEVEL_ERROR, "ae is NULL");
        return handle_error(o2pt, RSC_CONTENTS_UNACCEPTABLE, "insufficient mandatory attribute(s)");
    }


    if (cJSON_GetObjectItem(ae, "aei"))
    {
        logger("O2M", LOG_LEVEL_ERROR, "attribute `aei` is not allowed");
        return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `aei` is not allowed");
    }


    if (op == OP_CREATE) // 문제 발견!!
    {
        pjson = cJSON_GetObjectItem(ae, "api");
        if (!pjson)
        {
            logger("O2M", LOG_LEVEL_ERROR, "insufficient mandatory attribute(s): api is missing");
            return handle_error(o2pt, RSC_CONTENTS_UNACCEPTABLE, "insufficient mandatory attribute(s)");
        }
        ptr = pjson->valuestring;
        logger("O2M", LOG_LEVEL_DEBUG, "api 값: %s", ptr);
        logger("O2M", LOG_LEVEL_DEBUG, "rvi 값: %d", o2pt->rvi);
        // o2pt->rvi = "3";
        if (!strcmp(o2pt->rvi, "1") || !strcmp(o2pt->rvi, "2") || !strcmp(o2pt->rvi, "2a") || !strcmp(o2pt->rvi, "3"))
        {
            if (ptr[0] != 'R' && ptr[0] != 'N' && ptr[0] != 'r')
            {
                logger("O2M", LOG_LEVEL_ERROR, "attribute `api` prefix is invalid");
                handle_error(o2pt, RSC_BAD_REQUEST, "attribute `api` prefix is invalid");
                return RSC_BAD_REQUEST;
            }
        }
        else
        {   
            if (ptr[0] != 'R' && ptr[0] != 'N')
            {
                logger("O2M", LOG_LEVEL_ERROR, "attribute `api` prefix is invalid");
                return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `api` prefix is invalid");
            }
        }
    }
    
    if ((pjson = cJSON_GetObjectItem(ae, "rn")))
    {
        if (strstr(pjson->valuestring, "/"))
        {
            logger("O2M", LOG_LEVEL_ERROR, "attribute `rn` is invalid");
            return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `rn` is invalid");
        }
    }

    cJSON *attrs = cJSON_GetObjectItem(ATTRIBUTES, "m2m:ae");
    if (!attrs) {
        logger("O2M", LOG_LEVEL_ERROR, "attrs is NULL");
    }
    cJSON *general = cJSON_GetObjectItem(ATTRIBUTES, "general");
    if (!general) {
        logger("O2M", LOG_LEVEL_ERROR, "general is NULL");
    }

    if (cJSON_GetObjectItem(ae, "aa"))
    {
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
                logger("O2M", LOG_LEVEL_ERROR, "attribute `aa` is invalid: %s", pjson->valuestring);
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

    pjson = cJSON_GetObjectItem(ae, "acpi");
    if (pjson)
    {
        if (op == OP_CREATE)
        {
            if (pjson)
            {
                int result = validate_acpi(o2pt, pjson, op);
                if (result != RSC_OK) {
                    logger("O2M", LOG_LEVEL_ERROR, "validate_acpi 실패: %d", result);
                    return result;
                }
            }
        }
        else if (op == OP_UPDATE)
        {
            if (pjson && cJSON_GetArraySize(pjson) > 1)
            {
                logger("O2M", LOG_LEVEL_ERROR, "only attribute `acpi` is allowed when updating `acpi`");
                handle_error(o2pt, RSC_BAD_REQUEST, "only attribute `acpi` is allowed when updating `acpi`");
                return RSC_BAD_REQUEST;
            }
        }

        if (!cJSON_IsNull(pjson) && cJSON_GetArraySize(pjson) == 0)
        {
            logger("O2M", LOG_LEVEL_ERROR, "empty `acpi` is not allowed");
            return handle_error(o2pt, RSC_BAD_REQUEST, "empty `acpi` is not allowed");
        }
    }

    logger("O2M", LOG_LEVEL_DEBUG, "validate_ae 완료");
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
    handle_error(o2pt, RSC_BAD_REQUEST, "originator is invalid");
    return -1;
}
