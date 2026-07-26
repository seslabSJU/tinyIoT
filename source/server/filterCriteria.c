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

typedef enum {
    FC_TYPE_TIMESTAMP,
    FC_TYPE_NON_NEG_INT,
    FC_TYPE_POS_INT,
    FC_TYPE_ENUM,
    FC_TYPE_STRING,
    FC_TYPE_INT_ARRAY,
    FC_TYPE_STRING_ARRAY,
    FC_TYPE_BOOLEAN
} FcValueType;

typedef struct {
    const char   *short_name;
    FcValueType   type;
} FcTagDef;

static const FcTagDef FC_TAG_TABLE[] = {
    {"fu", FC_TYPE_ENUM}, // filterUsage
    { "crb",  FC_TYPE_TIMESTAMP},  // createdBefore
    { "cra",  FC_TYPE_TIMESTAMP},  // createdAfter
    { "ms",   FC_TYPE_TIMESTAMP},  // modifiedSince
    { "us",   FC_TYPE_TIMESTAMP},  // unmodifiedSince
    { "exb",  FC_TYPE_TIMESTAMP},  // expireBefore
    { "exa",  FC_TYPE_TIMESTAMP},  // expireAfter
    { "sts",  FC_TYPE_POS_INT},  // stateTagSmaller
    { "stb",  FC_TYPE_NON_NEG_INT},  // stateTagBigger
    { "sza",  FC_TYPE_NON_NEG_INT},  // sizeAbove
    { "szb",  FC_TYPE_POS_INT},  // sizeBelow
    { "lbl",  FC_TYPE_STRING_ARRAY},  // labels
    { "clbl",  FC_TYPE_STRING_ARRAY},  // childLabels
    { "palb",  FC_TYPE_STRING_ARRAY},  // parentLabels
    { "lbq",  FC_TYPE_STRING},  // labelsQuery
    { "ty",   FC_TYPE_INT_ARRAY},  // resourceType
    { "chty",  FC_TYPE_INT_ARRAY},  // childResourceType
    { "pty",  FC_TYPE_INT_ARRAY},  // parentResourceType
    { "cty",  FC_TYPE_STRING},  // contentType
    { "ops", FC_TYPE_ENUM },  // operations bitmask (0~63)
    { "fo",   FC_TYPE_ENUM},  // filterOperation
    { "cfs",  FC_TYPE_ENUM},  // contentFilterSyntax
    { "lim",  FC_TYPE_NON_NEG_INT},  // limit
    { "lvl",  FC_TYPE_POS_INT},  // level
    { "ofst", FC_TYPE_POS_INT},  // offset
    { "cfq",  FC_TYPE_STRING},  // contentFilterQuery
    { "arp", FC_TYPE_STRING},  // applyRelativePath

    /* 추후 확인 필요 */
    { "gq",   FC_TYPE_STRING},  // geoQuery (복합타입, 일단 STRING)
    { "smf",  FC_TYPE_STRING},  // semanticsFilter (SPARQL)

    /*atr, catr, patr 추가 필요*/
    /* 공통 */
    { "rn",   FC_TYPE_STRING},  // resourceName
    { "ri",   FC_TYPE_STRING},  // resourceID
    { "pi",   FC_TYPE_STRING},  // parentID
    { "cr",   FC_TYPE_STRING},  // creator
    { "or",   FC_TYPE_STRING},  // ontologyRef

    /* AE */
    { "apn",  FC_TYPE_STRING},  // appName
    { "api",  FC_TYPE_STRING},  // App-ID
    { "aei",  FC_TYPE_STRING},  // AE-ID
    { "rr",   FC_TYPE_BOOLEAN},  // requestReachability
    { "nl",   FC_TYPE_STRING},  // nodeLink
    { "poa",  FC_TYPE_STRING},  // pointOfAccess

    /* container */
    { "mni",  FC_TYPE_NON_NEG_INT},  // maxNrOfInstances
    { "mbs",  FC_TYPE_NON_NEG_INT},  // maxByteSize
    { "mia",  FC_TYPE_NON_NEG_INT},  // maxInstanceAge
    { "cni",  FC_TYPE_NON_NEG_INT},  // currentNrOfInstances
    { "cbs",  FC_TYPE_NON_NEG_INT},  // currentByteSize
    { "disr", FC_TYPE_BOOLEAN},  // disableRetrieval

    /* contentInstance */
    { "con",  FC_TYPE_STRING}, // content
    { "dlc",  FC_TYPE_POS_INT}, // deletionCnt

    /* flexContainer */
    { "cnd",  FC_TYPE_STRING}, // containerDefinition

    /* annc resource */
    { "lnk", FC_TYPE_STRING} // link
};
#define FC_TAG_COUNT (sizeof(FC_TAG_TABLE) / sizeof(FcTagDef))

bool isFCAttrValid(cJSON *fc){
    cJSON *item;
    cJSON_ArrayForEach(item, fc) {
        char *key = item->string;
        // 나중에 포함 되도록 테이블 변경 필요        
        if (!strcmp(key, "fo")) {
            if (!cJSON_IsNumber(item)) {
                return false;
            }
            int fo_value = cJSON_GetNumberValue(item);
            if (fo_value < 0 || fo_value > 3) {
                return false;
            }
            continue;
        }
            
        else if (!strcmp(key, "ty") || !strcmp(key, "chty") || !strcmp(key, "pty")) {
            static const int VALID_TYPES[] = {
                RT_MIXED, RT_ACP, RT_AE, RT_CNT, RT_CIN, RT_CSE, 
                RT_GRP, RT_MGMTOBJ, RT_NOD, RT_PCH, RT_CSR, RT_REQ, 
                RT_SUB, RT_SMD, RT_FCNT, RT_TS, RT_TSI, RT_CRS, 
                RT_FCIN, RT_FCNT_LA, RT_FCNT_OL, RT_TSB, RT_ACTR, 
                RT_ACPA, RT_AEA, RT_CNTA, RT_CINA, RT_CBA, RT_GRPA, RT_FCNTA
            };
            static const int TOTAL_TYPES = sizeof(VALID_TYPES) / sizeof(int);
            int val = 0;
            int is_in = 0;
            cJSON *array_item;
            cJSON_ArrayForEach(array_item, item) {
                if (!cJSON_IsNumber(array_item)) {
                    return false;
                }
                val = cJSON_GetNumberValue(array_item);
                for (int i = 0; i < TOTAL_TYPES; i++) {
                if (VALID_TYPES[i] == val) {
                    is_in = 1;
                    break;
                }
                }
                if (!is_in) {
                    return false;
                }
            }
            continue;
        }




        for (int i = 0; i < FC_TAG_COUNT; i++) {
            if (strcmp(key, FC_TAG_TABLE[i].short_name) != 0) {
                continue;
            }
            switch (FC_TAG_TABLE[i].type) {
                case FC_TYPE_TIMESTAMP:
                    int read_char = 0;
                    int year, month, day, hour, minute, second;
                    if (sscanf(cJSON_GetStringValue(item), "%4d%2d%2dT%2d%2d%2d%n", &year, &month, &day, &hour, &minute, &second, &read_char) != 6) {
                        return false;
                    }
                    if(read_char != 15) {
                        return false;
                    }

                    char *ptr = cJSON_GetStringValue(item) + read_char;
                    read_char = 0;
                    if (*ptr == ',' || *ptr == '.') {
                        ptr++;
                    }
                    while(isdigit(*ptr)) {
                        ptr++;
                        read_char++;
                    }
                    if (read_char > 6) {
                        return false;
                    }
                    break;
                case FC_TYPE_NON_NEG_INT:
                    if (cJSON_GetNumberValue(item) <= 0) {
                        return false;
                    }
                    break;
                case FC_TYPE_POS_INT:
                    if (cJSON_GetNumberValue(item) < 0) {
                        return false;
                    }
                    break;
                case FC_TYPE_ENUM:
                case FC_TYPE_BOOLEAN:
                case FC_TYPE_STRING:
                case FC_TYPE_INT_ARRAY:
                case FC_TYPE_STRING_ARRAY:
            }
        }
    }
    return true;

}

bool isValidFcAttr(char* attr){
    for(int i = 0 ; i < FC_TAG_COUNT ; i++){
        if(!strcmp(attr, FC_TAG_TABLE[i].short_name)) return true;
    }
    return false;
}

int validate_filter_criteria(oneM2MPrimitive *o2pt){
    cJSON *pjson = NULL;
    cJSON *fc = o2pt->fc;
    char buf[256] = {0};
    if(!fc) return 0;
    pjson = fc->child;

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

bool parse_qs(cJSON *qs){
    cJSON *pjson = NULL;
    cJSON *ptr = NULL;
    for (int i = 0; i < FC_TAG_COUNT; i++) {
        if (!(pjson = cJSON_GetObjectItem(qs, FC_TAG_TABLE[i].short_name))) {
            if (strcmp(FC_TAG_TABLE[i].short_name, "fo") == 0) {
                cJSON_AddNumberToObject(qs, "fo", 1);
            }
            continue;
        };
        switch (FC_TAG_TABLE[i].type) {
        case FC_TYPE_NON_NEG_INT:
        case FC_TYPE_POS_INT:
        case FC_TYPE_ENUM:
            if (cJSON_IsString(pjson)) {
                errno = 0; 
                char *endptr = NULL;
                long parsed_long = strtol(pjson->valuestring, &endptr, 10);

                if (errno == ERANGE || parsed_long > INT_MAX || parsed_long < INT_MIN) return false; 
                if (endptr == pjson->valuestring || *endptr != '\0') return false;
                cJSON_ReplaceItemInObject(qs, FC_TAG_TABLE[i].short_name, cJSON_CreateNumber((int)parsed_long));
            }
            break;
        case FC_TYPE_TIMESTAMP:
        case FC_TYPE_STRING:
            break;
        case FC_TYPE_INT_ARRAY:
            long parsed_long;
            char *endptr;
            errno = 0;
            if (cJSON_IsArray(pjson)) {
                cJSON_ArrayForEach(ptr, pjson) {
                    if(cJSON_IsString(ptr)) {
                        parsed_long = strtol(ptr->valuestring, &endptr, 10);
                        if (errno == ERANGE || parsed_long > INT_MAX || parsed_long < INT_MIN) return false; 
                        if (endptr == ptr->valuestring || *endptr != '\0') return false;

                        if (ptr->valuestring != NULL) {
                            free(ptr->valuestring);
                            ptr->valuestring = NULL;
                        }
                        ptr->type = cJSON_Number;

                        cJSON_SetIntValue(ptr, (int)parsed_long);
                    }
                }
            }
            else if(cJSON_IsString(pjson)){
                ptr = cJSON_CreateArray();
                parsed_long = strtol(pjson->valuestring, &endptr, 10);
                if (errno == ERANGE || parsed_long > INT_MAX || parsed_long < INT_MIN) return false; 
                if (endptr == pjson->valuestring || *endptr != '\0') return false;
                cJSON_ReplaceItemInObject(qs, FC_TAG_TABLE[i].short_name, cJSON_CreateNumber((int)parsed_long));
            }
            break;
        case FC_TYPE_STRING_ARRAY:
            if(cJSON_IsString(pjson)){
                ptr = cJSON_CreateArray();
                cJSON_AddItemToArray(ptr, cJSON_CreateString(pjson->valuestring));
                cJSON_ReplaceItemInObject(qs, FC_TAG_TABLE[i].short_name, ptr); 
            }
        }
    }

    if (pjson = cJSON_GetObjectItem(qs, "rcn")) {
        if (cJSON_IsString(pjson)) {
            errno = 0; 
            char *endptr = NULL;
            long parsed_long = strtol(pjson->valuestring, &endptr, 10);

            if (errno == ERANGE || parsed_long > INT_MAX || parsed_long < INT_MIN) return false; 
            if (endptr == pjson->valuestring || *endptr != '\0') return false;
            cJSON_ReplaceItemInObject(qs, "rcn", cJSON_CreateNumber((int)parsed_long));
        }
    }
    if (pjson = cJSON_GetObjectItem(qs, "drt")) {
        if (cJSON_IsString(pjson)) {
            errno = 0; 
            char *endptr = NULL;
            long parsed_long = strtol(pjson->valuestring, &endptr, 10);

            if (errno == ERANGE || parsed_long > INT_MAX || parsed_long < INT_MIN) return false; 
            if (endptr == pjson->valuestring || *endptr != '\0') return false;
            cJSON_ReplaceItemInObject(qs, "drt", cJSON_CreateNumber((int)parsed_long));
        }
    }

    return true;
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
        if(!FC_isAptPty(pjson, rtnode->parent->ty)){
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