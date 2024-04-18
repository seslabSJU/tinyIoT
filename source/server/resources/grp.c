#include <stdlib.h>
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;

int create_grp(oneM2MPrimitive *o2pt, RTNode *parent_rtnode)
{
    int rsc = 0;
    if (parent_rtnode->ty != RT_AE && parent_rtnode->ty != RT_CSE)
    {
        return o2pt->rsc = RSC_INVALID_CHILD_RESOURCETYPE;
    }

    cJSON *root = cJSON_Duplicate(o2pt->request_pc, 1);
    cJSON *grp = cJSON_GetObjectItem(root, "m2m:grp");

    add_general_attribute(grp, parent_rtnode, RT_GRP);
    rsc = validate_grp(o2pt, grp);
    if (rsc >= 4000)
    {
        return logger("O2M", LOG_LEVEL_DEBUG, "Group Validation failed");
    }
    cJSON *pjson = NULL;
    // Add cr attribute
    if ((pjson = cJSON_GetObjectItem(grp, "cr")))
    {
        if (pjson->type == cJSON_NULL)
        {
            cJSON_AddStringToObject(grp, "cr", o2pt->fr);
        }
        else
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "creator attribute with arbitary value is not allowed");
            cJSON_Delete(root);
            return o2pt->rsc;
        }
    }

    cJSON_AddItemToObject(grp, "cnm", cJSON_CreateNumber(cJSON_GetArraySize(cJSON_GetObjectItem(grp, "mid"))));

    // if(o2pt->pc) free(o2pt->pc);
    // o2pt->pc = cJSON_PrintUnformatted(root);

    o2pt->rsc = RSC_CREATED;

    cJSON *rn = cJSON_GetObjectItem(grp, "rn");
    char *uri = (char *)malloc((strlen(rn->valuestring) + strlen(parent_rtnode->uri) + 2) * sizeof(char));
    sprintf(uri, "%s/%s", parent_rtnode->uri, rn->valuestring);

    int result = db_store_resource(grp, uri);
    if (result != 1)
    {
        handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail");
        return RSC_INTERNAL_SERVER_ERROR;
    }

    RTNode *child_rtnode = create_rtnode(grp, RT_GRP);
    add_child_resource_tree(parent_rtnode, child_rtnode);
    set_grp_member(child_rtnode);

    make_response_body(o2pt, child_rtnode);

    cJSON_DetachItemFromObject(root, "m2m:grp");
    cJSON_Delete(root);

    free(uri);
    uri = NULL;
    return rsc;
}

/* GROUP IMPLEMENTATION */
int update_grp(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{
    int rsc = 0, result = 0;
    char invalid_key[6][4] = {"ty", "pi", "ri", "rn", "ct", "mtv"};
    cJSON *m2m_grp = cJSON_GetObjectItem(o2pt->request_pc, "m2m:grp");
    cJSON *pjson = NULL;

    int invalid_key_size = 6;
    for (int i = 0; i < invalid_key_size; i++)
    {
        if (cJSON_GetObjectItem(m2m_grp, invalid_key[i]))
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "{\"m2m:dbg\": \"unsupported attribute on update\"}");
            return RSC_BAD_REQUEST;
        }
    }

    if ((rsc = validate_grp_update(o2pt, target_rtnode->obj, m2m_grp)) >= 4000)
    {
        o2pt->rsc = rsc;
        return rsc;
    }

    if ((pjson = cJSON_GetObjectItem(m2m_grp, "mid")))
    {
        cJSON_SetIntValue(cJSON_GetObjectItem(target_rtnode->obj, "cnm"), cJSON_GetArraySize(pjson));
    }
    update_resource(target_rtnode->obj, m2m_grp);
    result = db_update_resource(m2m_grp, cJSON_GetObjectItem(target_rtnode->obj, "ri")->valuestring, RT_GRP);

    if (!result)
    {
        logger("O2M", LOG_LEVEL_ERROR, "DB update Failed");
        return RSC_INTERNAL_SERVER_ERROR;
    }

    set_grp_member(target_rtnode);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "m2m:grp", target_rtnode->obj);
    make_response_body(o2pt, target_rtnode);
    o2pt->rsc = RSC_UPDATED;

    cJSON_DetachItemFromObject(root, "m2m:grp");
    cJSON_Delete(root);
    root = NULL;

    return RSC_UPDATED;
}
int validate_grp(oneM2MPrimitive *o2pt, cJSON *grp)
{
    logger("UTIL", LOG_LEVEL_DEBUG, "Validating GRP");
    int rsc = 0;
    bool hasFopt = false;
    bool isLocalResource = true;
    char *p = NULL;
    char *tStr = NULL;
    cJSON *pjson = NULL;

    int mt = 0;
    int csy = DEFAULT_CONSISTENCY_POLICY;

    if ((pjson = cJSON_GetObjectItem(grp, "mt")))
    {
        mt = pjson->valueint;
    }
    else
    {
        mt = RT_MIXED; // default is MIXED
        cJSON_AddItemToObject(grp, "mt", cJSON_CreateNumber(mt));
        // return handle_error(o2pt, RSC_BAD_REQUEST, "`mt` is mandatory");
    }

    if ((pjson = cJSON_GetObjectItem(grp, "csy")))
    {
        csy = pjson->valueint;
        if (csy < 0 || csy > 3)
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "`csy` is invalid");
            return RSC_BAD_REQUEST;
        }
    }

    if ((pjson = cJSON_GetObjectItem(grp, "mtv")))
    {
        if (pjson->valueint == 1)
            return RSC_OK;
    }

    cJSON *midArr = cJSON_GetObjectItem(grp, "mid");
    cJSON *mid_obj = NULL;

    if (!midArr)
    {
        handle_error(o2pt, RSC_BAD_REQUEST, "`mid` is mandatory");
        return RSC_BAD_REQUEST;
    }

    if (midArr && !cJSON_IsArray(midArr))
    {
        handle_error(o2pt, RSC_BAD_REQUEST, "`mid` should be array");
        return RSC_BAD_REQUEST;
    }

    if ((pjson = cJSON_GetObjectItem(grp, "mnm")))
    {

        if (pjson->valueint < cJSON_GetArraySize(midArr))
        {
            return handle_error(o2pt, RSC_MAX_NUMBER_OF_MEMBER_EXCEEDED, "`mnm` is less than `mid` size");
        }
    }

    // validate macp
    if ((pjson = cJSON_GetObjectItem(grp, "macp")))
    {
        if (!cJSON_IsArray(pjson))
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "`macp` should be array");
            return RSC_BAD_REQUEST;
        }
        else if (cJSON_GetArraySize(pjson) == 0)
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "`macp` should not be empty");
        }
        else
        {
            if (validate_acpi(o2pt, pjson, OP_CREATE) != RSC_OK)
                return o2pt->rsc;
        }
    }

    pjson = cJSON_GetObjectItem(grp, "acpi");
    if (pjson)
    {
        int result = validate_acpi(o2pt, pjson, OP_CREATE);
        if (result != RSC_OK)
            return result;
    }

    cJSON *final_mid = cJSON_CreateArray();

    int result = validate_grp_member(grp, final_mid, csy, mt);
    if (result >= RSC_BAD_REQUEST)
    {
        return handle_error(o2pt, result, "GRP member is invalid");
    }

    if (result == 0)
    {
        cJSON_AddBoolToObject(grp, "mtv", false);
    }
    else if (result == 1)
    {
        cJSON_AddBoolToObject(grp, "mtv", true);
    }

    cJSON_DeleteItemFromObject(grp, "mid");
    cJSON_AddItemToObject(grp, "mid", final_mid);
    return RSC_OK;
}

/**
 * @brief validate grp member ids\nreturns validated grp member ids
 * @param grp cJSON of grp resource
 * @param final_mid cJSON of validated grp member ids
 * @param csy consistency policy
 * @param mt member type
 * @return 0 if mtv is false, 1 if mtv is true, RSC error code if error
 */
int validate_grp_member(cJSON *grp, cJSON *final_mid, int csy, int mt)
{
    bool isLocalResource = false;
    cJSON *mid_obj = NULL;
    cJSON *midArr = cJSON_GetObjectItem(grp, "mid");
    cJSON *pjson = NULL;
    bool hasFopt = false;
    bool mtv = false;
    int resourceLocation = 0; // 0 : not Found 1 : local, 2 : remote
    char *p = NULL;
    char tStr[MAX_URI_LENGTH] = {0};

    int rsc = 0;

    bool duplicated = false;
    RTNode *rt_node;
    cJSON_ArrayForEach(mid_obj, midArr)
    {
        rsc = 0;
        memset(tStr, 0, MAX_URI_LENGTH);
        resourceLocation = 0;
        char *mid = cJSON_GetStringValue(mid_obj);

        // resource check
        hasFopt = isUriFopt(mid);
        strncpy(tStr, mid, strlen(mid));
        if (hasFopt && strlen(mid) > 5) // remove fopt
            tStr[strlen(mid) - 5] = '\0';
        // logger("util-t", LOG_LEVEL_DEBUG, "%s",tStr);
        logger("UTIL", LOG_LEVEL_DEBUG, "Validating GRP member %s", tStr);
        ResourceAddressingType rat = checkResourceAddressingType(tStr);

        // check local resource
        if (rat == CSE_RELATIVE)
        {
            logger("UTIL", LOG_LEVEL_DEBUG, "CSE RELATIVE", tStr);
            rt_node = find_rtnode(tStr);
            resourceLocation = 1;
            // check remote resource
        }
        else if (rat == SP_RELATIVE)
        {
            logger("UTIL", LOG_LEVEL_DEBUG, "SP RELATIVE", tStr);
            if (isSpRelativeLocal(tStr))
            {
                logger("UTIL", LOG_LEVEL_DEBUG, "SP RELATIVE LOCAL", tStr);
                rt_node = find_rtnode(tStr + strlen(CSE_BASE_RI) + 2);
                resourceLocation = 1;
            }
            else
            {
                logger("UTIL", LOG_LEVEL_DEBUG, "SP RELATIVE REMOTE", tStr);
                rt_node = get_remote_resource(tStr, &rsc);
                resourceLocation = 2;
                if (!rt_node)
                { // error
                    logger("UTIL", LOG_LEVEL_DEBUG, "Remote GRP member %s not present", tStr);
                    if (rsc == RSC_ORIGINATOR_HAS_NO_PRIVILEGE)
                    {
                        logger("UTIL", LOG_LEVEL_DEBUG, "GRP member %s has no privilege rsc = %d", tStr, rt_node);
                        if (csy == CSY_ABANDON_GROUP)
                            return RSC_RECEIVER_HAS_NO_PRIVILEGES;
                    }

                    if (csy == CSY_ABANDON_GROUP)
                        return RSC_GROUPMEMBER_TYPE_INCONSISTENT;
                    else if (csy == CSY_ABANDON_MEMBER)
                    {
                        // CSY_ABANDON_MEMBER - member unsaved
                        continue;
                    }
                }
                if (rt_node == NULL)
                {
                    return 0;
                }
            }
        }
        else
        {
            logger("UTIL", LOG_LEVEL_DEBUG, "GRP member %s not present", tStr);
            if (csy == CSY_ABANDON_GROUP)
                return RSC_BAD_REQUEST;
            else if (csy == CSY_ABANDON_MEMBER)
            {
                // CSY_ABANDON_MEMBER - member unsaved
                continue;
            }
        }
        if (!rt_node)
        {
            logger("UTIL", LOG_LEVEL_DEBUG, "GRP member %s not present", tStr);
            if (csy == CSY_ABANDON_GROUP)
                return RSC_BAD_REQUEST;
            else if (csy == CSY_ABANDON_MEMBER)
            {
                // CSY_ABANDON_MEMBER - member unsaved
                continue;
            }
        }

        if (!hasFopt && mt != 0 && rt_node->ty != mt)
        {
            logger("UTIL", LOG_LEVEL_DEBUG, "GRP member %s type is inconsistent", tStr);
            if (csy == CSY_ABANDON_GROUP)
            {
                if (resourceLocation == 2)
                    free_rtnode(rt_node);
                return RSC_GROUPMEMBER_TYPE_INCONSISTENT;
            }
            else if (csy == CSY_SET_MIXED)
            {
                cJSON_SetIntValue(cJSON_GetObjectItem(grp, "mt"), RT_MIXED);
                mt = RT_MIXED;
            }
            else
            {
                // CSY_ABANDON_MEMBER - member unsaved
                if (resourceLocation == 2)
                    free_rtnode(rt_node);
                continue;
            }
        }

        if (hasFopt && rt_node->ty == RT_GRP)
        {
            logger("UTIL", LOG_LEVEL_DEBUG, "Validating GRP member of %s", tStr);
            cJSON *pGrp = rt_node->obj;
            cJSON *result = cJSON_CreateArray();
            rsc = validate_grp_member(pGrp, result, CSY_ABANDON_GROUP, mt); // validate sub grp member
            if (rsc == RSC_GROUPMEMBER_TYPE_INCONSISTENT)
            {
                if (csy == CSY_ABANDON_GROUP)
                    return RSC_GROUPMEMBER_TYPE_INCONSISTENT;
                else if (csy == CSY_ABANDON_MEMBER)
                {
                    // CSY_ABANDON_MEMBER - member unsaved
                    if (resourceLocation == 2)
                        free_rtnode(rt_node);
                    continue;
                }
            }
            else
            {
                logger("UTIL", LOG_LEVEL_DEBUG, "Validated GRP member of %s successful", tStr);
            }
        }

        // check duplicated with heirarchical address
        for (int j = 0; j < cJSON_GetArraySize(final_mid); j++)
        {
            if (!strcmp(rt_node->uri, cJSON_GetStringValue(cJSON_GetArrayItem(final_mid, j))))
            {
                duplicated = true;
                break;
            }
        }

        if (duplicated)
        {
            if (resourceLocation == 2)
                free_rtnode(rt_node);
            duplicated = false;
            continue;
        }
        if (hasFopt)
        {
            sprintf(tStr, "%s/fopt", rt_node->uri);
            cJSON_AddItemToArray(final_mid, cJSON_CreateString(tStr));
        }
        else
        {
            cJSON_AddItemToArray(final_mid, cJSON_CreateString(rt_node->uri));
        }

        if (resourceLocation == 2)
            free_rtnode(rt_node);
    }
    return 1;
}

int validate_grp_update(oneM2MPrimitive *o2pt, cJSON *grp_old, cJSON *grp_new)
{
    logger("UTIL", LOG_LEVEL_DEBUG, "Validating GRP Update");
    int rsc = 0;
    bool hasFopt = false;
    bool isLocalResource = true;
    char *p = NULL;
    char *tStr = NULL;
    cJSON *pjson = NULL;

    int mt = 0;
    int csy = DEFAULT_CONSISTENCY_POLICY;

    pjson = cJSON_GetObjectItem(grp_new, "acpi");
    if (pjson)
    {
        if (cJSON_GetArraySize(grp_new) > 1)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "`acpi` should be only attribute when updating");
        }
        int result = validate_acpi(o2pt, pjson, OP_UPDATE);
        if (result != RSC_OK)
            return result;
    }

    if ((pjson = cJSON_GetObjectItem(grp_new, "mt")))
    {
        mt = pjson->valueint;
    }

    if ((pjson = cJSON_GetObjectItem(grp_new, "csy")))
    {
        csy = pjson->valueint;
        if (csy < 0 || csy > 3)
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "`csy` is invalid");
            return RSC_BAD_REQUEST;
        }
    }

    cJSON *midArr = cJSON_GetObjectItem(grp_new, "mid");
    cJSON *mid_obj = NULL;
    if (midArr && !cJSON_IsArray(midArr))
    {
        handle_error(o2pt, RSC_BAD_REQUEST, "`mid` should be array");
        return RSC_BAD_REQUEST;
    }

    if ((pjson = cJSON_GetObjectItem(grp_new, "mnm")))
    {
        if (pjson->valueint < cJSON_GetArraySize(midArr))
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "`mnm` is less than `mid` size");
            return RSC_BAD_REQUEST;
        }

        if (pjson->valueint < cJSON_GetObjectItem(grp_old, "cnm")->valueint)
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "`mnm` can't be smaller than `cnm` size");
            return RSC_BAD_REQUEST;
        }
    }
    else if ((pjson = cJSON_GetObjectItem(grp_old, "mnm")))
    {
        if (pjson->valueint < cJSON_GetArraySize(midArr))
        {
            handle_error(o2pt, RSC_MAX_NUMBER_OF_MEMBER_EXCEEDED, "`mnm` is less than `mid` size");
            return RSC_MAX_NUMBER_OF_MEMBER_EXCEEDED;
        }
    }

    if ((pjson = cJSON_GetObjectItem(grp_new, "macp")))
    {
        if (validate_acpi(o2pt, pjson, OP_UPDATE) != RSC_OK)
            return RSC_BAD_REQUEST;

        if (cJSON_GetArraySize(pjson) == 0)
        {
            cJSON_DeleteItemFromObject(grp_new, "macp");
        }
    }

    if (midArr)
    {
        if (mt == 0 && (pjson = cJSON_GetObjectItem(grp_old, "mt")) != NULL)
        {
            mt = pjson->valueint;
        }
        if (csy == DEFAULT_CONSISTENCY_POLICY && (pjson = cJSON_GetObjectItem(grp_old, "csy")) != NULL)
        {
            csy = pjson->valueint;
        }
        cJSON *final_mid = cJSON_CreateArray();
        bool mtv = validate_grp_member(grp_new, final_mid, csy, mt);
        cJSON_SetBoolValue(cJSON_GetObjectItem(grp_new, "mtv"), mtv);

        cJSON_ArrayForEach(mid_obj, midArr)
        {
            char *mid = cJSON_GetStringValue(mid_obj);
            if (cJSON_getArrayIdx(final_mid, mid) == -1)
            {
                deleteNodeList(find_rtnode(mid)->memberOf, find_rtnode(mid));
            }
        }

        if (final_mid != NULL)
        {
            cJSON_DeleteItemFromObject(grp_new, "mid");
            cJSON_AddItemToObject(grp_new, "mid", final_mid);
        }
    }
    return RSC_OK;
}

// int set_grp_member(RTNode *grp_rtnode)
// {
//     cJSON *mid_Arr = cJSON_GetObjectItem(grp_rtnode->obj, "mid");
//     cJSON *mid_obj = NULL;
//     NodeList *nl = NULL;
//     bool duplicated = false;

//     cJSON_ArrayForEach(mid_obj, mid_Arr)
//     {
//         char *mid = cJSON_GetStringValue(mid_obj);
//         RTNode *mid_rtnode = find_rtnode(mid);
//         if (mid_rtnode)
//         {
//             duplicated = false;
//             nl = mid_rtnode->memberOf;
//             while (nl)
//             {
//                 if (nl->rtnode == grp_rtnode)
//                 {
//                     duplicated = true;
//                     break;
//                 }
//                 nl = nl->next;
//             }

//             if (!duplicated)
//             {
//                 logger("UTIL", LOG_LEVEL_DEBUG, "set_grp_member : %s is now member of %s", mid, grp_rtnode->uri);
//                 addNodeList(mid_rtnode->memberOf, grp_rtnode);
//             }
//             else
//             {
//                 logger("UTIL", LOG_LEVEL_DEBUG, "set_grp_member : %s is already member of %s", mid, grp_rtnode->uri);
//             }
//         }
//     }
// }
