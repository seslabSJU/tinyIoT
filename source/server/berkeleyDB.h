#ifndef __BERKELEYDB_H__
#define __BERKELEYDB_H__

int db_display(char* database);

int db_store_cse(CSE* cse_object);
int db_store_ae(AE* ae_object);
int db_store_cnt(CNT* cnt_object);
int db_store_cin(CIN* cin_object);
int db_store_sub(Sub *sub_object);
int db_store_acp(ACP *acp_object);

CSE* db_get_cse();
AE* db_get_ae(char *ri);
CNT* db_get_cnt(char *ri);
CIN* db_get_cin(char *ri);
Sub* db_get_sub(char* ri);
ACP* db_get_acp(char* ri);

int db_delete_onem2m_resource(char *ri);
int db_delete_sub(char* ri);
int db_delete_acp(char* ri);

RTNode* db_get_all_cse();
RTNode* db_get_all_ae();
RTNode* db_get_all_cnt();
RTNode* db_get_all_cin();
RTNode* db_get_all_sub();
RTNode* db_get_all_acp();

RTNode* db_get_cin_rtnode_list_by_pi(char* pi);

#define DB_STR_MAX 2048
#define DB_SEP ";"

#endif