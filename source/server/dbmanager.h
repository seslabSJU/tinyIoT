#ifndef __dbmanager_H__
#define __dbmanager_H__
#include "config.h"
#include "onem2m.h"

#ifdef SQLITE_DB
#include "sqlite/sqlite3.h"
#endif

int init_dbp();
int close_dbp();

int db_store_cse(CSE *cse_object);
int db_store_ae(AE *ae_object);
int db_store_cnt(CNT* cnt_object);
int db_store_cin(CIN* cin_object);
int db_store_grp(GRP* grp_object);
int db_store_sub(SUB* sub_object);
int db_store_acp(ACP *acp_object);

CSE* db_get_cse(char *ri);
AE* db_get_ae(char *ri);
CNT* db_get_cnt(char *ri);
CIN* db_get_cin(char *ri);
SUB* db_get_sub(char* ri);
GRP* db_get_grp(char* ri);
ACP* db_get_acp(char* ri);

#ifdef SQLITE_DB
int db_update_ae(AE *ae_object);
int db_update_cnt(CNT *cnt_object);
int db_update_acp(ACP * acp_object);
int db_update_grp(GRP *grp_object);
#endif

int db_delete_onem2m_resource(RTNode *rtnode);
int db_delete_sub(char* ri);
int db_delete_acp(char* ri);
int db_delete_grp(char* ri);

RTNode* db_get_all_cse();
RTNode* db_get_all_ae_rtnode();
RTNode* db_get_all_cnt_rtnode();
RTNode* db_get_all_cin();
RTNode* db_get_all_sub_rtnode();
RTNode* db_get_all_acp_rtnode();
RTNode* db_get_all_grp_rtnode();

RTNode* db_get_cin_rtnode_list(RTNode *rtnode);
#ifdef SQLITE_DB
CIN *db_get_cin_laol(RTNode *parent_rtnode, int laol);
cJSON* db_get_filter_criteria(char *to, FilterCriteria *fc);
cJSON *db_get_parent_filter_criteria(char *to, FilterCriteria *fc);
cJSON *db_get_child_filter_criteria(char *to, FilterCriteria *fc);

void db_test_and_set_bind_text(sqlite3_stmt *stmt, int index, char* context);
void db_test_and_set_bind_int(sqlite3_stmt *stmt, int index, int value);
#endif

#ifdef BERKELEY_DB
int db_delete_child_cin(RTNode *rtnode)
#endif

#define DB_STR_MAX 65565
#define DB_SEP ";"

#endif