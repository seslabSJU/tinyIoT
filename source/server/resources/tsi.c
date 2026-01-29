#define _XOPEN_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libpq-fe.h> 
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"
#include "../jsonparser.h"
#include "../monitor.h"
#include <pthread.h>      
#include "../onem2m.h"

extern PGconn *pg_conn;
extern pthread_mutex_t main_lock; 
extern void notify_missing_data(RTNode *ts_node, int current_mdc, mdc_source_t src);
extern void ts_mdc_update_db_with_mdlt(PGconn *conn, const char *ts_ri, int val, const char *time_str);

static int tsi_get_count_db(PGconn *conn, const char *ts_ri) {
    if (!conn || !ts_ri) return 0;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT COUNT(*) FROM tsi JOIN general ON tsi.id = general.id WHERE general.pi = '%s';",
             ts_ri);
    PGresult *r = PQexec(conn, sql);
    int count = 0;
    if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
        count = atoi(PQgetvalue(r, 0, 0));
    }
    if (r) PQclear(r);
    return count;
}


static int tsi_get_max_snr_db(PGconn *conn, const char *ts_ri) {
    if (!conn || !ts_ri) return 0;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT COALESCE(MAX(tsi.snr),0) FROM tsi JOIN general ON tsi.id = general.id WHERE general.pi = '%s';",
             ts_ri);
    PGresult *r = PQexec(conn, sql);
    int max_snr = 0;
    if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
        max_snr = atoi(PQgetvalue(r, 0, 0));
    }
    if (r) PQclear(r);
    return max_snr;
}

static int tsi_get_min_snr_db(PGconn *conn, const char *ts_ri) {
    if (!conn || !ts_ri) return 0;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT COALESCE(MIN(tsi.snr),0) FROM tsi JOIN general ON tsi.id = general.id WHERE general.pi = '%s';",
             ts_ri);
    PGresult *r = PQexec(conn, sql);
    int min_snr = 0;
    if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
        min_snr = atoi(PQgetvalue(r, 0, 0));
    }
    if (r) PQclear(r);
    return min_snr;
}

static int tsi_check_snr_dup_db(PGconn *conn, const char *ts_ri, int snr) {
    if (!conn || !ts_ri) return 0;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT 1 FROM tsi JOIN general ON tsi.id = general.id WHERE general.pi = '%s' AND tsi.snr = %d LIMIT 1;",
             ts_ri, snr);
    PGresult *r = PQexec(conn, sql);
    int exists = 0;
    if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
        exists = 1;
    }
    if (r) PQclear(r);
    return exists;
}

long long parse_time_to_us_tsi(char *s) {
    if (!s || strlen(s) < 15) return 0;

    struct tm t = {0};
    if (sscanf(s, "%4d%2d%2dT%2d%2d%2d", &t.tm_year, &t.tm_mon, &t.tm_mday,
               &t.tm_hour, &t.tm_min, &t.tm_sec) < 6) return 0;

    t.tm_year -= 1900;
    t.tm_mon -= 1;
    t.tm_isdst = -1;

    time_t epoch_sec = timegm(&t);
    if (epoch_sec == -1) return 0;

    long long t_us = (long long)epoch_sec * 1000000LL;

    char *comma = strchr(s, ',');
    if (comma) {
        char us_str[7] = "000000";
        for (int i = 0; i < 6 && comma[1 + i] >= '0' && comma[1 + i] <= '9'; i++) {
            us_str[i] = comma[1 + i];
        }
        t_us += atoll(us_str);
    }
    return t_us;
}

void us_to_iso8601_tsi(long long us, char *buf) {
    time_t sec = us / 1000000;
    int micro = us % 1000000;
    struct tm *t = localtime(&sec);
    sprintf(buf, "%04d%02d%02dT%02d%02d%02d,%06d",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, micro);
}

void internal_delete_resource(char *ri) {
    if (!ri || !pg_conn) return;
    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM general WHERE ri = '%s'", ri);
    PQclear(PQexec(pg_conn, sql));
}


static int ts_get_mdc_db(PGconn *conn, const char *ts_ri) {
    if (!conn || !ts_ri) return 0;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT COALESCE(mdc,0) FROM ts WHERE id = (SELECT id FROM general WHERE ri = '%s');", ts_ri);
    PGresult *r = PQexec(conn, sql);
    int v = 0;
    if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
        v = atoi(PQgetvalue(r, 0, 0));
    }
    if (r) PQclear(r);
    return v;
}

static void ts_mdc_update_db(PGconn *conn, const char *ts_ri, int val) {
    if (!conn || !ts_ri) return;
    char sql[512];
    snprintf(sql, sizeof(sql), "UPDATE ts SET mdc = %d WHERE id = (SELECT id FROM general WHERE ri = '%s');", val, ts_ri);
    PGresult *r = PQexec(conn, sql);
    if (r) PQclear(r);
}

void delete_oldest_tsi(RTNode *parent) {
    if (!parent || !parent->child) return;
    RTNode *child = parent->child;
    RTNode *oldest = NULL;
    while (child) {
        if (child->ty == RT_TSI) {
            if (!oldest) oldest = child;
            else {
                char *ct_old = cJSON_GetObjectItem(oldest->obj, "ct")->valuestring;
                char *ct_curr = cJSON_GetObjectItem(child->obj, "ct")->valuestring;
                if (strcmp(ct_curr, ct_old) < 0) oldest = child;
            }
        }
        child = child->sibling_right;
    }
    if (oldest) {
        cJSON *cni = cJSON_GetObjectItem(parent->obj, "cni");
        cJSON *cbs = cJSON_GetObjectItem(parent->obj, "cbs");
        int cs = cJSON_GetObjectItem(oldest->obj, "cs")->valueint;
        if (cni) cJSON_SetNumberValue(cni, cni->valueint - 1);
        if (cbs) cJSON_SetNumberValue(cbs, cbs->valueint - cs);
        internal_delete_resource(get_ri_rtnode(oldest));
        if (parent->child == oldest) parent->child = oldest->sibling_right;
        else {
            RTNode *prev = parent->child;
            while (prev->sibling_right != oldest) prev = prev->sibling_right;
            prev->sibling_right = oldest->sibling_right;
        }
        free_rtnode(oldest);
    }
}

int db_check_tsi_dgt_dup(char *pi, char *dgt) {
    if (!pi || !dgt || !pg_conn) return 0;
    char sql[512];
    snprintf(sql, sizeof(sql), 
        "SELECT tsi.id FROM tsi "
        "JOIN general ON tsi.id = general.id "
        "WHERE general.pi = '%s' AND tsi.dgt = '%s'", pi, dgt);
    
    PGresult *res = PQexec(pg_conn, sql);
    int rows = 0;
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        rows = PQntuples(res);
    }
    PQclear(res);
    return (rows > 0); 
}

int create_tsi(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
    if (parent_rtnode->ty != RT_TS) return handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "Parent not TS");

    cJSON *root = cJSON_Duplicate(o2pt->request_pc, 1);
    cJSON *tsi = cJSON_DetachItemFromObject(root, "m2m:tsi");
    if (!tsi) { cJSON_Delete(root); return handle_error(o2pt, RSC_BAD_REQUEST, "No tsi body"); }

    cJSON *p_con = cJSON_GetObjectItem(tsi, "con");
    cJSON *p_dgt = cJSON_GetObjectItem(tsi, "dgt");
    cJSON *p_snr = cJSON_GetObjectItem(tsi, "snr");
    if (!p_con) { cJSON_Delete(tsi); cJSON_Delete(root); return handle_error(o2pt, RSC_BAD_REQUEST, "con missing"); }
    if (!p_dgt) { cJSON_Delete(tsi); cJSON_Delete(root); return handle_error(o2pt, RSC_BAD_REQUEST, "dgt missing"); }

    cJSON_DeleteItemFromObject(tsi, "cs");

    if (db_check_tsi_dgt_dup(get_ri_rtnode(parent_rtnode), p_dgt->valuestring)) {
        cJSON_Delete(tsi); cJSON_Delete(root); return handle_error(o2pt, RSC_CONFLICT, "dgt dup");
    }

    int cs = strlen(p_con->valuestring);
    cJSON_AddNumberToObject(tsi, "cs", cs);

    cJSON *p_obj = parent_rtnode->obj;
    int mbs = cJSON_GetObjectItem(p_obj, "mbs")->valueint;
    if (cs > mbs) { cJSON_Delete(tsi); cJSON_Delete(root); return handle_error(o2pt, RSC_NOT_ACCEPTABLE, "mbs exceed"); }

    int current_cni = 0;
    int current_cbs = 0;
    if (pg_conn) {
        char sql_cc[512];
        sprintf(sql_cc,
                "SELECT COALESCE(cni,0), COALESCE(cbs,0) FROM ts WHERE id = (SELECT id FROM general WHERE ri = '%s');",
                get_ri_rtnode(parent_rtnode));
        PGresult *res_cc = PQexec(pg_conn, sql_cc);
        if (res_cc && PQresultStatus(res_cc) == PGRES_TUPLES_OK && PQntuples(res_cc) > 0) {
            current_cni = atoi(PQgetvalue(res_cc, 0, 0));
            current_cbs = atoi(PQgetvalue(res_cc, 0, 1));
        } else {
            cJSON *cni_item = cJSON_GetObjectItem(p_obj, "cni");
            cJSON *cbs_item = cJSON_GetObjectItem(p_obj, "cbs");
            current_cni = (cni_item && cJSON_IsNumber(cni_item)) ? (int)cJSON_GetNumberValue(cni_item) : 0;
            current_cbs = (cbs_item && cJSON_IsNumber(cbs_item)) ? (int)cJSON_GetNumberValue(cbs_item) : 0;
        }
        if (res_cc) PQclear(res_cc);
    }

    if (cJSON_GetObjectItem(p_obj, "cni")) cJSON_SetNumberValue(cJSON_GetObjectItem(p_obj, "cni"), current_cni);
    else cJSON_AddNumberToObject(p_obj, "cni", current_cni);

    if (cJSON_GetObjectItem(p_obj, "cbs")) cJSON_SetNumberValue(cJSON_GetObjectItem(p_obj, "cbs"), current_cbs);
    else cJSON_AddNumberToObject(p_obj, "cbs", current_cbs);

    int snr = 0;

    if (p_snr) {
        if (!cJSON_IsNumber(p_snr)) {
            cJSON_Delete(tsi); cJSON_Delete(root);
            return handle_error(o2pt, RSC_BAD_REQUEST, "snr invalid");
        }
        snr = (int)cJSON_GetNumberValue(p_snr);
        if (snr < 0) {
            cJSON_Delete(tsi); cJSON_Delete(root);
            return handle_error(o2pt, RSC_BAD_REQUEST, "snr invalid");
        }
    } else {
        snr = (pg_conn ? tsi_get_max_snr_db(pg_conn, get_ri_rtnode(parent_rtnode)) : current_cni) + 1;
        cJSON_AddNumberToObject(tsi, "snr", snr);
    }

    if (!cJSON_GetObjectItem(tsi, "rn")) {
        char rn[64];
        if (p_snr) {
            int idx = 0;
            if (pg_conn) {
                idx = tsi_get_count_db(pg_conn, get_ri_rtnode(parent_rtnode));
            } else {
                idx = current_cni;
            }
            snprintf(rn, sizeof(rn), "tsi_%d_%d", snr, idx);
        } else {
            snprintf(rn, sizeof(rn), "tsi_%d", snr);
        }
        cJSON_AddStringToObject(tsi, "rn", rn);
    }

    add_general_attribute(tsi, parent_rtnode, RT_TSI);
    char *uri = malloc(2048);
    sprintf(uri, "%s/%s", get_uri_rtnode(parent_rtnode), cJSON_GetObjectItem(tsi, "rn")->valuestring);

    if (db_store_resource(tsi, uri) != 1) {
        free(uri); cJSON_Delete(tsi); cJSON_Delete(root);
        return handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB fail");
    }

    int new_cni = current_cni + 1;
    if (pg_conn) new_cni = tsi_get_count_db(pg_conn, get_ri_rtnode(parent_rtnode));
    cJSON_SetNumberValue(cJSON_GetObjectItem(p_obj, "cni"), new_cni);
    cJSON_SetNumberValue(cJSON_GetObjectItem(p_obj, "cbs"), cJSON_GetObjectItem(p_obj, "cbs")->valueint + cs);

    cJSON *mdd = cJSON_GetObjectItem(p_obj, "mdd");
    cJSON *pei = cJSON_GetObjectItem(p_obj, "pei");
    cJSON *peid_obj = cJSON_GetObjectItem(p_obj, "peid");

    logger("TSI_TRACE", LOG_LEVEL_DEBUG, "Checking MDD conditions for TS [%s]", get_ri_rtnode(parent_rtnode));

    if (mdd && cJSON_IsTrue(mdd) && pei && pei->valueint > 0) {
        pthread_mutex_lock(&main_lock);

        int current_mdc = ts_get_mdc_db(pg_conn, get_ri_rtnode(parent_rtnode));
    
        char sql_mdlt[512];
        sprintf(sql_mdlt, "SELECT mdlt FROM ts WHERE id = (SELECT id FROM general WHERE ri = '%s');", get_ri_rtnode(parent_rtnode));
        PGresult *res_mdlt = PQexec(pg_conn, sql_mdlt);
        if (res_mdlt && PQresultStatus(res_mdlt) == PGRES_TUPLES_OK && PQntuples(res_mdlt) > 0) {
            char *db_mdlt_str = PQgetvalue(res_mdlt, 0, 0);
            if (db_mdlt_str && strlen(db_mdlt_str) > 0) {
                cJSON_ReplaceItemInObject(p_obj, "mdlt", cJSON_Parse(db_mdlt_str));
            }
        }
        if (res_mdlt) PQclear(res_mdlt);
        cJSON_SetNumberValue(cJSON_GetObjectItem(p_obj, "mdc"), current_mdc);

        long long t_curr_dgt = parse_time_to_us_tsi(p_dgt->valuestring);
        long long pei_us = (long long)pei->valueint * 1000;
        long long peid_us = (peid_obj) ? (long long)peid_obj->valueint * 1000 : 0;
        bool is_missing = false;
        int mdc_to_add = 0;

        long long t_ct = parse_time_to_us_tsi(cJSON_GetObjectItem(p_obj, "ct")->valuestring);

        int base_snr = 0;
        if (pg_conn) {
            base_snr = tsi_get_min_snr_db(pg_conn, get_ri_rtnode(parent_rtnode));
        }

        long long expected = t_ct + (long long)(snr - base_snr) * pei_us;
        long long lower = expected - peid_us;
        long long upper = expected + peid_us;

        if (t_curr_dgt < lower) {
            if (snr != base_snr) {
                long long early_gap = lower - t_curr_dgt;
                mdc_to_add = (int)((early_gap + pei_us - 1) / pei_us); 
                if (mdc_to_add > 0) is_missing = true;
            }
        } else if (t_curr_dgt > upper) {
            long long late_gap = t_curr_dgt - upper;
            mdc_to_add = (int)((late_gap + pei_us - 1) / pei_us);    
            if (mdc_to_add > 0) is_missing = true;
        }

        if (is_missing && mdc_to_add > 0) {
            current_mdc += mdc_to_add;
            char *now_str = get_local_time(0);
            ts_mdc_update_db_with_mdlt(pg_conn, get_ri_rtnode(parent_rtnode), current_mdc, now_str);
        
            cJSON_SetNumberValue(cJSON_GetObjectItem(p_obj, "mdc"), current_mdc);
            cJSON *mdlt = cJSON_GetObjectItem(p_obj, "mdlt");
            if (!mdlt) mdlt = cJSON_AddArrayToObject(p_obj, "mdlt");
            cJSON_AddItemToArray(mdlt, cJSON_CreateString(now_str));
        
            free(now_str);
            notify_missing_data(parent_rtnode, current_mdc, MDC_SRC_TSI_GAP);
        }
        pthread_mutex_unlock(&main_lock);
    }

    char *new_lt_final = get_local_time(0);
    if (cJSON_GetObjectItem(p_obj, "lt")) cJSON_ReplaceItemInObject(p_obj, "lt", cJSON_CreateString(new_lt_final));
    else cJSON_AddStringToObject(p_obj, "lt", new_lt_final);
    free(new_lt_final);

    RTNode *tsi_node = create_rtnode(tsi, RT_TSI);
    add_child_resource_tree(parent_rtnode, tsi_node);

    int mni = cJSON_GetObjectItem(p_obj, "mni")->valueint;
    while (cJSON_GetObjectItem(p_obj, "cni")->valueint > mni || cJSON_GetObjectItem(p_obj, "cbs")->valueint > mbs) {
        delete_oldest_tsi(parent_rtnode);
    }

    db_update_resource(p_obj, get_ri_rtnode(parent_rtnode), RT_TS);

    o2pt->rsc = RSC_CREATED;
    make_response_body(o2pt, tsi_node);
    cJSON_Delete(root);
    free(uri);
    return RSC_CREATED;
}