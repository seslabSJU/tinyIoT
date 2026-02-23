#ifndef __dbmanager_H__
#define __dbmanager_H__
#include "config.h"
#include "onem2m.h"
#include "cJSON.h"

// Avoid including DB-backend headers in this public interface header.
// Keep this header backend-agnostic so SQLite builds don't require libpq headers, etc.
#if DB_TYPE == DB_SQLITE
    // Forward declaration for statement type used by helper binding functions.
    typedef struct sqlite3_stmt sqlite3_stmt;
#endif

int init_dbp();
int close_dbp();

int db_store_resource(cJSON *obj, char *uri);
int db_update_resource(cJSON *obj, char *ri, ResourceType ty);
cJSON *db_get_resource(char *ri, ResourceType ty);

cJSON *db_get_resource_by_uri(char *uri, ResourceType ty);
int db_begin_tx();
int db_commit_tx();
int db_rollback_tx();
// Delete a resource by resource ID (ri). Used by TS/TSI deletion paths.
int db_delete_resource(char *ri);

// ---- TS / TSI helpers (keep resource logic out of TS/TSI modules) ----
// TSI aggregates under a TS resource (identified by TS ri)
int db_tsi_get_count(const char *ts_ri);
int db_tsi_get_max_snr(const char *ts_ri);
int db_tsi_get_min_snr(const char *ts_ri);
int db_tsi_check_snr_dup(const char *ts_ri, int snr);
int db_tsi_check_dgt_dup(const char *ts_ri, const char *dgt);

// TS missing-data tracking fields
int db_ts_get_mdc(const char *ts_ri);
int db_ts_set_mdc(const char *ts_ri, int mdc);
// Set TS.mdc and append one timestamp string into TS.mdlt (best-effort per DB backend)
int db_ts_set_mdc_and_append_mdlt(const char *ts_ri, int mdc, const char *time_str);

// general table fields
int db_general_set_lt(const char *ri, const char *lt);

// For monitor thread: list TS resources with mdd==true. Returns a cJSON array of objects:
// [{"ri":"...","pei":<number>,"lt":"...","mdc":<number>}, ...]
cJSON *db_get_mdd_ts_list();

cJSON *db_get_tsi_laol(RTNode *parent_rtnode, int laol);

int db_delete_onem2m_resource(RTNode *rtnode);

RTNode *db_get_all_resource_as_rtnode();

RTNode *db_get_cin_rtnode_list(RTNode *rtnode);
cJSON *db_get_cin_laol(RTNode *parent_rtnode, int laol);
cJSON *db_get_filter_criteria(oneM2MPrimitive *o2pt);
RTNode *db_get_latest_cins();
bool db_check_cin_rn_dup(char *rn, char *pi);

int db_delete_one_cin_mni(RTNode *cnt);
cJSON *getForbiddenUri(cJSON *acp_list);

#if DB_TYPE == DB_SQLITE
    void db_test_and_bind_value(sqlite3_stmt *stmt, int index, cJSON *obj);
    void db_test_and_set_bind_text(sqlite3_stmt *stmt, int index, char *context);
    void db_test_and_set_bind_int(sqlite3_stmt *stmt, int index, int value);
#endif

#define DB_STR_MAX 65565
#define DB_SEP ";"

#endif