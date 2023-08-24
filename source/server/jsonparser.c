#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include "onem2m.h"
#include "jsonparser.h"
#include "cJSON.h"
#include "onem2mTypes.h"
#include "config.h"
#include "util.h"

extern char *PORT;

void remove_quotation_mark(char *s){
	int len = strlen(s);
	int index = 0;

	for(int i=0; i<len; i++) {
		if(s[i] != '\\' && s[i] != '\"')
			s[index++] = s[i];
	}
	s[index] = '\0';
}

char* rtnode_to_json(RTNode *rtnode) {
	char *json = NULL;

	cJSON *obj = NULL;
	cJSON *child = NULL;

	/* Our "obj" item: */
	obj = cJSON_CreateObject();
	cJSON_AddStringToObject(obj, "ri", get_ri_rtnode(rtnode));
	cJSON_AddStringToObject(obj, "rn", get_rn_rtnode(rtnode));
	cJSON_AddStringToObject(obj, "pi", get_pi_rtnode(rtnode));
	cJSON_AddNumberToObject(obj, "ty", rtnode->ty);

	child = cJSON_CreateArray();
	cJSON_AddItemToObject(obj, "children", child);

	json = cJSON_PrintUnformatted(obj);

	cJSON_Delete(obj);

	return json;
}

char *cjson_string_list_item_to_string(cJSON *key) {
	if (key == NULL) {
		return NULL;
	}
	else {
		int item_size = cJSON_GetArraySize(key);
		if (item_size == 0) {
			return NULL;
		}
		else {
			char item_str[MAX_ATTRIBUTE_SIZE] = { '\0' };
			for (int i = 0; i < item_size; i++) {
				if(cJSON_GetArrayItem(key, i)->valuestring == NULL){
					return NULL;
				}
				// TODO - if int value should convert to string.
				
				if (isspace(cJSON_GetArrayItem(key, i)->valuestring[0]) || (cJSON_GetArrayItem(key, i)->valuestring[0] == 0)) {
					return NULL;
				}
				else {
					strcat(item_str, cJSON_GetArrayItem(key, i)->valuestring);
					if (i < item_size - 1) {
						strcat(item_str, ",");
					}
				}
			}
			char *ret = (char *)malloc(sizeof(char *) * strlen(item_str) + 1);
			strcpy(ret, item_str);

			return ret;
		}
	}
}

char *cjson_int_list_item_to_string(cJSON *key) {
	if (key == NULL) {
		return NULL;
	} else {
		int item_size = cJSON_GetArraySize(key);
		if (item_size == 0) {
			return NULL;
		} else {
			char item_str[MAX_ATTRIBUTE_SIZE] = { '\0' };
			char buf[16];
			for (int i = 0; i < item_size; i++) {
				int num = cJSON_GetArrayItem(key, i)->valueint;
				sprintf(buf,"%d",num);
				strcat(item_str, buf);
				if (i < item_size - 1) {
					strcat(item_str, ",");
				}
			}

			char *ret = (char *)malloc(sizeof(char *) * strlen(item_str) + 1);
			strcpy(ret, item_str);

			return ret;
		}
	}
}

cJSON *string_to_cjson_string_list_item(char *string){
	cJSON *cjson_arr = cJSON_CreateArray();
	if(!string) return cjson_arr;
	
	char buf[MAX_ATTRIBUTE_SIZE];
	strcpy(buf, string);

	char *pstr = strtok(buf, ",");

	do {
		cJSON_AddItemToArray(cjson_arr, cJSON_CreateString(pstr));
		pstr = strtok(NULL, ",");
	} while(pstr != NULL);

	return cjson_arr;
}

cJSON *string_to_cjson_int_list_item(char *string){
	cJSON *cjson_arr = cJSON_CreateArray();
	if(!string) return cjson_arr;
	
	char buf[MAX_ATTRIBUTE_SIZE];
	strcpy(buf, string);

	char *pstr = strtok(buf, ",");

	do {
		cJSON_AddItemToArray(cjson_arr, cJSON_CreateNumber(atoi(pstr)));
		pstr = strtok(NULL, ",");
	} while(pstr != NULL);

	return cjson_arr;
}