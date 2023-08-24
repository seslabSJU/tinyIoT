#ifndef __dbmanager_H__
#define __dbmanager_H__
#include "config.h"
#include "onem2m.h"
#include "cJSON.h"
#include "sqlite/sqlite3.h"

int init_dbp();
int close_dbp();


int db_store_resource(cJSON *obj, char *uri);
int db_update_resource(cJSON *obj, char *ri, ResourceType ty);
cJSON *db_get_resource(char *ri, ResourceType ty);



int db_delete_onem2m_resource(RTNode *rtnode);
int db_delete_sub(char* ri);
int db_delete_acp(char* ri);
int db_delete_grp(char* ri);
int db_delete_csr(char* ri);

RTNode* db_get_all_cse();
RTNode* db_get_all_csr_rtnode();
RTNode* db_get_all_ae_rtnode();
RTNode* db_get_all_cnt_rtnode();
RTNode* db_get_all_cin();
RTNode* db_get_all_sub_rtnode();
RTNode* db_get_all_acp_rtnode();
RTNode* db_get_all_grp_rtnode();

RTNode* db_get_cin_rtnode_list(RTNode *rtnode);
cJSON *db_get_cin_laol(RTNode *parent_rtnode, int laol);
cJSON* db_get_filter_criteria(char *to, cJSON *fc);
cJSON *db_get_parent_filter_criteria(char *to, cJSON *fc);
cJSON *db_get_child_filter_criteria(char *to, cJSON *fc);

void db_test_and_set_bind_text(sqlite3_stmt *stmt, int index, char* context);
void db_test_and_set_bind_int(sqlite3_stmt *stmt, int index, int value);


#define DB_STR_MAX 65565
#define DB_SEP ";"

#endif