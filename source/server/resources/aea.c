#include <stdlib.h>
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;

int create_aea(oneM2MPrimitive *o2pt, RTNode *parent_rtnode)
{
    int e = check_aei_invalid(o2pt);
    if (e != -1)
        e = check_rn_invalid(o2pt, RT_AE);
    if (e == -1)
        return o2pt->rsc;

    if (!(parent_rtnode->ty == RT_CBA || parent_rtnode->ty == RT_CSR))
    {
        handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
        return o2pt->rsc;
    }
    cJSON *root = cJSON_Duplicate(o2pt->request_pc, 1);
    cJSON *aea = cJSON_GetObjectItem(root, "m2m:aeA");

    add_general_attribute(aea, parent_rtnode, RT_AEA);

    // int rsc = validate_aea(o2pt, ae, OP_CREATE); // TODO
    int rsc = RSC_OK;
    if (rsc != RSC_OK)
    {
        cJSON_Delete(root);
        return rsc;
    }

    // Add uri attribute
    char *ptr = malloc(1024);
    cJSON *rn = cJSON_GetObjectItem(aea, "rn");
    sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);
    // Save to DB
    int result = db_store_resource(aea, ptr);

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
    RTNode *child_rtnode = create_rtnode(aea, RT_AEA);
    add_child_resource_tree(parent_rtnode, child_rtnode);

    o2pt->response_pc = root;

    return o2pt->rsc = RSC_CREATED;
}

int update_aea(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{
    char invalid_key[][8] = {"ty", "pi", "ri", "rn", "ct"};
    cJSON *m2m_aea = cJSON_GetObjectItem(o2pt->request_pc, "m2m:aea");
    int invalid_key_size = sizeof(invalid_key) / (8 * sizeof(char));

    int updateAttrCnt = cJSON_GetArraySize(m2m_aea);

    for (int i = 0; i < invalid_key_size; i++)
    {
        if (cJSON_GetObjectItem(m2m_aea, invalid_key[i]))
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "unsupported attribute on update");
        }
    }

    cJSON *ast = cJSON_GetObjectItem(target_rtnode->obj, "ast");
    if (ast)
    {
        if (ast->valueint != AST_BI_DIRECTIONAL)
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "resource is uni-directional");
        }
    }
    else
    {
        return handle_error(o2pt, RSC_BAD_REQUEST, "resource is uni-directional");
    }

    int result = 0;

    // int result = validate_aea(o2pt, m2m_aea, OP_UPDATE);
    // if(result != RSC_OK) return result;

    cJSON *aea = target_rtnode->obj;
    cJSON *pjson = NULL;

    update_resource(target_rtnode->obj, m2m_aea);

    result = db_update_resource(m2m_aea, cJSON_GetObjectItem(target_rtnode->obj, "ri")->valuestring, RT_AEA);

    for (int i = 0; i < updateAttrCnt; i++)
    {
        cJSON_DeleteItemFromArray(m2m_aea, 0);
    }

    make_response_body(o2pt, target_rtnode);

    o2pt->rsc = RSC_UPDATED;
    return RSC_UPDATED;
}
