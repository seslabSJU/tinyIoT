#include <stdlib.h>
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;

int create_cba(oneM2MPrimitive *o2pt, RTNode *parent_rtnode)
{
    // int e = check_rn_invalid(o2pt, RT_CBA);
    // if(e == -1) return o2pt->rsc;
    logger("UTIL", LOG_LEVEL_DEBUG, "%s", o2pt->request_pc);

    if (parent_rtnode->ty != RT_CSE)
    {
        handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
        return o2pt->rsc;
    }

    if (SERVER_TYPE == ASN_CSE)
    {
        handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "operation not allowed");
        return o2pt->rsc;
    }

    cJSON *root = cJSON_Duplicate(o2pt->request_pc, 1);
    cJSON *cba = cJSON_GetObjectItem(root, "m2m:cba");

    add_general_attribute(cba, parent_rtnode, RT_CBA);
    cJSON_AddStringToObject(cba, "cb", o2pt->fr);
    cJSON_ReplaceItemInObject(cba, "ri", cJSON_Duplicate(cJSON_GetObjectItem(cba, "ri"), 1));

    // int rsc = validate_csr(o2pt, parent_rtnode, cba, OP_CREATE);
    // if(rsc != RSC_OK){
    // 	cJSON_Delete(root);
    // 	return rsc;
    // }

    o2pt->response_pc = root;
    o2pt->rsc = RSC_CREATED;

    // Add uri attribute
    char *ptr = malloc(1024);
    cJSON *rn = cJSON_GetObjectItem(cba, "rn");
    sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);

    // Save to DB
    int result = db_store_resource(cba, ptr);
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

    RTNode *rtnode = create_rtnode(cba, RT_CBA);
    add_child_resource_tree(parent_rtnode, rtnode);
    return RSC_CREATED;
}