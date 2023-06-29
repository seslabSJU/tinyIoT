#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <malloc.h>
#include <sys/timeb.h>
#include <limits.h>
#include "onem2m.h"
#include "dbmanager.h"
#include "jsonparser.h"
#include "httpd.h"
#include "mqttClient.h"
#include "onem2mTypes.h"
#include "config.h"
#include "util.h"
#include "cJSON.h"

extern ResourceTree *rt;

void init_cse(CSE* cse) {
	char *ct = get_local_time(0);
	char *ri = CSE_BASE_RI;
	char *rn = CSE_BASE_NAME;
	
	cse->ri = (char*)malloc((strlen(ri) + 1) * sizeof(char));
	cse->rn = (char*)malloc((strlen(rn) + 1) * sizeof(char));
	cse->ct = (char*)malloc((strlen(ct) + 1) * sizeof(char));
	cse->lt = (char*)malloc((strlen(ct) + 1) * sizeof(char));
	cse->csi = (char*)malloc((strlen(rn) + 2) * sizeof(char));
	cse->pi = (char*)malloc((strlen("NULL") + 1) * sizeof(char));
	
	strcpy(cse->ri, ri);
	strcpy(cse->rn, rn);
	strcpy(cse->ct, ct);
	strcpy(cse->lt, ct);
	strcpy(cse->csi,"/");
	strcat(cse->csi,rn);
	strcpy(cse->pi, "NULL");
	cse->uri = strdup(rn);
	
	cse->ty = RT_CSE;
	
	free(ct); ct = NULL;
}

void init_ae(AE* ae, RTNode *parent_rtnode, char *origin) {
	int origin_size;
	char *uri[128];

	ae->ct = get_local_time(0);
	ae->et = get_local_time(DEFAULT_EXPIRE_TIME);

	if(origin && (origin_size = strlen(origin)) > 0) {
		if(origin[0] != 'C') {
			// need rejection logic
			ae->ri = resource_identifier(RT_AE, ae->ct); // temporary logic
		} else {
			ae->ri = (char *)malloc(sizeof(char) * (origin_size + 1));
			strcpy(ae->ri, origin);
		}
	} else {
		ae->ri = resource_identifier(RT_AE, ae->ct);
	}

	if(!ae->rn) 
		ae->rn = strdup(ae->ri);
	
	ae->pi = strdup(get_ri_rtnode(parent_rtnode));
	ae->lt = strdup(ae->ct);
	ae->aei = strdup(ae->ri);

	sprintf(uri, "%s/%s", get_uri_rtnode(parent_rtnode), ae->rn);
	ae->uri = strdup(uri);

	
	ae->ty = RT_AE;
}

void init_cnt(CNT* cnt, RTNode *parent_rtnode) {
	char uri[128] = {0};
	cnt->ct = get_local_time(0);
	cnt->et = get_local_time(DEFAULT_EXPIRE_TIME);
	cnt->ri = resource_identifier(RT_CNT, cnt->ct);
	
	if(!cnt->rn) {
		cnt->rn = (char*)malloc((strlen(cnt->ri) + 1) * sizeof(char));
		strcpy(cnt->rn, cnt->ri);
	}
	
	cnt->pi = strdup(get_ri_rtnode(parent_rtnode));
	cnt->lt = strdup(cnt->ct);;
	
	cnt->ty = RT_CNT;
	cnt->st = 0;
	cnt->cni = 0;
	cnt->cbs = 0;

	sprintf(uri, "%s/%s", get_uri_rtnode(parent_rtnode), cnt->rn);
	cnt->uri = strdup(uri);
}

void init_cin(CIN* cin, RTNode *parent_rtnode) {
	char uri[256] = {0};
	cin->ct = get_local_time(0);
	cin->et = get_local_time(DEFAULT_EXPIRE_TIME);
	cin->ri = resource_identifier(RT_CIN, cin->ct);
	
	cin->rn = strdup(cin->ri);
	cin->pi = strdup(get_ri_rtnode(parent_rtnode));
	cin->lt = strdup(cin->ct);
	
	cin->ty = RT_CIN;
	cin->st = 0;
	cin->cs = strlen(cin->con);
	sprintf(uri, "%s/%s",get_uri_rtnode(parent_rtnode), cin->rn);
	cin->uri = strdup(uri);
}

void init_sub(SUB* sub, RTNode *parent_rtnode) {
	char uri[256] = {0};
	sub->ct = get_local_time(0);
	sub->et = get_local_time(DEFAULT_EXPIRE_TIME);
	sub->ri = resource_identifier(RT_SUB, sub->ct);

	if(!sub->rn) {
		sub->rn = (char*)malloc((strlen(sub->ri) + 1) * sizeof(char));
		strcpy(sub->rn, sub->ri);
	}

	if(!sub->net) {
		sub->net = (char*)malloc(2*sizeof(char));
		strcpy(sub->net,"1");
	}

	sprintf(uri, "/%s/%s", get_uri_rtnode(parent_rtnode), sub->rn);
	sub->pi = strdup(get_ri_rtnode(parent_rtnode));
	sub->lt = strdup(sub->ct);

	sub->sur = strdup(uri);
	sub->uri = strdup(uri);

	sub->ty = RT_SUB;
	sub->nct = 0;
}

void init_acp(ACP* acp, RTNode *parent_rtnode) {
	char *ct = get_local_time(0);
	char *et = get_local_time(DEFAULT_EXPIRE_TIME);
	char *ri = resource_identifier(RT_ACP, ct);
	char uri[128] = {0};

	if(!acp->rn) {
		acp->rn = (char*)malloc((strlen(acp->ri) + 1) * sizeof(char));
		strcpy(acp->rn, acp->ri);
	}
	
	acp->ri = strdup(ri);
	acp->pi = strdup(get_ri_rtnode(parent_rtnode));
	acp->et = strdup(et);
	acp->ct = strdup(ct);
	acp->lt = strdup(ct);

	sprintf(uri, "%s/%s", get_uri_rtnode(parent_rtnode), acp->rn);
	acp->uri = strdup(uri);
	
	acp->ty = RT_ACP;
}

RTNode* create_rtnode(void *obj, ResourceType ty){
	RTNode* rtnode = (RTNode *)calloc(1, sizeof(RTNode));

	rtnode->ty = ty;
	rtnode->obj = obj;

	rtnode->parent = NULL;
	rtnode->child = NULL;
	rtnode->sibling_left = NULL;
	rtnode->sibling_right = NULL;
	
	return rtnode;
}

int create_ae(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	int e = check_aei_duplicate(o2pt, parent_rtnode);
	if(e != -1) e = check_rn_invalid(o2pt, RT_AE);
	if(e == -1) return o2pt->rsc;

	if(parent_rtnode->ty != RT_CSE) {
		handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
		return o2pt->rsc;
	}

	AE* ae = cjson_to_ae(o2pt->cjson_pc);

	if(!ae) {
		handle_error(o2pt, RSC_CONTENTS_UNACCEPTABLE, "insufficient mandatory attribute(s)");
		return RSC_CONTENTS_UNACCEPTABLE;
	}
	if(ae->api[0] != 'R' && ae->api[0] != 'N') {
		handle_error(o2pt, RSC_BAD_REQUEST, "attribute `api` prefix is invalid");
		free_ae(ae);
		return RSC_BAD_REQUEST;
	}
	init_ae(ae, parent_rtnode, o2pt->fr);
	
	int result = db_store_ae(ae);
	if(result != 1) { 
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail");
		free_ae(ae); ae = NULL;
		return RSC_INTERNAL_SERVER_ERROR;
	}
	
	RTNode* child_rtnode = create_rtnode(ae, RT_AE);
	add_child_resource_tree(parent_rtnode, child_rtnode);
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = ae_to_json(ae);
	return o2pt->rsc = RSC_CREATED;
}

int create_cnt(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	if(parent_rtnode->ty != RT_CNT && parent_rtnode->ty != RT_AE && parent_rtnode->ty != RT_CSE) {
		handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
		return RSC_INVALID_CHILD_RESOURCETYPE;
	}

	CNT* cnt = cjson_to_cnt(o2pt->cjson_pc);

	if(!cnt) {
		handle_error(o2pt, RSC_CONTENTS_UNACCEPTABLE, "insufficient mandatory attribute(s)");
		return o2pt->rsc;
	}
	if(cnt->mbs != INT_MIN && cnt->mbs < 0) {
		handle_error(o2pt, RSC_BAD_REQUEST, "attribute `mbs` is invalid");
		free(cnt); cnt = NULL;
		return o2pt->rsc;
	}
	if(cnt->mni != INT_MIN && cnt->mni < 0) {
		handle_error(o2pt, RSC_BAD_REQUEST, "attribute `mni` is invalid");
		return o2pt->rsc;
	}
	init_cnt(cnt,parent_rtnode);

	int result = db_store_cnt(cnt);
	if(result != 1) { 
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail"); 
		free_cnt(cnt); cnt = NULL;
		return o2pt->rsc;
	}
	
	RTNode* child_rtnode = create_rtnode(cnt, RT_CNT);
	add_child_resource_tree(parent_rtnode,child_rtnode);
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cnt_to_json(cnt);
	o2pt->rsc = RSC_CREATED;
	return RSC_CREATED;
}

int create_cin(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	if(parent_rtnode->ty != RT_CNT) {
		handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
		return o2pt->rsc;
	}

	CIN* cin = cjson_to_cin(o2pt->cjson_pc);
	CNT* cnt = (CNT *)parent_rtnode->obj;

	if(!cin) {
		handle_error(o2pt, RSC_CONTENTS_UNACCEPTABLE, "insufficient mandatory attribute(s)");
		return o2pt->rsc;
	} else if(cnt->mbs >= 0 && cin->cs > cnt->mbs) {
		handle_error(o2pt, RSC_NOT_ACCEPTABLE, "too large content size");
		free(cin); cin = NULL;
		return o2pt->rsc;
	}
	init_cin(cin,parent_rtnode);

	int result = db_store_cin(cin);
	if(result != 1) { 
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail"); 
		free_cin(cin); cin = NULL;
		return o2pt->rsc;
	}

	RTNode *cin_rtnode = create_rtnode(cin, RT_CIN);
	update_cnt_cin(parent_rtnode, cin_rtnode, 1);
	
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cin_to_json(cin);
	o2pt->rsc = RSC_CREATED;
	free_rtnode(cin_rtnode);
	return RSC_CREATED;
}

int create_sub(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	if(parent_rtnode->ty == RT_CIN || parent_rtnode->ty == RT_SUB) {
		handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
		return o2pt->rsc;
	}

	SUB* sub = cjson_to_sub(o2pt->cjson_pc);

	if(!sub) {
		handle_error(o2pt, RSC_CONTENTS_UNACCEPTABLE, "insufficient mandatory attribute(s)");
		return o2pt->rsc;
	}
	init_sub(sub, parent_rtnode);
	
	int result = db_store_sub(sub);
	if(result != 1) { 
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail");
		free_sub(sub); sub = NULL;
		return o2pt->rsc;
	}

	RTNode* child_rtnode = create_rtnode(sub, RT_SUB);
	add_child_resource_tree(parent_rtnode,child_rtnode);
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = sub_to_json(sub);
	o2pt->rsc = RSC_CREATED;
	return RSC_CREATED;
}

int create_acp(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	if(parent_rtnode->ty != RT_CSE && parent_rtnode->ty != RT_AE) {
		handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
		return o2pt->rsc;
	}

	ACP* acp = cjson_to_acp(o2pt->cjson_pc);
	if(!acp) {
		handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
		return o2pt->rsc;
	}
	init_acp(acp, parent_rtnode);
	
	int result = db_store_acp(acp);
	if(result != 1) { 
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail"); 
		free_acp(acp); acp = NULL;
		return o2pt->rsc;
	}
	
	RTNode* child_rtnode = create_rtnode(acp, RT_ACP);
	add_child_resource_tree(parent_rtnode, child_rtnode);
	
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = acp_to_json(acp);
	o2pt->rsc = RSC_CREATED;
	return RSC_CREATED;
}

int retrieve_cse(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	CSE* cse = (CSE *)target_rtnode->obj;
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cse_to_json(cse);
	o2pt->rsc = RSC_OK;
	//free_cse(gcse); gcse = NULL;
	return RSC_OK;
}

int retrieve_ae(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	AE* ae = (AE *)target_rtnode->obj;
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = ae_to_json(ae);
	o2pt->rsc = RSC_OK;
	return RSC_OK;
}

int retrieve_cnt(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	CNT* cnt = (CNT *)target_rtnode->obj;
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cnt_to_json(cnt);
	o2pt->rsc = RSC_OK;

	return RSC_OK;
}

int retrieve_cin(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	CIN* cin = (CIN *)target_rtnode->obj;
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cin_to_json(cin);
	o2pt->rsc = RSC_OK;

	return RSC_OK;
}

int retrieve_sub(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	SUB* sub = (SUB *)target_rtnode->obj;
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = sub_to_json(sub);
	o2pt->rsc = RSC_OK;

	return RSC_OK;
}

int retrieve_acp(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	ACP* acp = (ACP *)target_rtnode->obj;
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = acp_to_json(acp);
	o2pt->rsc = RSC_OK;

	return RSC_OK;
}

int retrieve_grp(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	GRP *grp = (GRP *)target_rtnode->obj;
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = grp_to_json(grp);
	o2pt->rsc = RSC_OK;

	return RSC_OK;
}

int update_ae(oneM2MPrimitive *o2pt, RTNode *target_rtnode) {
	char invalid_key[][8] = {"ty", "pi", "ri", "rn", "ct"};
	cJSON *m2m_ae = cJSON_GetObjectItem(o2pt->cjson_pc, "m2m:ae");
	int invalid_key_size = sizeof(invalid_key)/(8*sizeof(char));
	for(int i=0; i<invalid_key_size; i++) {
		if(cJSON_GetObjectItem(m2m_ae, invalid_key[i])) {
			handle_error(o2pt, RSC_BAD_REQUEST, "unsupported attribute on update");
			return RSC_BAD_REQUEST;
		}
	}

	int result;
	AE *ae = (AE*)target_rtnode->obj;

	result = set_ae_update(o2pt, m2m_ae, ae);
	if(result != 1) {
		return o2pt->rsc;
	}
	#ifdef SQLITE_DB
	result = db_update_ae(ae);
	#else
	result = db_delete_onem2m_resource(target_rtnode);
	result = db_store_ae(ae);
	#endif
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = ae_to_json(ae);
	o2pt->rsc = RSC_UPDATED;
	return RSC_UPDATED;
}

int update_cnt(oneM2MPrimitive *o2pt, RTNode *target_rtnode) {
	char invalid_key[][8] = {"ty", "pi", "ri", "rn", "ct"};
	cJSON *m2m_cnt = cJSON_GetObjectItem(o2pt->cjson_pc, "m2m:cnt");
	int invalid_key_size = sizeof(invalid_key)/(8*sizeof(char));
	for(int i=0; i<invalid_key_size; i++) {
		if(cJSON_GetObjectItem(m2m_cnt, invalid_key[i])) {
			handle_error(o2pt, RSC_BAD_REQUEST, "unsupported attribute on update");
			return RSC_BAD_REQUEST;
		}
	}

	CNT* cnt = (CNT *)target_rtnode->obj;
	int result;

	set_cnt_update(o2pt, m2m_cnt, cnt);
	if(cnt->mbs != INT_MIN && cnt->mbs < 0) {
		handle_error(o2pt, RSC_BAD_REQUEST, "attribute `mbs` is invalid");
		return o2pt->rsc;
	}
	if(cnt->mni != INT_MIN && cnt->mni < 0) {
		handle_error(o2pt, RSC_BAD_REQUEST, "attribute `mni` is invalid");
		return o2pt->rsc;
	}
	//set_rtnode_update(target_rtnode, after);
	delete_cin_under_cnt_mni_mbs(target_rtnode);
	cnt->st++;
	#ifdef SQLITE_DB
	result = db_update_cnt(cnt);
	#else
	result = db_delete_onem2m_resource(target_rtnode);
	result = db_store_cnt(cnt);
	#endif
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cnt_to_json(cnt);
	o2pt->rsc = RSC_UPDATED;
	return RSC_UPDATED;
}

int update_acp(oneM2MPrimitive *o2pt, RTNode *target_rtnode) {
	char invalid_key[][8] = {"ty", "pi", "ri", "rn", "ct"};
	cJSON *m2m_acp = cJSON_GetObjectItem(o2pt->cjson_pc, "m2m:acp");
	int invalid_key_size = sizeof(invalid_key)/(8*sizeof(char));
	for(int i=0; i<invalid_key_size; i++) {
		if(cJSON_GetObjectItem(m2m_acp, invalid_key[i])) {
			handle_error(o2pt, RSC_BAD_REQUEST, "unsupported attribute on update");
			return RSC_BAD_REQUEST;
		}
	}
	ACP* acp = (ACP *)target_rtnode->obj;
	int result;
	
	result = set_acp_update(o2pt, m2m_acp, acp);
	if(result != 1) return result;
	
	#ifdef SQLITE_DB
	result = db_update_acp(acp);
	#else
	result = db_delete_onem2m_resource(target_rtnode);
	result = db_store_acp(acp);
	#endif
	
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = acp_to_json(acp);
	o2pt->rsc = RSC_UPDATED;
	return RSC_UPDATED;
}

int set_ae_update(oneM2MPrimitive *o2pt, cJSON *m2m_ae, AE* ae) {
	cJSON *rr = cJSON_GetObjectItemCaseSensitive(m2m_ae, "rr");
	cJSON *lbl = cJSON_GetObjectItem(m2m_ae, "lbl");
	cJSON *srv = cJSON_GetObjectItem(m2m_ae, "srv");
	cJSON *et = cJSON_GetObjectItem(m2m_ae, "et");
	cJSON *acpi = cJSON_GetObjectItem(m2m_ae, "acpi");

	if(acpi) {
		if(rr || lbl || srv || et) {
			handle_error(o2pt, RSC_BAD_REQUEST, "`acpi` must be only attribute in update");
			return -1;
		} else if(!ae->acpi && strcmp(o2pt->fr, ae->origin)) {
			handle_error(o2pt, RSC_ORIGINATOR_HAS_NO_PRIVILEGE, "originator has no privilege on udpate `acpi`");
			return -1;
		} else if(!check_acpi_valid(o2pt, acpi)) {
			return -1;
		} else {
			if(ae->acpi) free(ae->acpi);
			ae->acpi = cjson_string_list_item_to_string(acpi);
		}
	}

	if(rr) {
		if(cJSON_IsTrue(rr)) {
			ae->rr = true;
		} else {
			ae->rr = false;
		}
	}

	if(et) {
		if(ae->et) free(ae->et);
		ae->et = (char *)malloc((strlen(et->valuestring) + 1) * sizeof(char));
		strcpy(ae->et, et->valuestring);
	}

	if(lbl) {
		if(ae->lbl) free(ae->lbl);
		ae->lbl = cjson_string_list_item_to_string(lbl);
	}

	if(srv) {
		if(ae->srv) free(ae->srv);
		ae->srv = cjson_string_list_item_to_string(srv);
	}

	if(ae->lt) free(ae->lt);
	ae->lt = get_local_time(0);
	return 1;
}

int set_cnt_update(oneM2MPrimitive *o2pt, cJSON *m2m_cnt, CNT* cnt) {
	cJSON *lbl = cJSON_GetObjectItem(m2m_cnt, "lbl");
	cJSON *acpi = cJSON_GetObjectItem(m2m_cnt, "acpi");
	cJSON *mni = cJSON_GetObjectItem(m2m_cnt, "mni");
	cJSON *mbs = cJSON_GetObjectItem(m2m_cnt, "mbs");
	cJSON *et = cJSON_GetObjectItem(m2m_cnt, "et");

	if(et) {
		if(cnt->et) free(cnt->et);
		cnt->et = (char *)malloc((strlen(et->valuestring) + 1) * sizeof(char));
		strcpy(cnt->et, et->valuestring);
	}

	if(acpi) {
		if(cnt->acpi) free(cnt->acpi);
		cnt->acpi = cjson_string_list_item_to_string(acpi);
	}
	
	if(lbl) {
		if(cnt->lbl) free(cnt->lbl);
		cnt->lbl = cjson_string_list_item_to_string(lbl);
	}

	if(mni) {
		cnt->mni = mni->valueint;
	}

	if(mbs) {
		cnt->mbs = mbs->valueint;
	}

	if(cnt->lt) free(cnt->lt);
	cnt->lt = get_local_time(0);

	return 1;
}

int set_acp_update(oneM2MPrimitive *o2pt, cJSON *m2m_acp, ACP* acp) {
	cJSON *pv_acr = cJSON_GetObjectItem(m2m_acp, "pv");
	cJSON *pvs_acr = cJSON_GetObjectItem(m2m_acp, "pvs");
	cJSON *et = cJSON_GetObjectItem(m2m_acp, "et");
	cJSON *lbl = cJSON_GetObjectItem(m2m_acp, "lbl");
	cJSON *pv_acor = NULL;
	cJSON *pv_acop = NULL;
	cJSON *pvs_acor = NULL;
	cJSON *pvs_acop = NULL;
	char pv_acor_str[256] = {'\0'};
	char pv_acop_str[256] = {'\0'};
	char pvs_acor_str[256] =  {'\0'};
	char pvs_acop_str[256] =  {'\0'};

	if(pvs_acr) {
		pvs_acr = cJSON_GetObjectItem(pvs_acr, "acr");

		if(pvs_acr) {
			int acr_size = cJSON_GetArraySize(pvs_acr);

			for(int i=0; i<acr_size; i++) {
				cJSON *arr_item = cJSON_GetArrayItem(pvs_acr, i);
				pvs_acor = cJSON_GetObjectItem(arr_item, "acor");
				pvs_acop = cJSON_GetObjectItem(arr_item, "acop");

				if(pvs_acor && pvs_acop) {
					char acop[3];
					sprintf(acop, "%d", pvs_acop->valueint);
					int acor_size = cJSON_GetArraySize(pvs_acor);

					for(int j=0; j<acor_size; j++) {
						char *acor = cJSON_GetArrayItem(pvs_acor, j)->valuestring;
						strcat(pvs_acor_str, acor);
						strcat(pvs_acop_str, acop);
						if(j != acor_size - 1) {
							strcat(pvs_acor_str, ",");
							strcat(pvs_acop_str, ",");
						}
					}
				}
			}
		}
		if(pvs_acor_str[0] != '\0' && pvs_acop_str[0] != '\0') {
			if(acp->pvs_acor) free(acp->pvs_acor);
			if(acp->pvs_acop) free(acp->pvs_acop);
			acp->pvs_acor = strdup(pvs_acor_str);
			acp->pvs_acop = strdup(pvs_acop_str);
		} else {
			handle_error(o2pt, RSC_BAD_REQUEST, "attribute `pvs` is mandatory");
			return RSC_BAD_REQUEST;
		}
	}

	if(lbl) {
		if(acp->lbl) free(acp->lbl);
		acp->lbl = cjson_string_list_item_to_string(lbl);
	}

	if(et) {
		if(acp->et) free(acp->et);
		acp->et = (char *)malloc((strlen(et->valuestring) + 1) * sizeof(char));
		strcpy(acp->et, et->valuestring);
	}

	if(pv_acr) {
		pv_acr = cJSON_GetObjectItem(pv_acr, "acr");

		if(pv_acr) {
			int acr_size = cJSON_GetArraySize(pv_acr);

			for(int i=0; i<acr_size; i++) {
				cJSON *arr_item = cJSON_GetArrayItem(pv_acr, i);
				pv_acor = cJSON_GetObjectItem(arr_item, "acor");
				pv_acop = cJSON_GetObjectItem(arr_item, "acop");

				if(pv_acor && pv_acop) {
					char acop[3];
					sprintf(acop, "%d", pv_acop->valueint);
					int acor_size = cJSON_GetArraySize(pv_acor);

					for(int j=0; j<acor_size; j++) {
						char *acor = cJSON_GetArrayItem(pv_acor, j)->valuestring;
						strcat(pv_acor_str, acor);
						strcat(pv_acop_str, acop);
						if(j != acor_size - 1) {
							strcat(pv_acor_str, ",");
							strcat(pv_acop_str, ",");
						}
					}
				}
			}
		}
		if(pv_acor_str[0] != '\0' && pv_acop_str[0] != '\0') {
			if(acp->pv_acor) free(acp->pv_acor);
			if(acp->pv_acop) free(acp->pv_acop);
			acp->pv_acor = strdup(pv_acor_str);
			acp->pv_acop = strdup(pv_acop_str);
		} else {
			acp->pv_acor = acp->pv_acop = NULL;
		}
	}	

	if(acp->lt) free(acp->lt);
	acp->lt = get_local_time(0);

	return 1;
}

int update_cnt_cin(RTNode *cnt_rtnode, RTNode *cin_rtnode, int sign) {
	CNT *cnt = (CNT *) cnt_rtnode->obj;
	CIN *cin = (CIN *) cin_rtnode->obj;
	cnt->cni += sign;
	cnt->cbs += sign*(cin->cs);
	delete_cin_under_cnt_mni_mbs(cnt_rtnode);	
	cnt->st++;
	#ifdef SQLITE_DB
	db_update_cnt(cnt);
	#else
	db_delete_onem2m_resource(cnt_rtnode);
	db_store_cnt(cnt);
	#endif

	return 1;
}

int delete_onem2m_resource(oneM2MPrimitive *o2pt, RTNode* target_rtnode) {
	logger("O2M", LOG_LEVEL_INFO, "Delete oneM2M resource");
	if(target_rtnode->ty == RT_AE || target_rtnode->ty == RT_CNT || target_rtnode->ty == RT_GRP) {
		if(check_privilege(o2pt, target_rtnode, ACOP_DELETE) == -1) {
			return o2pt->rsc;
		}
	}
	if(target_rtnode->ty == RT_CSE) {
		handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "CSE can not be deleted");
		return RSC_OPERATION_NOT_ALLOWED;
	}
	delete_rtnode_and_db_data(o2pt, target_rtnode,1);
	target_rtnode = NULL;
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = NULL;
	o2pt->rsc = RSC_DELETED;
	return RSC_DELETED;
}

int delete_rtnode_and_db_data(oneM2MPrimitive *o2pt, RTNode *rtnode, int flag) {
	switch(rtnode->ty) {
	case RT_AE : 
	case RT_CNT : 
		#ifdef BERKELEY_DB
		db_delete_child_cin(rtnode);
		#endif
		db_delete_onem2m_resource(rtnode); 
		break;
	case RT_CIN :
		db_delete_onem2m_resource(rtnode);
		update_cnt_cin(rtnode->parent, rtnode,-1);
		break;
	case RT_SUB :
		db_delete_sub(get_ri_rtnode(rtnode));
		break;
	case RT_ACP :
		db_delete_acp(get_ri_rtnode(rtnode));
		break;
	case RT_GRP:
		db_delete_grp(get_ri_rtnode(rtnode));
		break;
	}

	notify_onem2m_resource(o2pt, rtnode);
	if(rtnode->ty == RT_CIN) return 1;

	RTNode *left = rtnode->sibling_left;
	RTNode *right = rtnode->sibling_right;
	
	if(rtnode->ty != RT_CIN) {
		if(flag == 1) {
			if(left) left->sibling_right = right;
			else rtnode->parent->child = right;
			if(right) right->sibling_left = left;
		} else {
			if(right) delete_rtnode_and_db_data(o2pt, right, 0);
		}
	}
	
	if(rtnode->child) delete_rtnode_and_db_data(o2pt, rtnode->child, 0);
	
	free_rtnode(rtnode); rtnode = NULL;
	return 1;
}

void free_cse(CSE *cse) {
	if(cse->ct) free(cse->ct);
	if(cse->lt) free(cse->lt);
	if(cse->rn) free(cse->rn);
	if(cse->ri) free(cse->ri);
	if(cse->csi) free(cse->csi);
	if(cse->pi) free(cse->pi);
	if(cse->uri) free(cse->uri);
	free(cse); cse = NULL;
}

void free_ae(AE *ae) {
	if(ae->et) free(ae->et);
	if(ae->ct) free(ae->ct);
	if(ae->lt) free(ae->lt);
	if(ae->rn) free(ae->rn);
	if(ae->ri) free(ae->ri);
	if(ae->pi) free(ae->pi);
	if(ae->api) free(ae->api);
	if(ae->aei) free(ae->aei);
	if(ae->lbl) free(ae->lbl);
	if(ae->srv) free(ae->srv);
	if(ae->acpi) free(ae->acpi);
	if(ae->uri) free(ae->uri);
	free(ae); ae = NULL;
}

void free_cnt(CNT *cnt) {
	if(cnt->et) free(cnt->et);
	if(cnt->ct) free(cnt->ct);
	if(cnt->lt) free(cnt->lt);
	if(cnt->rn) free(cnt->rn);
	if(cnt->ri) free(cnt->ri);
	if(cnt->pi) free(cnt->pi);
	if(cnt->acpi) free(cnt->acpi);
	if(cnt->lbl) free(cnt->lbl);
	if(cnt->uri) free(cnt->uri);
	free(cnt); cnt = NULL;
}

void free_cin(CIN* cin) {
	if(!cin) return;
	if(cin->et) free(cin->et);
	if(cin->ct) free(cin->ct);
	if(cin->lt) free(cin->lt);
	if(cin->rn) free(cin->rn);
	if(cin->ri) free(cin->ri);
	if(cin->pi) free(cin->pi);
	if(cin->con) free(cin->con);
	if(cin->uri) free(cin->uri);
	free(cin); cin = NULL;
}

void free_sub(SUB* sub) {
	if(sub->et) free(sub->et);
	if(sub->ct) free(sub->ct);
	if(sub->lt) free(sub->lt);
	if(sub->rn) free(sub->rn);
	if(sub->ri) free(sub->ri);
	if(sub->pi) free(sub->pi);
	if(sub->nu) free(sub->nu);
	if(sub->net) free(sub->net);
	if(sub->uri) free(sub->uri);
	free(sub); sub = NULL;
}

void free_acp(ACP* acp) {
	if(acp->et) free(acp->et);
	if(acp->ct) free(acp->ct);
	if(acp->lt) free(acp->lt);
	if(acp->rn) free(acp->rn);
	if(acp->ri) free(acp->ri);
	if(acp->pi) free(acp->pi);
	if(acp->pv_acor) free(acp->pv_acor);
	if(acp->pv_acop) free(acp->pv_acop);
	if(acp->pvs_acor) free(acp->pvs_acor);
	if(acp->pvs_acop) free(acp->pvs_acop);
	if(acp->uri) free(acp->uri);
	free(acp); acp = NULL;
}

void free_grp(GRP *grp) {
	if(!grp) return;
	if(grp->rn) free(grp->rn);
	if(grp->ri) free(grp->ri);
	if(grp->ct) free(grp->ct);
	if(grp->et) free(grp->et);
	if(grp->lt) free(grp->lt);
	if(grp->pi) free(grp->pi);
	if(grp->acpi) free(grp->acpi);
	if(grp->uri) free(grp->uri);

	if(grp->mid){
		for(int i = 0 ; i < grp->cnm ; i++){
			if(grp->mid[i]){
				free(grp->mid[i]);
				grp->mid[i] = NULL;
			}
		}
		free(grp->mid); 
		grp->mid = NULL;
	}
	free(grp); 
	grp = NULL;
}

void free_rtnode(RTNode *rtnode) {
	if(rtnode->uri && rtnode->ty != RT_CSE)
		free(rtnode->uri);

	switch(rtnode->ty) {
		case RT_CSE:
			free_cse((CSE *)rtnode->obj);
			break;
		case RT_AE:
			free_ae((AE *)rtnode->obj);
			break;
		case RT_CNT:
			free_cnt((CNT *)rtnode->obj);
			break;
		case RT_CIN:
			free_cin((CIN *)rtnode->obj);
			break;
		case RT_SUB:
			free_sub((SUB *)rtnode->obj);
			break;
		case RT_ACP:
			free_acp((ACP *)rtnode->obj);
			break;
		case RT_GRP:
			free_grp((GRP *)rtnode->obj);
			break;
	}

	free(rtnode);
}

void free_rtnode_list(RTNode *rtnode) {
	RTNode *right = NULL;
	while(rtnode) {
		right = rtnode->sibling_right;
		free_rtnode(rtnode);
		rtnode = right;
	}
}

int set_sub_update(oneM2MPrimitive *o2pt, cJSON *m2m_sub, SUB* sub) {
	cJSON *nu = cJSON_GetObjectItem(m2m_sub, "nu");
	cJSON *net = cJSON_GetObjectItem(cJSON_GetObjectItem(m2m_sub, "enc"), "net");

	if(nu) {
		if(sub->nu) free(sub->nu);
		sub->nu = cjson_string_list_item_to_string(nu);
	}

	if(net) {
		if(sub->net) free(sub->net);
		sub->net = cjson_int_list_item_to_string(net);
	}

	if(sub->lt) free(sub->lt);
	sub->lt = get_local_time(0);
}

/*
void remove_invalid_char_json(char* json) { 
	int size = (int)malloc_usable_size(json); // segmentation fault if json memory not in heap (malloc)
	int index = 0;

	for(int i=0; i<size; i++) {
		if(is_json_valid_char(json[i]) && json[i] != '\\') {
			json[index++] = json[i];
		}
	}

	json[index] = '\0';
}

void set_node_uri(RTNode* node) {
	if(!node->uri) node->uri = (char*)calloc(MAX_URI_SIZE,sizeof(char));

	RTNode *p = node;
	char uri_copy[32][MAX_URI_SIZE];
	int index = -1;

	while(p) {
		strcpy(uri_copy[++index],p->rn);
		p = p->parent;
	}

	for(int i=index; i>=0; i--) {
		strcat(node->uri,"/");
		strcat(node->uri,uri_copy[i]);
	}

	return;
}
*/
/* GROUP IMPLEMENTATION */

void init_grp(GRP *grp, RTNode *parent_rtnode){
	char *ct = get_local_time(0);
	char *et = get_local_time(DEFAULT_EXPIRE_TIME);
	char *ri = resource_identifier(RT_GRP, ct);
	char uri[128] = {0};

	grp->ct = (char *) malloc((strlen(ct) + 1) * sizeof(char));
	grp->et = (char *) malloc((strlen(et) + 1) * sizeof(char));
	grp->ri = (char *) malloc((strlen(ri) + 1) * sizeof(char));
	grp->lt = (char *) malloc((strlen(ri) + 1) * sizeof(char));
	
	if(!grp->rn) grp->rn = strdup(ri);
	strcpy(grp->ct, ct);
	strcpy(grp->et, et);
	strcpy(grp->lt, ct);
	strcpy(grp->ri, ri);
	grp->pi = strdup( get_ri_rtnode(parent_rtnode));

	sprintf(uri, "%s/%s", get_uri_rtnode(parent_rtnode), grp->rn);
	grp->uri = strdup(uri);

	if(grp->csy == 0) grp->csy = CSY_ABANDON_MEMBER;

	free(ct); ct = NULL;
	free(et); et = NULL;
	free(ri); ri = NULL;
}

int set_grp_update(oneM2MPrimitive *o2pt, cJSON *m2m_grp, GRP* grp){
	if(!grp) return RSC_BAD_REQUEST;
	cJSON *acpi = cJSON_GetObjectItem(m2m_grp, "acpi");
	cJSON *et = cJSON_GetObjectItem(m2m_grp, "et");
	cJSON *mt = cJSON_GetObjectItem(m2m_grp, "mt");
	cJSON *mnm = cJSON_GetObjectItem(m2m_grp, "mnm");
	cJSON *mid = cJSON_GetObjectItem(m2m_grp, "mid");
	cJSON *mc = NULL;

	grp->mtv = false;

	if(acpi){
		grp->acpi = cjson_string_list_item_to_string(acpi);
	}

	if(et) {
		grp->et = strdup(et->valuestring);
	}

	if(mt)
		grp->mt = mt->valueint;
	
	if(mnm){
		if(mnm->valueint < grp->cnm){
			return RSC_MAX_NUMBER_OF_MEMBER_EXCEEDED;
		}
		//grp->mnm = mnm->valueint;
		
	}

	int new_cnm = 0;

	if(mid){
		size_t mid_size = cJSON_GetArraySize(mid);
		if(mid_size > grp->mnm) return RSC_MAX_NUMBER_OF_MEMBER_EXCEEDED;
		new_cnm = mid_size;
		int midx = 0;
		for(int i = 0 ; i < grp->mnm ; i++){ //re initialize mid
			if(grp->mid[i])
				free(grp->mid[i]);

			grp->mid[i] = NULL;
		}
		if(mnm)
			grp->mnm = mnm->valueint; // change mnm to new value if valid;

		for(int i = 0 ; i < grp->mnm; i++){
			
			if(i < mid_size){
				mc = cJSON_GetArrayItem(mid, i);
				if(!isMinDup(grp->mid, midx, mc->valuestring)){
					
					logger("o2m-t", LOG_LEVEL_DEBUG, "updating %s to mid[%d]", mc->valuestring, midx);
					grp->mid[midx] = strdup(mc->valuestring);
					midx++;
				}
				else{
					logger("json-t", LOG_LEVEL_DEBUG, "declining %s", mc->valuestring);
					grp->mid[i] = NULL;
					new_cnm--;
				}
				
			}else{
				grp->mid[i] = NULL;
			}
		}
		grp->cnm = new_cnm;
	}

	if(grp->lt) free(grp->lt);
	grp->lt = get_local_time(0);
	return RSC_OK;
}

int update_grp(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	int rsc = 0;
	char invalid_key[][4] = {"ty", "pi", "ri", "rn", "ct"};
	cJSON *m2m_grp = cJSON_GetObjectItem(o2pt->cjson_pc, "m2m:grp");
	int invalid_key_size = sizeof(invalid_key)/(4*sizeof(char));
	for(int i=0; i<invalid_key_size; i++) {
		if(cJSON_GetObjectItem(m2m_grp, invalid_key[i])) {
			handle_error(o2pt, RSC_BAD_REQUEST, "{\"m2m:dbg\": \"unsupported attribute on update\"}");
			return RSC_BAD_REQUEST;
		}
	}

	GRP *new_grp = db_get_grp(get_ri_rtnode(target_rtnode));
	
	int result;
	if( (o2pt->rsc = set_grp_update(o2pt, m2m_grp, new_grp)) >= 4000){
		return o2pt->rsc;
	}
	new_grp->mtv = false;
	if( (rsc = validate_grp(new_grp))  >= 4000){
		o2pt->rsc = rsc;
		return rsc;
	}
	#ifdef SQLITE_DB
	result = db_update_grp(new_grp);
	#else
	result = db_delete_grp(new_grp->ri);
	result = db_store_grp(new_grp);
	#endif
	if(!result){
		logger("O2M", LOG_LEVEL_ERROR, "DB update Failed");
		free_grp(new_grp);
		return RSC_INTERNAL_SERVER_ERROR;
	}

	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = grp_to_json(new_grp);

	o2pt->rsc = RSC_UPDATED;
	if(target_rtnode->obj){
		free_grp((GRP *) target_rtnode->obj);
		target_rtnode->obj = new_grp;
	}

	return RSC_UPDATED;
}

int create_grp(oneM2MPrimitive *o2pt, RTNode *parent_rtnode){
	int e = 1;
	int rsc = 0;
	if( parent_rtnode->ty != RT_AE && parent_rtnode->ty != RT_CSE ) {
		return o2pt->rsc = RSC_INVALID_CHILD_RESOURCETYPE;
	}

	GRP *grp = (GRP *)calloc(1, sizeof(GRP));
	o2pt->rsc = rsc = cjson_to_grp(o2pt->cjson_pc, grp); 
	if(rsc >= 4000){
		free_grp(grp);
		grp = NULL;
		o2pt->rsc = rsc;
		return rsc;
	}
	init_grp(grp, parent_rtnode);
	rsc = validate_grp(grp);
	if(rsc >= 4000){
		logger("O2M", LOG_LEVEL_DEBUG, "Group Validation failed");
		
		free_grp(grp);
		grp = NULL;
		return o2pt->rsc = rsc;
	}

	int result = db_store_grp(grp);
	if(result != 1){
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail");
		free_grp(grp); 
		grp = NULL;
		return RSC_INTERNAL_SERVER_ERROR;
	}

	RTNode *child_rtnode = create_rtnode(grp, RT_GRP);
	add_child_resource_tree(parent_rtnode, child_rtnode);

	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = grp_to_json(grp);

	//free_grp(grp); grp = NULL;
	return rsc;
}

int update_sub(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	char invalid_key[][8] = {"ty", "pi", "ri", "rn", "ct"};
	cJSON *m2m_sub = cJSON_GetObjectItem(o2pt->cjson_pc, "m2m:sub");
	int invalid_key_size = sizeof(invalid_key)/(8*sizeof(char));
	for(int i=0; i<invalid_key_size; i++) {
		if(cJSON_GetObjectItem(m2m_sub, invalid_key[i])) {
			handle_error(o2pt, RSC_BAD_REQUEST, "{\"m2m:dbg\": \"unsupported attribute on update\"}");
			return RSC_BAD_REQUEST;
		}
	}

	SUB* sub = (SUB *)target_rtnode->obj;
	int result;
	
	set_sub_update(o2pt, m2m_sub, sub);
	result = db_delete_sub(sub->ri);
	result = db_store_sub(sub);
	
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = sub_to_json(sub);
	o2pt->rsc = RSC_UPDATED;
	return RSC_UPDATED;
}

int create_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	int rsc = 0;
	int e = check_resource_type_invalid(o2pt);
	if(e != -1) e = check_payload_empty(o2pt);
	if(e != -1) e = check_payload_format(o2pt);
	if(e != -1) e = check_resource_type_equal(o2pt);
	if(e != -1) e = check_privilege(o2pt, parent_rtnode, ACOP_CREATE);
	if(e != -1) e = check_rn_duplicate(o2pt, parent_rtnode);
	if(e == -1) return o2pt->rsc;

	switch(o2pt->ty) {	
	case RT_AE :
		logger("O2M", LOG_LEVEL_INFO, "Create AE");
		rsc = create_ae(o2pt, parent_rtnode);
		break;	

	case RT_CNT :
		logger("O2M", LOG_LEVEL_INFO, "Create CNT");
		rsc = create_cnt(o2pt, parent_rtnode);
		break;
		
	case RT_CIN :
		logger("O2M", LOG_LEVEL_INFO, "Create CIN");
		rsc = create_cin(o2pt, parent_rtnode);
		break;

	case RT_SUB :
		logger("O2M", LOG_LEVEL_INFO, "Create SUB");
		rsc = create_sub(o2pt, parent_rtnode);
		break;
	
	case RT_ACP :
		logger("O2M", LOG_LEVEL_INFO, "Create ACP");
		rsc = create_acp(o2pt, parent_rtnode);
		break;

	case RT_GRP:
		logger("O2M", LOG_LEVEL_INFO, "Create GRP");
		rsc = create_grp(o2pt, parent_rtnode);
		break;

	case RT_MIXED :
		handle_error(o2pt, RSC_BAD_REQUEST, "resource type error");
		rsc = o2pt->rsc;
	}	
	return rsc;
}

int retrieve_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode) {
	int rsc = 0;
	int e = check_privilege(o2pt, target_rtnode, ACOP_RETRIEVE);

	if(e == -1) return o2pt->rsc;

	switch(target_rtnode->ty) {
		
	case RT_CSE :
		logger("O2M", LOG_LEVEL_INFO, "Retrieve CSE");
        rsc = retrieve_cse(o2pt, target_rtnode);
      	break;
	
	case RT_AE : 
		logger("O2M", LOG_LEVEL_INFO, "Retrieve AE");
		rsc = retrieve_ae(o2pt, target_rtnode);	
		break;	
			
	case RT_CNT :
		logger("O2M", LOG_LEVEL_INFO, "Retrieve CNT");
		rsc = retrieve_cnt(o2pt, target_rtnode);			
		break;
			
	case RT_CIN :
		logger("O2M", LOG_LEVEL_INFO, "Retrieve CIN");
		rsc = retrieve_cin(o2pt, target_rtnode);			
		break;

	case RT_GRP :
		logger("O2M", LOG_LEVEL_INFO, "Retrieve GRP");
		rsc = retrieve_grp(o2pt, target_rtnode);	
		break;

	case RT_SUB :
		logger("O2M", LOG_LEVEL_INFO, "Retrieve SUB");
		rsc = retrieve_sub(o2pt, target_rtnode);			
		break;

	case RT_ACP :
		logger("O2M", LOG_LEVEL_INFO, "Retrieve ACP");
		rsc = retrieve_acp(o2pt, target_rtnode);			
		break;
	}	

	return rsc;
}

int update_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode) {
	int rsc = 0;
	o2pt->ty = target_rtnode->ty;
	int e = check_payload_empty(o2pt);
	if(e != -1) e = check_payload_format(o2pt);
	ResourceType ty = parse_object_type_cjson(o2pt->cjson_pc);
	if(e != -1) e = check_resource_type_equal(o2pt);
	if(e != -1) e = check_privilege(o2pt, target_rtnode, ACOP_UPDATE);
	if(e != -1) e = check_rn_duplicate(o2pt, target_rtnode->parent);
	if(e == -1) return o2pt->rsc;
	
	switch(ty) {
		case RT_AE :
			logger("O2M", LOG_LEVEL_INFO, "Update AE");
			rsc = update_ae(o2pt, target_rtnode);
			break;

		case RT_CNT :
			logger("O2M", LOG_LEVEL_INFO, "Update CNT");
			rsc = update_cnt(o2pt, target_rtnode);
			break;

		case RT_SUB :
			logger("O2M", LOG_LEVEL_INFO, "Update SUB");
			rsc = update_sub(o2pt, target_rtnode);
			break;
		
		case RT_ACP :
			logger("O2M", LOG_LEVEL_INFO, "Update ACP");
			rsc = update_acp(o2pt, target_rtnode);
			break;

		case RT_GRP:
			logger("O2M", LOG_LEVEL_INFO, "Update GRP");
			rsc = update_grp(o2pt, target_rtnode);
			break;

		default :
			handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "operation `update` is unsupported");
			rsc = o2pt->rsc;
		}
	return rsc;
}

int fopt_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *parent_rtnode){
	int rsc = 0;
	int cnt = 0;

	RTNode *target_rtnode = NULL;
	oneM2MPrimitive *req_o2pt = NULL;
	cJSON *new_pc = NULL;
	cJSON *agr = NULL;
	cJSON *rsp = NULL;
	cJSON *json = NULL;
	GRP *grp = NULL;
	
	if(parent_rtnode == NULL){
		o2pt->rsc = RSC_NOT_FOUND;
		return RSC_NOT_FOUND;
	}
	logger("O2M", LOG_LEVEL_DEBUG, "handle fopt");


	grp = db_get_grp(get_ri_rtnode(parent_rtnode));
	if(!grp){
		o2pt->rsc = RSC_INTERNAL_SERVER_ERROR;
		return RSC_INTERNAL_SERVER_ERROR;
	}
	
	if(grp->cnm == 0){
		logger("O2M", LOG_LEVEL_DEBUG, "No member to fanout");
		free_grp(grp);
		return o2pt->rsc = RSC_NO_MEMBERS;
	}

	o2ptcpy(&req_o2pt, o2pt);

	new_pc = cJSON_CreateObject();
	cJSON_AddItemToObject(new_pc, "m2m:agr", agr = cJSON_CreateObject());
	cJSON_AddItemToObject(agr, "m2m:rsp", rsp = cJSON_CreateArray());

	for(int i = 0 ; i < grp->cnm ; i++){
		if(req_o2pt->to) free(req_o2pt->to);
		if(o2pt->fopt)
			req_o2pt->to = malloc(strlen(grp->mid[i]) + strlen(o2pt->fopt) + 1);
		else
			req_o2pt->to = malloc(strlen(grp->mid[i]) + 1);
		
		strcpy(req_o2pt->to, grp->mid[i]);
		if(o2pt->fopt) strcat(req_o2pt->to, o2pt->fopt);

		req_o2pt->isFopt = false;
		
		target_rtnode = parse_uri(req_o2pt, rt->cb);
		if(target_rtnode && target_rtnode->ty == RT_AE){
			req_o2pt->fr = strdup(get_ri_rtnode(target_rtnode));
		}
		
		if(target_rtnode){
			rsc = handle_onem2m_request(req_o2pt, target_rtnode);
			if(rsc < 4000) cnt++;
			json = o2pt_to_json(req_o2pt);
			if(json) {
				cJSON_AddItemToArray(rsp, json);
			}
			if(req_o2pt->op != OP_DELETE && target_rtnode->ty == RT_CIN){
				free_rtnode(target_rtnode);
				target_rtnode = NULL;
			}

		} else{
			logger("O2M", LOG_LEVEL_DEBUG, "rtnode not found");
		}
	}

	if(o2pt->pc) free(o2pt->pc); //TODO double free bug
	o2pt->pc = cJSON_PrintUnformatted(new_pc);

	cJSON_Delete(new_pc);
	
	o2pt->rsc = RSC_OK;	

	free_o2pt(req_o2pt);
	req_o2pt = NULL;
	free_grp(grp);
	return RSC_OK;
}

/**
 * Discover Resources based on Filter Criteria
*/
int discover_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	logger("MAIN", LOG_LEVEL_DEBUG, "Discover Resource");
	cJSON *root = cJSON_CreateObject();
	cJSON *uril = NULL;
	int urilSize = 0;
	if(!o2pt->fc){
		logger("O2M", LOG_LEVEL_WARN, "Empty Filter Criteria");
		return RSC_BAD_REQUEST;
	}

	#ifdef SQLITE_DB
	uril = db_get_filter_criteria(o2pt->to, o2pt->fc);
	#else
	RTNode *pn = NULL;
	o2pt->fc->o2pt = o2pt;
	if(target_rtnode->ty == RT_CNT){
		target_rtnode->child = pn = db_get_cin_rtnode_list(target_rtnode);
		pn->parent = target_rtnode;	
	}else{
		pn = target_rtnode->child;
	}
	uril = fc_scan_resource_tree(pn, o2pt->fc, 1);
	#endif
	
	urilSize = cJSON_GetArraySize(uril);
	if(o2pt->fc->lim < urilSize - o2pt->fc->ofst){
		logger("MAIN", LOG_LEVEL_DEBUG, "limit exceeded");
		for(int i = 0 ; i < o2pt->fc->ofst ; i++){
			cJSON_DeleteItemFromArray(uril, 0);
		}
		urilSize = cJSON_GetArraySize(uril);
		for(int i = o2pt->fc->lim ; i < urilSize; i++){
			cJSON_DeleteItemFromArray(uril, o2pt->fc->lim);
		}
		o2pt->cnst = CS_PARTIAL_CONTENT;
		o2pt->cnot = o2pt->fc->ofst + o2pt->fc->lim;
	}
	cJSON_AddItemToObject(root, "m2m:uril", uril);

	if(o2pt->pc)
		free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);

	cJSON_Delete(root);

	return o2pt->rsc = RSC_OK;

}

int notify_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode) {
	if(!target_rtnode) {
		logger("O2M", LOG_LEVEL_ERROR, "target_rtnode is NULL");
		return -1;
	}
	int net = NET_NONE;

	switch(o2pt->op) {
		case OP_CREATE:
			net = NET_CREATE_OF_DIRECT_CHILD_RESOURCE;
			break;
		case OP_UPDATE:
			net = NET_UPDATE_OF_RESOURCE;
			break;
		case OP_DELETE:
			net = NET_DELETE_OF_RESOURCE;
			break;
	}

	cJSON *noti_cjson, *sgn, *nev;
	noti_cjson = cJSON_CreateObject();
	cJSON_AddItemToObject(noti_cjson, "m2m:sgn", sgn = cJSON_CreateObject());
	cJSON_AddItemToObject(sgn, "nev", nev = cJSON_CreateObject());
	cJSON_AddNumberToObject(nev, "net", net);
	cJSON_AddStringToObject(nev, "rep", o2pt->pc);


	RTNode *child = target_rtnode->child;

	while(child) {
		if(child->ty == RT_SUB) {
			cJSON_AddStringToObject(sgn, "sur", child->uri);
			notify_to_nu(o2pt, child, noti_cjson, net);
			cJSON_DeleteItemFromObject(sgn, "sur");
		}
		child = child->sibling_right;
	}

	if(net == NET_DELETE_OF_RESOURCE) {
		net = NET_DELETE_OF_DIRECT_CHILD_RESOURCE;
		cJSON_DeleteItemFromObject(nev, "net");
		cJSON_AddNumberToObject(nev, "net", net);
		child = target_rtnode->parent->child;
		while(child) {
			if(child->ty == RT_SUB) {
				cJSON_AddStringToObject(sgn, "sur", child->uri);
				notify_to_nu(o2pt, child, noti_cjson, net);
				cJSON_DeleteItemFromObject(sgn, "sur");
			}
			child = child->sibling_right;
		}
	}

	cJSON_Delete(noti_cjson);

	return 1;
}
