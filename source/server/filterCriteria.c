#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <malloc.h>
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

    if(pjson = cJSON_GetObjectItem(fc, "ty")){
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

    if(pjson = cJSON_GetObjectItem(fc, "chty")){
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

    if(pjson = cJSON_GetObjectItem(fc, "pty")){
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
    char *fcAttr[33] = {
    "crb", "cra", "ms", "us", "sts", "stb", "exb", "exa", "lbl","clbl", "palb", "lbq", "ty", "chty", "pty", "sza", "szb", "cty", 
    "atr", "catr", "patr", "fu", "lim", "smf", "fo", "cfs", "cfq", "lvl", "ofst", "arp", "gq", "ops", "la"};

    for(int i = 0 ; i < 33 ; i++){
        if(!strcmp(attr, fcAttr[i])) return true;
    }
    return false;
}

int validate_filter_criteria(oneM2MPrimitive *o2pt){
    cJSON *pjson = NULL;
    cJSON *fc = o2pt->fc;
    char buf[256] = {0};
    if(!fc) return NULL;
    pjson = fc->child;

    //check FilterUsage == Discovery
    if ( cJSON_GetNumberValue(cJSON_GetObjectItem(fc, "fu")) != FU_DISCOVERY ){
        return handle_error(o2pt, RSC_NOT_IMPLEMENTED, "Only filter usage DISCOVERY supported");
    }

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
        if(pjson = cJSON_GetObjectItem(fc, int_Attrs[i])){
            if(cJSON_IsString(pjson)){
                cJSON_ReplaceItemInObject(fc, int_Attrs[i], cJSON_CreateNumber(atoi(cJSON_GetStringValue(pjson))));
            }
        }
    }
}