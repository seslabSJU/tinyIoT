#ifndef __JSONPARSER_H__
#define __JSONPARSER_H__

#include "cJSON.h"

CSE* cjson_to_cse(cJSON *cjson);
AE* cjson_to_ae(cJSON *cjson);
CNT* cjson_to_cnt(cJSON *cjson);
CIN* cjson_to_cin(cJSON *cjson);
SUB* cjson_to_sub(cJSON *cjson);
SUB* cjson_to_sub_db(cJSON *cjson);
ACP* cjson_to_acp(cJSON *cjson);
int cjson_to_grp(cJSON *cjson, GRP *grp);

char* cse_to_json(CSE* cse_object);
char* ae_to_json(AE* ae_object);
char* cnt_to_json(CNT* cnt_object);
char* cin_to_json(CIN* cin_object);
char* sub_to_json(SUB *sub_object);
char *grp_to_json(GRP *grp_object);
char* notification_to_json(char *sur, int net, char *rep);
char* rtnode_to_json(RTNode *node);
char* acp_to_json(ACP *acp_object);
char* discovery_to_json(char **result, int size);
char *cjson_string_list_item_to_string(cJSON *key);
char *cjson_int_list_item_to_string(cJSON *key);
cJSON *string_to_cjson_string_list_item(char *string);
cJSON *string_to_cjson_int_list_item(char *string);

#endif