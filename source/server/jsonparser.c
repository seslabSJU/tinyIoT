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

void remove_quotation_mark(char *s){
	int len = strlen(s);
	int index = 0;

	for(int i=0; i<len; i++) {
		if(s[i] != '\\' && s[i] != '\"')
			s[index++] = s[i];
	}
	s[index] = '\0';
}

AE* cjson_to_ae(cJSON *cjson) {
	cJSON *root = NULL;
	cJSON *api = NULL;
	cJSON *rr = NULL;
	cJSON *rn = NULL;
	cJSON *lbl = NULL;
	cJSON *srv = NULL;
	cJSON *acpi = NULL;
	cJSON *pjson = NULL;

	if (cjson == NULL) {
		return NULL;
	}

	root = cJSON_GetObjectItem(cjson, "m2m:ae");
	if(!root) return NULL;

	AE *ae = (AE *)calloc(1,sizeof(AE));

	// api (mandatory)
	api = cJSON_GetObjectItem(root, "api");
	rr = cJSON_GetObjectItemCaseSensitive(root, "rr");	// + rr
	if (api == NULL || api->valuestring[0] == 0 || isspace(api->valuestring[0])) {
		fprintf(stderr, "Invalid api\n");
		if (!cJSON_IsTrue(rr) && !cJSON_IsFalse(rr)) {
			fprintf(stderr, "Invalid rr\n");
		}
		free_ae(ae);
		return NULL;
	}
	else {
		ae->api = cJSON_PrintUnformatted(api);
		remove_quotation_mark(ae->api);
	}


	// rr (mandatory)
	if (!cJSON_IsTrue(rr) && !cJSON_IsFalse(rr))
	{
		fprintf(stderr, "Invalid rr\n");
		free_ae(ae);
		return NULL;
	}
	else if (cJSON_IsTrue(rr))
	{
		ae->rr = true;
	}
	else if (cJSON_IsFalse(rr))
	{
		ae->rr = false;
	}

	// srv (madatory)
	srv = cJSON_GetObjectItem(root, "srv");
	if(srv == NULL) {
		fprintf(stderr, "Invalid srv\n");
		free_ae(ae);
		return NULL;
	}
	ae->srv = cjson_string_list_item_to_string(srv);
	

	// rn (optional)
	rn = cJSON_GetObjectItem(root, "rn");
	if (rn == NULL || isspace(rn->valuestring[0])) {
		ae->rn = NULL;
	}
	else {
		ae->rn = cJSON_PrintUnformatted(rn);
		remove_quotation_mark(ae->rn);
	}

	// lbl (optional)
	lbl = cJSON_GetObjectItem(root, "lbl");
	ae->lbl = cjson_string_list_item_to_string(lbl);

	// acpi (optional)
	acpi = cJSON_GetObjectItem(root, "acpi");
	ae->acpi = cjson_string_list_item_to_string(acpi);

	if(pjson = cJSON_GetObjectItem(root, "pi")){
		ae->pi = strdup(pjson->valuestring);

	}
	if(pjson = cJSON_GetObjectItem(root, "ri") ){
		ae->ri = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "ct")){
		ae->ct = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "lt")){
		ae->lt = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "et")){
		ae->et = strdup(pjson->valuestring);
	}if(pjson = cJSON_GetObjectItem(root, "uri")){
		ae->uri = strdup(pjson->valuestring);
	}

	ae->ty = RT_AE;
	return ae;
}

CNT* cjson_to_cnt(cJSON *cjson) {
	cJSON *root = NULL;
	cJSON *pi = NULL;
	cJSON *ri = NULL;
	cJSON *rn = NULL;
	cJSON *acpi = NULL;
	cJSON *lbl = NULL;
	cJSON *mni = NULL;
	cJSON *mbs = NULL;
	cJSON *pjson = NULL;

	if (cjson == NULL) {
		return NULL;
	}

	root = cJSON_GetObjectItem(cjson, "m2m:cnt");
	if(!root) return NULL;

	CNT *cnt = (CNT *)calloc(1,sizeof(CNT));

	// rn (optional)
	rn = cJSON_GetObjectItem(root, "rn");
	if (rn == NULL || isspace(rn->valuestring[0])) {
		cnt->rn = NULL;
	}
	else {
		cnt->rn = cJSON_PrintUnformatted(rn);
		remove_quotation_mark(cnt->rn);
	}

	// acpi (optional)
	acpi = cJSON_GetObjectItem(root, "acpi");
	cnt->acpi = cjson_string_list_item_to_string(acpi);

	// lbl (optional)
	lbl = cJSON_GetObjectItem(root, "lbl");
	cnt->lbl = cjson_string_list_item_to_string(lbl);

	//mni (Optional)
	mni = cJSON_GetObjectItem(root, "mni");
	if(mni) {
		cnt->mni = mni->valueint;
	} else {
		cnt->mni = INT_MIN;
	}

	//mbs (Optional)
	mbs = cJSON_GetObjectItem(root, "mbs");
	if(mbs) {
		cnt->mbs = mbs->valueint;
	} else {
		cnt->mbs = INT_MIN;
	}

	if(pjson = cJSON_GetObjectItem(root, "pi")){
		cnt->pi = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "ri") ){
		cnt->ri = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "ct")){
		cnt->ct = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "lt")){
		cnt->lt = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "et")){
		cnt->et = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "uri")){
		cnt->uri = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "st")){
		cnt->st = pjson->valueint;
	}


	return cnt;
}

CIN* cjson_to_cin(cJSON *cjson) {
	cJSON *root = NULL;
	cJSON *rn = NULL;
	cJSON *con = NULL;
	cJSON *pjson = NULL;

	const char *error_ptr = NULL;

	if (cjson == NULL) {
		return NULL;
	}

	CIN *cin = (CIN *)calloc(1,sizeof(CIN));

	root = cJSON_GetObjectItem(cjson, "m2m:cin");
	if(!root) return NULL;

	// con (mandatory)
	con = cJSON_GetObjectItem(root, "con");
	if (con == NULL || con->valuestring == NULL || con->valuestring[0] == 0 || isspace(con->valuestring[0]))
	{
		fprintf(stderr, "Invalid con\n");
		return NULL;
	}
	cin->con = cJSON_PrintUnformatted(con);
	remove_quotation_mark(cin->con);
	cin->cs = strlen(cin->con);

	if(pjson = cJSON_GetObjectItem(root, "rn")){
		cin->rn = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "st")){
		cin->st = pjson->valueint;
	}
	if(pjson = cJSON_GetObjectItem(root, "cs")){
		cin->cs = pjson->valueint;
	}
	if(pjson = cJSON_GetObjectItem(root, "pi")){
		cin->pi = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "ri") ){
		cin->ri = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "ct")){
		cin->ct = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "lt")){
		cin->lt = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "et")){
		cin->et = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "uri")){
		cin->uri = strdup(pjson->valuestring);
	}
	cin->ty = RT_CIN;

	return cin;
}

SUB *cjson_to_sub_db(cJSON *cjson){
	cJSON *root = NULL;
	cJSON *pjson = NULL;

	root = cJSON_GetObjectItem(cjson, "m2m:sub");
	if(!root) return NULL;

	SUB *sub = (SUB *) calloc(1, sizeof(SUB));

	pjson = cJSON_GetObjectItem(root, "nu");
	if(pjson){
		sub->nu = strdup(pjson->valuestring);
	}
	pjson = cJSON_GetObjectItem(root, "net");
	if(pjson){
		sub->net = strdup(pjson->valuestring);
	}

	pjson = cJSON_GetObjectItem(root, "nct");
	if(pjson){
		sub->nct = pjson->valueint;
	}
	pjson = cJSON_GetObjectItem(root, "pi");
	if(pjson){
		sub->pi = strdup(pjson->valuestring);
	}
	pjson = cJSON_GetObjectItem(root, "ri");
	if(pjson){
		sub->ri = strdup(pjson->valuestring);
	}
	pjson = cJSON_GetObjectItem(root, "ct");
	if(pjson){
		sub->ct = strdup(pjson->valuestring);
	}
	pjson = cJSON_GetObjectItem(root, "et");
	if(pjson){
		sub->et= strdup(pjson->valuestring);
	}
	pjson = cJSON_GetObjectItem(root, "lt");
	if(pjson){
		sub->lt = strdup(pjson->valuestring);
	}
	pjson = cJSON_GetObjectItem(root, "uri");
	if(pjson){
		sub->uri = strdup(pjson->valuestring);
	}
	pjson = cJSON_GetObjectItem(root, "sur");
	if(pjson){
		sub->sur = strdup(pjson->valuestring);
	}

	return sub;

}

SUB* cjson_to_sub(cJSON *cjson) {
	cJSON *root = NULL;
	cJSON *rn = NULL;
	cJSON *enc = NULL;
	cJSON *net = NULL;
	cJSON *nu = NULL;
	cJSON *pjson = NULL;

	if (cjson == NULL) {
		return NULL;
	}

	root = cJSON_GetObjectItem(cjson, "m2m:sub");
	if(!root) return NULL;

	SUB *sub = (SUB *)calloc(1,sizeof(SUB));

	// nu (mandatory)
	nu = cJSON_GetObjectItem(root, "nu");
	if(cJSON_IsArray(nu)){
		int nu_size = cJSON_GetArraySize(nu);
		if (nu == NULL || nu_size == 0) {
			return NULL;
		} else {
			sub->nu = cjson_string_list_item_to_string(nu);
		}
	}else if(cJSON_IsString(nu)){
		if(strlen(nu->valuestring) == 0){
			return NULL;
		}
		sub->nu = cjson_string_list_item_to_string(nu);
	}


	// rn (optional)
	rn = cJSON_GetObjectItem(root, "rn");
	if (rn == NULL || isspace(rn->valuestring[0])) {
		sub->rn = NULL;
	}
	else {
		sub->rn = cJSON_PrintUnformatted(rn);
		remove_quotation_mark(sub->rn);
	}

	// enc (optional)
	enc = cJSON_GetObjectItem(root, "enc");
	if (enc == NULL) {
		sub->net = NULL;
	}
	else {
		// net (optional)
		net = cJSON_GetObjectItem(enc, "net");
		sub->net = cjson_int_list_item_to_string(net);
	}

	pjson = cJSON_GetObjectItem(root, "nct");
	if(pjson){
		sub->nct = pjson->valueint;
	}
	pjson = cJSON_GetObjectItem(root, "pi");
	if(pjson){
		sub->pi = strdup(pjson->valuestring);
	}
	pjson = cJSON_GetObjectItem(root, "ri");
	if(pjson){
		sub->ri = strdup(pjson->valuestring);
	}
	pjson = cJSON_GetObjectItem(root, "ct");
	if(pjson){
		sub->ct = strdup(pjson->valuestring);
	}
	pjson = cJSON_GetObjectItem(root, "et");
	if(pjson){
		sub->et= strdup(pjson->valuestring);
	}
	pjson = cJSON_GetObjectItem(root, "lt");
	if(pjson){
		sub->lt = strdup(pjson->valuestring);
	}
	pjson = cJSON_GetObjectItem(root, "uri");
	if(pjson){
		sub->uri = strdup(pjson->valuestring);
	}
	pjson = cJSON_GetObjectItem(root, "sur");
	if(pjson){
		sub->sur = strdup(pjson->valuestring);
	}

	sub->ty = RT_SUB;

	return sub;
}

ACP* cjson_to_acp(cJSON *cjson) {
	cJSON *root = NULL;
	cJSON *rn = NULL;
	cJSON *pv = NULL;
	cJSON *pvs = NULL;
	cJSON *acrs = NULL;
	cJSON *acr = NULL;
	cJSON *acor = NULL;
	cJSON *acop = NULL;
	cJSON *lbl = NULL;
	cJSON *pjson = NULL;

	if (cjson == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			fprintf(stderr, "Error before: %s\n", error_ptr);
		}
		return NULL;
	}

	root = cJSON_GetObjectItem(cjson, "m2m:acp");
	ACP *acp = NULL;

	// pvs
	pvs = cJSON_GetObjectItem(root, "pvs");
	if (pvs == NULL) {
		return NULL;
	}
	else {
		// acr
		acp = (ACP *)calloc(1, sizeof(ACP));
		acrs = cJSON_GetObjectItem(pvs, "acr");
		int acr_size = cJSON_GetArraySize(acrs);
		if (acrs == NULL || acr_size == 0) {
			return NULL;
		}
		else {
			char acor_str[100] = { '\0' };
			char acop_str[100] = { '\0' };
			int i = 0;
			cJSON_ArrayForEach(acr, acrs) {
				acor = cJSON_GetObjectItem(acr, "acor");
				int acor_size = cJSON_GetArraySize(acor);
				if (acor == NULL || acor_size == 0) {
					fprintf(stderr, "Invalid pvs-acr-acor\n");
					if (acor_size == 0) {
						fprintf(stderr, "Invalid pvs-acr-acop\n");
					}
					return NULL;
				}
				else {
					for (int j = 0; j < acor_size; j++) {
						if (isspace(cJSON_GetArrayItem(acor, j)->valuestring[0]) || (cJSON_GetArrayItem(acor, j)->valuestring[0] == 0)) {
							fprintf(stderr, "Invalid pvs-acr-acor\n");
							return NULL;
						}
						else {
							strcat(acor_str, cJSON_GetArrayItem(acor, j)->valuestring);
							if (j < acor_size - 1) {
								strcat(acor_str, ",");
							}
						}
					}
					if (i < acr_size - 1)
						strcat(acor_str, ",");
				}

				acop = cJSON_GetObjectItem(acr, "acop");
				int acop_int = acop->valueint;
				char tmp[1024]={'\0'};
				for (int j = 0; j < acor_size; j++) {
					sprintf(tmp, "%d", acop_int);
					strcat(acop_str, tmp);
						if (j < acor_size - 1) {
							strcat(acop_str, ",");
						}
					}
					if (i < acr_size - 1)
						strcat(acop_str, ",");
					i++;
			}
			acp->pvs_acor = (char *)malloc(sizeof(char) * strlen(acor_str) + 1);
			strcpy(acp->pvs_acor, acor_str);

			acp->pvs_acop = (char *)malloc(sizeof(char) * strlen(acop_str) + 1);
			strcpy(acp->pvs_acop, acop_str);
		}
	}
	// pv
	pv = cJSON_GetObjectItem(root, "pv");
	if(pv) {
		// acr
		acrs = cJSON_GetObjectItem(pv, "acr");
		int acr_size = cJSON_GetArraySize(acrs);
		if (acrs == NULL || acr_size == 0) {
			fprintf(stderr, "Invalid pv-acr\n");
			return NULL;
		}
		else {
			char acor_str[1024] = { '\0' };
			char acop_str[1024] = { '\0' };
			int i = 0;
			cJSON_ArrayForEach(acr, acrs) {
				acor = cJSON_GetObjectItem(acr, "acor");
				int acor_size = cJSON_GetArraySize(acor);
				if (acor == NULL || acor_size == 0) {
					fprintf(stderr, "Invalid pv-acr-acor\n");
					if (acor_size == 0) {
						fprintf(stderr, "Invalid pv-acr-acop\n");
					}
					return NULL;
				}
				else {
					for (int j = 0; j < acor_size; j++) {
						if (isspace(cJSON_GetArrayItem(acor, j)->valuestring[0]) || (cJSON_GetArrayItem(acor, j)->valuestring[0] == 0)) {
							fprintf(stderr, "Invalid pv-acr-acor\n");
							return NULL;
						}
						else {
							strcat(acor_str, cJSON_GetArrayItem(acor, j)->valuestring);
							if (j < acor_size - 1) {
								strcat(acor_str, ",");
							}
						}
					}
					if (i < acr_size - 1)
						strcat(acor_str, ",");
				}

				acop = cJSON_GetObjectItem(acr, "acop");
				int acop_int = acop->valueint;
				char tmp[1024]={'\0'};
				for (int j = 0; j < acor_size; j++) {
					sprintf(tmp, "%d", acop_int);
					strcat(acop_str, tmp);
						if (j < acor_size - 1) {
							strcat(acop_str, ",");
						}
					}
					if (i < acr_size - 1)
						strcat(acop_str, ",");
					i++;
			
			}
			acp->pv_acor = (char *)malloc(sizeof(char) * strlen(acor_str) + 1);
			strcpy(acp->pv_acor, acor_str);
		
			acp->pv_acop = (char *)malloc(sizeof(char) * strlen(acop_str) + 1);
			strcpy(acp->pv_acop, acop_str);
		}
	}

	// rn (optional)
	rn = cJSON_GetObjectItem(root, "rn");
	if (rn == NULL || isspace(rn->valuestring[0])) {
		acp->rn = NULL;
	}
	else {
		acp->rn = cJSON_PrintUnformatted(rn);
		remove_quotation_mark(acp->rn);
	}

	lbl = cJSON_GetObjectItem(root, "lbl");
	acp->lbl = cjson_string_list_item_to_string(lbl);


	if(pjson = cJSON_GetObjectItem(root, "pi")){
		acp->pi = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "ri") ){
		acp->ri = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "ct")){
		acp->ct = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "lt")){
		acp->lt = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "et")){
		acp->et = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "uri")){
		acp->uri = strdup(pjson->valuestring);
	}


	return acp;
}

int cjson_to_grp(cJSON *cjson, GRP *grp){
	cJSON *root = NULL;
	cJSON *rn = NULL;
	cJSON *mt = NULL;
	cJSON *mnm = NULL;
	cJSON *acpi = NULL;
	cJSON *mid = NULL;
	cJSON *pmid = NULL;
	cJSON *csy = NULL;
	cJSON *macp = NULL;

	cJSON *pjson = NULL;

	if(cjson == NULL){
		return 0;
	}
	
	root = cJSON_GetObjectItem(cjson, "m2m:grp");

	rn = cJSON_GetObjectItem(root, "rn");
	mt = cJSON_GetObjectItem(root, "mt");
	mnm = cJSON_GetObjectItem(root, "mnm");
	mid = cJSON_GetObjectItem(root, "mid");
	acpi = cJSON_GetObjectItem(root, "acpi");
	csy = cJSON_GetObjectItem(root, "csy");
	macp = cJSON_GetObjectItem(root, "macp");


	// mnm & mid mandatory
	if(mnm == NULL || mid == NULL){
		logger("JSON", LOG_LEVEL_DEBUG, "Invalid request");
		return RSC_INVALID_ARGUMENTS;
	}

	// validate mnm >= 0
	if(mnm->valueint < 0){
		logger("JSON", LOG_LEVEL_DEBUG, "Invalid Maximum Member");
		return RSC_INVALID_ARGUMENTS;
	} 

	// set rn(optional);
	if(rn){
		grp->rn = (char*) malloc( sizeof(char) * strlen(rn->valuestring) + 1 );
		strcpy(grp->rn, rn->valuestring);
	}

	if(mt){
		grp->mt = (unsigned) mt->valueint;
	}

	if(acpi){
		grp->acpi = cjson_string_list_item_to_string(acpi);
	}

	if(macp){
		grp->macp = cjson_string_list_item_to_string(macp);
	}

	if(csy){
		grp->csy = csy->valueint;
	}
	grp->mnm = mnm->valueint;

	grp->mid = (char **) calloc(1, sizeof(char*) * grp->mnm);

	int mid_size = cJSON_GetArraySize(mid);
	if(mid_size > grp->mnm){
		return RSC_MAX_NUMBER_OF_MEMBER_EXCEEDED;
	}
	
	grp->cnm = mid_size;
	int midx = 0;
	for(int i = 0 ; i < grp->mnm; i++){
		if(midx < mid_size){
			pmid = cJSON_GetArrayItem(mid, i);
			if(!isMinDup(grp->mid, midx, pmid->valuestring)){
				//logger("json-t", LOG_LEVEL_DEBUG, "adding %s to mid[%d]", pmid->valuestring, midx);
				grp->mid[midx] = strdup(pmid->valuestring);
				midx++;
			}
			else{
				//logger("json-t", LOG_LEVEL_DEBUG, "declining %s", pmid->valuestring);
				grp->mid[i] = NULL;
				grp->cnm--;
			}
		}
	}
	if(pjson = cJSON_GetObjectItem(root, "pi")){
		grp->pi = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "ri") ){
		grp->ri = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "ct")){
		grp->ct = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "lt")){
		grp->lt = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "et")){
		grp->et = strdup(pjson->valuestring);
	}
	if(pjson = cJSON_GetObjectItem(root, "mtv")){
		grp->mtv = pjson->valueint;
	}
	if(pjson = cJSON_GetObjectItem(root, "uri")){
		grp->uri = strdup(pjson->valuestring);
	}

	return RSC_CREATED;
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

char* cse_to_json(CSE* cse_object) {
	char *json = NULL;

	cJSON *root = NULL;
	cJSON *cse = NULL;

	/* Our "cse" item: */
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:cb", cse = cJSON_CreateObject());
	cJSON_AddStringToObject(cse, "pi", cse_object->pi);
	cJSON_AddStringToObject(cse, "ri", cse_object->ri);
	cJSON_AddNumberToObject(cse, "ty", cse_object->ty);
	cJSON_AddStringToObject(cse, "ct", cse_object->ct);
	cJSON_AddStringToObject(cse, "rn", cse_object->rn);
	cJSON_AddStringToObject(cse, "lt", cse_object->lt);
	cJSON_AddStringToObject(cse, "csi", cse_object->csi);

	json = cJSON_PrintUnformatted(root);

	cJSON_Delete(root);

	return json;
}

char* ae_to_json(AE *ae_object) {
	char *json = NULL;

	cJSON *root = NULL;
	cJSON *ae = NULL;
	cJSON *lbl = NULL;
	cJSON *srv = NULL;
	cJSON *acpi = NULL;

	/* Our "ae" item: */
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:ae", ae = cJSON_CreateObject());
	cJSON_AddStringToObject(ae, "rn", ae_object->rn);
	cJSON_AddNumberToObject(ae, "ty", ae_object->ty);
	cJSON_AddStringToObject(ae, "pi", ae_object->pi);
	cJSON_AddStringToObject(ae, "ri", ae_object->ri);
	cJSON_AddStringToObject(ae, "ct", ae_object->ct);
	cJSON_AddStringToObject(ae, "lt", ae_object->lt);
	cJSON_AddStringToObject(ae, "et", ae_object->et);
	cJSON_AddStringToObject(ae, "api", ae_object->api);
	cJSON_AddBoolToObject(ae, "rr", ae_object->rr);
	cJSON_AddStringToObject(ae, "aei", ae_object->aei);

	//lbl
	if(ae_object->lbl) {
		lbl = string_to_cjson_string_list_item(ae_object->lbl);
		cJSON_AddItemToObject(ae, "lbl", lbl);
	}

	//acpi
	if(ae_object->acpi) {
		acpi = string_to_cjson_string_list_item(ae_object->acpi);
		cJSON_AddItemToObject(ae, "acpi", acpi);
	}

	//srv
	if(ae_object->srv) {
		srv = string_to_cjson_string_list_item(ae_object->srv);
		cJSON_AddItemToObject(ae, "srv", srv);
	}

	json = cJSON_PrintUnformatted(root);

	cJSON_Delete(root);

	return json;
}

char* cnt_to_json(CNT* cnt_object) {
	logger("cJSON", LOG_LEVEL_DEBUG, "Call cnt_to_json");
	char *json = NULL;

	cJSON *root = NULL;
	cJSON *cnt = NULL;
	cJSON *acpi = NULL;
	cJSON *lbl = NULL;

	/* Our "cnt" item: */
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:cnt", cnt = cJSON_CreateObject());
	cJSON_AddStringToObject(cnt, "rn", cnt_object->rn);
	cJSON_AddNumberToObject(cnt, "ty", cnt_object->ty);
	cJSON_AddStringToObject(cnt, "pi", cnt_object->pi);
	cJSON_AddStringToObject(cnt, "ri", cnt_object->ri);
	cJSON_AddStringToObject(cnt, "ct", cnt_object->ct);
	cJSON_AddStringToObject(cnt, "lt", cnt_object->lt);
	cJSON_AddNumberToObject(cnt, "st", cnt_object->st);
	cJSON_AddStringToObject(cnt, "et", cnt_object->et);
	cJSON_AddNumberToObject(cnt, "cni", cnt_object->cni);
	cJSON_AddNumberToObject(cnt, "cbs", cnt_object->cbs);
	if(cnt_object->mni != INT_MIN) cJSON_AddNumberToObject(cnt, "mni", cnt_object->mni);
	if(cnt_object->mbs != INT_MIN) cJSON_AddNumberToObject(cnt, "mbs", cnt_object->mbs);

	// acpi
	if(cnt_object->acpi) {
		acpi = string_to_cjson_string_list_item(cnt_object->acpi);
		cJSON_AddItemToObject(cnt, "acpi", acpi);
	}

	//lbl
	if(cnt_object->lbl) {
		lbl = string_to_cjson_string_list_item(cnt_object->lbl);
		cJSON_AddItemToObject(cnt, "lbl", lbl);
	}
	json = cJSON_PrintUnformatted(root);

	cJSON_Delete(root);

	return json;
}

char* cin_to_json(CIN* cin_object) {
	char *json = NULL;

	cJSON *root = NULL;
	cJSON *cin = NULL;

	/* Our "cin" item: */
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:cin", cin = cJSON_CreateObject());
	cJSON_AddStringToObject(cin, "rn", cin_object->rn);
	cJSON_AddNumberToObject(cin, "ty", cin_object->ty);
	cJSON_AddStringToObject(cin, "pi", cin_object->pi);
	cJSON_AddStringToObject(cin, "ri", cin_object->ri);
	cJSON_AddStringToObject(cin, "ct", cin_object->ct);
	cJSON_AddStringToObject(cin, "lt", cin_object->lt);
	cJSON_AddNumberToObject(cin, "st", cin_object->st);
	cJSON_AddStringToObject(cin, "et", cin_object->et);
	cJSON_AddNumberToObject(cin, "cs", cin_object->cs);
	cJSON_AddStringToObject(cin, "con", cin_object->con);

	json = cJSON_PrintUnformatted(root);

	cJSON_Delete(root);

	return json;
}

char* sub_to_json(SUB *sub_object) {
	char *json = NULL;
	cJSON *root = NULL;
	cJSON *sub = NULL;
	cJSON *nu = NULL;
	cJSON *enc = NULL;
	cJSON *net = NULL;

	/* Our "sub" item: */
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:sub", sub = cJSON_CreateObject());
	cJSON_AddStringToObject(sub, "rn", sub_object->rn);
	cJSON_AddNumberToObject(sub, "ty", sub_object->ty);
	cJSON_AddStringToObject(sub, "pi", sub_object->pi);
	cJSON_AddStringToObject(sub, "ri", sub_object->ri);
	cJSON_AddStringToObject(sub, "ct", sub_object->ct);
	cJSON_AddStringToObject(sub, "lt", sub_object->lt);
	cJSON_AddStringToObject(sub, "et", sub_object->et);

	// nu
	if(sub_object->nu) {
		nu = string_to_cjson_string_list_item(sub_object->nu);
		cJSON_AddItemToObject(sub, "nu", nu);
	}

	// net
	cJSON_AddItemToObject(sub, "enc", enc = cJSON_CreateObject());

	if(sub_object->net) {
		net = string_to_cjson_int_list_item(sub_object->net);
		cJSON_AddItemToObject(enc, "net", net);
	}

	// nct
	cJSON_AddNumberToObject(sub, "nct", sub_object->nct);

	json = cJSON_PrintUnformatted(root);

	cJSON_Delete(root);

	return json;
}

char* notification_to_json(char *sur, int net, char *rep) {
	char *json = NULL;

	cJSON *root = NULL;
	cJSON *sgn = NULL;
	cJSON *nev = NULL;

	/* Our "noti" item: */
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:sgn", sgn = cJSON_CreateObject());
	cJSON_AddStringToObject(sgn, "sur", sur);
	cJSON_AddItemToObject(sgn, "nev", nev = cJSON_CreateObject());
	cJSON_AddNumberToObject(nev, "net", net);
	cJSON_AddStringToObject(nev, "rep", rep);

	json = cJSON_PrintUnformatted(root);

	cJSON_Delete(root);

	return json;
}

char* acp_to_json(ACP *acp_object) {
	char *json = NULL;

	cJSON *root = NULL;
	cJSON *acp = NULL;
	cJSON *pv = NULL;
	cJSON *pvs = NULL;
	cJSON *acrs = NULL;
	cJSON *acr = NULL;
	cJSON *acor = NULL;
	cJSON *lbl = NULL;

	char *acor_copy = NULL;
	char *acor_remainder = NULL;
	char *acor_str = NULL;

	char *acop_copy = NULL;
	char *acop_remainder = NULL;
	char *acop_str = NULL;
	
	/* Our "acp" item: */
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:acp", acp = cJSON_CreateObject());
	cJSON_AddStringToObject(acp, "rn", acp_object->rn);
	cJSON_AddNumberToObject(acp, "ty", acp_object->ty);
	cJSON_AddStringToObject(acp, "pi", acp_object->pi);
	cJSON_AddStringToObject(acp, "ri", acp_object->ri);
	cJSON_AddStringToObject(acp, "ct", acp_object->ct);
	cJSON_AddStringToObject(acp, "lt", acp_object->lt);
	cJSON_AddStringToObject(acp, "et", acp_object->et);

	if(acp_object->lbl) {
		lbl = string_to_cjson_string_list_item(acp_object->lbl);
		cJSON_AddItemToObject(acp, "lbl", lbl);
	}
	// pv
	if(acp_object->pv_acor && acp_object->pv_acop) {
		cJSON_AddItemToObject(acp, "pv", pv = cJSON_CreateObject());

		// acr
		acrs = cJSON_CreateArray();
		cJSON_AddItemToObject(pv, "acr", acrs);

		// acor
		acor_copy = (char *)malloc(sizeof(char) * strlen(acp_object->pv_acor) + 1);
		acor_copy = strcpy(acor_copy, acp_object->pv_acor);
		acor_str = strtok_r(acor_copy, ",", &acor_remainder);

		// acop
		acop_copy = (char *)malloc(sizeof(char) * strlen(acp_object->pv_acop) + 1);
		acop_copy = strcpy(acop_copy, acp_object->pv_acop);
		acop_str = strtok_r(acop_copy, ",", &acop_remainder);

		while (1) {
			if (acop_str == NULL) {
				break;
			}

			char *acop = acop_str;

			cJSON_AddItemToArray(acrs, acr = cJSON_CreateObject());

			acor = cJSON_CreateArray();

			do {
				cJSON_AddItemToArray(acor, cJSON_CreateString(acor_str));
				acor_str = strtok_r(NULL, ",", &acor_remainder);

				acop_str = strtok_r(NULL, ",", &acop_remainder);
			} while (acop_str != NULL && strcmp(acop, acop_str) == 0);
			cJSON_AddItemToObject(acr, "acor", acor);
			cJSON_AddItemToObject(acr, "acop", cJSON_CreateNumber(atoi(acop)));
		}
		free(acor_copy);
		free(acop_copy);
	}

	// pvs
	cJSON_AddItemToObject(acp, "pvs", pvs = cJSON_CreateObject());

	// acr
	acrs = cJSON_CreateArray();
	cJSON_AddItemToObject(pvs, "acr", acrs);

	// acor
	acor_copy = (char *)malloc(sizeof(char) * strlen(acp_object->pvs_acor) + 1);
	acor_copy = strcpy(acor_copy, acp_object->pvs_acor);
	acor_remainder = NULL;
	acor_str = strtok_r(acor_copy, ",", &acor_remainder);
	
	// acop
	acop_copy = (char *)malloc(sizeof(char) * strlen(acp_object->pvs_acop) + 1);
	acop_copy = strcpy(acop_copy, acp_object->pvs_acop);
	acop_remainder = NULL;
	acop_str = strtok_r(acop_copy, ",", &acop_remainder);

	while (1) {
		if (acop_str == NULL) {
			break;
		}

		char *acop = acop_str;

		cJSON_AddItemToArray(acrs, acr = cJSON_CreateObject());

		acor = cJSON_CreateArray();

		do {
			cJSON_AddItemToArray(acor, cJSON_CreateString(acor_str));
			acor_str = strtok_r(NULL, ",", &acor_remainder);

			acop_str = strtok_r(NULL, ",", &acop_remainder);
		} while (acop_str != NULL && strcmp(acop, acop_str) == 0);
		cJSON_AddItemToObject(acr, "acor", acor);
		cJSON_AddItemToObject(acr, "acop", cJSON_CreateNumber(atoi(acop)));
	}

	json = cJSON_PrintUnformatted(root);

	cJSON_Delete(root);

	free(acor_copy);
	free(acop_copy);
	return json;
}

char* discovery_to_json(char **result, int size) {
	char *json = NULL;

	cJSON *root = NULL;
	cJSON *uril = NULL;

	/* Our "cnt" item: */
	root = cJSON_CreateObject();

	// uril
	uril = cJSON_CreateArray();

	for(int i=0; i<size; i++) {
		cJSON_AddItemToArray(uril, cJSON_CreateString(result[i]));
	}
	cJSON_AddItemToObject(root, "m2m:uril", uril);

	json = cJSON_PrintUnformatted(root);

	cJSON_Delete(root);

	return json;
}

char *grp_to_json(GRP *grp_object){

	char *json = NULL;

	cJSON *root;
	cJSON *grp;
	cJSON *acpi;
	cJSON *macp;
	cJSON *mid;

	int cnm = 0;

	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:grp", grp = cJSON_CreateObject());
	cJSON_AddStringToObject(grp, "ri", grp_object->ri);
	cJSON_AddStringToObject(grp, "pi", grp_object->pi);
	cJSON_AddStringToObject(grp, "rn", grp_object->rn);
	cJSON_AddStringToObject(grp, "ct", grp_object->ct);
	cJSON_AddStringToObject(grp, "lt", grp_object->lt);
	cJSON_AddStringToObject(grp, "et", grp_object->et);
	cJSON_AddBoolToObject(grp, "mtv", grp_object->mtv);

	cJSON_AddNumberToObject(grp, "mnm", grp_object->mnm);
	cJSON_AddNumberToObject(grp, "cnm", grp_object->cnm);
	cJSON_AddNumberToObject(grp, "mt", grp_object->mt);
	cJSON_AddNumberToObject(grp, "csy", grp_object->csy);
	cJSON_AddNumberToObject(grp, "ty", 9);

	if(grp_object->acpi){
		acpi = string_to_cjson_string_list_item(grp_object->acpi);
		cJSON_AddItemToObject(grp, "acpi", acpi);
	}
	if(grp_object->macp){
		macp = string_to_cjson_string_list_item(grp_object->macp);
		cJSON_AddItemToObject(grp, "macp", macp);
	} 

	if(grp_object->mid){
		mid = cJSON_CreateArray();
		for(int i = 0 ; i < grp_object->cnm ; i++){
			cJSON_AddItemToArray(mid, cJSON_CreateString(grp_object->mid[i]));
		}
	}
	cJSON_AddItemToObject(grp, "mid", mid);
	

	json = cJSON_PrintUnformatted(root);

	cJSON_Delete(root);

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