#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <curl/curl.h>
#include <math.h>
#include <ctype.h>
#include <malloc.h>
#include <sys/timeb.h>
#include "onem2m.h"
#include "berkeleyDB.h"
#include "jsonparse.h"
#include "httpd.h"
#include "mqttClient.h"
#include "onem2mTypes.h"
#include "config.h"

RTNode* parse_uri(oneM2MPrimitive *o2pt, RTNode *cb) {
	logger("O2M", LOG_LEVEL_DEBUG, "Call parse_uri");
	char uri_array[MAX_URI_SIZE];
	char *uri_parse = uri_array;
	strcpy(uri_array, o2pt->to);

	char uri_strtok[64][MAX_URI_SIZE] = {"\0", };
	int index_start = 0, index_end = -1;

	uri_parse = strtok(uri_array, "/");

	while(uri_parse) {
		strcpy(uri_strtok[++index_end], uri_parse);
		uri_parse = strtok(NULL, "/");
	}

	int latest_oldest_flag = -1;

	if(!strcmp(uri_strtok[0], "viewer")) index_start++;
	if(!strcmp(uri_strtok[index_end], "la") || !strcmp(uri_strtok[index_end], "latest")) {
		latest_oldest_flag = 0; index_end--;
	} else if(!strcmp(uri_strtok[index_end], "ol") || !strcmp(uri_strtok[index_end], "oldest")) {
		latest_oldest_flag = 1; index_end--;
	}

	strcpy(uri_array, "\0");
	for(int i=index_start; i<=index_end; i++) {
		strcat(uri_array,"/"); strcat(uri_array,uri_strtok[i]);
	}
	RTNode* rtnode = find_rtnode_by_uri(cb, uri_array);
	
	if(rtnode && latest_oldest_flag != -1) rtnode = find_latest_oldest(rtnode, latest_oldest_flag);

	if(index_start == 1) o2pt->op = OP_VIEWER;

	return rtnode;
}

RTNode *find_rtnode_by_uri(RTNode *cb, char *target_uri) {
	RTNode *rtnode = cb, *parent_rtnode = NULL;
	target_uri = strtok(target_uri, "/");

	if(!target_uri) return NULL;

	char uri_array[64][MAX_URI_SIZE];
	int index = -1;

	while(target_uri) {
		strcpy(uri_array[++index], target_uri);
		target_uri = strtok(NULL, "/");
	}

	for(int i=0; i<=index; i++) {
		while(rtnode) {
			if(!strcmp(rtnode->rn, uri_array[i])) break;
			rtnode = rtnode->sibling_right;
		}
		if(i == index-1) parent_rtnode = rtnode;
		if(!rtnode) break;
		if(i != index) rtnode = rtnode->child;
	}

	if(rtnode) return rtnode;

	if(parent_rtnode) {
		CIN *cin = db_get_cin(uri_array[index]);
		if(cin) {
			if(!strcmp(cin->pi, parent_rtnode->ri)) {
				rtnode = create_rtnode(cin, TY_CIN);
				rtnode->parent = parent_rtnode;
			}
			free_cin(cin);
		}
	}

	return rtnode;
}

RTNode *find_latest_oldest(RTNode* rtnode, int flag) {
	if(rtnode->ty == TY_CNT) {
		RTNode *head = db_get_cin_rtnode_list_by_pi(rtnode->ri);
		RTNode *cin = head;

		if(cin) {
			if(flag == 1) {
				head = head->sibling_right;
				cin->sibling_right = NULL;			
			} else {
				while(cin->sibling_right) cin = cin->sibling_right;
				if(cin->sibling_left) {
					cin->sibling_left->sibling_right = NULL;
					cin->sibling_left = NULL;
				}
			}
			if(head != cin) free_rtnode_list(head);
			if(cin) {
				cin->parent = rtnode;
				return cin;
			} else {
				return NULL;
			}
		}
		return NULL;
	}
}

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
	
	cse->ty = TY_CSE;
	
	free(ct); ct = NULL;
}

RTNode* create_rtnode(void *resource, ObjectType ty){
	RTNode* rtnode = NULL;

	switch(ty) {
	case TY_CSE: rtnode = create_cse_rtnode((CSE*)resource); break;
	case TY_AE: rtnode = create_ae_rtnode((AE*)resource); break;
	case TY_CNT: rtnode = create_cnt_rtnode((CNT*)resource); break;
	case TY_CIN: rtnode = create_cin_rtnode((CIN*)resource); break;
	case TY_SUB: rtnode = create_sub_rtnode((Sub*)resource); break;
	case TY_ACP: rtnode = create_acp_rtnode((ACP*)resource); break;
	}

	rtnode->parent = NULL;
	rtnode->child = NULL;
	rtnode->sibling_left = NULL;
	rtnode->sibling_right = NULL;
	
	return rtnode;
}

RTNode* create_cse_rtnode(CSE *cse) {
	RTNode* rtnode = calloc(1, sizeof(RTNode));

	rtnode->rn = (char*)malloc((strlen(cse->rn) + 1) * sizeof(char));
	rtnode->ri = (char*)malloc((strlen(cse->ri) + 1) * sizeof(char));
	rtnode->pi = (char*)malloc((strlen(cse->pi) + 1) * sizeof(char));
	
	strcpy(rtnode->rn, cse->rn);
	strcpy(rtnode->ri, cse->ri);
	strcpy(rtnode->pi, cse->pi);

	rtnode->ty = TY_CSE;

	return rtnode;
}

RTNode* create_ae_rtnode(AE *ae) {
	RTNode* rtnode = calloc(1, sizeof(RTNode));

	rtnode->rn = (char*)malloc((strlen(ae->rn) + 1) * sizeof(char));
	rtnode->ri = (char*)malloc((strlen(ae->ri) + 1) * sizeof(char));
	rtnode->pi = (char*)malloc((strlen(ae->pi) + 1) * sizeof(char));
	
	strcpy(rtnode->rn, ae->rn);
	strcpy(rtnode->ri, ae->ri);
	strcpy(rtnode->pi, ae->pi);

	rtnode->ty = TY_AE;

	return rtnode;
}

RTNode* create_cnt_rtnode(CNT *cnt) {
	RTNode* rtnode = calloc(1, sizeof(RTNode));

	rtnode->rn = (char*)malloc((strlen(cnt->rn) + 1) * sizeof(char));
	rtnode->ri = (char*)malloc((strlen(cnt->ri) + 1) * sizeof(char));
	rtnode->pi = (char*)malloc((strlen(cnt->pi) + 1) * sizeof(char));
	
	strcpy(rtnode->rn, cnt->rn);
	strcpy(rtnode->ri, cnt->ri);
	strcpy(rtnode->pi, cnt->pi);

	if(cnt->acpi) {
		rtnode->acpi = (char*)malloc((strlen(cnt->acpi) + 1) * sizeof(char));
		strcpy(rtnode->acpi, cnt->acpi);
	}

	rtnode->ty = TY_CNT;
	rtnode->cni = cnt->cni;
	rtnode->cbs = cnt->cbs;
	rtnode->mni = cnt->mni;
	rtnode->mbs = cnt->mbs;

	return rtnode;
}

RTNode* create_cin_rtnode(CIN *cin) {
	RTNode* rtnode = calloc(1, sizeof(RTNode));

	rtnode->rn = (char*)malloc((strlen(cin->rn) + 1) * sizeof(char));
	rtnode->ri = (char*)malloc((strlen(cin->ri) + 1) * sizeof(char));
	rtnode->pi = (char*)malloc((strlen(cin->pi) + 1) * sizeof(char));
	
	strcpy(rtnode->rn, cin->rn);
	strcpy(rtnode->ri, cin->ri);
	strcpy(rtnode->pi, cin->pi);

	rtnode->ty = TY_CIN;
	rtnode->cs = cin->cs;

	return rtnode;
}

RTNode* create_sub_rtnode(Sub *sub) {
	RTNode* rtnode = calloc(1, sizeof(RTNode));

	rtnode->rn = (char*)malloc((strlen(sub->rn) + 1) * sizeof(char));
	rtnode->ri = (char*)malloc((strlen(sub->ri) + 1) * sizeof(char));
	rtnode->pi = (char*)malloc((strlen(sub->pi) + 1) * sizeof(char));
	rtnode->nu = (char*)malloc((strlen(sub->nu) + 1) * sizeof(char));
	rtnode->sur = (char*)malloc((strlen(sub->sur) + 1) * sizeof(char));
	
	strcpy(rtnode->rn, sub->rn);
	strcpy(rtnode->ri, sub->ri);
	strcpy(rtnode->pi, sub->pi);
	strcpy(rtnode->nu, sub->nu);
	strcpy(rtnode->sur, sub->sur);

	rtnode->ty = TY_SUB;
	rtnode->net = net_to_bit(sub->net);

	return rtnode;
}

RTNode* create_acp_rtnode(ACP *acp) {
	RTNode* rtnode = calloc(1, sizeof(RTNode));

	rtnode->rn = (char*)malloc((strlen(acp->rn) + 1) * sizeof(char));
	rtnode->ri = (char*)malloc((strlen(acp->ri) + 1) * sizeof(char));
	rtnode->pi = (char*)malloc((strlen(acp->pi) + 1) * sizeof(char));
	rtnode->pv_acor = (char*)malloc((strlen(acp->pv_acor) + 1) * sizeof(char));
	rtnode->pv_acop = (char*)malloc((strlen(acp->pv_acop) + 1) * sizeof(char));
	rtnode->pvs_acor = (char*)malloc((strlen(acp->pvs_acor) + 1) * sizeof(char));
	rtnode->pvs_acop = (char*)malloc((strlen(acp->pvs_acop) + 1) * sizeof(char));

	strcpy(rtnode->rn, acp->rn);
	strcpy(rtnode->ri, acp->ri);
	strcpy(rtnode->pi, acp->pi);
	strcpy(rtnode->pv_acor, acp->pv_acor);
	strcpy(rtnode->pv_acop, acp->pv_acop);
	strcpy(rtnode->pvs_acor, acp->pvs_acor);
	strcpy(rtnode->pvs_acop, acp->pvs_acop);

	rtnode->ty = TY_ACP;

	return rtnode;
}

void free_cse(CSE *cse) {
	if(cse->ct) free(cse->ct);
	if(cse->lt) free(cse->lt);
	if(cse->rn) free(cse->rn);
	if(cse->ri) free(cse->ri);
	if(cse->csi) free(cse->csi);
	if(cse->pi) free(cse->pi);
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
	free(ae); ae = NULL;
}

void free_cnt(CNT *cnt) {
	if(cnt->et) free(cnt->et);
	if(cnt->ct) free(cnt->ct);
	if(cnt->lt) free(cnt->lt);
	if(cnt->rn) free(cnt->rn);
	if(cnt->ri) free(cnt->ri);
	if(cnt->pi) free(cnt->pi);
	free(cnt); cnt = NULL;
}

void free_cin(CIN* cin) {
	if(cin->et) free(cin->et);
	if(cin->ct) free(cin->ct);
	if(cin->lt) free(cin->lt);
	if(cin->rn) free(cin->rn);
	if(cin->ri) free(cin->ri);
	if(cin->pi) free(cin->pi);
	if(cin->con) free(cin->con);
	free(cin); cin = NULL;
}

void free_sub(Sub* sub) {
	if(sub->et) free(sub->et);
	if(sub->ct) free(sub->ct);
	if(sub->lt) free(sub->lt);
	if(sub->rn) free(sub->rn);
	if(sub->ri) free(sub->ri);
	if(sub->pi) free(sub->pi);
	if(sub->nu) free(sub->nu);
	if(sub->net) free(sub->net);
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
	free(acp); acp = NULL;
}

void free_rtnode(RTNode *rtnode) {
	free(rtnode->ri);
	free(rtnode->rn);
	free(rtnode->pi);
	if(rtnode->nu) free(rtnode->nu);
	if(rtnode->sur) free(rtnode->sur);
	if(rtnode->acpi) free(rtnode->acpi);
	if(rtnode->pv_acop) free(rtnode->pv_acop);
	if(rtnode->pv_acor) free(rtnode->pv_acor);
	if(rtnode->pvs_acor) free(rtnode->pvs_acor);
	if(rtnode->pvs_acop) free(rtnode->pvs_acop);
	if(rtnode->uri) free(rtnode->uri);
	free(rtnode); rtnode = NULL;
}

void free_rtnode_list(RTNode *rtnode) {
	while(rtnode) {
		RTNode *right = rtnode->sibling_right;
		free_rtnode(rtnode);
		rtnode = right;
	}
}

int net_to_bit(char *net) {
	int netLen = strlen(net);
	int ret = 0;

	for(int i=0; i<netLen; i++) {
		int exp = atoi(net+i);
		if(exp > 0) ret = (ret | (int)pow(2, exp - 1));
	}

	return ret;
}

int add_child_resource_tree(RTNode *parent, RTNode *child) {
	RTNode *node = parent->child;
	child->parent = parent;

	logger("O2M", LOG_LEVEL_DEBUG, "Add Resource Tree Node [Parent-ID] : %s, [Child-ID] : %s",parent->ri, child->ri);
	
	if(!node) {
		parent->child = child;
	} else if(node) {
		while(node->sibling_right && node->sibling_right->ty <= child->ty) { 	
				node = node->sibling_right;
		}

		if(parent->child == node && child->ty < node->ty) {
			parent->child = child;
			child->sibling_right = node;
			node->sibling_left = child;
		} else {
			if(node->sibling_right) {
				node->sibling_right->sibling_left = child;
				child->sibling_right = node->sibling_right;
			}

			node->sibling_right = child;
			child->sibling_left = node;
		}
	}
	
	
	
	return 1;
}

ObjectType http_parse_object_type() {
	char *content_type = request_header("Content-Type");
	if(!content_type) return TY_NONE;
	char *str_ty = strstr(content_type, "ty=");
	if(!str_ty) return TY_NONE;
	int object_type = atoi(str_ty+3);

	ObjectType ty;
	
	switch(object_type) {
	case 1 : ty = TY_ACP; break;
	case 2 : ty = TY_AE; break;
	case 3 : ty = TY_CNT; break;
	case 4 : ty = TY_CIN; break;
	case 5 : ty = TY_CSE; break;
	case 23 : ty = TY_SUB; break;
	default : ty = TY_NONE; break;
	}
	
	return ty;
}

void create_ae(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	int e = check_aei_duplicate(o2pt, parent_rtnode);
	if(e != -1) e = check_rn_invalid(o2pt, TY_AE);
	if(e == -1) return;

	if(parent_rtnode->ty != TY_CSE) {
		child_type_error(o2pt);
		return;
	}
	AE* ae = cjson_to_ae(o2pt->cjson_pc);
	if(!ae) {
		no_mandatory_error(o2pt);
		return;
	}
	if(ae->api[0] != 'R' && ae->api[0] != 'N') {
		free_ae(ae);
		api_prefix_invalid(o2pt);
		return;
	}
	init_ae(ae,parent_rtnode->ri, o2pt->fr);
	
	int result = db_store_ae(ae);
	if(result != 1) { 
		db_store_fail(o2pt); free_ae(ae); ae = NULL;
		return;
	}
	
	RTNode* child_rtnode = create_rtnode(ae, TY_AE);
	add_child_resource_tree(parent_rtnode, child_rtnode);
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = ae_to_json(ae);
	o2pt->rsc = RSC_CREATED;
	respond_to_client(o2pt, 201);
	// notify_onem2m_resource(pnode->child, response_payload, NOTIFICATION_EVENT_3);
	free_ae(ae); ae = NULL;
}

void create_cnt(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	if(parent_rtnode->ty != TY_CNT && parent_rtnode->ty != TY_AE && parent_rtnode->ty != TY_CSE) {
		child_type_error(o2pt);
		return;
	}
	CNT* cnt = cjson_to_cnt(o2pt->cjson_pc);
	if(!cnt) {
		no_mandatory_error(o2pt);
		return;
	}
	if(cnt->mbs != INT_MIN && cnt->mbs < 0) {
		mni_mbs_invalid(o2pt, "mbs"); free(cnt); cnt = NULL;
		return;
	}
	if(cnt->mni != INT_MIN && cnt->mni < 0) {
		mni_mbs_invalid(o2pt, "mni"); free(cnt); cnt = NULL;
		return;
	}
	init_cnt(cnt,parent_rtnode->ri);

	int result = db_store_cnt(cnt);
	if(result != 1) { 
		db_store_fail(o2pt); free_cnt(cnt); cnt = NULL;
		return;
	}
	
	RTNode* child_rtnode = create_rtnode(cnt, TY_CNT);
	add_child_resource_tree(parent_rtnode,child_rtnode);
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cnt_to_json(cnt);
	o2pt->rsc = RSC_CREATED;
	respond_to_client(o2pt, 201);
	//notify_onem2m_resource(pnode->child, response_payload, NOTIFICATION_EVENT_3);
	free_cnt(cnt); cnt = NULL;
}


void create_cin(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	if(parent_rtnode->ty != TY_CNT) {
		child_type_error(o2pt);
		return;
	}
	CIN* cin = cjson_to_cin(o2pt->cjson_pc);
	if(!cin) {
		no_mandatory_error(o2pt);
		return;
	} else if(parent_rtnode->mbs >= 0 && cin->cs > parent_rtnode->mbs) {
		too_large_content_size_error(o2pt); free(cin); cin = NULL;
		return;
	}
	init_cin(cin,parent_rtnode->ri);

	int result = db_store_cin(cin);
	if(result != 1) { 
		db_store_fail(o2pt); free_cin(cin); cin = NULL;
		return;
	}

	RTNode *cin_rtnode = create_rtnode(cin, TY_CIN);
	update_cnt_cin(parent_rtnode, cin_rtnode, 1);
	free_rtnode(cin_rtnode);
	
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cin_to_json(cin);
	o2pt->rsc = RSC_CREATED;
	respond_to_client(o2pt, 201);
	//notify_onem2m_resource(pnode->child, response_payload, NOTIFICATION_EVENT_3);
	free_cin(cin); cin = NULL;
}

void create_sub(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	if(parent_rtnode->ty == TY_CIN || parent_rtnode->ty == TY_SUB) {
		child_type_error(o2pt);
		return;
	}
	Sub* sub = cjson_to_sub(o2pt->cjson_pc);
	if(!sub) {
		no_mandatory_error(o2pt);
		return;
	}
	init_sub(sub, parent_rtnode->ri, o2pt->to);
	
	int result = db_store_sub(sub);
	if(result != 1) { 
		db_store_fail(o2pt); free_sub(sub); sub = NULL;
		return;
	}
	
	RTNode* child_rtnode = create_rtnode(sub, TY_SUB);
	add_child_resource_tree(parent_rtnode,child_rtnode);
	
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = sub_to_json(sub);
	o2pt->rsc = RSC_CREATED;
	respond_to_client(o2pt, 201);
	//notify_onem2m_resource(pnode->child, response_payload, NOTIFICATION_EVENT_3);
	free_sub(sub); sub = NULL;
}

void create_acp(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	if(parent_rtnode->ty != TY_CSE && parent_rtnode->ty != TY_AE) {
		child_type_error(o2pt);
		return;
	}
	ACP* acp = cjson_to_acp(o2pt->cjson_pc);
	if(!acp) {
		no_mandatory_error(o2pt);
		return;
	}
	init_acp(acp, parent_rtnode->ri);
	
	int result = db_store_acp(acp);
	if(result != 1) { 
		db_store_fail(o2pt); free_acp(acp); acp = NULL;
		return;
	}
	
	RTNode* child_rtnode = create_rtnode(acp, TY_ACP);
	add_child_resource_tree(parent_rtnode, child_rtnode);
	
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = acp_to_json(acp);
	o2pt->rsc = RSC_CREATED;
	respond_to_client(o2pt, 201);
	//notify_onem2m_resource(pnode->child, response_payload, NOTIFICATION_EVENT_3);
	free_acp(acp); acp = NULL;
}

void retrieve_cse(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	CSE* gcse = db_get_cse(target_rtnode->ri);
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cse_to_json(gcse);
	o2pt->rsc = RSC_OK;
	respond_to_client(o2pt, 200);
	free_cse(gcse); gcse = NULL;
}


void retrieve_ae(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	AE* gae = db_get_ae(target_rtnode->ri);
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = ae_to_json(gae);
	o2pt->rsc = RSC_OK;
	respond_to_client(o2pt, 200);
	free_ae(gae); gae = NULL;
}

void retrieve_cnt(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	CNT* gcnt = db_get_cnt(target_rtnode->ri);
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cnt_to_json(gcnt);
	o2pt->rsc = RSC_OK;
	respond_to_client(o2pt, 200);
	free_cnt(gcnt); gcnt = NULL;
}

void retrieve_cin(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	CIN* gcin = db_get_cin(target_rtnode->ri);
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cin_to_json(gcin);
	o2pt->rsc = RSC_OK;
	respond_to_client(o2pt, 200); 
	free_cin(gcin); gcin = NULL;
}

void retrieve_sub(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	Sub* gsub = db_get_sub(target_rtnode->ri);
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = sub_to_json(gsub);
	o2pt->rsc = RSC_OK;
	respond_to_client(o2pt, 200); 
	free_sub(gsub); gsub = NULL;
}

void retrieve_acp(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	ACP* gacp = db_get_acp(target_rtnode->ri);
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = acp_to_json(gacp);
	o2pt->rsc = RSC_OK;
	respond_to_client(o2pt, 200);
	free_acp(gacp); gacp = NULL;
}

void delete_rtnode_and_db_data(RTNode *rtnode, int flag) {
	switch(rtnode->ty) {
	case TY_AE : 
		db_delete_onem2m_resource(rtnode->ri); 
		break;
	case TY_CNT : 
		db_delete_onem2m_resource(rtnode->ri); 
		//char *noti_json = (char*)malloc(sizeof("resource is deleted successfully") + 1);
		//strcpy(noti_json, "resource is deleted successfully");
		//notify_onem2m_resource(node->child,noti_json,NOTIFICATION_EVENT_2); 
		//free(noti_json); noti_json = NULL;
		break;
	case TY_CIN :
		db_delete_onem2m_resource(rtnode->ri);
		update_cnt_cin(rtnode->parent, rtnode,-1);
		return;
	case TY_SUB :
		db_delete_sub(rtnode->ri);
		break;
	case TY_ACP :
		db_delete_acp(rtnode->ri);
		break;
	}

	RTNode *left = rtnode->sibling_left;
	RTNode *right = rtnode->sibling_right;
	
	if(rtnode->ty != TY_CIN) {
		if(flag == 1) {
			if(left) left->sibling_right = right;
			else rtnode->parent->child = right;
			if(right) right->sibling_left = left;
		} else {
			if(right) delete_rtnode_and_db_data(right, 0);
		}
	}
	
	if(rtnode->child) delete_rtnode_and_db_data(rtnode->child, 0);
	
	free_rtnode(rtnode); rtnode = NULL;
	
}

char *get_local_time(int diff) {
	time_t t = time(NULL) - diff;
	struct tm tm = *localtime(&t);
	
	char year[5], mon[3], day[3], hour[3], minute[3], sec[3]; 
	
	sprintf(year,"%d", tm.tm_year+1900);
	sprintf(mon,"%02d",tm.tm_mon+1);
	sprintf(day,"%02d",tm.tm_mday);
	sprintf(hour,"%02d",tm.tm_hour);
	sprintf(minute,"%02d",tm.tm_min);
	sprintf(sec,"%02d",tm.tm_sec);
	
	char* local_time = (char*)malloc(16 * sizeof(char));
	
	*local_time = '\0';
	strcat(local_time,year);
	strcat(local_time,mon);
	strcat(local_time,day);
	strcat(local_time,"T");
	strcat(local_time,hour);
	strcat(local_time,minute);
	strcat(local_time,sec);
	
	return local_time;
}

void set_o2pt_pc(oneM2MPrimitive *o2pt, char *pc, ...){
	if(o2pt->pc) free(o2pt->pc);

	o2pt->pc = (char *)malloc((strlen(pc) + 1) * sizeof(char));
	strcpy(o2pt->pc, pc);
}

void set_o2pt_rsc(oneM2MPrimitive *o2pt, int rsc){
	o2pt->rsc = rsc;
}

int is_json_valid_char(char c){
	return (('!' <= c && c <= '~') || c == ' ');
}

void respond_to_client(oneM2MPrimitive *o2pt, int status) {
	if(!o2pt->pc) {
		logger("O2M", LOG_LEVEL_ERROR, "Response payload is NULL");
		return;
	}

	switch(o2pt->prot) {
		case PROT_HTTP:
			http_respond_to_client(o2pt, status); 
			break;
		case PROT_MQTT:
			mqtt_respond_to_client(o2pt);
			break;
	}
}

ObjectType parse_object_type_cjson(cJSON *cjson) {
	ObjectType ty;

	if(!cjson) return TY_NONE;
	
	if(cJSON_GetObjectItem(cjson, "m2m:cb") || cJSON_GetObjectItem(cjson, "m2m:cse")) ty = TY_CSE;
	else if(cJSON_GetObjectItem(cjson, "m2m:ae")) ty = TY_AE;
	else if(cJSON_GetObjectItem(cjson, "m2m:cnt")) ty = TY_CNT;
	else if(cJSON_GetObjectItem(cjson, "m2m:cin")) ty = TY_CIN;
	else if(cJSON_GetObjectItem(cjson, "m2m:sub")) ty = TY_SUB;
	else if(cJSON_GetObjectItem(cjson, "m2m:acp")) ty = TY_ACP;
	else ty = TY_NONE;
	
	return ty;
}

void init_ae(AE* ae, char *pi, char *origin) {
	char *ct = get_local_time(0);
	char *et = get_local_time(EXPIRE_TIME);
	char ri[128] = {'\0'};

	if(origin) {
		if(origin[0] != 'C') strcpy(ri, "C");
		strcat(ri, origin);
	} else {
		strcpy(ri, "CAE");
		strcat(ri, ct);
	}

	if(!ae->rn) {
		ae->rn = (char*)malloc((strlen(ri) + 1) * sizeof(char));
		strcpy(ae->rn, ri);
	}
	
	ae->ri = (char*)malloc((strlen(ri) + 1) * sizeof(char));
	ae->pi = (char*)malloc((strlen(pi) + 1) * sizeof(char));
	ae->et = (char*)malloc((strlen(et) + 1) * sizeof(char));
	ae->ct = (char*)malloc((strlen(ct) + 1) * sizeof(char));
	ae->lt = (char*)malloc((strlen(ct) + 1) * sizeof(char));
	ae->aei = (char*)malloc((strlen(ri) + 1) * sizeof(char));
	strcpy(ae->ri, ri);
	strcpy(ae->pi, pi);
	strcpy(ae->et, et);
	strcpy(ae->ct, ct);
	strcpy(ae->lt, ct);
	strcpy(ae->aei, ri);
	
	ae->ty = TY_AE;
	
	free(ct); ct = NULL;
	free(et); et = NULL;
}

void init_cnt(CNT* cnt, char *pi) {
	char *ct = get_local_time(0);
	char *et = get_local_time(EXPIRE_TIME);
	char *ri = resource_identifier(TY_CNT, ct);
	
	if(!cnt->rn) {
		cnt->rn = (char*)malloc((strlen(ri) + 1) * sizeof(char));
		strcpy(cnt->rn, ri);
	}
	
	cnt->ri = (char*)malloc((strlen(ri) + 1) * sizeof(char));
	cnt->pi = (char*)malloc((strlen(pi) + 1) * sizeof(char));
	cnt->et = (char*)malloc((strlen(et) + 1) * sizeof(char));
	cnt->ct = (char*)malloc((strlen(ct) + 1) * sizeof(char));
	cnt->lt = (char*)malloc((strlen(ct) + 1) * sizeof(char));
	strcpy(cnt->ri, ri);
	strcpy(cnt->pi, pi);
	strcpy(cnt->et, et);
	strcpy(cnt->ct, ct);
	strcpy(cnt->lt, ct);
	
	cnt->ty = TY_CNT;
	cnt->st = 0;
	cnt->cni = 0;
	cnt->cbs = 0;
	
	free(ct); ct = NULL;
	free(et); et = NULL;
	free(ri); ri = NULL;
}

void init_cin(CIN* cin, char *pi) {
	char *ct = get_local_time(0);
	char *et = get_local_time(EXPIRE_TIME);
	char *ri = resource_identifier(TY_CIN, ct);
	
	cin->rn = (char*)malloc((strlen(ri) + 1) * sizeof(char));
	cin->ri = (char*)malloc((strlen(ri) + 1) * sizeof(char));
	cin->pi = (char*)malloc((strlen(pi) + 1) * sizeof(char));
	cin->et = (char*)malloc((strlen(et) + 1) * sizeof(char));
	cin->ct = (char*)malloc((strlen(ct) + 1) * sizeof(char));
	cin->lt = (char*)malloc((strlen(ct) + 1) * sizeof(char));
	strcpy(cin->rn, ri);
	strcpy(cin->ri, ri);
	strcpy(cin->pi, pi);
	strcpy(cin->et, et);
	strcpy(cin->ct, ct);
	strcpy(cin->lt, ct);
	
	cin->ty = TY_CIN;
	cin->st = 0;
	cin->cs = strlen(cin->con);
	
	free(ct); ct = NULL;
	free(et); et = NULL;
	free(ri); ri = NULL;
}

void init_sub(Sub* sub, char *pi, char *uri) {
	char *ct = get_local_time(0);
	char *et = get_local_time(EXPIRE_TIME);
	char *ri = resource_identifier(TY_SUB, ct);

	if(!sub->rn) {
		sub->rn = (char*)malloc((strlen(ri) + 1) * sizeof(char));
		strcpy(sub->rn, ri);
	}

	if(!sub->net) {
		sub->net = (char*)malloc(2*sizeof(char));
		strcpy(sub->net,"1");
	}

	sub->ri = (char*)malloc((strlen(ri) + 1) * sizeof(char));
	sub->pi = (char*)malloc((strlen(pi) + 1) * sizeof(char));
	sub->et = (char*)malloc((strlen(et) + 1) * sizeof(char));
	sub->ct = (char*)malloc((strlen(ct) + 1) * sizeof(char));
	sub->lt = (char*)malloc((strlen(ct) + 1) * sizeof(char));
	sub->sur = (char *)malloc((strlen(uri) + strlen(sub->rn) + 2) * sizeof(char));

	sprintf(sub->sur, "/%s/%s",uri,sub->rn);
	strcpy(sub->ri, ri);
	strcpy(sub->pi, pi);
	strcpy(sub->et, et);
	strcpy(sub->ct, ct);
	strcpy(sub->lt, ct);
	sub->ty = TY_SUB;
	sub->nct = 0;

	free(ct); ct = NULL;
	free(et); et = NULL;
	free(ri); ri = NULL;
}

void init_acp(ACP* acp, char *pi) {
	char *ct = get_local_time(0);
	char *et = get_local_time(EXPIRE_TIME);
	char *ri = resource_identifier(TY_ACP, ct);

	if(!acp->rn) {
		acp->rn = (char*)malloc((strlen(ri) + 1) * sizeof(char));
		strcpy(acp->rn, ri);
	}
	
	acp->ri = (char*)malloc((strlen(ri) + 1) * sizeof(char));
	acp->pi = (char*)malloc((strlen(pi) + 1) * sizeof(char));
	acp->et = (char*)malloc((strlen(et) + 1) * sizeof(char));
	acp->ct = (char*)malloc((strlen(ct) + 1) * sizeof(char));
	acp->lt = (char*)malloc((strlen(ct) + 1) * sizeof(char));
	strcpy(acp->ri, ri);
	strcpy(acp->pi, pi);
	strcpy(acp->et, et);
	strcpy(acp->ct, ct);
	strcpy(acp->lt, ct);
	
	acp->ty = TY_ACP;
	
	free(ct); ct = NULL;
	free(et); et = NULL;
	free(ri); ri = NULL;
}

void set_ae_update(cJSON *m2m_ae, AE* after) {
	cJSON *rr = cJSON_GetObjectItemCaseSensitive(m2m_ae, "rr");
	cJSON *lbl = cJSON_GetObjectItem(m2m_ae, "lbl");
	cJSON *srv = cJSON_GetObjectItem(m2m_ae, "srv");

	if(rr) {
		if(cJSON_IsTrue(rr)) {
			after->rr = true;
		} else {
			after->rr = false;
		}
	}

	if(lbl) {
		if(after->lbl) free(after->lbl);
		after->lbl = cjson_list_item_to_string(lbl);
	}

	if(srv) {
		if(after->srv) free(after->srv);
		after->srv = cjson_list_item_to_string(srv);
	}

	if(after->lt) free(after->lt);
	after->lt = get_local_time(0);
}

void set_cnt_update(cJSON *m2m_cnt, CNT* after) {
	cJSON *lbl = cJSON_GetObjectItem(m2m_cnt, "lbl");
	cJSON *acpi = cJSON_GetObjectItem(m2m_cnt, "acpi");
	cJSON *mni = cJSON_GetObjectItem(m2m_cnt, "mni");
	cJSON *mbs = cJSON_GetObjectItem(m2m_cnt, "mbs");

	if(acpi) {
		if(after->acpi) free(after->acpi);
		after->acpi = cjson_list_item_to_string(acpi);
	}
	
	if(lbl) {
		if(after->lbl) free(after->lbl);
		after->lbl = cjson_list_item_to_string(lbl);
	}

	if(mni) {
		after->mni = mni->valueint;
	}

	if(mbs) {
		after->mbs = mbs->valueint;
	}

	if(after->lt) free(after->lt);
	after->lt = get_local_time(0);
}

char *resource_identifier(ObjectType ty, char *ct) {
	char *ri = (char *)calloc(24, sizeof(char));

	switch(ty) {
		case TY_CNT : strcpy(ri, "3-"); break;
		case TY_CIN : strcpy(ri, "4-"); break;
		case TY_SUB : strcpy(ri, "23-"); break;
		case TY_ACP : strcpy(ri, "1-"); break;
	}

	struct timespec specific_time;
    int millsec;

	char buf[32] = "\0";

    clock_gettime(CLOCK_REALTIME, &specific_time);
    millsec = floor(specific_time.tv_nsec/1.0e6);

	sprintf(buf, "%s%03d",ct, millsec);

	strcat(ri, buf);

	return ri;
}

void set_rtnode_update(RTNode *rtnode, void *after) {
	ObjectType ty = rtnode->ty;
	if(rtnode->acpi) {free(rtnode->acpi); rtnode->acpi = NULL;}
	if(rtnode->nu) {free(rtnode->nu); rtnode->nu = NULL;}
	if(rtnode->pv_acor && rtnode->pv_acop) {
		free(rtnode->pv_acor); rtnode->pv_acor = NULL; 
		free(rtnode->pv_acop); rtnode->pv_acop = NULL;
	}
	if(rtnode->pvs_acor && rtnode->pvs_acop) {
		free(rtnode->pvs_acor); rtnode->pvs_acor = NULL;
		free(rtnode->pvs_acop); rtnode->pvs_acop = NULL;
	}
	
	switch(ty) {
	case TY_CNT:
		CNT *cnt = (CNT*)after;
		if(cnt->acpi) {
			rtnode->acpi = (char*)malloc((strlen(cnt->acpi) + 1)*sizeof(char));
			strcpy(rtnode->acpi, cnt->acpi);
		}
		break;

	case TY_SUB:
		Sub *sub = (Sub*)after;
		rtnode->net = net_to_bit(sub->net);
		if(sub->nu) {
			rtnode->nu = (char*)malloc((strlen(sub->nu) + 1)*sizeof(char));
			strcpy(rtnode->nu, sub->nu);
		}
		break;

	case TY_ACP:
		ACP *acp = (ACP*)after;
		if(acp->pv_acor && acp->pv_acop) {
			rtnode->pv_acor = (char*)malloc((strlen(acp->pv_acor) + 1)*sizeof(char));
			rtnode->pv_acop = (char*)malloc((strlen(acp->pv_acop) + 1)*sizeof(char));
			strcpy(rtnode->pv_acor, acp->pv_acor);
			strcpy(rtnode->pv_acop, acp->pv_acop);
		}
		if(acp->pvs_acor && acp->pvs_acop) {
			rtnode->pvs_acor = (char*)malloc((strlen(acp->pvs_acor) + 1)*sizeof(char));
			rtnode->pvs_acop = (char*)malloc((strlen(acp->pvs_acop) + 1)*sizeof(char));
			strcpy(rtnode->pvs_acor, acp->pvs_acor);
			strcpy(rtnode->pvs_acop, acp->pvs_acop);
		}
		break;
	}
}

void update_cnt_cin(RTNode *cnt_rtnode, RTNode *cin_rtnode, int sign) {
	CNT *cnt = db_get_cnt(cnt_rtnode->ri);
	cnt->cni += sign;
	cnt->cbs += sign*(cin_rtnode->cs);
	delete_cin_under_cnt_mni_mbs(cnt);	
	cnt_rtnode->cni = cnt->cni;
	cnt_rtnode->cbs = cnt->cbs;
	cnt->st++;
	db_delete_onem2m_resource(cnt_rtnode->ri);
	db_store_cnt(cnt);
	free_cnt(cnt);
}

void delete_cin_under_cnt_mni_mbs(CNT *cnt) {
	if(cnt->cni <= cnt->mni && cnt->cbs <= cnt->mbs) return;

	RTNode *head = db_get_cin_rtnode_list_by_pi(cnt->ri);
	RTNode *right;

	while((cnt->mni >= 0 && cnt->cni > cnt->mni) || (cnt->mbs >= 0 && cnt->cbs > cnt->mbs)) {
		if(head) {
			right = head->sibling_right;
			db_delete_onem2m_resource(head->ri);
			cnt->cbs -= head->cs;
			cnt->cni--;
			free_rtnode(head);
			head = right;
		} else {
			break;
		}
	}

	if(head) free_rtnode_list(head);
}

void too_large_content_size_error(oneM2MPrimitive *o2pt) {
	logger("O2M", LOG_LEVEL_ERROR, "Too large content size");
	set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"too large content size\"}");
	o2pt->rsc = RSC_NOT_ACCEPTABLE;
	respond_to_client(o2pt, 400);
}

/*

void tree_viewer_api(RTNode *node) {
	fprintf(stderr,"\x1b[43mTree Viewer API\x1b[0m\n");
	char arr_viewer_data[MAX_TREE_VIEWER_SIZE] = "[";
	char *viewer_data = arr_viewer_data;
	
	RTNode *p = node;
	while(p = p->parent) {
		char *json = node_to_json(p);
		strcat(viewer_data,",");
		strcat(viewer_data,json);
		free(json); json = NULL;
	}
	
	int cinSize = 1;
	
	char *la = strstr(qs,"la=");
	if(la) cinSize = atoi(la+3);
	
	fprintf(stderr,"Latest CIN Size : %d\n", cinSize);
	
	tree_viewer_data(node, &viewer_data, cinSize);
	strcat(viewer_data,"]\0");
	char res[MAX_TREE_VIEWER_SIZE] = "";
	int index = 0;
	
	for(int i=0; i<MAX_TREE_VIEWER_SIZE; i++) {
		if(i == 1) continue;
		if(is_json_valid_char(viewer_data[i])) {
			res[index++] = viewer_data[i];
		}
	}
	
	fprintf(stderr,"Content-Size : %ld\n",strlen(res));

	respond_to_client(200, res, 2000);
}

void tree_viewer_data(RTNode *node, char **viewer_data, int cin_size) {
	char *json = node_to_json(node);

	strcat(*viewer_data, ",");
	strcat(*viewer_data, json);
	free(json); json = NULL;

	RTNode *child = node->child;
	while(child) {
		tree_viewer_data(child, viewer_data, cin_size);
		child = child->sibling_right;
	}

	if(node->ty != TY_SUB && node->ty != TY_ACP) {
		RTNode *cin_list_head = db_get_cin_rtnode_list_by_pi(node->ri);

		if(cin_list_head) cin_list_head = latest_cin_list(cin_list_head, cin_size);

		RTNode *p = cin_list_head;

		while(p) {
			json = node_to_json(p);
			strcat(*viewer_data, ",");
			strcat(*viewer_data, json);
			free(json); json = NULL;
			p = p->sibling_right;		
		}
		free_rtnode_list(cin_list_head);
	}
}

RTNode *latest_cin_list(RTNode* cinList, int num) {
	RTNode *head, *tail;
	head = tail = cinList;
	int cnt = 1;
	
	while(tail->sibling_right) {
		tail = tail->sibling_right;
		cnt++;
	}
	
	for(int i=0; i < cnt-num; i++) {
		head = head->sibling_right;
		free_rtnode(head->sibling_left); head->sibling_left = NULL;
	}
	
	return head;
}

void set_sub_update(Sub* after) {
	char *rn = get_json_value_char("rn", payload);
	char *nu = NULL;
	char *net = NULL;

	if(strstr(payload,"\"nu\"") != NULL) {
		nu = get_json_value_list("nu", payload);
		if(!strcmp(nu, "\0")) {
			free(nu); nu = after->nu = NULL;
		}
	}
	if(strstr(payload,"\"enc\"") != NULL) {
		if(strstr(payload, "\"net\"") != NULL) {
			net = get_json_value_list("enc-net", payload);
			if(!strcmp(net, "\0")) {
				free(net); net = after->net = NULL;
			}
		}
	}

	if(rn) {
		free(after->rn);
		after->rn = (char*)malloc((strlen(rn) + 1) * sizeof(char));
		strcpy(after->rn, rn);
	}

	if(nu) {
		if(after->nu) free(after->nu);
		after->nu = (char*)malloc((strlen(nu) + 1) * sizeof(char));
		strcpy(after->nu, nu);
	}

	if(net) {
		if(after->net) free(after->net);
		after->net = (char*)malloc((strlen(net) + 1) * sizeof(char));
		strcpy(after->net, net);
	}

	if(after->lt) free(after->lt);
	after->lt = get_local_time(0);
}

void set_acp_update(ACP* after) {
	char *rn = get_json_value_char("rn", payload);
	char *pv_acor = NULL;
	char *pv_acop = NULL;
	char *pvs_acor = NULL;
	char *pvs_acop = NULL;

	if(strstr(payload, "\"pv\"")) {
		if(strstr(payload, "\"acr\"")) {
			if(strstr(payload, "\"acor\"") && strstr(payload, "acop")) {
				pv_acor = get_json_value_list("pv-acr-acor", payload); 
				pv_acop = get_json_value_list("pv-acr-acop", payload);
				if(!strcmp(pv_acor, "\0") || !strcmp(pv_acop, "\0")) {
					free(pv_acor); pv_acor = after->pv_acor = NULL;
					free(pv_acop); pv_acop = after->pv_acop = NULL;
				}
			}
		}
	}

	if(strstr(payload, "\"pvs\"")) {
		if(strstr(payload, "\"acr\"")) {
			if(strstr(payload, "\"acor\"") && strstr(payload, "\"acop\"")) {
				pvs_acor = get_json_value_list("pvs-acr-acor", payload);
				pvs_acop = get_json_value_list("pvs-acr-acop", payload);
				if(!strcmp(pvs_acor, "\0") || !strcmp(pvs_acop, "\0")) {
					free(pvs_acor); pvs_acor = after->pvs_acor = NULL;
					free(pvs_acop); pvs_acop = after->pvs_acop = NULL;
				}
			}
		}
	}

	if(rn) {
		free(after->rn);
		after->rn = (char*)malloc((strlen(rn) + 1) * sizeof(char));
		strcpy(after->rn, rn);
	}

	if(pv_acor && pv_acop) {
		if(after->pv_acor) free(after->pv_acor);
		if(after->pv_acop) free(after->pv_acop);
		after->pv_acor = (char*)malloc((strlen(pv_acor) + 1) * sizeof(char));
		after->pv_acop = (char*)malloc((strlen(pv_acop) + 1) * sizeof(char));
		strcpy(after->pv_acor, pv_acor);
		strcpy(after->pv_acop, pv_acop);
	}

	if(pvs_acor && pvs_acop) {
		if(after->pvs_acor) free(after->pvs_acor);
		if(after->pvs_acop) free(after->pvs_acop);
		after->pvs_acor = (char*)malloc((strlen(pvs_acor) + 1) * sizeof(char));
		after->pvs_acop = (char*)malloc((strlen(pvs_acop) + 1) * sizeof(char));
		strcpy(after->pvs_acor, pvs_acor);
		strcpy(after->pvs_acop, pvs_acop);
	}

	if(after->lt) free(after->lt);
	after->lt = get_local_time(0);
}

void notify_onem2m_resource(RTNode *node, char *response_payload, NET net) {
	remove_invalid_char_json(response_payload);
	while(node) {
		if(node->ty == TY_SUB && (net & node->net) == net) {
			if(!node->uri) set_node_uri(node);
			char *notify_json = notification_to_json(node->uri, (int)log2((double)net ) + 1, response_payload);
			int result = send_http_packet(node->nu, notify_json);
			free(notify_json); notify_json = NULL;
		}
		node = node->sibling_right;
	}
}

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

size_t write_data(void *ptr, size_t size, size_t nmemb, struct url_data *data) {
    size_t index = data->size;
    size_t n = (size * nmemb);
    char* tmp;

    data->size += (size * nmemb);
    tmp = realloc(data->data, data->size + 1); // +1 for '\0' 

    if(tmp) {
        data->data = tmp;
    } else {
        if(data->data) {
            free(data->data);
        }
        fprintf(stderr, "Failed to allocate memory.\n");
        return 0;
    }

    memcpy((data->data + index), ptr, n);
    data->data[data->size] = '\0';

    return size * nmemb;
}

int send_http_packet(char* target, char *post_data) {
    CURL *curl;
    struct url_data data;

    data.size = 0;
    data.data = malloc(4096 * sizeof(char)); // reasonable size initial buffer

    if(NULL == data.data) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return EXIT_FAILURE;
    }
	if(post_data) remove_invalid_char_json(post_data);

	char nu[MAX_PROPERTY_SIZE];
	strcpy(nu, target);

	target = strtok(nu, ",");

    CURLcode res;

    curl = curl_easy_init();

    if (curl) {
		if(post_data) curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2);
		
		while(target) {
			data.data[0] = '\0';
			curl_easy_setopt(curl, CURLOPT_URL, target);
			res = curl_easy_perform(curl);
			
			if(res != CURLE_OK) {
				fprintf(stderr, "curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));
			}
			target = strtok(NULL, ",");
		}
		
        curl_easy_cleanup(curl);
    }

	if(data.data) free(data.data);

    return EXIT_SUCCESS;
}

int get_acop(RTNode *node) {
	char *request_header_origin = request_header("X-M2M-Origin");
	char origin[128];
	int acop = 0;
	
	if(request_header_origin) {
		strcpy(origin, request_header_origin);
	} else {
		strcpy(origin, "all");
	}

	if(node->ty == TY_ACP) {
		acop = (acop | get_acop_origin(origin, node, 1));
		acop = (acop | get_acop_origin("all", node, 1));
		return acop;
	}

	if(!node->acpi) return ALL_ACOP;

	RTNode *cb = node;
	while(cb->parent) cb = cb->parent;
	
	int uri_cnt = 0;
	char arr_acp_uri[512][1024] = {"\0", }, arr_acpi[MAX_PROPERTY_SIZE] = "\0";
	char *acp_uri = NULL;

	if(node->acpi)  {
		strcpy(arr_acpi, node->acpi);

		acp_uri = strtok(arr_acpi, ",");

		while(acp_uri) { 
			strcpy(arr_acp_uri[uri_cnt++],acp_uri);
			acp_uri = strtok(NULL, ",");
		}
	}

	for(int i=0; i<uri_cnt; i++) {
		RTNode *acp = find_rtnode_by_uri(cb, arr_acp_uri[i]);

		if(acp) {
			acop = (acop | get_acop_origin(origin, acp, 0));
			acop = (acop | get_acop_origin("all", acp, 0));
		}
	}

	return acop;
}

int get_acop_origin(char *origin, RTNode *acp, int flag) {
	if(!origin) return 0;

	int ret_acop = 0, cnt = 0;
	char *acor, *acop, arr_acor[1024], arr_acop[1024];

	if(flag) {
		if(!acp->pvs_acor) {
			fprintf(stderr,"pvs_acor is NULL\n"); 
			return 0;
		}
		strcpy(arr_acor, acp->pvs_acor);
		strcpy(arr_acop, acp->pvs_acop);
	} else {
		if(!acp->pv_acor) {
			fprintf(stderr,"pv_acor is NULL\n"); 
			return 0;
		}
		strcpy(arr_acor, acp->pv_acor);
		strcpy(arr_acop, acp->pv_acop);
	}

	acor = strtok(arr_acor, ",");

	while(acor) {
		if(!strcmp(acor, origin)) break;
		acor = strtok(NULL, ",");
		cnt++;
	}

	acop = strtok(arr_acop, ",");

	for(int i=0; i<cnt; i++) acop = strtok(NULL,",");

	if(acop) ret_acop = (ret_acop | atoi(acop));

	return ret_acop;
}

int get_value_querystring_int(char *key) {
	char *value = strstr(qs, key);
	if(!value) return -1;

	value = value + strlen(key) + 1;

	return atoi(value);
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

int check_origin() {
	if(request_header("X-M2M-Origin")) {
		return 1;
	} else {
		HTTP_403;
		printf("{\"m2m:dbg\": \"DB store fail\"}");
		return 0;
	}
}
*/