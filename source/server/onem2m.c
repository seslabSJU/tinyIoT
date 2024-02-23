#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <sys/timeb.h>
#include <limits.h>
#include "onem2m.h"
#include "dbmanager.h"
#include "httpd.h"
#include "mqttClient.h"
#include "onem2mTypes.h"
#include "config.h"
#include "util.h"
#include "cJSON.h"
// #include "coap.h"

extern ResourceTree *rt;


void init_cse(cJSON* cse) {
	char *ct = get_local_time(0);
	char *csi = (char*)malloc(strlen(CSE_BASE_RI) + 2);
	sprintf(csi, "/%s", CSE_BASE_RI);

	cJSON *srt = cJSON_CreateArray();
	cJSON_AddItemToArray(srt, cJSON_CreateNumber(RT_ACP));
	cJSON_AddItemToArray(srt, cJSON_CreateNumber(RT_AE));
	cJSON_AddItemToArray(srt, cJSON_CreateNumber(RT_CNT));
	cJSON_AddItemToArray(srt, cJSON_CreateNumber(RT_CIN));
	cJSON_AddItemToArray(srt, cJSON_CreateNumber(RT_CSE));
	cJSON_AddItemToArray(srt, cJSON_CreateNumber(RT_GRP));
	cJSON_AddItemToArray(srt, cJSON_CreateNumber(RT_CSR));
	cJSON_AddItemToArray(srt, cJSON_CreateNumber(RT_SUB));
	cJSON_AddItemToArray(srt, cJSON_CreateNumber(RT_CBA));
	cJSON_AddItemToArray(srt, cJSON_CreateNumber(RT_AEA));

	
	cJSON_AddStringToObject(cse, "ct", ct);
	cJSON_AddStringToObject(cse, "ri", CSE_BASE_RI);
	cJSON_AddStringToObject(cse, "lt", ct);
	cJSON_AddStringToObject(cse, "rn", CSE_BASE_NAME);
	cJSON_AddNumberToObject(cse, "cst", SERVER_TYPE);
	cJSON_AddItemToObject(cse, "srt", srt);

	cJSON *srv = cJSON_CreateArray();
	cJSON_AddItemToArray(srv, cJSON_CreateString("2a"));

	cJSON_AddItemToObject(cse, "srv", srv);
	cJSON_AddItemToObject(cse, "pi", cJSON_CreateNull());
	cJSON_AddNumberToObject(cse, "ty", RT_CSE);
	cJSON_AddStringToObject(cse, "uri", CSE_BASE_NAME);
	cJSON_AddStringToObject(cse, "csi", csi);
	cJSON_AddBoolToObject(cse, "rr", true);
	
	// TODO - add acpi, poa
	
	free(ct); ct = NULL;
	free(csi); csi = NULL;
}

void init_csr(cJSON *csr){
	char buf[256] = {0};

	sprintf(buf, "/%s/%s", CSE_BASE_RI, CSE_BASE_NAME);
	cJSON_AddItemToObject(csr, "cb", cJSON_CreateString(buf));
	cJSON *dcse = cJSON_CreateArray();
	cJSON_AddItemToObject(csr, "dcse", dcse);

	cJSON *csz =  cJSON_CreateArray();
	cJSON_AddItemToArray(csz, cJSON_CreateString("application/json"));


	cJSON_SetIntValue(cJSON_GetObjectItem(csr, "ty"), RT_CSR);
	cJSON_SetValuestring(cJSON_GetObjectItem(csr, "rn"), CSE_BASE_RI);
	cJSON_DeleteItemFromObject(csr, "ri");
	cJSON_DeleteItemFromObject(csr, "lt");
	cJSON_DeleteItemFromObject(csr, "ct");
	cJSON_DeleteItemFromObject(csr, "et");
	cJSON_DeleteItemFromObject(csr, "srt");
	cJSON_DeleteItemFromObject(csr, "ty");
	cJSON_DeleteItemFromObject(csr, "pi");
	cJSON_DeleteItemFromObject(csr, "at");
	cJSON_DeleteItemFromObject(csr, "aa");
}

void add_general_attribute(cJSON *root, RTNode *parent_rtnode, ResourceType ty){
	char *ct = get_local_time(0);
	char *ptr = NULL;

	cJSON_AddNumberToObject(root, "ty", ty);
	cJSON_AddStringToObject(root, "ct", ct);

	ptr = resource_identifier(ty, ct);
	cJSON_AddStringToObject(root, "ri", ptr);

	if(cJSON_GetObjectItem(root, "rn") == NULL){
		cJSON_AddStringToObject(root, "rn", ptr);
	}
	free(ptr); ptr = NULL;

	cJSON_AddStringToObject(root, "lt", ct);

	cJSON *pi = cJSON_GetObjectItem(parent_rtnode->obj, "ri");
	cJSON_AddStringToObject(root, "pi", pi->valuestring);

	ptr = get_local_time(DEFAULT_EXPIRE_TIME);
	cJSON_AddStringToObject(root, "et", ptr);
	free(ptr); ptr = NULL;
	free(ct);
}

RTNode* create_rtnode(cJSON *obj, ResourceType ty){
	RTNode* rtnode = (RTNode *)calloc(1, sizeof(RTNode));
	cJSON *uri = NULL;

	rtnode->ty = ty;
	rtnode->obj = obj;

	rtnode->parent = NULL;
	rtnode->child = NULL;
	rtnode->subs = NULL;
	rtnode->sibling_left = NULL;
	rtnode->sibling_right = NULL;
	rtnode->sub_cnt = 0;
	if(uri = cJSON_GetObjectItem(obj, "uri"))
	{
		rtnode->uri = strdup(uri->valuestring);
		cJSON_DeleteItemFromObject(obj, "uri");
	}
	
	rtnode->rn = strdup(cJSON_GetObjectItem(obj, "rn")->valuestring);
	
	return rtnode;
}

int create_csr(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	int e = check_rn_invalid(o2pt, RT_CSR);
	if(e == -1) return o2pt->rsc;

	if(parent_rtnode->ty != RT_CSE) {
		handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
		return o2pt->rsc;
	}

	if(SERVER_TYPE == ASN_CSE){
		handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "operation not allowed");
		return o2pt->rsc;
	}

	cJSON *root = cJSON_Duplicate(o2pt->cjson_pc, 1);
	cJSON *csr = cJSON_GetObjectItem(root, "m2m:csr");

	int rsc = validate_csr(o2pt, parent_rtnode, csr, OP_CREATE);
	if(rsc != RSC_OK){
		cJSON_Delete(root);
		return rsc;
	}

	add_general_attribute(csr, parent_rtnode, RT_CSR);
	cJSON_AddStringToObject(csr, "csi", o2pt->fr);
	cJSON_ReplaceItemInObject(csr, "ri", cJSON_Duplicate( cJSON_GetObjectItem(csr, "rn"), 1));


	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);
	o2pt->rsc = RSC_CREATED;
	
	// Add uri attribute
	char *ptr = malloc(1024);
	cJSON *rn = cJSON_GetObjectItem(csr, "rn");
	sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);	

	// Save to DB
	int result = db_store_resource(csr, ptr);
	if(result == -1) {
		cJSON_Delete(root);
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "database error");
		free(ptr);	ptr = NULL;
		return RSC_INTERNAL_SERVER_ERROR;
	}
	free(ptr);	ptr = NULL;

	RTNode* rtnode = create_rtnode(csr, RT_CSR);
	add_child_resource_tree(parent_rtnode, rtnode);

	// update descendent cse
	cJSON *dcse = cJSON_GetObjectItem(rt->cb->obj, "dcse");
	if(dcse == NULL){
		dcse = cJSON_CreateArray();
		cJSON_AddItemToObject(rt->cb->obj, "dcse", dcse);
	} 
	logger("UTIL", LOG_LEVEL_INFO, "add dcse %s", cJSON_GetObjectItem(csr, "csi")->valuestring);
	cJSON_AddItemToArray(dcse, cJSON_CreateString(cJSON_GetObjectItem(csr, "csi")->valuestring));
	update_remote_csr_dcse(rtnode);

	return RSC_CREATED;
}

int create_cba(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	// int e = check_rn_invalid(o2pt, RT_CBA);
	// if(e == -1) return o2pt->rsc;
	logger("UTIL", LOG_LEVEL_DEBUG, "%s", o2pt->cjson_pc);

	if(parent_rtnode->ty != RT_CSE) {
		handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
		return o2pt->rsc;
	}

	if(SERVER_TYPE == ASN_CSE){
		handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "operation not allowed");
		return o2pt->rsc;
	}



	cJSON *root = cJSON_Duplicate(o2pt->cjson_pc, 1);
	cJSON *cba = cJSON_GetObjectItem(root, "m2m:cba");

	add_general_attribute(cba, parent_rtnode, RT_CBA);
	cJSON_AddStringToObject(cba, "cb", o2pt->fr);
	cJSON_ReplaceItemInObject(cba, "ri", cJSON_Duplicate( cJSON_GetObjectItem(cba, "ri"), 1));

	// int rsc = validate_csr(o2pt, parent_rtnode, cba, OP_CREATE);
	// if(rsc != RSC_OK){
	// 	cJSON_Delete(root);
	// 	return rsc;
	// }

	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);
	o2pt->rsc = RSC_CREATED;
	
	// Add uri attribute
	char *ptr = malloc(1024);
	cJSON *rn = cJSON_GetObjectItem(cba, "rn");
	sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);	

	// Save to DB
	int result = db_store_resource(cba, ptr);
	if(result == -1) {
		cJSON_Delete(root);
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "database error");
		free(ptr);	ptr = NULL;
		return RSC_INTERNAL_SERVER_ERROR;
	}
	free(ptr);	ptr = NULL;

	RTNode* rtnode = create_rtnode(cba, RT_CBA);
	add_child_resource_tree(parent_rtnode, rtnode);
}

int create_ae(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	int e = check_aei_duplicate(o2pt, parent_rtnode);
	if(e != -1) e = check_rn_invalid(o2pt, RT_AE);
	if(e != -1) e = check_aei_invalid(o2pt);
	if(e == -1) return o2pt->rsc;

	if(parent_rtnode->ty != RT_CSE) {
		handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
		return o2pt->rsc;
	}
	cJSON *root = cJSON_Duplicate(o2pt->cjson_pc, 1);
	cJSON *ae = cJSON_GetObjectItem(root, "m2m:ae");

	add_general_attribute(ae, parent_rtnode, RT_AE);
	if(o2pt->fr && strlen(o2pt->fr) > 0) {
		cJSON* ri = cJSON_GetObjectItem(ae, "ri");
		cJSON_SetValuestring(ri, o2pt->fr);
	}else{
		if(o2pt->fr) free(o2pt->fr);
		o2pt->fr = strdup(cJSON_GetObjectItem(ae, "ri")->valuestring);
	}
	cJSON_AddStringToObject(ae, "aei", cJSON_GetObjectItem(ae, "ri")->valuestring);
	
	int rsc = validate_ae(o2pt, ae, OP_CREATE);

	if(rsc != RSC_OK){
		cJSON_Delete(root);
		return rsc;
	}

	cJSON *final_at = cJSON_CreateArray();
	if( handle_annc_create(parent_rtnode, ae, cJSON_GetObjectItem(ae, "at"), final_at) == -1){
		cJSON_Delete(root);
		cJSON_Delete(final_at);
		return handle_error(o2pt, RSC_BAD_REQUEST, "invalid attribute in `aa`");
	}
	// cJSON *at = NULL;
	// char *at_str = NULL;
	// cJSON_ArrayForEach(at, cJSON_GetObjectItem(ae, "at")){
	// 	at_str = create_remote_annc(parent_rtnode, ae, at->valuestring, false);
	// 	if(at_str == -1){
	// 		cJSON_Delete(root);
	// 		return handle_error(o2pt, RSC_BAD_REQUEST, "invalid aa");
	// 	}
	// 	if(!at_str){
	// 		continue;
	// 	}
		
	// 	cJSON_AddItemToArray(final_at, cJSON_CreateString(at_str));
	// }

	if(cJSON_GetArraySize(final_at) > 0){
		cJSON_DeleteItemFromObject(ae, "at");
		cJSON_AddItemToObject(ae, "at", final_at);
	}else{
		cJSON_Delete(final_at);
		cJSON_DeleteItemFromObject(ae, "at");
	}
	
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);
	// Add uri attribute
	char *ptr = malloc(1024);
	cJSON *rn = cJSON_GetObjectItem(ae, "rn");
	sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);
	// Save to DB
	int result = db_store_resource(ae, ptr);

	if(result != 1) { 
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail");
		cJSON_Delete(root);
		free(ptr); ptr = NULL;
		return RSC_INTERNAL_SERVER_ERROR;
	}

	free(ptr);	ptr = NULL;

	// Add to resource tree
	RTNode* child_rtnode = create_rtnode(ae, RT_AE);
	add_child_resource_tree(parent_rtnode, child_rtnode); 

	cJSON_DetachItemFromObject(root, "m2m:ae");
	cJSON_Delete(root);

	return o2pt->rsc = RSC_CREATED;
}

int create_aea(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	int e = check_aei_invalid(o2pt);
	if(e != -1) e = check_rn_invalid(o2pt, RT_AE);
	if(e == -1) return o2pt->rsc;
	// logger("UTIL", LOG_LEVEL_DEBUG, "%s", cJSON_PrintUnformatted(parent_rtnode->obj));
	if(parent_rtnode->ty != RT_CBA) {
		handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
		return o2pt->rsc;
	}
	cJSON *root = cJSON_Duplicate(o2pt->cjson_pc, 1);
	cJSON *aea = cJSON_GetObjectItem(root, "m2m:aeA");

	add_general_attribute(aea, parent_rtnode, RT_AEA);

	// int rsc = validate_aea(o2pt, ae, OP_CREATE); // TODO
	int rsc = RSC_OK;
	if(rsc != RSC_OK){
		cJSON_Delete(root);
		return rsc;
	}
	
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);
	// Add uri attribute
	char *ptr = malloc(1024);
	cJSON *rn = cJSON_GetObjectItem(aea, "rn");
	sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);
	// Save to DB
	int result = db_store_resource(aea, ptr);

	if(result != 1) { 
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail");
		cJSON_Delete(root);
		free(ptr); ptr = NULL;
		return RSC_INTERNAL_SERVER_ERROR;
	}

	free(ptr);	ptr = NULL;

	// Add to resource tree
	RTNode* child_rtnode = create_rtnode(aea, RT_AEA);
	add_child_resource_tree(parent_rtnode, child_rtnode);

	cJSON_DetachItemFromObject(root, "m2m:aeA");
	cJSON_Delete(root);

	return o2pt->rsc = RSC_CREATED;
}

int create_cnt(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	cJSON *root = cJSON_Duplicate(o2pt->cjson_pc, 1);
	cJSON *pjson = NULL;
	if(parent_rtnode->ty != RT_CNT && parent_rtnode->ty != RT_AE && parent_rtnode->ty != RT_CSE) {
		handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
		return RSC_INVALID_CHILD_RESOURCETYPE;
	}

	cJSON *cnt = cJSON_GetObjectItem(root, "m2m:cnt");

	add_general_attribute(cnt, parent_rtnode, RT_CNT);

	int rsc = validate_cnt(o2pt, cnt, OP_CREATE);
	if(rsc != RSC_OK){
		cJSON_Delete(root);
		return rsc;
	}
	
	// Add cr attribute
	if(pjson = cJSON_GetObjectItem(cnt, "cr")){
		if(pjson->type == cJSON_NULL){
			cJSON_AddStringToObject(cnt, "cr", o2pt->fr);
		}else{
			handle_error(o2pt, RSC_BAD_REQUEST, "creator attribute with arbitary value is not allowed");
			cJSON_Delete(root);
			return o2pt->rsc;
		}
	}

	// Add st, cni, cbs, mni, mbs attribute
	cJSON_AddNumberToObject(cnt, "st", 0);
	cJSON_AddNumberToObject(cnt, "cni", 0);
	cJSON_AddNumberToObject(cnt, "cbs", 0);

	cJSON *final_at = cJSON_CreateArray();
	if( handle_annc_create(parent_rtnode, cnt, cJSON_GetObjectItem(cnt, "at"), final_at) == -1){
		cJSON_Delete(root);
		cJSON_Delete(final_at);
		return handle_error(o2pt, RSC_BAD_REQUEST, "invalid attribute in `aa`");
	}

	if(cJSON_GetArraySize(final_at) > 0){
		cJSON_DeleteItemFromObject(cnt, "at");
		cJSON_AddItemToObject(cnt, "at", final_at);
	}else{
		cJSON_Delete(final_at);
	}

	if(rsc != RSC_OK){
		cJSON_Delete(root);
		return rsc;
	}

	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);
	o2pt->rsc = RSC_CREATED;

	// Add uri attribute
	char *ptr = malloc(1024);
	cJSON *rn = cJSON_GetObjectItem(cnt, "rn");
	sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);

	// Store to DB
	int result = db_store_resource(cnt, ptr);
	if(result != 1) { 
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail"); 
		cJSON_Delete(root);
		free(ptr);	ptr = NULL;
		return o2pt->rsc;
	}
	free(ptr);	ptr = NULL;

	RTNode* child_rtnode = create_rtnode(cnt, RT_CNT);
	add_child_resource_tree(parent_rtnode,child_rtnode);

	cJSON_DetachItemFromObject(root, "m2m:cnt");
	cJSON_Delete(root);

	return RSC_CREATED;
}

int create_cin(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	if(parent_rtnode->ty != RT_CNT) {
		handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
		return o2pt->rsc;
	}

	cJSON *root = cJSON_Duplicate(o2pt->cjson_pc, 1);
	cJSON *cin = cJSON_GetObjectItem(root, "m2m:cin");

	// check if rn exists
	cJSON *rn = cJSON_GetObjectItem(cin, "rn");
	if(rn != NULL){
		handle_error(o2pt, RSC_NOT_IMPLEMENTED, "rn attribute for cin is assigned by CSE");
		cJSON_Delete(root);
		return o2pt->rsc;
	}

	add_general_attribute(cin, parent_rtnode, RT_CIN);
	
	// add cs attribute
	cJSON *con = cJSON_GetObjectItem(cin, "con");
	if(cJSON_IsString(con))
		cJSON_AddNumberToObject(cin, "cs", strlen(cJSON_GetStringValue(con)));

	// Add st attribute
	cJSON *st = cJSON_GetObjectItem(parent_rtnode->obj, "st");
	cJSON_AddNumberToObject(cin, "st", st->valueint);


	int rsc = validate_cin(o2pt, parent_rtnode->obj, cin, OP_CREATE);
	if(rsc != RSC_OK){
		cJSON_Delete(root);
		return rsc;
	}
	cJSON *pjson = NULL;
	if(pjson = cJSON_GetObjectItem(cin, "cr")){
		if(pjson->type == cJSON_NULL){
			cJSON_DeleteItemFromObject(cin, "cr");
			cJSON_AddStringToObject(cin, "cr", o2pt->fr);
		}else{
			handle_error(o2pt, RSC_BAD_REQUEST, "creator attribute with arbitary value is not allowed");
			cJSON_Delete(root);
			return o2pt->rsc;
		}
	}

	cJSON *final_at = cJSON_CreateArray();
	if( handle_annc_create(parent_rtnode, cin, cJSON_GetObjectItem(cin, "at"), final_at) == -1){
		cJSON_Delete(root);
		cJSON_Delete(final_at);
		return handle_error(o2pt, RSC_BAD_REQUEST, "invalid attribute in `aa`");
	}

	if(cJSON_GetArraySize(final_at) > 0){
		cJSON_DeleteItemFromObject(cin, "at");
		cJSON_AddItemToObject(cin, "at", final_at);
	}else{
		cJSON_Delete(final_at);
	}

	RTNode *cin_rtnode = create_rtnode(cin, RT_CIN);
	update_cnt_cin(parent_rtnode, cin_rtnode, 1);

	
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);
	o2pt->rsc = RSC_CREATED;

	// Add uri attribute
	char ptr[1024] = {0};
	rn = cJSON_GetObjectItem(cin, "rn");
	sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);

	// Store to DB
	int result = db_store_resource(cin, ptr);
	if(result != 1) { 
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail"); 
		free_rtnode(cin_rtnode);
		return o2pt->rsc;
	}
	if(parent_rtnode->child){
		cJSON_Delete(parent_rtnode->child->obj);
		parent_rtnode->child->obj = cJSON_Duplicate(cin, 1);
		free_rtnode(cin_rtnode);
	}else{
		parent_rtnode->child = cin_rtnode;
		cin_rtnode->parent = parent_rtnode;
		cin_rtnode->rn = strdup("la");
	}
	cJSON_Delete(root);
	cin_rtnode->obj = NULL;
	return RSC_CREATED;
}

int create_sub(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	if(parent_rtnode->ty == RT_CIN || parent_rtnode->ty == RT_SUB) {
		return handle_error(o2pt, RSC_TARGET_NOT_SUBSCRIBABLE, "target not subscribable");
	}

	cJSON *root = cJSON_Duplicate(o2pt->cjson_pc, 1);
	cJSON *sub = cJSON_GetObjectItem(root, "m2m:sub");


	add_general_attribute(sub, parent_rtnode, RT_SUB);

	int rsc = validate_sub(o2pt, sub, OP_CREATE);

	cJSON *pjson = NULL;
	if(pjson = cJSON_GetObjectItem(sub, "cr")){
		if(pjson->type == cJSON_NULL){
			cJSON_DeleteItemFromObject(sub, "cr");
			cJSON_AddStringToObject(sub, "cr", o2pt->fr);
		}else{
			handle_error(o2pt, RSC_BAD_REQUEST, "creator attribute with arbitary value is not allowed");
			cJSON_Delete(root);
			return o2pt->rsc;
		}
	}

	if(rsc != RSC_OK){
		cJSON_Delete(root);
		return rsc;
	}

	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);
	o2pt->rsc = RSC_CREATED;

	// Add uri attribute
	char ptr[1024];
	cJSON *rn = cJSON_GetObjectItem(sub, "rn");
	sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);	

	// Store to DB
	int result = db_store_resource(sub, ptr);
	if(result != 1) { 
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail"); 
		cJSON_Delete(root);
		return o2pt->rsc;
	}

	RTNode* child_rtnode = create_rtnode(sub, RT_SUB);
	add_child_resource_tree(parent_rtnode,child_rtnode);

	cJSON_DetachItemFromObject(root, "m2m:sub");
	cJSON_Delete(root);

	return RSC_CREATED;
}

int create_acp(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	if(parent_rtnode->ty != RT_CSE && parent_rtnode->ty != RT_AE) {
		handle_error(o2pt, RSC_INVALID_CHILD_RESOURCETYPE, "child type is invalid");
		return o2pt->rsc;
	}

	cJSON *root = cJSON_Duplicate(o2pt->cjson_pc, 1);
	cJSON *acp = cJSON_GetObjectItem(root, "m2m:acp");

	add_general_attribute(acp, parent_rtnode, RT_ACP);

	int rsc = validate_acp(o2pt, acp, OP_CREATE);

	if(rsc != RSC_OK){
		cJSON_Delete(root);
		return rsc;
	}
	
	cJSON *final_at = cJSON_CreateArray();
	cJSON *at = NULL;
	char *at_str = NULL;
	cJSON_ArrayForEach(at, cJSON_GetObjectItem(acp, "at")){
		at_str = create_remote_annc(parent_rtnode, acp, at->valuestring, false);
		if(at_str == -1){
			cJSON_Delete(root);
			return handle_error(o2pt, RSC_BAD_REQUEST, "invalid aa");
		}
		if(!at_str){
			continue;
		}
		logger("UTIL", LOG_LEVEL_DEBUG, "add at %s", at_str);
		cJSON_AddItemToArray(final_at, cJSON_CreateString(at_str));
	}

	if(cJSON_GetArraySize(final_at) > 0){
		cJSON_DeleteItemFromObject(acp, "at");
		cJSON_AddItemToObject(acp, "at", final_at);
	}else{
		cJSON_Delete(final_at);
		cJSON_DeleteItemFromObject(acp, "at");
	}

	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);

	// Add uri attribute
	char *ptr = malloc(1024);
	cJSON *rn = cJSON_GetObjectItem(acp, "rn");
	sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);
	// Save to DB
	int result = db_store_resource(acp, ptr);
	if(result != 1) { 
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail");
		cJSON_Delete(root);
		free(ptr); ptr = NULL;
		return RSC_INTERNAL_SERVER_ERROR;
	}

	free(ptr);	ptr = NULL;

	// Add to resource tree
	RTNode* child_rtnode = create_rtnode(acp, RT_ACP);
	add_child_resource_tree(parent_rtnode, child_rtnode); 

	cJSON_DetachItemFromObject(root, "m2m:acp");
	cJSON_Delete(root);

	return o2pt->rsc = RSC_CREATED;
}

int update_cb(cJSON *csr, cJSON *cb){ 
	cJSON *dcse = cJSON_GetObjectItem(csr, "dcse");
	cJSON *dcse_item = NULL;
	cJSON *cb_item = NULL;
	int idx = 0;
	cJSON_ArrayForEach(dcse_item, dcse){
		if(cb_item = cJSON_GetObjectItem(dcse_item, "cb")){
			if(strcmp(cb_item->valuestring, cb->valuestring) == 0){
				cJSON_DeleteItemFromArray(dcse, idx);
				break;
			}
		}
		idx++;
	}
	cJSON_AddItemToArray(dcse, cb);
	return 1;
}

int update_ae(oneM2MPrimitive *o2pt, RTNode *target_rtnode) { // TODO deannounce when at removed
	char invalid_key[][8] = {"ty", "pi", "ri", "rn", "ct"};
	cJSON *m2m_ae = cJSON_GetObjectItem(o2pt->cjson_pc, "m2m:ae");
	cJSON *pjson  = NULL;
	int invalid_key_size = sizeof(invalid_key)/(8*sizeof(char));
	for(int i=0; i<invalid_key_size; i++) {
		if(cJSON_GetObjectItem(m2m_ae, invalid_key[i])) {
			handle_error(o2pt, RSC_BAD_REQUEST, "unsupported attribute on update");
			return RSC_BAD_REQUEST;
		}
	}

	if(cJSON_GetObjectItem(m2m_ae, "acpi") && cJSON_GetArraySize(m2m_ae) > 1){
		handle_error(o2pt, RSC_BAD_REQUEST, "only `acpi` should be updated");
		return RSC_BAD_REQUEST;
	}

	int result = validate_ae(o2pt, m2m_ae, OP_UPDATE);
	
	if(result != RSC_OK){
		logger("O2", LOG_LEVEL_ERROR, "validation failed");
		return result;
	}

	cJSON *at = NULL;
	if(at = cJSON_GetObjectItem(m2m_ae, "at")){
		cJSON *final_at = cJSON_CreateArray();
		handle_annc_update(target_rtnode, at, final_at);
		cJSON_DeleteItemFromObject(m2m_ae, "at");
		cJSON_AddItemToObject(m2m_ae, "at", final_at);
	}

	
	//merge update resource
	update_resource(target_rtnode->obj, m2m_ae); 

	// remove acpi if acpi is null
	if(pjson = cJSON_GetObjectItem(target_rtnode->obj, "acpi")){
		if(pjson->type == cJSON_NULL)
			cJSON_DeleteItemFromObject(target_rtnode->obj, "acpi");
		pjson = NULL;
	}

	// announce_to_annc(target_rtnode);
	
	result = db_update_resource(m2m_ae, cJSON_GetObjectItem(target_rtnode->obj, "ri")->valuestring, RT_AE);

	cJSON *root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:ae", target_rtnode->obj);

	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);
	o2pt->rsc = RSC_UPDATED;

	cJSON_DetachItemFromObject(root, "m2m:ae");
	cJSON_Delete(root);
	return RSC_UPDATED;
}

int update_aea(oneM2MPrimitive *o2pt, RTNode *target_rtnode) {
	char invalid_key[][8] = {"ty", "pi", "ri", "rn", "ct"};
	cJSON *m2m_aea = cJSON_GetObjectItem(o2pt->cjson_pc, "m2m:aea");
	int invalid_key_size = sizeof(invalid_key)/(8*sizeof(char));
	for(int i=0; i<invalid_key_size; i++) {
		if(cJSON_GetObjectItem(m2m_aea, invalid_key[i])) {
			return handle_error(o2pt, RSC_BAD_REQUEST, "unsupported attribute on update");
		}
	}

	cJSON *ast = cJSON_GetObjectItem(target_rtnode->obj, "ast");
	if(ast){
		if( ast->valueint != AST_BI_DIRECTIONAL){
			return handle_error(o2pt, RSC_BAD_REQUEST, "resource is uni-directional");
		}
	}else{
		return handle_error(o2pt, RSC_BAD_REQUEST, "resource is uni-directional");
	}

	int result = 0;

	// int result = validate_aea(o2pt, m2m_aea, OP_UPDATE);
	// if(result != RSC_OK) return result;

	cJSON* aea = target_rtnode->obj;
	cJSON *pjson = NULL;

	update_resource(target_rtnode->obj, m2m_aea);
	
	result = db_update_resource(m2m_aea, cJSON_GetObjectItem(target_rtnode->obj, "ri")->valuestring, RT_AEA);

	cJSON *root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:aea", target_rtnode->obj);

	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);
	o2pt->rsc = RSC_UPDATED;

	cJSON_DetachItemFromObject(root, "m2m:aea");
	cJSON_Delete(root);
	return RSC_UPDATED;
}

int update_cnt(oneM2MPrimitive *o2pt, RTNode *target_rtnode) {
	char invalid_key[][9] = {"ty", "pi", "ri", "rn", "ct", "cr"};
	cJSON *m2m_cnt = cJSON_GetObjectItem(o2pt->cjson_pc, "m2m:cnt");
	int invalid_key_size = sizeof(invalid_key)/(9*sizeof(char));
	for(int i=0; i<invalid_key_size; i++) {
		if(cJSON_GetObjectItem(m2m_cnt, invalid_key[i])) {
			handle_error(o2pt, RSC_BAD_REQUEST, "unsupported attribute on update");
			return RSC_BAD_REQUEST;
		}
	}

	cJSON* cnt = target_rtnode->obj;
	int result;
	cJSON *pjson = NULL;

	result = validate_cnt(o2pt, m2m_cnt, OP_UPDATE); //TODO - add UPDATE validation
	if(result != RSC_OK) return result;

	cJSON_AddNumberToObject(m2m_cnt, "st", cJSON_GetObjectItem(cnt, "st")->valueint + 1);

	cJSON *final_at = cJSON_CreateArray();

	cJSON *at = NULL;
	if(at = cJSON_GetObjectItem(m2m_cnt, "at")){
		cJSON *final_at = cJSON_CreateArray();
		handle_annc_update(target_rtnode, at, final_at);
		cJSON_DeleteItemFromObject(m2m_cnt, "at");
		cJSON_AddItemToObject(m2m_cnt, "at", final_at);
	}
	update_resource(target_rtnode->obj, m2m_cnt);

	delete_cin_under_cnt_mni_mbs(target_rtnode);

	result = db_update_resource(m2m_cnt, cJSON_GetObjectItem(target_rtnode->obj, "ri")->valuestring, RT_CNT);

	cJSON *root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:cnt", target_rtnode->obj);

	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);
	o2pt->rsc = RSC_UPDATED;

	cJSON_DetachItemFromObject(root, "m2m:cnt");
	cJSON_Delete(root);
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

	int result = validate_acp(o2pt, m2m_acp, OP_UPDATE);
	if(result != RSC_OK) return result;

	cJSON* acp = target_rtnode->obj;
	cJSON *pjson = NULL;

	cJSON *at = NULL;
	if(at = cJSON_GetObjectItem(m2m_acp, "at")){
		cJSON *final_at = cJSON_CreateArray();
		handle_annc_update(target_rtnode, at, final_at);
		cJSON_DeleteItemFromObject(m2m_acp, "at");
		cJSON_AddItemToObject(m2m_acp, "at", final_at);
	}

	update_resource(target_rtnode->obj, m2m_acp);
	
	result = db_update_resource(m2m_acp, cJSON_GetObjectItem(target_rtnode->obj, "ri")->valuestring, RT_ACP);

	cJSON *root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:acp", target_rtnode->obj);

	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);
	o2pt->rsc = RSC_UPDATED;

	cJSON_DetachItemFromObject(root, "m2m:acp");
	cJSON_Delete(root);
	return RSC_UPDATED;
}


int update_csr(oneM2MPrimitive *o2pt, RTNode *target_rtnode) {
	char invalid_key[][8] = {"ty", "pi", "ri", "rn", "ct"};
	cJSON *m2m_csr = cJSON_GetObjectItem(o2pt->cjson_pc, "m2m:csr");
	int invalid_key_size = sizeof(invalid_key)/(8*sizeof(char));
	for(int i=0; i<invalid_key_size; i++) {
		if(cJSON_GetObjectItem(m2m_csr, invalid_key[i])) {
			handle_error(o2pt, RSC_BAD_REQUEST, "unsupported attribute on update");
			return RSC_BAD_REQUEST;
		}
	}
	cJSON* csr = target_rtnode->obj;
	int result;

	//result = validate_csr(o2pt, target_rtnode->p)
	//if(result != 1) return result;
	
	update_resource(target_rtnode->obj, m2m_csr);

	result = db_update_resource(m2m_csr, cJSON_GetStringValue(cJSON_GetObjectItem(csr, "ri")), RT_CSR);
	
	update_remote_csr_dcse(target_rtnode);
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(csr);
	o2pt->rsc = RSC_UPDATED;
	return RSC_UPDATED;
}

/**
 * @brief update cnt on cin change
 * @param cnt_rtnode rtnode of cnt
 * @param cin_rtnode rtnode of cin
 * @param sign 1 for create, -1 for delete
 * @return int 1 for success, -1 for fail
*/
int update_cnt_cin(RTNode *cnt_rtnode, RTNode *cin_rtnode, int sign) {
	cJSON *cnt = cnt_rtnode->obj;
	cJSON *cin = cin_rtnode->obj;
	cJSON *cni = cJSON_GetObjectItem(cnt, "cni");
	cJSON *cbs = cJSON_GetObjectItem(cnt, "cbs");
	cJSON *st = cJSON_GetObjectItem(cnt, "st");

	cJSON_SetIntValue(cni, cni->valueint + sign);
	cJSON_SetIntValue(cbs, cbs->valueint + (sign * cJSON_GetObjectItem(cin, "cs")->valueint));
	cJSON_SetIntValue(st, st->valueint + 1);
	logger("O2", LOG_LEVEL_DEBUG, "cni: %d, cbs: %d, st: %d", cni->valueint, cbs->valueint, st->valueint);
	delete_cin_under_cnt_mni_mbs(cnt_rtnode);	

	db_update_resource(cnt, get_ri_rtnode(cnt_rtnode), RT_CNT);


	return 1;
}

int delete_onem2m_resource(oneM2MPrimitive *o2pt, RTNode* target_rtnode) {
	logger("O2M", LOG_LEVEL_INFO, "Delete oneM2M resource");
	if(target_rtnode->ty == RT_CSE) {
		handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "CSE can not be deleted");
		return RSC_OPERATION_NOT_ALLOWED;
	}
	if(check_privilege(o2pt, target_rtnode, ACOP_DELETE) == -1) {
		return o2pt->rsc;
	}
	
	delete_process(o2pt, target_rtnode);
		
	db_delete_onem2m_resource(target_rtnode);
	free_rtnode(target_rtnode);
	target_rtnode = NULL;
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = NULL;
	o2pt->rsc = RSC_DELETED;
	return RSC_DELETED;
}

int delete_process(oneM2MPrimitive *o2pt, RTNode *rtnode) {
	cJSON *csr, *dcse, *dcse_item;
	switch(rtnode->ty) {
		case RT_ACP :
			break;
		case RT_AE : 
			break;
		case RT_CNT : 
			break;
		case RT_CIN :
			update_cnt_cin(rtnode->parent, rtnode,-1);
			break;
		case RT_SUB :
			detach_subs(rtnode->parent, rtnode);
			break;
		case RT_CSR:
			csr = rtnode->obj;
			dcse = cJSON_GetObjectItem(rt->cb->obj, "dcse");
			dcse_item = NULL;
			int idx = 0;
			cJSON_ArrayForEach(dcse_item, dcse){
				if(strcmp(dcse_item->valuestring, cJSON_GetObjectItem(csr, "csi")->valuestring) == 0){
					break;
				}
				idx++;
			}
			if(idx < cJSON_GetArraySize(dcse))
				cJSON_DeleteItemFromArray(dcse, idx);
			update_remote_csr_dcse(rtnode);
			logger("O2M", LOG_LEVEL_DEBUG, "delete csr");
			detach_csrlist(rtnode);
			break;
		case RT_GRP:
			break;
		case RT_AEA:
			break;
		case RT_CBA:
			break;
	}

	notify_onem2m_resource(o2pt, rtnode);

	cJSON *at_list = cJSON_GetObjectItem(rtnode->obj, "at");
	cJSON *at = NULL;

	cJSON_ArrayForEach(at, at_list){
		oneM2MPrimitive * o2pt = (oneM2MPrimitive *)calloc(1, sizeof(oneM2MPrimitive));
		o2pt->op = OP_DELETE;
		o2pt->to = strdup(at->valuestring);
		o2pt->fr = "/"CSE_BASE_RI;
		o2pt->ty = RT_AE;
		o2pt->rqi = strdup("deannounce");
		o2pt->pc = NULL;
		o2pt->isForwarding = true;

		forwarding_onem2m_resource(o2pt, find_csr_rtnode_by_uri(at->valuestring));
	}
	
	if(rtnode->child) delete_process(o2pt, rtnode->child);
	return 1;
}

void free_rtnode(RTNode *rtnode) {
	if(rtnode->uri){
		free(rtnode->uri);
		rtnode->uri = NULL;
	}
	if(rtnode->rn){
		free(rtnode->rn);
		rtnode->rn = NULL;
	
	}
	if(rtnode->obj);
		cJSON_Delete(rtnode->obj);
	if(rtnode->parent && rtnode->parent->child == rtnode){
		rtnode->parent->child = rtnode->sibling_right;
	}
	if(rtnode->sibling_left){
		rtnode->sibling_left->sibling_right = rtnode->sibling_right;
	}
	if(rtnode->sibling_right){
		rtnode->sibling_right->sibling_left = rtnode->sibling_left;
	}
	if(rtnode->child){
		free_rtnode_list(rtnode->child);
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


/* GROUP IMPLEMENTATION */
int update_grp(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	int rsc = 0, result = 0;
	char invalid_key[6][4] = {"ty", "pi", "ri", "rn", "ct", "mtv"};
	cJSON *m2m_grp = cJSON_GetObjectItem(o2pt->cjson_pc, "m2m:grp");
	cJSON *pjson = NULL;

	int invalid_key_size = 6;
	for(int i=0; i<invalid_key_size; i++) {
		if(cJSON_GetObjectItem(m2m_grp, invalid_key[i])) {
			handle_error(o2pt, RSC_BAD_REQUEST, "{\"m2m:dbg\": \"unsupported attribute on update\"}");
			return RSC_BAD_REQUEST;
		}
	}

	if( (rsc = validate_grp_update(o2pt, target_rtnode->obj, m2m_grp))  >= 4000){
		o2pt->rsc = rsc;
		return rsc;
	}

	if(pjson = cJSON_GetObjectItem(m2m_grp, "mid")){
		cJSON_SetIntValue(cJSON_GetObjectItem(target_rtnode->obj, "cnm"), cJSON_GetArraySize(pjson));
	}
	update_resource(target_rtnode->obj, m2m_grp);
	result = db_update_resource(m2m_grp, cJSON_GetObjectItem(target_rtnode->obj, "ri")->valuestring, RT_GRP);

	if(!result){
		logger("O2M", LOG_LEVEL_ERROR, "DB update Failed");
		return RSC_INTERNAL_SERVER_ERROR;
	}

	cJSON *root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:grp", target_rtnode->obj);
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);
	o2pt->rsc = RSC_UPDATED;

	cJSON_DetachItemFromObject(root, "m2m:grp");
	cJSON_Delete(root);
	root = NULL;

	return RSC_UPDATED;
}

int create_grp(oneM2MPrimitive *o2pt, RTNode *parent_rtnode){
	int e = 1;
	int rsc = 0;
	if( parent_rtnode->ty != RT_AE && parent_rtnode->ty != RT_CSE ) {
		return o2pt->rsc = RSC_INVALID_CHILD_RESOURCETYPE;
	}

	cJSON *root = cJSON_Duplicate(o2pt->cjson_pc, 1);
	cJSON *grp = cJSON_GetObjectItem(root, "m2m:grp");

	add_general_attribute(grp, parent_rtnode, RT_GRP);

	cJSON_AddItemToObject(grp, "cnm", cJSON_CreateNumber(cJSON_GetArraySize(cJSON_GetObjectItem(grp, "mid"))));

	rsc = validate_grp(o2pt, grp);
	if(rsc >= 4000){
		logger("O2M", LOG_LEVEL_DEBUG, "Group Validation failed");
		return o2pt->rsc = rsc;
	}
	
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);
	o2pt->rsc = RSC_CREATED;

	cJSON *rn = cJSON_GetObjectItem(grp, "rn");
	char *uri = (char *)malloc((strlen(rn->valuestring) + strlen(parent_rtnode->uri) + 2) * sizeof(char));
	sprintf(uri, "%s/%s", parent_rtnode->uri, rn->valuestring);

	int result = db_store_resource(grp, uri);
	if(result != 1){
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail");
		return RSC_INTERNAL_SERVER_ERROR;
	}
	

	RTNode *child_rtnode = create_rtnode(grp, RT_GRP);
	add_child_resource_tree(parent_rtnode, child_rtnode);

	cJSON_DetachItemFromObject(root, "m2m:grp");
	cJSON_Delete(root);

	free(uri); uri = NULL;
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

	cJSON* sub = target_rtnode->obj;
	int result;
	
	validate_sub(o2pt, m2m_sub, o2pt->op);
	update_resource(sub, m2m_sub);
	db_update_resource(m2m_sub, cJSON_GetObjectItem(sub, "ri")->valuestring, RT_SUB);
	
	cJSON *root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:sub", target_rtnode->obj);
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);
	o2pt->rsc = RSC_UPDATED;

	cJSON_DetachItemFromObject(root, "m2m:sub");
	cJSON_Delete(root);
	o2pt->rsc = RSC_UPDATED;
	return RSC_UPDATED;
}

int create_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	int rsc = 0;
	char err_msg[256] = {0};
	int e = check_resource_type_invalid(o2pt);
	if(e != -1) e = check_payload_empty(o2pt);
	if(e != -1) e = check_payload_format(o2pt);
	if(e != -1) e = check_resource_type_equal(o2pt);
	if(e != -1) e = check_privilege(o2pt, parent_rtnode, ACOP_CREATE);
	if(e != -1 && o2pt->ty != RT_CIN) e = check_rn_duplicate(o2pt, parent_rtnode);
	if(e == -1) return o2pt->rsc;

	if(!is_attr_valid(o2pt->cjson_pc, o2pt->ty, err_msg)){
		return handle_error(o2pt, RSC_BAD_REQUEST, err_msg);
	}

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

	case RT_CSR:
		logger("O2M", LOG_LEVEL_INFO, "Create CSR");
		rsc = create_csr(o2pt, parent_rtnode);
		break;

	case RT_AEA:
		logger("O2M", LOG_LEVEL_INFO, "Create AEA");
		rsc = create_aea(o2pt, parent_rtnode);
		break;

	case RT_CBA:
		logger("O2M", LOG_LEVEL_INFO, "Create CBA");
		rsc = create_cba(o2pt, parent_rtnode);
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
	cJSON *root = cJSON_CreateObject();

	cJSON_AddItemToObject(root, get_resource_key(target_rtnode->ty), target_rtnode->obj);

	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cJSON_PrintUnformatted(root);
	cJSON_DetachItemFromObject(root, get_resource_key(target_rtnode->ty));
	cJSON_Delete(root);
	o2pt->rsc = RSC_OK;
	return RSC_OK;
}

int update_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode) {
	int rsc = 0;
	char err_msg[256] = {0};
	o2pt->ty = target_rtnode->ty;
	int e = check_payload_empty(o2pt);
	if(e != -1) e = check_payload_format(o2pt);
	ResourceType ty = parse_object_type_cjson(o2pt->cjson_pc);
	if(e != -1) e = check_resource_type_equal(o2pt);
	if(e != -1) e = check_privilege(o2pt, target_rtnode, ACOP_UPDATE);
	if(e != -1) e = check_rn_duplicate(o2pt, target_rtnode->parent);
	if(e == -1) return o2pt->rsc;
	

	if(!is_attr_valid(o2pt->cjson_pc, o2pt->ty, err_msg)){
		return handle_error(o2pt, RSC_BAD_REQUEST, err_msg);
	}
	
	switch(ty) {
		case RT_AE :
			logger("O2M", LOG_LEVEL_INFO, "Update AE");
			rsc = update_ae(o2pt, target_rtnode);
			break;

		case RT_CNT :
			logger("O2M", LOG_LEVEL_INFO, "Update CNT");
			rsc = update_cnt(o2pt, target_rtnode);
			break;

		case RT_CIN : 
			logger("O2M", LOG_LEVEL_INFO, "Update CIN");
			rsc = handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "operation `update` for cin is not allowed");
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

		case RT_AEA:
			logger("O2M", LOG_LEVEL_INFO, "Update AEA");
			rsc = update_aea(o2pt, target_rtnode);
			break;
		
		case RT_CSR:
			logger("O2M", LOG_LEVEL_INFO, "Update CSR");
			rsc = update_csr(o2pt, target_rtnode);
			break;

		// case RT_CBA:
		// 	logger("O2M", LOG_LEVEL_INFO, "Update CBA");
		// 	rsc = update_cba(o2pt, target_rtnode);
		// 	break;

		default :
			handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "operation `update` is unsupported");
			rsc = o2pt->rsc;
		}
	announce_to_annc(target_rtnode);
	return rsc;
}

int fopt_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *parent_rtnode){
	int rsc = 0;
	int cnt = 0;
	int cnm = 0;

	RTNode *target_rtnode = NULL;
	oneM2MPrimitive *req_o2pt = NULL;
	cJSON *new_pc = NULL;
	cJSON *agr = NULL;
	cJSON *rsp = NULL;
	cJSON *json = NULL;
	cJSON *grp = NULL;
	
	if(parent_rtnode == NULL){
		o2pt->rsc = RSC_NOT_FOUND;
		return RSC_NOT_FOUND;
	}
	logger("O2M", LOG_LEVEL_DEBUG, "handle fopt");


	if(cJSON_GetObjectItem(parent_rtnode->obj, "macp")){
		if( check_macp_privilege(o2pt, parent_rtnode, o2pt->op) == -1){
			return o2pt->rsc;
		}
	}else if(cJSON_GetObjectItem(parent_rtnode->obj, "acpi")){
		if(check_privilege(o2pt, parent_rtnode, o2pt->op) == -1){
			return o2pt->rsc;
		}
	}

	grp = parent_rtnode->obj;
	if(!grp){
		o2pt->rsc = RSC_INTERNAL_SERVER_ERROR;
		return RSC_INTERNAL_SERVER_ERROR;
	}
	
	if((cnm = cJSON_GetObjectItem(grp, "cnm")->valueint) == 0){
		logger("O2M", LOG_LEVEL_DEBUG, "No member to fanout");
		return o2pt->rsc = RSC_NO_MEMBERS;
	}

	o2ptcpy(&req_o2pt, o2pt);

	new_pc = cJSON_CreateObject();
	cJSON_AddItemToObject(new_pc, "m2m:agr", agr = cJSON_CreateObject());
	cJSON_AddItemToObject(agr, "m2m:rsp", rsp = cJSON_CreateArray());
	cJSON *mid_obj = NULL;

	cJSON_ArrayForEach(mid_obj, cJSON_GetObjectItem(grp, "mid")){
		char *mid = cJSON_GetStringValue(mid_obj);
		if(req_o2pt->to) free(req_o2pt->to);
		if(req_o2pt->pc) free(req_o2pt->pc);
		req_o2pt->pc = strdup(o2pt->pc);
		if(o2pt->fopt){
			if(strncmp(mid, CSE_BASE_NAME, strlen(CSE_BASE_NAME)) != 0 && mid[strlen(CSE_BASE_NAME) ] != '/'){
				mid = ri_to_uri(mid);
			}
			req_o2pt->to = malloc(strlen(mid) + strlen(o2pt->fopt) + 2);
			sprintf(req_o2pt->to, "%s%s", mid, o2pt->fopt);
		}else{
			req_o2pt->to = malloc(strlen(mid) + 2);
			sprintf(req_o2pt->to, "%s", mid);
		}
		req_o2pt->isFopt = false;
		ResourceAddressingType RAT = checkResourceAddressingType(mid);
		logger("O2M", LOG_LEVEL_DEBUG, "mid : %s", mid);
		if(RAT == CSE_RELATIVE){
			target_rtnode = find_rtnode(req_o2pt->to);
		}else if(RAT == SP_RELATIVE){
			if(isSpRelativeLocal(req_o2pt->to)){
				target_rtnode = find_rtnode(req_o2pt->to);
			}else{
				target_rtnode = get_remote_resource(req_o2pt->to);
				if(target_rtnode > 2000 && target_rtnode < 8000){
					req_o2pt->rsc = target_rtnode;
					target_rtnode = NULL;
				}
			}
		}

		// target_rtnode =  find_rtnode_by_uri(mid);
		// if(target_rtnode && target_rtnode->ty == RT_AE){
		// 	req_o2pt->fr = strdup(get_ri_rtnode(target_rtnode));
		// }
		json = o2pt_to_json(req_o2pt);
		// logger("O2M", LOG_LEVEL_DEBUG, "reqo2pt : %s", cJSON_PrintUnformatted(json));
		if(target_rtnode){
			rsc = handle_onem2m_request(req_o2pt, target_rtnode);
			json = o2pt_to_json(req_o2pt);
			// logger("O2M", LOG_LEVEL_DEBUG, "reqo2pt2 : %s", cJSON_PrintUnformatted(json));
			if(rsc < 4000) cnt++;
			json = o2pt_to_json(req_o2pt);
			if(json) {
				cJSON_AddItemToArray(rsp, json);
			}
			if(req_o2pt->op != OP_DELETE && target_rtnode->ty == RT_CIN){
				free_rtnode(target_rtnode);
				target_rtnode = NULL;
			}
			if(RAT == SP_RELATIVE && !isSpRelativeLocal(mid)){
				free_rtnode(target_rtnode);
				target_rtnode = NULL;
			}

			logger("O2M", LOG_LEVEL_DEBUG, "rsc : %d", rsc);
		} else{
			logger("O2M", LOG_LEVEL_DEBUG, "rtnode not found");
			if(RAT == SP_RELATIVE && !isSpRelativeLocal(mid)){
				cJSON_AddItemToArray(rsp, o2pt_to_json(req_o2pt));
			}
		}
	}

	if(o2pt->pc) free(o2pt->pc); //TODO double free bug
	o2pt->pc = cJSON_PrintUnformatted(new_pc);

	cJSON_Delete(new_pc);
	
	o2pt->rsc = RSC_OK;	

	free_o2pt(req_o2pt);
	req_o2pt = NULL;
	return RSC_OK;
}

/**
 * Discover Resources based on Filter Criteria
*/
int discover_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	logger("MAIN", LOG_LEVEL_DEBUG, "Discover Resource");
	cJSON *fc = o2pt->fc;
	cJSON *pjson = NULL, *pjson2 = NULL;
	cJSON *acpi_obj = NULL;
	cJSON *root = cJSON_CreateObject();
	cJSON *uril = NULL, *list = NULL;
	cJSON *json = NULL;

	int lSize = 0;
	if(check_privilege(o2pt, target_rtnode, ACOP_DISCOVERY) == -1){
		uril = cJSON_CreateArray();
		cJSON_AddItemToObject(root, "m2m:uril", uril);
		if(o2pt->pc) free(o2pt->pc);
		o2pt->pc = cJSON_PrintUnformatted(root);
		cJSON_Delete(root);
		return RSC_OK;
	}

	if(!o2pt->fc){
		logger("O2M", LOG_LEVEL_WARN, "Empty Filter Criteria");
		return RSC_BAD_REQUEST;
	}

	list = db_get_filter_criteria(o2pt);
	lSize = cJSON_GetArraySize(list);
	cJSON *lim_obj = cJSON_GetObjectItem(fc, "lim");
	cJSON *ofst_obj = cJSON_GetObjectItem(fc, "ofst");
	int lim = INT_MAX;
	int ofst = 0;
	if(lim_obj){
		lim = cJSON_GetNumberValue(lim_obj);
	}
	if(ofst_obj){
		ofst = cJSON_GetNumberValue(ofst_obj);
	}
	
	if(lSize > lim){
		logger("O2M", LOG_LEVEL_DEBUG, "limit exceeded");
		cJSON_DeleteItemFromArray(list, lSize);
		o2pt->cnst = CS_PARTIAL_CONTENT;
		o2pt->cnot = ofst + lim;
	}

    int drt = cJSON_GetNumberValue(cJSON_GetObjectItem(o2pt->drt, "drt"));
	if(drt == DRT_STRUCTURED) cJSON_AddItemToObject(root, "m2m:uril", list);
	else cJSON_AddItemToObject(root, "m2m:rrl", list);

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

	if(target_rtnode->sub_cnt == 0) {
		return 1;
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

	if(o2pt->op == OP_CREATE && o2pt->ty == RT_SUB) {
		cJSON_AddItemToObject(sgn, "vrq", cJSON_CreateBool(true));
	}

	NodeList *node = target_rtnode->subs;

	while(node){
		cJSON_AddStringToObject(sgn, "sur", node->rtnode->uri);
		notify_to_nu(node->rtnode, noti_cjson, net);
		cJSON_DeleteItemFromObject(sgn, "sur");
		node = node->next;
	}

	if(net == NET_DELETE_OF_RESOURCE) {
		net = NET_DELETE_OF_DIRECT_CHILD_RESOURCE;
		cJSON_SetNumberValue(cJSON_GetObjectItem(nev, "net"), net);
		// child = target_rtnode->parent->child;
		NodeList *node = target_rtnode->parent->subs;

		while(node){
			cJSON_AddStringToObject(sgn, "sur", node->rtnode->uri);
			notify_to_nu(node->rtnode, noti_cjson, net);
			cJSON_DeleteItemFromObject(sgn, "sur");
			node = node->next;
		}
	}

	cJSON_Delete(noti_cjson);

	return 1;
}

char *create_remote_annc(RTNode *parent_rtnode, cJSON *obj, char *at, bool isParent){
	extern cJSON* ATTRIBUTES;
	char buf[256] = {0};
	bool pannc = false;

	// Check Parent Resource has attribute at
	cJSON *pat = cJSON_GetObjectItem(parent_rtnode->obj, "at");
	int ty = cJSON_GetObjectItem(obj, "ty")->valueint;
	pannc = false;
	char *parent_target = NULL;
	cJSON *pjson = NULL;
	ResourceAddressingType RAT = checkResourceAddressingType(at);
	cJSON_ArrayForEach(pjson, pat){
		if(!strncmp(pjson->valuestring, at, strlen(at)) && pjson->valuestring[strlen(at)] == '/'){
			pannc = true;
			parent_target = strdup(pjson->valuestring);
			break;
		}
	}
	
	if(!pannc){ // if parent resource has no attribute at
		logger("UTIL", LOG_LEVEL_DEBUG, "Announcement for %s not created", parent_rtnode->uri);
		if(parent_rtnode->parent){
			logger("UTIL", LOG_LEVEL_DEBUG, "Creating remote annc for %s", parent_rtnode->parent->uri);
			parent_target = create_remote_annc(parent_rtnode->parent, parent_rtnode->obj, at, true);
			if ( parent_target == -1){
				logger("UTIL", LOG_LEVEL_ERROR, "Announcement for %s not created", parent_rtnode->uri);
				return NULL;
			}
			if(!parent_target){
				logger("UTIL", LOG_LEVEL_ERROR, "Announcement for %s not created", parent_rtnode->uri);
				return NULL;
			}
		}
		else{
			logger("UTIL", LOG_LEVEL_DEBUG, "Creating cbA");
			if( create_remote_cba(at, &parent_target) == -1){
				logger("UTIL", LOG_LEVEL_ERROR, "Announcement for %s not created", parent_rtnode->uri);
				return NULL;
			}
		}
	}
	
	if(RAT == SP_RELATIVE){
		oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(sizeof(oneM2MPrimitive), 1);
		o2pt->fr = strdup("/"CSE_BASE_RI);
		o2pt->to = strdup(parent_target);
		o2pt->op = OP_CREATE;
		o2pt->ty = ty+10000;
		o2pt->rqi = strdup("create-annc");
		o2pt->rvi = strdup("3");

		cJSON *root = cJSON_CreateObject();
		cJSON *annc = cJSON_CreateObject();
		cJSON *pjson = NULL;
		cJSON_AddItemToObject(root, get_resource_key(ty+10000), annc);
		sprintf(buf, "/%s/%s/%s", CSE_BASE_RI, get_uri_rtnode(parent_rtnode), cJSON_GetObjectItem(obj, "rn")->valuestring);
		cJSON_AddItemToObject(annc, "lnk", cJSON_CreateString(buf));

		switch(ty){
			case RT_ACP:
				if(pjson = cJSON_Duplicate( cJSON_GetObjectItem(obj, "pv"), true) )
					cJSON_AddItemToObject(annc, "pv", pjson);
				if(pjson = cJSON_Duplicate( cJSON_GetObjectItem(obj, "pvs"), true))
					cJSON_AddItemToObject(annc, "pvs", pjson);
				break;
			case RT_AE:
				pjson = cJSON_Duplicate( cJSON_GetObjectItem(obj, "srv"), true);
				cJSON_AddItemToObject(annc, "srv", pjson);
				break;
			case RT_CNT:
				break;
			case RT_CIN:
				break;
			case RT_SUB:
				break;
			case RT_GRP:
				break;
		}


		cJSON *lbl = cJSON_GetObjectItem(obj, "lbl");
		if(lbl)
			cJSON_AddItemToObject(annc, "lbl", cJSON_Duplicate(lbl, true));

		cJSON *ast = cJSON_GetObjectItem(obj, "ast");
		if(ast)
			cJSON_AddItemToObject(annc, "ast", cJSON_Duplicate(ast, true));

		cJSON* aa = cJSON_GetObjectItem(obj, "aa");
		cJSON *attr = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(ty));
		cJSON_ArrayForEach(pjson, aa){
			if(strcmp(pjson->valuestring, "lbl") == 0) continue;
			if(strcmp(pjson->valuestring, "ast") == 0) continue;
			if(strcmp(pjson->valuestring, "lnk") == 0) continue;
			if(!cJSON_GetObjectItem(attr, pjson->valuestring)){
				logger("UTIL", LOG_LEVEL_ERROR, "invalid attribute in aa");
				return -1;
			}
			cJSON *temp =  cJSON_GetObjectItem(obj, pjson->valuestring);
			cJSON_AddItemToObject(annc, pjson->valuestring, cJSON_Duplicate(temp, true));
		}

		o2pt->pc = cJSON_PrintUnformatted(root);
		cJSON_Delete(root);
		o2pt->isForwarding = true;
		RTNode *rtnode = find_csr_rtnode_by_uri(at);
		if(!rtnode){
			logger("UTIL", LOG_LEVEL_ERROR, "at target not found");
			return NULL;
		}
		int rsc = 0;
		if( rsc = forwarding_onem2m_resource(o2pt, rtnode) >= 4000 ){
			free_o2pt(o2pt);
			logger("UTIL", LOG_LEVEL_ERROR, "Creation failed");
			return NULL;
		}

		cJSON *result = cJSON_Parse(o2pt->pc);
		cJSON *annc_obj = cJSON_GetObjectItem(result, get_resource_key(ty+10000));
		char *annc_ri = cJSON_GetObjectItem(annc_obj, "ri")->valuestring;
		sprintf(buf, "%s/%s", at, annc_ri);
		if(isParent){
			cJSON *at = cJSON_GetObjectItem(obj, "at");
			if(!at){
				at = cJSON_CreateArray();
				cJSON_AddItemToObject(obj, "at", at);
			}
			cJSON_AddItemToArray(at, cJSON_CreateString(buf));
			db_update_resource(obj, cJSON_GetObjectItem(obj, "ri")->valuestring, ty);
		}
		cJSON_Delete(result);
		free_o2pt(o2pt);
		free(parent_target);

		return strdup(buf);
	}		
	return NULL;
}


/**
 * @brief forward oneM2M resource to remote CSE\nreturn stored in o2pt
 * @param o2pt oneM2M request primitive
 * @param target_rtnode target csr
 * @return int result code
*/
int forwarding_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	Protocol protocol = 0;
	char *host = NULL;
	int port = 0;
	char *path = NULL;
	int rsc = 0;
	char buf[256] = {0};

	logger("O2M", LOG_LEVEL_DEBUG, "Forwarding Resource");
	
	if(!target_rtnode){
		if(rt->registrar_csr){
			logger("O2M", LOG_LEVEL_DEBUG, "local csr not found, forwarding to registrar");
			target_rtnode = rt->registrar_csr;
		}else{
			return o2pt->rsc = RSC_NOT_FOUND;
		}
	}

	if(target_rtnode->ty != RT_CSR){
		logger("O2M", LOG_LEVEL_ERROR, "target_rtnode is not CSR");
		return o2pt->rsc = RSC_NOT_FOUND;
	}

	if(checkResourceAddressingType(o2pt->fr) == CSE_RELATIVE){
		sprintf(buf, "/%s/%s", CSE_BASE_RI, o2pt->fr);
		free(o2pt->fr);
		o2pt->fr = strdup(buf);
	}

	cJSON *csr = target_rtnode->obj;
	cJSON *poa_list = cJSON_GetObjectItem(csr, "poa");
	cJSON *poa = NULL;
	cJSON_ArrayForEach(poa, poa_list){
		if(parsePoa(poa->valuestring, &protocol, &host, &port, &path) == -1){
			logger("O2M", LOG_LEVEL_ERROR, "poa parse error");
			continue;
		}
		if(protocol == PROT_HTTP){
			http_forwarding(o2pt, host, port);
		}

		#ifdef ENABLE_MQTT
		else if(protocol == PROT_MQTT){
			mqtt_forwarding(o2pt, host, port, csr);
		}
		#endif

		#ifdef ENABLE_COAP
		else if(protocol == PROT_COAP){
			coap_forwarding(o2pt, host, port);
		}
		#endif
		free(host);
		free(path);

		if(o2pt->rsc == RSC_OK){
			break;
		}
	}

	if(o2pt->rsc >= 4000){
		logger("O2M", LOG_LEVEL_ERROR, "forwarding failed");
		return o2pt->rsc;
	}

	return o2pt->rsc;
}

int check_cse_reachable(RRNode *rrnode){
	int status_code = 0;

	oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(sizeof(oneM2MPrimitive), 1);
	o2pt->ty = 0;
	o2pt->op = OP_RETRIEVE;
	o2pt->to = strdup(rrnode->uri);
	o2pt->fr = strdup("/"CSE_BASE_RI);
	o2pt->rqi = strdup("check-reachability");
	o2pt->pc = NULL;
	o2pt->fc = NULL;
	o2pt->isForwarding = true;
	forwarding_onem2m_resource(o2pt, rrnode->rtnode);
	

	logger("UTIL", LOG_LEVEL_DEBUG, "RR rslt: %d", o2pt->rsc);
	status_code = o2pt->rsc;

	return status_code;
}