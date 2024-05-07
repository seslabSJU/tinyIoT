#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include "onem2mTypes.h"
#include "util.h"
#include "cJSON.h"
#include "onem2m.h"
#include "jsonparser.h"
#include "logger.h"

bool isFCAttrValid(cJSON *fc){
    cJSON *pjson = NULL, *ptr = NULL;
    if(cJSON_GetNumberValue(cJSON_GetObjectItem(fc, "sts")) < 0) return false;
    if(cJSON_GetNumberValue(cJSON_GetObjectItem(fc, "stb")) < 0) return false;
    
    if(cJSON_GetNumberValue(cJSON_GetObjectItem(fc, "sza")) < 0) return false;
    if(cJSON_GetNumberValue(cJSON_GetObjectItem(fc, "szb")) < 0) return false;

    if(cJSON_GetNumberValue(cJSON_GetObjectItem(fc, "lim")) < 0) return false;

    if(cJSON_GetNumberValue(cJSON_GetObjectItem(fc, "lvl")) < 0) return false;
    if(cJSON_GetNumberValue(cJSON_GetObjectItem(fc, "ofst")) < 0) return false;

    if( (pjson = cJSON_GetObjectItem(fc, "ty")) ){
        if(cJSON_IsArray(pjson)){
            cJSON_ArrayForEach(ptr, pjson){
                if(cJSON_GetNumberValue(ptr) < 0){
                    return false;
                }
            }
        }
        else if(cJSON_IsNumber(pjson)){
            if(cJSON_GetNumberValue(pjson) < 0){
                return false;
            }
        }
    }

    if( (pjson = cJSON_GetObjectItem(fc, "chty")) ){
        if(cJSON_IsArray(pjson)){
            cJSON_ArrayForEach(ptr, pjson){
                if(cJSON_GetNumberValue(ptr) < 0){
                    return false;
                }
            }
        }
        else if(cJSON_IsNumber(pjson)){
            if(cJSON_GetNumberValue(pjson) < 0){
                return false;
            }
        }
    }

    if( (pjson = cJSON_GetObjectItem(fc, "pty")) ){
        if(cJSON_IsArray(pjson)){
            cJSON_ArrayForEach(ptr, pjson){
                if(cJSON_GetNumberValue(ptr) < 0){
                    return false;
                }
            }
        }
        else if(cJSON_IsNumber(pjson)){
            if(cJSON_GetNumberValue(pjson) < 0){
                return false;
            }
        }
    }
    
    return true;
}

bool isValidFcAttr(char* attr){
    char *fcAttr[34] = {
    "crb", "cra", "ms", "us", "sts", "stb", "exb", "exa", "lbl","clbl", "palb", "lbq", "ty", "chty", "pty", "sza", "szb", "cty", 
    "atr", "catr", "patr", "fu", "lim", "smf", "fo", "cfs", "cfq", "lvl", "ofst", "arp", "gq", "ops", "la",
    "drt"};

    for(int i = 0 ; i < 34 ; i++){
        if(!strcmp(attr, fcAttr[i])) return true;
    }
    return false;
}

int validate_filter_criteria(oneM2MPrimitive *o2pt){
    cJSON *pjson = NULL;
    cJSON *fc = o2pt->fc;
    char buf[256] = {0};
    if(!fc) return 0;
    pjson = fc->child;

    //check FilterUsage == Discovery
    // if ( cJSON_GetNumberValue(cJSON_GetObjectItem(fc, "fu")) != FU_DISCOVERY ){
    //     return handle_error(o2pt, RSC_NOT_IMPLEMENTED, "Only filter usage DISCOVERY supported");
    // }

    while(pjson != NULL){
        if(!isValidFcAttr(pjson->string)){
            sprintf(buf, "attr `%s` is not valid(or supported)", pjson->string);
            return handle_error(o2pt, RSC_BAD_REQUEST, buf);
        }
        pjson = pjson->next;
    }

    if(!isFCAttrValid(fc)){ // TODO - If rcn == 11(discovery result references) fu must be 1
        return handle_error(o2pt, RSC_BAD_REQUEST, "Invalid Filter Criteria");
    }
    return RSC_OK;
}

void parse_filter_criteria(cJSON *fc){
    char int_Attrs[14][5] = {"ty", "sts", "stb", "sza", "szb", "lim", "fo", "fu", "lvl", 
    "ofst", "ops", "la", "pty", "chty"};
    cJSON *pjson = NULL, *ptr = NULL;
    for(int i = 0 ; i < 14 ; i++){
        if( (pjson = cJSON_GetObjectItem(fc, int_Attrs[i])) ){
            if(cJSON_IsString(pjson)){
                cJSON_ReplaceItemInObject(fc, int_Attrs[i], cJSON_CreateNumber(atoi(cJSON_GetStringValue(pjson))));
            }
        }
    }

    if(!cJSON_GetObjectItem(fc, "fo")){
        cJSON_AddNumberToObject(fc, "fo", 1);
    }
}

void parse_qs(cJSON *qs){
    char int_Attrs[15][5] = {"sts", "stb", "sza", "szb", "lim", "fo", "fu", "lvl", "ofst", "ops", "la", "pty", "chty",
                            "drt", "rcn"};
    cJSON *pjson = NULL, *ptr = NULL;
    for(int i = 0 ; i < 15; i++){
        if(pjson = cJSON_GetObjectItem(qs, int_Attrs[i])){
            if(cJSON_IsString(pjson)){
                cJSON_ReplaceItemInObject(qs, int_Attrs[i], cJSON_CreateNumber(atoi(cJSON_GetStringValue(pjson))));
            }
        }
    }

    if(pjson = cJSON_GetObjectItem(qs, "ty")){
        if(cJSON_IsArray(pjson)){
            cJSON_ArrayForEach(ptr, pjson){
                if(cJSON_IsString(ptr)){
                    cJSON_SetIntValue(ptr, atoi(ptr->valuestring));
                    
                    // ptr->valueint = atoi(ptr->valuestring);
                    // ptr->type = cJSON_;
                    // free(ptr->valuestring);
                    // ptr->valuestring = NULL;
                }
            }
        }
        else if(cJSON_IsString(pjson)){
            ptr = cJSON_CreateArray();
            cJSON_AddItemToArray(ptr, cJSON_CreateNumber(atoi(pjson->valuestring)));
            cJSON_ReplaceItemInObject(qs, "ty", ptr);
        }
    }

    if( (pjson = cJSON_GetObjectItem(qs, "lbl")) ){
        if(cJSON_IsString(pjson)){
            ptr = cJSON_CreateArray();
            cJSON_AddItemToArray(ptr, cJSON_CreateString(pjson->valuestring));
            cJSON_ReplaceItemInObject(qs, "lbl", ptr); 
        }
    }
    if( (pjson = cJSON_GetObjectItem(qs, "clbl")) ){
        if(cJSON_IsString(pjson)){
            ptr = cJSON_CreateArray();
            cJSON_AddItemToArray(ptr, cJSON_CreateString(pjson->valuestring));
            cJSON_ReplaceItemInObject(qs, "clbl", ptr); 
        }
    }
    if( (pjson = cJSON_GetObjectItem(qs, "palb")) ){
        if(cJSON_IsString(pjson)){
            ptr = cJSON_CreateArray();
            cJSON_AddItemToArray(ptr, cJSON_CreateString(pjson->valuestring));
            cJSON_ReplaceItemInObject(qs, "palb", ptr); 
        }
    }

    if(!cJSON_GetObjectItem(qs, "fo")){
        cJSON_AddNumberToObject(qs, "fo", 1);
    }
}

char *fc_to_qs(cJSON *fc){
    char *qs = NULL;
    char *key = NULL;
    char *value = NULL;
    cJSON *pjson = NULL, *ptr = NULL;
    cJSON *fc_copy = fc;
    pjson = fc_copy->child;

    qs = (char *)calloc(256, sizeof(char));
    while(pjson != NULL){
        key = pjson->string;
        strcat(qs, key);
        if(cJSON_IsArray(pjson)){
            strcat(qs, "=");
            value = cJSON_PrintUnformatted(pjson);
            strcat(qs, value);
            free(value);
        }
        else if(cJSON_IsNumber(pjson)){
            strcat(qs, "=");
            value = cJSON_PrintUnformatted(pjson);
            strcat(qs, value);
            free(value);
        }
        else if(cJSON_IsString(pjson)){
            strcat(qs, "=");
            value = cJSON_PrintUnformatted(pjson);
            strcat(qs, value);
            free(value);
        }
        strcat(qs, "&");
        pjson = pjson->next;
    }
    qs[strlen(qs) - 1] = '\0';
    return qs;
}

bool FC_isAptCrb(char *fcCrb, RTNode *rtnode){
    if(!rtnode || !fcCrb) return false;
    cJSON *pjson = cJSON_GetObjectItem(rtnode->obj, "ct");
    char *ct = pjson->valuestring;
    if(strcmp(fcCrb, ct) > 0) return true;

    return false;
}

bool FC_isAptCra(char* fcCra, RTNode *rtnode){
    if(!rtnode || !fcCra) return false;
    cJSON *pjson = cJSON_GetObjectItem(rtnode->obj, "ct");
    char *ct = pjson->valuestring;

    if(strcmp(fcCra, ct) <= 0) return true;

    return false;
}

bool FC_isAptMs(char *fcMs, RTNode *rtnode){
    if(!rtnode || !fcMs) return false;

    cJSON *pjson = cJSON_GetObjectItem(rtnode->obj, "lt");
    char *lt = pjson->valuestring;
    if(strcmp(fcMs, lt) <= 0) return true;

    return false;
}

bool FC_isAptUs(char *fcUs, RTNode *rtnode){
    if(!rtnode || !fcUs) return false;
    cJSON *pjson = cJSON_GetObjectItem(rtnode->obj, "lt");
    char *lt = pjson->valuestring;
    if(strcmp(fcUs, lt) > 0) return true;

    return false;
}

bool FC_isAptStb(int fcStb, RTNode *rtnode){
    if(!rtnode) return false;

    cJSON *pjson = cJSON_GetObjectItem(rtnode->obj, "st");
    if(!pjson) return false;
    int st = pjson->valueint;
    if(fcStb <= st) return true;

    return false;
}

bool FC_isAptSts(int fcSts, RTNode *rtnode){
    if(!rtnode) return false;

    cJSON *pjson = cJSON_GetObjectItem(rtnode->obj, "st");
    if(!pjson) return false;


    int st = pjson->valueint;
    if(st < fcSts) return true;

    return false;
}

bool FC_isAptExa(char *fcExa, RTNode *rtnode){
    if(!rtnode || !fcExa) return false;

    cJSON *pjson = cJSON_GetObjectItem(rtnode->obj, "et");
    if(!pjson) return false;

    char *et = pjson->valuestring;
    if(strcmp(fcExa, et) >= 0) return false;

    return true;
}

bool FC_isAptExb(char *fcExb, RTNode *rtnode){
    if(!rtnode || !fcExb) return false;

    cJSON *pjson = cJSON_GetObjectItem(rtnode->obj, "et");
    if(!pjson) return false;

    char *et = pjson->valuestring;
    if(strcmp(fcExb, et) < 0) return false;

    return true;
}

bool FC_isAptLbl(cJSON* fcLbl, RTNode *rtnode){

    // TODO
    bool result = false;
    int nodeSize = 0, fcSize = 0;

    if(!rtnode || !fcLbl) return false;

    cJSON *nodelbl = cJSON_GetObjectItem(rtnode->obj, "lbl");
    fcSize = cJSON_GetArraySize(fcLbl);
    for(int i = 0 ; i < cJSON_GetArraySize(nodelbl) ; i++){
        // logger("fc", LOG_LEVEL_DEBUG, "nodelbl: %s\n", cJSON_GetArrayItem(nodelbl, i)->valuestring);
        for(int j = 0 ; j < fcSize ; j++){
            // logger("fc", LOG_LEVEL_DEBUG, "fcLbl: %s\n", cJSON_GetArrayItem(fcLbl, j)->valuestring);
            if(!strcmp(cJSON_GetArrayItem(fcLbl, j)->valuestring, cJSON_GetArrayItem(nodelbl, i)->valuestring)){
                result = true;
                break;
            }
        }
        if(result) break;
    }
    return result;
}

bool FC_isAptPalb(cJSON *fcPalb, RTNode *rtnode){
    bool result = false;
    int nodeSize = 0, fcSize = 0;
    if(!rtnode || !fcPalb) return false;
    if(!rtnode->parent) return false;

    cJSON *nodelbl = cJSON_GetObjectItem(rtnode->parent->obj, "lbl");
    nodeSize = cJSON_GetArraySize(nodelbl);
    fcSize = cJSON_GetArraySize(fcPalb);

    for(int i = 0 ; i < nodeSize ; i++){
        for(int j = 0 ; j < fcSize ; j++){
            if(!strcmp(cJSON_GetArrayItem(fcPalb, j)->valuestring, cJSON_GetArrayItem(nodelbl, i)->valuestring)){
                result = true;
                break;
            }
        }
        if(result) break;
    }
    return result;
}

bool FC_isAptClbl(cJSON *fcClbl, RTNode *rtnode){
    bool result = false;
    int nodeSize = 0, fcSize = 0;
    RTNode *prt = NULL;
    if(!rtnode || !fcClbl) return false;
    if(!rtnode->child) return false;

    cJSON *nodelbl = NULL;

    prt = rtnode->child;

    while(prt){
        nodelbl = cJSON_GetObjectItem(prt->obj, "lbl");
        nodeSize = cJSON_GetArraySize(nodelbl);

        fcSize = cJSON_GetArraySize(fcClbl);

        for(int i = 0 ; i < nodeSize ; i++){
            for(int j = 0 ; j < fcSize ; j++){
                if(!strcmp(cJSON_GetArrayItem(fcClbl, j)->valuestring, cJSON_GetArrayItem(nodelbl, i)->valuestring)){
                    result = true;
                    break;
                }
            }
            if(result) break;
        }
        if(result) break;
        prt = prt->sibling_right;
    }
    

    
    return result;
}

bool FC_isAptTy(cJSON *ty_list, int ty){
    cJSON *ty_list_ptr = NULL;
    cJSON_ArrayForEach(ty_list_ptr, ty_list){
        if(ty == ty_list_ptr->valueint) return true;
    }

    return false;
}

bool FC_isAptChty(cJSON *chty_list, int ty){
    cJSON *chty_list_ptr = NULL;
    cJSON_ArrayForEach(chty_list_ptr, chty_list){
        if(ty == chty_list_ptr->valueint) return true;
    }

    return false;
}

bool FC_isAptPty(cJSON *fcPty, int ty){
    cJSON *pty_list_ptr = NULL;
    cJSON_ArrayForEach(pty_list_ptr, fcPty){
        if(ty == pty_list_ptr->valueint) return true;
    }
    return false;
}

bool FC_isAptSza(int fcSza, RTNode *rtnode){
    cJSON *pjson = cJSON_GetObjectItem(rtnode->obj, "cs");
    if(!pjson) return false;
    int cs = pjson->valueint;
    if(fcSza <= cs) return true;

    return false;
}

bool FC_isAptSzb(int fcSzb, RTNode *rtnode){
    cJSON *pjson = cJSON_GetObjectItem(rtnode->obj, "cs");
    if(!pjson) return false;
    int cs = pjson->valueint;
    if(cs < fcSzb) return true;

    return false;
}

bool FC_isAptOps(ACOP fcAcop, oneM2MPrimitive *o2pt, RTNode *rtnode){
    if(check_privilege(o2pt, rtnode, fcAcop) == -1)
        return false;

    return true;
}

bool isResourceAptFC(oneM2MPrimitive* o2pt, RTNode *rtnode, cJSON *fc){
    void *obj;
    int flag = 0;
	RTNode *prtnode = NULL;
	FilterOperation fo = cJSON_GetObjectItem(fc, "fo")->valueint;
    cJSON *pjson, *pjson2;
    if(!rtnode || !fc) return false;

	// check Created Time
    pjson = cJSON_GetObjectItem(fc, "cra");
    pjson2 = cJSON_GetObjectItem(fc, "crb");
	if(pjson && pjson2){
		if(strcmp(pjson->valuestring, pjson2->valuestring) >= 0 && fo == FO_AND) return false;
	}
    if(pjson){
		if(!FC_isAptCra(pjson->valuestring, rtnode)) {
			if(fo == FO_AND)
				return false;
		}else{
			if(fo == FO_OR)
				return true;
		}
    }
	if(pjson2){
		if(!FC_isAptCrb(pjson2->valuestring, rtnode)){
			if(fo == FO_AND)
				return false;
		}else{
			if(fo == FO_OR)
				return true;
		}
	}

	// check Last Modified
    pjson = cJSON_GetObjectItem(fc, "ms");
    pjson2 = cJSON_GetObjectItem(fc, "us");
    if(pjson && pjson2){
        if(strcmp(pjson->valuestring, pjson2->valuestring) >= 0 && fo == FO_AND) return false;
    }

    if(pjson){
        if(!FC_isAptMs(pjson->valuestring, rtnode)){
            if(fo == FO_AND)
                return false;
        }else{
            if(fo == FO_OR)
                return true;
        }
    }
    if(pjson2){
        if(!FC_isAptUs(pjson2->valuestring, rtnode)){
            if(fo == FO_AND)
                return false;
        }else{
            if(fo == FO_OR)
                return true;
        }
    }
    

	// check state tag
    pjson = cJSON_GetObjectItem(fc, "stb");
    pjson2 = cJSON_GetObjectItem(fc, "sts");

    if(pjson && pjson2){
        if(pjson->valueint >= pjson2->valueint && fo == FO_AND) return false;
    }

    if(pjson){
        if(!FC_isAptStb(pjson->valueint, rtnode)){
            if(fo == FO_AND)
                return false;
        }else{
            if(fo == FO_OR)
                return true;
        }
    }
    if(pjson2){
        if(!FC_isAptSts(pjson2->valueint, rtnode)){
            if(fo == FO_AND)
                return false;
        }else{
            if(fo == FO_OR)
                return true;
        }
    }

	// check Expiration Time
    pjson = cJSON_GetObjectItem(fc, "exa");
    pjson2 = cJSON_GetObjectItem(fc, "exb");
    if(pjson && pjson2){
        if(strcmp(pjson->valuestring, pjson2->valuestring) >= 0 && fo == FO_AND) return false;
    }

    if(pjson){
        if(!FC_isAptExa(pjson->valuestring, rtnode)){
            if(fo == FO_AND)
                return false;
        }else{
            if(fo == FO_OR)
                return true;
        }
    }
    if(pjson2){
        if(!FC_isAptExb(pjson2->valuestring, rtnode)){
            if(fo == FO_AND)
                return false;
        }else{
            if(fo == FO_OR)
                return true;
        }
    }
	// check label
    pjson = cJSON_GetObjectItem(fc, "lbl");
    if(pjson){
        if(!FC_isAptLbl(pjson, rtnode)){
            if(fo == FO_AND)
                return false;
        }else{
            if(fo == FO_OR)
                return true;
        }
    
    }

    // check clbl
    pjson = cJSON_GetObjectItem(fc, "clbl");
    if(pjson){
        if(!FC_isAptClbl(pjson, rtnode)){
            if(fo == FO_AND)
                return false;
        }else{
            if(fo == FO_OR)
                return true;
        }
    }

    // check palb
    pjson = cJSON_GetObjectItem(fc, "palb");

    if(pjson){
        if(!FC_isAptPalb(pjson, rtnode)){
            if(fo == FO_AND)
                return false;
        }else{
            if(fo == FO_OR)
                return true;
        }
    }

	// check TY
    pjson = cJSON_GetObjectItem(fc, "ty");
    if(pjson){
        if(!FC_isAptTy(pjson, rtnode->ty)){
            if(fo == FO_AND)
                return false;
        }else{
            if(fo == FO_OR)
                return true;
        }
    }
    
	// check chty
    pjson = cJSON_GetObjectItem(fc, "chty");
    if(pjson){
        if(!rtnode->child){
            if(fo == FO_AND)
                return false;
        }else{
            prtnode = rtnode->child;
            while(prtnode){
                if(FC_isAptChty(pjson, prtnode->ty)){
                    flag = 1;
                    break;
                }
                prtnode = prtnode->sibling_right;
            }
            if(flag){
                if(fo == FO_OR)
                    return true;
            }else{
                if(fo == FO_AND)
                    return false;
            }
        }
        
    
    }

	// check pty
    pjson = cJSON_GetObjectItem(fc, "pty");

    if(pjson){
        if(!rtnode->parent){
            if(fo == FO_AND)
                return false;
        }
        if(!FC_isAptPty(pjson, rtnode->ty)){
            if(fo == FO_AND)
                return false;
        }else{
            if(fo == FO_OR)
                return true;
        }
    }

	//check cs
    pjson = cJSON_GetObjectItem(fc, "sza");
    pjson2 = cJSON_GetObjectItem(fc, "szb");

    if(pjson && pjson2){
        if(pjson->valueint >= pjson2->valueint && fo == FO_AND) return false;
    }

    if(pjson){
        if(!FC_isAptSza(pjson->valueint, rtnode)){
            if(fo == FO_AND)
                return false;
        }else{
            if(fo == FO_OR)
                return true;
        }
    }

    if(pjson2){
        if(!FC_isAptSzb(pjson2->valueint, rtnode)){
            if(fo == FO_AND)
                return false;
        }else{
            if(fo == FO_OR)
                return true;
        }
    }

    // check ops
    pjson = cJSON_GetObjectItem(fc, "ops");
    if(pjson){
        if(!FC_isAptOps(pjson->valueint, o2pt, rtnode)){
            if(fo == FO_AND)
                return false;
        }else{
            if(fo == FO_OR)
                return true;
        }
    }

    return true;
}