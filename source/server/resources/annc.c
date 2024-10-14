#include <stdlib.h>
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;

int create_annc(oneM2MPrimitive *o2pt, RTNode *parent_rtnode)
{

    // type specific validation
    switch (o2pt->ty)
    {
    case RT_ACPA:
        break;
    case RT_AEA:
        check_aei_invalid(o2pt);
        break;
    case RT_CBA:
        break;
    case RT_CNTA:
        break;
    case RT_GRPA:
        break;
    case RT_CINA:
        break;
    }

    cJSON *root = cJSON_Duplicate(o2pt->request_pc, 1);
    cJSON *resource = cJSON_GetObjectItem(root, get_resource_key(o2pt->ty));

    add_general_attribute(resource, parent_rtnode, o2pt->ty);

    int rsc = RSC_OK;
    if (rsc != RSC_OK)
    {
        cJSON_Delete(root);
        return rsc;
    }

    // Add uri attribute
    char *ptr = malloc(1024);
    cJSON *rn = cJSON_GetObjectItem(resource, "rn");
    sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);
    // Save to DB
    int result = db_store_resource(resource, ptr);

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
    RTNode *child_rtnode = create_rtnode(resource, o2pt->ty);
    add_child_resource_tree(parent_rtnode, child_rtnode);

    make_response_body(o2pt, child_rtnode);
    cJSON_DetachItemFromObject(root, get_resource_key(o2pt->ty));
    cJSON_Delete(root);

    return o2pt->rsc = RSC_CREATED;
}

int update_annc(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{
    int rsc;
    char invalid_key[][8] = {"ty", "pi", "ri", "rn", "ct"};
    cJSON *req_src = cJSON_GetObjectItem(o2pt->request_pc, get_resource_key(o2pt->ty));
    int invalid_key_size = sizeof(invalid_key) / (8 * sizeof(char));
    for (int i = 0; i < invalid_key_size; i++)
    {
        if (cJSON_GetObjectItem(req_src, invalid_key[i]))
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "unsupported attribute on update");
        }
    }
    char *lnk = cJSON_GetObjectItem(target_rtnode->obj, "lnk")->valuestring;
    logger("UTIL", LOG_LEVEL_DEBUG, "lnk : %s, fr : %s", lnk, o2pt->fr);
    if (!lnk)
    {
        return handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "lnk is empty");
    }
    if (strncmp(o2pt->fr, lnk, strlen(o2pt->fr)) == 0 && lnk[strlen(o2pt->fr)] == '/')
    {
        logger("UTIL", LOG_LEVEL_DEBUG, "update from originator");
    }
    else
    {
        cJSON *ast = cJSON_GetObjectItem(target_rtnode->obj, "ast");
        if (ast)
        {
            if (ast->valueint != AST_BI_DIRECTIONAL)
            {
                return handle_error(o2pt, RSC_BAD_REQUEST, "resource is uni-directional");
            }
            else
            {

                char *lnk = cJSON_GetObjectItem(target_rtnode->obj, "lnk")->valuestring;

                oneM2MPrimitive *req = calloc(1, sizeof(oneM2MPrimitive));
                o2ptcpy(&req, o2pt);
                req->to = strdup(lnk);
                cJSON_Delete(req->request_pc);
                req->request_pc = cJSON_CreateObject();
                cJSON_AddItemReferenceToObject(req->request_pc, get_resource_key(o2pt->ty - 10000), req_src);

                rsc = forwarding_onem2m_resource(req, find_csr_rtnode_by_uri(lnk));
                if (rsc != RSC_UPDATED)
                {
                    return handle_error(o2pt, RSC_BAD_REQUEST, "failed to update original resource");
                }
            }
        }
        else
        {
            return handle_error(o2pt, RSC_BAD_REQUEST, "resource is uni-directional");
        }
    }

    int result = 0;

    cJSON *aea = target_rtnode->obj;
    cJSON *pjson = NULL;

    update_resource(target_rtnode->obj, req_src);

    result = db_update_resource(req_src, cJSON_GetObjectItem(target_rtnode->obj, "ri")->valuestring, RT_AEA);

    make_response_body(o2pt, target_rtnode);

    o2pt->rsc = RSC_UPDATED;
    return RSC_UPDATED;
}
