#ifndef __JSONPARSER_H__
#define __JSONPARSER_H__

#include "cJSON.h"

CSE* cjson_to_cse(cJSON *cjson);


char* notification_to_json(char *sur, int net, char *rep);
char* rtnode_to_json(RTNode *node);
char* discovery_to_json(char **result, int size);
char *cjson_string_list_item_to_string(cJSON *key);
char *cjson_int_list_item_to_string(cJSON *key);
cJSON *string_to_cjson_string_list_item(char *string);
cJSON *string_to_cjson_int_list_item(char *string);

#endif