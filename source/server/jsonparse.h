#ifndef __JSONPARSE_H__
#define __JSONPARSE_H__

#include "cJSON.h"

CSE* cjson_to_cse(cJSON *cjson);
AE* cjson_to_ae(cJSON *cjson);
CNT* cjson_to_cnt(cJSON *cjson);
CIN* cjson_to_cin(cJSON *cjson);
Sub* cjson_to_sub(cJSON *cjson);
ACP* cjson_to_acp(cJSON *cjson);

char* cse_to_json(CSE* cse_object);
char* ae_to_json(AE* ae_object);
char* cnt_to_json(CNT* cnt_object);
char* cin_to_json(CIN* cin_object);
char* sub_to_json(Sub *sub_object);
char* notification_to_json(char *sur, int net, char *rep);
char* node_to_json(RTNode *node);
char* acp_to_json(ACP *acp_object);
char* discovery_to_json(char **result, int size);

char* get_json_value_string(char *json, char *key);
char* get_json_value_char(char *key, char *json);
int get_json_value_int(char *key, char *json);
int get_json_value_bool(char *key, char *json);
char *get_json_value_list(char *key, char *json);
char* get_json_value_list_v2(char *json, char *key);

bool json_key_exist(char *json, char *key);
char *cjson_list_item_to_string(cJSON *key);

#endif