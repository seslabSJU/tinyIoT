#define _XOPEN_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <unistd.h>
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"
#include "../jsonparser.h"

extern pthread_mutex_t main_lock; 
extern void delete_oldest_tsi(RTNode *parent);

int validate_ts(oneM2MPrimitive *o2pt, cJSON *ts, Operation op) {
    char *attrs[] = {"pei", "peid", "mni", "mbs", "mdn", "mdt"};
    for(int i = 0; i < 6; i++) {
        cJSON *p = cJSON_GetObjectItem(ts, attrs[i]);
        if (p && cJSON_IsNumber(p) && cJSON_GetNumberValue(p) < 0) {
            return handle_error(o2pt, RSC_BAD_REQUEST, "negative value not allowed");
        }
    }
    return RSC_OK;
}

int create_ts(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
    if (o2pt->rvi == RVI_1) return handle_error(o2pt, RSC_BAD_REQUEST, "TS not supported in R1");
    cJSON *root = cJSON_Duplicate(o2pt->request_pc, 1);
    cJSON *ts = cJSON_GetObjectItem(root, "m2m:ts");
    add_general_attribute(ts, parent_rtnode, RT_TS);
    
    if (validate_ts(o2pt, ts, OP_CREATE) != RSC_OK) { cJSON_Delete(root); return o2pt->rsc; }

    cJSON *pei_item = cJSON_GetObjectItem(ts, "pei");
    if (!pei_item) pei_item = cJSON_AddNumberToObject(ts, "pei", 0);
    if (!cJSON_GetObjectItem(ts, "peid")) {
        if (pei_item->valueint > 0) cJSON_AddNumberToObject(ts, "peid", pei_item->valueint / 2);
    } else if (pei_item->valueint <= 0) cJSON_DeleteItemFromObject(ts, "peid");

    if (!cJSON_GetObjectItem(ts, "mdd")) cJSON_AddBoolToObject(ts, "mdd", false);
    cJSON *req_mdd = cJSON_GetObjectItem(ts, "mdd");
    if (req_mdd && cJSON_IsTrue(req_mdd)) {
        cJSON *mdt_item = cJSON_GetObjectItem(ts, "mdt");
        cJSON *mdn_item = cJSON_GetObjectItem(ts, "mdn");
        if (!mdt_item || !mdn_item || !cJSON_IsNumber(mdt_item) || !cJSON_IsNumber(mdn_item) ||
            cJSON_GetNumberValue(mdt_item) <= 0 || cJSON_GetNumberValue(mdn_item) <= 0) {
            cJSON_Delete(root);
            return handle_error(o2pt, RSC_BAD_REQUEST, "mdt and mdn must be valid when mdd is true");
        }
    }

    if (!cJSON_GetObjectItem(ts, "mbs")) cJSON_AddNumberToObject(ts, "mbs", 262144);
    if (!cJSON_GetObjectItem(ts, "mni")) cJSON_AddNumberToObject(ts, "mni", 100);
    if (!cJSON_GetObjectItem(ts, "mdn")) cJSON_AddNumberToObject(ts, "mdn", 10);
    if (!cJSON_GetObjectItem(ts, "mdt")) cJSON_AddNumberToObject(ts, "mdt", 0);
    
    cJSON *cnf_item = cJSON_GetObjectItem(ts, "cnf");
    if (cnf_item && !cJSON_IsString(cnf_item)) {
        cJSON_Delete(root);
        return handle_error(o2pt, RSC_BAD_REQUEST, "invalid attribute type : cnf");
    }
    
    cJSON_DeleteItemFromObject(ts, "mdc");
    cJSON_DeleteItemFromObject(ts, "cni");
    cJSON_DeleteItemFromObject(ts, "cbs");
    cJSON_DeleteItemFromObject(ts, "mdlt");
    cJSON_AddNumberToObject(ts, "mdc", 0);
    cJSON_AddNumberToObject(ts, "cni", 0);
    cJSON_AddNumberToObject(ts, "cbs", 0);

    cJSON *p_peid = cJSON_GetObjectItem(ts, "peid");
    if (pei_item && p_peid && p_peid->valueint > pei_item->valueint / 2) {
        cJSON_Delete(root); return handle_error(o2pt, RSC_BAD_REQUEST, "peid > pei/2");
    }


    char ptr[1024];
    sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), cJSON_GetObjectItem(ts, "rn")->valuestring);
    if (db_store_resource(ts, ptr) != 1) { cJSON_Delete(root); return handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB fail"); }

    RTNode *child_rtnode = create_rtnode(ts, RT_TS);
    add_child_resource_tree(parent_rtnode, child_rtnode);
    o2pt->rsc = RSC_CREATED;
    make_response_body(o2pt, child_rtnode);
    cJSON_DetachItemFromObject(root, "m2m:ts");
    cJSON_Delete(root);
    return RSC_CREATED;
}

int update_ts(oneM2MPrimitive *o2pt, RTNode *target_rtnode) {
    char *ri = get_ri_rtnode(target_rtnode);
    cJSON *root = cJSON_Duplicate(o2pt->request_pc, 1);
    cJSON *ts = cJSON_GetObjectItem(root, "m2m:ts");
    if (!ts) { cJSON_Delete(root); return handle_error(o2pt, RSC_BAD_REQUEST, "Insufficient arguments"); }

    if (cJSON_GetObjectItem(ts, "mdc") || cJSON_GetObjectItem(ts, "mdlt") || 
        cJSON_GetObjectItem(ts, "cni") || cJSON_GetObjectItem(ts, "cbs")) {
        cJSON_Delete(root);
        return handle_error(o2pt, RSC_BAD_REQUEST, "Cannot manually update read-only attributes");
    }

    cJSON *cnf_item = cJSON_GetObjectItem(ts, "cnf");
    if (cnf_item && !cJSON_IsString(cnf_item)) {
        cJSON_Delete(root);
        return handle_error(o2pt, RSC_BAD_REQUEST, "invalid attribute type : cnf");
    }

    cJSON *cur_mdd_obj = cJSON_GetObjectItem(target_rtnode->obj, "mdd");
    int cur_mdd = 0;
    if (cur_mdd_obj && (cJSON_IsTrue(cur_mdd_obj) || cur_mdd_obj->valueint == 1)) {
        cur_mdd = 1;
    }
    
    cJSON *new_mdd_obj = cJSON_GetObjectItem(ts, "mdd");
    int will_be_active = cur_mdd;
    int is_mdd_update = 0;

    if (new_mdd_obj) {
        is_mdd_update = 1;
        if (cJSON_IsTrue(new_mdd_obj) || new_mdd_obj->valueint == 1) {
            will_be_active = 1;
        } else {
            will_be_active = 0;
        }
    }

    if (will_be_active) {
        if (cJSON_GetObjectItem(ts, "pei") || cJSON_GetObjectItem(ts, "peid") || 
            cJSON_GetObjectItem(ts, "mdt") || cJSON_GetObjectItem(ts, "mdn")) {
            cJSON_Delete(root);
            return handle_error(o2pt, RSC_BAD_REQUEST, "Cannot update params when mdd is true");
        }

        double check_mdt = -1.0;
        double check_mdn = -1.0;

        cJSON *req_mdt = cJSON_GetObjectItem(ts, "mdt");
        if (req_mdt) check_mdt = cJSON_GetNumberValue(req_mdt);
        else {
            cJSON *cur_mdt = cJSON_GetObjectItem(target_rtnode->obj, "mdt");
            if (cur_mdt) check_mdt = cJSON_GetNumberValue(cur_mdt);
        }

        cJSON *req_mdn = cJSON_GetObjectItem(ts, "mdn");
        if (req_mdn) check_mdn = cJSON_GetNumberValue(req_mdn);
        else {
            cJSON *cur_mdn = cJSON_GetObjectItem(target_rtnode->obj, "mdn");
            if (cur_mdn) check_mdn = cJSON_GetNumberValue(cur_mdn);
        }

        if (check_mdt <= 0 || check_mdn <= 0) {
             cJSON_Delete(root);
             return handle_error(o2pt, RSC_BAD_REQUEST, "mdt and mdn must be valid when mdd is true");
        }

    } else {
        if (is_mdd_update) { 
            if (cJSON_GetObjectItem(ts, "peid") || cJSON_GetObjectItem(ts, "mdt") || cJSON_GetObjectItem(ts, "mdn") || cJSON_GetObjectItem(ts, "pei")) {
                 cJSON_Delete(root);
                 return handle_error(o2pt, RSC_BAD_REQUEST, "pei/peid/mdt/mdn update not allowed when disabling mdd");
            }
        }
    }

    double new_pei = -1.0;
    double new_peid = -1.0;
    
    cJSON *item_pei = cJSON_GetObjectItem(ts, "pei");
    if (item_pei) new_pei = cJSON_GetNumberValue(item_pei);

    cJSON *item_peid = cJSON_GetObjectItem(ts, "peid");
    if (item_peid) new_peid = cJSON_GetNumberValue(item_peid);

    if (new_pei != -1.0 && new_peid == -1.0) {
        double calc_peid = new_pei / 2.0;
        cJSON_AddNumberToObject(ts, "peid", (int)calc_peid);
        new_peid = calc_peid;
    }

    double cur_pei = 0;
    cJSON *t_pei = cJSON_GetObjectItem(target_rtnode->obj, "pei");
    if (t_pei) cur_pei = cJSON_GetNumberValue(t_pei);

    double cur_peid = 0;
    cJSON *t_peid = cJSON_GetObjectItem(target_rtnode->obj, "peid");
    if (t_peid) cur_peid = cJSON_GetNumberValue(t_peid);

    double final_pei = (new_pei != -1.0) ? new_pei : cur_pei;
    double final_peid = (new_peid != -1.0) ? new_peid : cur_peid;

    if (final_pei > 0 && final_peid > final_pei / 2.0) { 
        cJSON_Delete(root); 
        return handle_error(o2pt, RSC_BAD_REQUEST, "peid > pei/2"); 
    }

    cJSON *mdd = cJSON_GetObjectItem(ts, "mdd");
    if (mdd && (cJSON_IsTrue(mdd) || mdd->valueint == 1)) {
        cJSON *mdc = cJSON_GetObjectItem(target_rtnode->obj, "mdc");
        if(mdc) cJSON_SetNumberValue(mdc, 0);
        else cJSON_AddNumberToObject(target_rtnode->obj, "mdc", 0);
        cJSON *mdlt = cJSON_GetObjectItem(target_rtnode->obj, "mdlt");
        if (mdlt) cJSON_DeleteItemFromObject(target_rtnode->obj, "mdlt");
        char *now = get_local_time(0);
        cJSON_ReplaceItemInObject(target_rtnode->obj, "lt", cJSON_CreateString(now));
        free(now);
    }

    update_resource(target_rtnode->obj, ts);
    cJSON *cni = cJSON_GetObjectItem(target_rtnode->obj, "cni");
    cJSON *cbs = cJSON_GetObjectItem(target_rtnode->obj, "cbs");
    cJSON *mni = cJSON_GetObjectItem(target_rtnode->obj, "mni");
    cJSON *mbs = cJSON_GetObjectItem(target_rtnode->obj, "mbs");
    int cni_val = cni ? cni->valueint : 0;
    int cbs_val = cbs ? cbs->valueint : 0;
    int mni_val = mni ? mni->valueint : 0;
    int mbs_val = mbs ? mbs->valueint : 0;

    while ((mni && cni_val > mni_val) || (mbs && cbs_val > mbs_val)) {
        delete_oldest_tsi(target_rtnode);
        cni_val = cJSON_GetObjectItem(target_rtnode->obj, "cni")->valueint;
        cbs_val = cJSON_GetObjectItem(target_rtnode->obj, "cbs")->valueint;
    }

    db_update_resource(target_rtnode->obj, ri, RT_TS);
    make_response_body(o2pt, target_rtnode);
    o2pt->rsc = RSC_UPDATED;
    cJSON_Delete(root);
    return RSC_UPDATED;
}
