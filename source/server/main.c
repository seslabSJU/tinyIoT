#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <limits.h>
#include "onem2m.h"
#include "jsonparse.h"
#include "berkeleyDB.h"
#include "httpd.h"
#include "cJSON.h"
#include "config.h"
#include "onem2mTypes.h"
#include "mqttClient.h"

ResourceTree *rt;
void *mqtt_serve();

int main(int c, char **v) {
	pthread_t mqtt;
	init_server();
	
	#ifdef ENABLE_MQTT
	int mqtt_thread_id;
	mqtt_thread_id = pthread_create(&mqtt, NULL, mqtt_serve, "mqtt Client");
	if(mqtt_thread_id < 0){
		fprintf(stderr, "MQTT thread create error\n");
		return 0;
	}
	#endif

	serve_forever(SERVER_PORT); // main oneM2M operation logic in void route()    

	return 0;
}

void handle_http_request() {
	oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(1, sizeof(oneM2MPrimitive));
	char *header;

	if(payload) {
		o2pt->pc = (char *)malloc((strlen(payload) + 1) * sizeof(char));
		strcpy(o2pt->pc, payload);
		o2pt->cjson_pc = cJSON_Parse(o2pt->pc);
		cJSON_GetObjectItem(o2pt->cjson_pc, "m2m:ae");
	} 

	if((header = request_header("X-M2M-Origin"))) {
		o2pt->fr = (char *)malloc((strlen(header) + 1) * sizeof(char));
		strcpy(o2pt->fr, header);
	} 

	if((header = request_header("X-M2M-RI"))) {
		o2pt->rqi = (char *)malloc((strlen(header) + 1) * sizeof(char));
		strcpy(o2pt->rqi, header);
	} 

	if((header = request_header("X-M2M-RVI"))) {
		o2pt->rvi = (char *)malloc((strlen(header) + 1) * sizeof(char));
		strcpy(o2pt->rvi, header);
	} 

	if(uri) {
		o2pt->to = (char *)malloc((strlen(uri) + 1) * sizeof(char));
		strcpy(o2pt->to, uri+1);
	} 

	o2pt->op = http_parse_operation();
	if(o2pt->op == OP_CREATE) o2pt->ty = http_parse_object_type();
	o2pt->prot = PROT_HTTP;

	route(o2pt);
}

void route(oneM2MPrimitive *o2pt) {
    double start;

    start = (double)clock() / CLOCKS_PER_SEC; // runtime check - start
	RTNode* target_rtnode = parse_uri(o2pt, rt->cb);
	int e = result_parse_uri(o2pt, target_rtnode);

	if(e != -1) e = check_payload_size(o2pt);
	if(e == -1) {
		log_runtime(start);
		return;
	}

	switch(o2pt->op) {
	
	case OP_CREATE:	
		create_onem2m_resource(o2pt, target_rtnode); break;
	
	case OP_RETRIEVE:
		retrieve_onem2m_resource(o2pt, target_rtnode); break;
		
	case OP_UPDATE: 
		update_onem2m_resource(o2pt, target_rtnode); break;
		
	case OP_DELETE:
		delete_onem2m_resource(o2pt, target_rtnode); break;

	//case OP_VIEWER:
		//tree_viewer_api(target_rtnode); break;
	
	//case OP_OPTIONS:
		//respond_to_client(200, "{\"m2m:dbg\": \"response about options method\"}", "2000");
		//break;
	
	default:
		logger("MAIN", LOG_LEVEL_ERROR, "Internal server error");
		set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"internal server error\"}");
		o2pt->rsc = RSC_INTERNAL_SERVER_ERROR;
		respond_to_client(o2pt, 500);
	}
	if(target_rtnode->ty == TY_CIN) free_rtnode(target_rtnode);

	log_runtime(start);

}

void create_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *parent_rtnode) {
	int e = check_resource_type_invalid(o2pt);
	if(e != -1) e = check_payload_empty(o2pt);
	if(e != -1) e = check_payload_format(o2pt);
	if(e != -1) e = check_resource_type_equal(o2pt);
	if(e != -1) e = check_privilege(o2pt, parent_rtnode, ACOP_CREATE);
	if(e != -1) e = check_rn_duplicate(o2pt, parent_rtnode);
	if(e == -1) return;

	switch(o2pt->ty) {	
	case TY_AE :
		logger("MAIN", LOG_LEVEL_INFO, "Create AE");
		create_ae(o2pt, parent_rtnode);
		break;	

	case TY_CNT :
		logger("MAIN", LOG_LEVEL_INFO, "Create CNT");
		create_cnt(o2pt, parent_rtnode);
		break;
		
	case TY_CIN :
		logger("MAIN", LOG_LEVEL_INFO, "Create CIN");
		create_cin(o2pt, parent_rtnode);
		break;

	case TY_SUB :
		logger("MAIN", LOG_LEVEL_INFO, "Create SUB");
		create_sub(o2pt, parent_rtnode);
		break;
	
	case TY_ACP :
		logger("MAIN", LOG_LEVEL_INFO, "Create ACP");
		create_acp(o2pt, parent_rtnode);
		break;

	case TY_NONE :
		logger("MAIN", LOG_LEVEL_ERROR, "Resource type is invalid");
		set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"resource type error\"}");
		o2pt->rsc = RSC_BAD_REQUEST;
		respond_to_client(o2pt, 400);
	}	
}

void retrieve_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode) {
	int e = check_privilege(o2pt, target_rtnode, ACOP_RETRIEVE);

	if(e == -1) return;
	/*
	int fu = get_value_querystring_int("fu");

	if(fu == 1) {
		fprintf(stderr,"\x1b[43mRetrieve FilterCriteria\x1b[0m\n");
		retrieve_object_filtercriteria(pnode);
		return;
	}
	*/

	switch(target_rtnode->ty) {
		
	case TY_CSE :
		logger("MAIN", LOG_LEVEL_INFO, "Retrieve CSE");
        retrieve_cse(o2pt, target_rtnode);
      	break;
	
	case TY_AE : 
		logger("MAIN", LOG_LEVEL_INFO, "Retrieve AE");
		retrieve_ae(o2pt, target_rtnode);	
		break;	
			
	case TY_CNT :
		logger("MAIN", LOG_LEVEL_INFO, "Retrieve CNT");
		retrieve_cnt(o2pt, target_rtnode);			
		break;
			
	case TY_CIN :
		logger("MAIN", LOG_LEVEL_INFO, "Retrieve CIN");
		retrieve_cin(o2pt, target_rtnode);			
		break;

	case TY_SUB :
		logger("MAIN", LOG_LEVEL_INFO, "Retrieve SUB");
		retrieve_sub(o2pt, target_rtnode);			
		break;

	case TY_ACP :
		logger("MAIN", LOG_LEVEL_INFO, "Retrieve ACP");
		retrieve_acp(o2pt, target_rtnode);			
		break;
	}	
}

void update_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode) {
	o2pt->ty = target_rtnode->ty;
	int e = check_payload_empty(o2pt);
	if(e != -1) e = check_payload_format(o2pt);
	ObjectType ty = parse_object_type_cjson(o2pt->cjson_pc);
	if(e != -1) e = check_resource_type_equal(o2pt);
	if(e != -1) e = check_privilege(o2pt, target_rtnode, ACOP_UPDATE);
	if(e != -1) e = check_rn_duplicate(o2pt, target_rtnode->parent);
	if(e == -1) return;
	
	switch(ty) {
	case TY_AE :
		logger("MAIN", LOG_LEVEL_INFO, "Update AE");
		update_ae(o2pt, target_rtnode);
		break;

	case TY_CNT :
		logger("MAIN", LOG_LEVEL_INFO, "Update CNT");
		update_cnt(o2pt, target_rtnode);
		break;

	// case TY_SUB :
	//	logger("MAIN", LOG_LEVEL_INFO, "Update SUB");
	// 	update_sub(pnode);
	// 	break;
	
	// case TY_ACP :
	// 	logger("MAIN", LOG_LEVEL_INFO, "Update ACP");
	// 	update_acp(pnode);
	// 	break;

	default :
		logger("MAIN", LOG_LEVEL_ERROR, "Resource type does not support Update");
		set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"`Update` operation is unsupported\"}");
		o2pt->rsc = RSC_OPERATION_NOT_ALLOWED;
		respond_to_client(o2pt, 400);
	}
}

void delete_onem2m_resource(oneM2MPrimitive *o2pt, RTNode* target_rtnode) {
	logger("MAIN", LOG_LEVEL_INFO, "Delete oneM2M resource");
	if(target_rtnode->ty == TY_AE || target_rtnode->ty == TY_CNT) {
		if(check_privilege(o2pt, target_rtnode, ACOP_DELETE) == -1) {
			return;
		}
	}
	if(target_rtnode->ty == TY_CSE) {
		set_o2pt_pc(o2pt,  "{\"m2m:dbg\": \"CSE can not be deleted\"}");
		o2pt->rsc = RSC_OPERATION_NOT_ALLOWED;
		respond_to_client(o2pt, 403);
		return;
	}
	delete_rtnode_and_db_data(target_rtnode,1);
	target_rtnode = NULL;
	set_o2pt_pc(o2pt,"{\"m2m:dbg\": \"resource is deleted successfully\"}");
	o2pt->rsc = RSC_DELETED;
	respond_to_client(o2pt, 200);
}

void update_ae(oneM2MPrimitive *o2pt, RTNode *target_rtnode) {
	char invalid_key[][4] = {"ty", "pi", "ri", "rn"};
	cJSON *m2m_ae = cJSON_GetObjectItem(o2pt->cjson_pc, "m2m:ae");
	int invalid_key_size = sizeof(invalid_key)/(4*sizeof(char));
	for(int i=0; i<invalid_key_size; i++) {
		if(cJSON_GetObjectItem(m2m_ae, invalid_key[i])) {
			logger("MAIN", LOG_LEVEL_ERROR, "Unsupported attribute on update");
			set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"unsupported attribute on update\"}");
			o2pt->rsc = RSC_BAD_REQUEST;
			respond_to_client(o2pt, 200);
			return;
		}
	}

	AE* after = db_get_ae(target_rtnode->ri);
	int result;

	set_ae_update(m2m_ae, after);
	set_rtnode_update(target_rtnode, after);
	result = db_delete_onem2m_resource(after->ri);
	result = db_store_ae(after);
	
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = ae_to_json(after);
	o2pt->rsc = RSC_UPDATED;
	respond_to_client(o2pt,200);
	//notify_onem2m_resource(pnode->child, response_payload, NOTIFICATION_EVENT_1);
	free_ae(after); after = NULL;
}

void update_cnt(oneM2MPrimitive *o2pt, RTNode *target_rtnode) {
	char invalid_key[][4] = {"ty", "pi", "ri", "rn"};
	cJSON *m2m_cnt = cJSON_GetObjectItem(o2pt->cjson_pc, "m2m:cnt");
	int invalid_key_size = sizeof(invalid_key)/(4*sizeof(char));
	for(int i=0; i<invalid_key_size; i++) {
		if(cJSON_GetObjectItem(m2m_cnt, invalid_key[i])) {
			logger("MAIN", LOG_LEVEL_ERROR, "Unsupported attribute on update");
			set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"unsupported attribute on update\"}");
			o2pt->rsc = 4000;
			respond_to_client(o2pt, 200);
			return;
		}
	}

	CNT* after = db_get_cnt(target_rtnode->ri);
	int result;

	set_cnt_update(m2m_cnt, after);
	if(after->mbs != INT_MIN && after->mbs < 0) {
		mni_mbs_invalid(o2pt, "mbs"); free(after); after = NULL;
		return;
	}
	if(after->mni != INT_MIN && after->mni < 0) {
		mni_mbs_invalid(o2pt, "mni"); free(after); after = NULL;
		return;
	}
	target_rtnode->mbs = after->mbs;
	target_rtnode->mni = after->mni;
	delete_cin_under_cnt_mni_mbs(after);
	after->st++;
	result = db_delete_onem2m_resource(after->ri);
	result = db_store_cnt(after);
	
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = cnt_to_json(after);
	o2pt->rsc = 2004;
	respond_to_client(o2pt, 200);
	//notify_onem2m_resource(pnode->child, response_payload, NOTIFICATION_EVENT_1);
	free_cnt(after); after = NULL;
}

void log_runtime(double start) {
	double end = (((double)clock()) / CLOCKS_PER_SEC); // runtime check - end
	logger("MAIN", LOG_LEVEL_DEBUG, "Run time : %lf", end-start);
}

void init_server() {
	rt = (ResourceTree *)malloc(sizeof(rt));
	
	CSE *cse;

	if(access("./RESOURCE.db", 0) == -1) {
		cse = (CSE*)malloc(sizeof(CSE));
		init_cse(cse);
		db_store_cse(cse);
	} else {
		cse = db_get_cse(CSE_BASE_RI);
	}
	
	rt->cb = create_rtnode(cse, TY_CSE);
	free_cse(cse); cse = NULL;

 	restruct_resource_tree();
}

void restruct_resource_tree(){
	RTNode *rtnode_list = (RTNode *)calloc(1,sizeof(RTNode));
	RTNode *tail = rtnode_list;
	
	if(access("./RESOURCE.db", 0) != -1) {
		RTNode* ae_list = db_get_all_ae();
		tail->sibling_right = ae_list;
		if(ae_list) ae_list->sibling_left = tail;
		while(tail->sibling_right) tail = tail->sibling_right;

		RTNode* cnt_list = db_get_all_cnt();
		tail->sibling_right = cnt_list;
		if(cnt_list) cnt_list->sibling_left = tail;
		while(tail->sibling_right) tail = tail->sibling_right;
	} else {
		logger("MAIN", LOG_LEVEL_DEBUG, "RESOURCE.db does not exist");
	}
	
	if(access("./SUB.db", 0) != -1) {
		RTNode* sub_list = db_get_all_sub();
		tail->sibling_right = sub_list;
		if(sub_list) sub_list->sibling_left = tail;
		while(tail->sibling_right) tail = tail->sibling_right;
	} else {
		logger("MAIN", LOG_LEVEL_DEBUG, "SUB.db does not exist");
	}

	if(access("./ACP.db", 0) != -1) {
		RTNode* acp_list = db_get_all_acp();
		tail->sibling_right = acp_list;
		if(acp_list) acp_list->sibling_left = tail;
		while(tail->sibling_right) tail = tail->sibling_right;
	} else {
		logger("MAIN", LOG_LEVEL_DEBUG, "ACP.db does not exist");
	}
	
	RTNode *prtnode = rtnode_list;
	rtnode_list = rtnode_list->sibling_right;
	if(rtnode_list) rtnode_list->sibling_left = NULL;
	free_rtnode(prtnode);
	
	if(rtnode_list) restruct_resource_tree_child(rt->cb, rtnode_list);
}

RTNode* restruct_resource_tree_child(RTNode *parent_rtnode, RTNode *list) {
	RTNode *rtnode = list;
	
	while(rtnode) {
		RTNode *right = rtnode->sibling_right;

		if(!strcmp(parent_rtnode->ri, rtnode->pi)) {
			RTNode *left = rtnode->sibling_left;
			
			if(!left) {
				list = right;
			} else {
				left->sibling_right = right;
			}
			
			if(right) right->sibling_left = left;
			rtnode->sibling_left = rtnode->sibling_right = NULL;
			add_child_resource_tree(parent_rtnode, rtnode);
		}
		rtnode = right;
	}
	RTNode *child = parent_rtnode->child;
	
	while(child) {
		list = restruct_resource_tree_child(child, list);
		child = child->sibling_right;
	}
	
	return list;
}

void *mqtt_serve(){
	int result = 0;
	result = mqtt_ser();
}

int check_payload_size(oneM2MPrimitive *o2pt) {
	if(o2pt->pc && strlen(o2pt->pc) > MAX_PAYLOAD_SIZE) {
		logger("MAIN", LOG_LEVEL_ERROR, "Request payload too large");
		set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"payload is too large\"}");
		o2pt->rsc = RSC_BAD_REQUEST;
		respond_to_client(o2pt, 413);
		return -1;
	}
	return 0;
}

int result_parse_uri(oneM2MPrimitive *o2pt, RTNode *rtnode) {
	if(!rtnode) {
		logger("MAIN", LOG_LEVEL_ERROR, "URI is invalid");
		set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"URI is invalid\"}");
		o2pt->rsc = RSC_NOT_FOUND;
		respond_to_client(o2pt, 404);
		return -1;
	} else {
		
		return 0;
	} 
}

int check_privilege(oneM2MPrimitive *o2pt, RTNode *rtnode, ACOP acop) {
	bool deny = false;

	RTNode *parent_rtnode = rtnode;

	while(parent_rtnode && parent_rtnode->ty != TY_AE) {
		parent_rtnode = parent_rtnode->parent;
	}

	if(!o2pt->fr) {
		if(!(o2pt->op == OP_CREATE && o2pt->ty == TY_AE)) {
			deny = true;
		}
	} else if(!strcmp(o2pt->fr, "CAdmin")) {
		deny = false;
	} else if((parent_rtnode && strcmp(o2pt->fr, parent_rtnode->ri))) {
		deny = true;
	}
	/*
	if(target_rtnode->ty == TY_CIN) target_rtnode = node->parent;

	if((target_rtnode->ty == TY_CNT || target_rtnode->ty == TY_ACP) && (get_acop(target_rtnode) & acop) != acop) {
		deny = true;
	}
	*/

	if(deny) {
		logger("MAIN", LOG_LEVEL_ERROR, "Originator has no privilege");
		set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"originator has no privilege.\"}");
		o2pt->rsc = RSC_ORIGINATOR_HAS_NO_PRIVILEGE;
		respond_to_client(o2pt, 403);
		return -1;
	}

	return 0;
}

int check_rn_duplicate(oneM2MPrimitive *o2pt, RTNode *rtnode) {
	if(!rtnode) return 0;
	cJSON *root = o2pt->cjson_pc;
	cJSON *resource, *rn;

	switch(o2pt->ty) {
		case TY_AE: resource = cJSON_GetObjectItem(root, "m2m:ae"); break;
		case TY_CNT: resource = cJSON_GetObjectItem(root, "m2m:cnt"); break;
		case TY_CIN: resource = cJSON_GetObjectItem(root, "m2m:cin"); break;
		case TY_SUB: resource = cJSON_GetObjectItem(root, "m2m:sub"); break;
		case TY_ACP: resource = cJSON_GetObjectItem(root, "m2m:acp"); break;
	}

	RTNode *child = rtnode->child;

	rn = cJSON_GetObjectItem(resource, "rn");
	if(rn) {
		char *resource_name = rn->valuestring;
		while(child) {
			if(!strcmp(child->rn, resource_name)) {
				logger("MAIN", LOG_LEVEL_ERROR, "Attribute `rn` is duplicated");
				set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"attribute `rn` is duplicated\"}");
				o2pt->rsc = RSC_CONFLICT;
				respond_to_client(o2pt, 209);
				return -1;
			}
			child = child->sibling_right;
		}
	}

	return 0;
}

int check_aei_duplicate(oneM2MPrimitive *o2pt, RTNode *rtnode) {
	if(!rtnode) return 0;

	char aei[1024] = {'\0', };
	if(!o2pt->fr) {
		return 0;
	} else if(o2pt->fr[0] != 'C'){
		aei[0] = 'C';
	}

	strcat(aei, o2pt->fr);

	RTNode *child = rtnode->child;

	while(child) {
		if(!strcmp(child->ri, aei)) {
			logger("MAIN", LOG_LEVEL_ERROR, "AE-ID is duplicated");
			set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"attribute `aei` is duplicated\"}");
			o2pt->rsc = RSC_ORIGINATOR_HAS_ALREADY_REGISTERD;
			respond_to_client(o2pt, 209);
			return -1;
		}
		child = child->sibling_right;
	}

	return 0;
}

int check_payload_format(oneM2MPrimitive *o2pt) {
	cJSON *cjson = o2pt->cjson_pc;
	
	if(cjson == NULL) {
		logger("MAIN", LOG_LEVEL_ERROR, "Payload format is invalid");
		set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"payload format is invalid\"}");
		o2pt->rsc = RSC_BAD_REQUEST;
		respond_to_client(o2pt, 400);
		return -1;
	}
	return 0;
}

int check_payload_empty(oneM2MPrimitive *o2pt) {
	if(!o2pt->pc) {
		logger("MAIN", LOG_LEVEL_ERROR, "Payload is empty");
		set_o2pt_pc(o2pt,  "{\"m2m:dbg\": \"payload is empty\"}");
		o2pt->rsc = RSC_INTERNAL_SERVER_ERROR;
		respond_to_client(o2pt, 500);
		return -1;
	}
	return 0;
}

int check_rn_invalid(oneM2MPrimitive *o2pt, ObjectType ty) {
	cJSON *root = o2pt->cjson_pc;
	cJSON *resource, *rn;

	switch(ty) {
		case TY_AE: resource = cJSON_GetObjectItem(root, "m2m:ae"); break;
		case TY_CNT: resource = cJSON_GetObjectItem(root, "m2m:cnt"); break;
		case TY_SUB: resource = cJSON_GetObjectItem(root, "m2m:sub"); break;
		case TY_ACP: resource = cJSON_GetObjectItem(root, "m2m:acp"); break;
	}

	rn = cJSON_GetObjectItem(resource, "rn");
	if(!rn) return 0;
	char *resource_name = rn->valuestring;
	int len_resource_name = strlen(resource_name);

	for(int i=0; i<len_resource_name; i++) {
		if(!is_rn_valid_char(resource_name[i])) {
			logger("MAIN", LOG_LEVEL_ERROR, "Resource name is invalid");
			set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"attribute `rn` is invalid\"}");
			o2pt->rsc = RSC_BAD_REQUEST;
			respond_to_client(o2pt, 406);
			return -1;
		}
	}

	return 0;
}

bool is_rn_valid_char(char c) {
	return ((48 <= c && c <=57) || (65 <= c && c <= 90) || (97 <= c && c <= 122) || (c == '_' || c == '-'));
}

int check_resource_type_equal(oneM2MPrimitive *o2pt) {	
	if(o2pt->ty != parse_object_type_cjson(o2pt->cjson_pc)) {
		logger("MAIN", LOG_LEVEL_ERROR, "Resource type is invalid");
		set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"resource type is invalid\"}");
		o2pt->rsc = RSC_BAD_REQUEST;
		respond_to_client(o2pt, 400);
		return -1;
	}
	return 0;
}

int check_resource_type_invalid(oneM2MPrimitive *o2pt) {
	if(o2pt->ty == TY_NONE) {
		logger("MAIN", LOG_LEVEL_ERROR, "Resource type is invalid");
		set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"resource type is invalid\"}");
		o2pt->rsc = RSC_BAD_REQUEST;
		respond_to_client(o2pt, 400);
		return -1;
	}
	return 0;
}

void child_type_error(oneM2MPrimitive *o2pt){
	logger("MAIN", LOG_LEVEL_ERROR, "Child type is invalid");
	set_o2pt_pc(o2pt,"{\"m2m:dbg\": \"child can not be created under the type of parent\"}");
	o2pt->rsc = RSC_INVALID_CHILD_RESOURCETYPE;
	respond_to_client(o2pt, 403);
}

void no_mandatory_error(oneM2MPrimitive *o2pt){
	logger("MAIN", LOG_LEVEL_ERROR, "Insufficient mandatory attribute(s)");
	set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"insufficient mandatory attribute(s)\"}");
	o2pt->rsc = RSC_CONTENTS_UNACCEPTABLE;
	respond_to_client(o2pt, 400);
}

void api_prefix_invalid(oneM2MPrimitive *o2pt) {
	logger("MAIN", LOG_LEVEL_ERROR, "API prefix is invalid");
	set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"attribute `api` prefix is invalid\"}");
	o2pt->rsc = RSC_BAD_REQUEST;
	respond_to_client(o2pt, 400);
}

void mni_mbs_invalid(oneM2MPrimitive *o2pt, char *attribute) {
	logger("MAIN", LOG_LEVEL_ERROR, "attribute `%s` is invalid", attribute);
	set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"attribute `%s` is invalid\"}", attribute);
	o2pt->rsc = RSC_BAD_REQUEST;
	respond_to_client(o2pt, 400);
}

void db_store_fail(oneM2MPrimitive *o2pt) {
	logger("MAIN", LOG_LEVEL_ERROR, "DB store fail");
	set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"DB store fail\"}");
	o2pt->rsc = RSC_INTERNAL_SERVER_ERROR;
	respond_to_client(o2pt, 500);
}

/*
void update_sub(RTNode *pnode) {
	Sub* after = db_get_sub(pnode->ri);
	int result;
	
	set_sub_update(after);
	set_rtnode_update(pnode, after);
	result = db_delete_sub(after->ri);
	result = db_store_sub(after);
	
	response_payload = sub_to_json(after);
	respond_to_client(200, NULL, "2004");
	free(response_payload); response_payload = NULL;
	free_sub(after); after = NULL;
}

void update_acp(RTNode *pnode) {
	ACP* after = db_get_acp(pnode->ri);
	int result;
	
	set_acp_update(after);
	set_rtnode_update(pnode, after);
	result = db_delete_acp(after->ri);
	result = db_store_acp(after);
	
	response_payload = acp_to_json(after);
	respond_to_client(200, NULL, "2004");
	notify_onem2m_resource(pnode->child, response_payload, NOTIFICATION_EVENT_1);
	free(response_payload); response_payload = NULL;
	free_acp(after); after = NULL;
}

void retrieve_filtercriteria_data(RTNode *node, ObjectType ty, char **discovery_list, int *size, int level, int curr, int flag) {
	if(!node || curr > level) return;

	RTNode *child[1024];
	RTNode *sibling[1024];
	int index = -1;

	if(flag == 1) {
		if(!node->uri) set_node_uri(node);
		child[++index] = node->child;
		sibling[index] = node;
	}

	while(node && flag == 0) {
		if(!node->uri) set_node_uri(node);

		if(ty != -1) {
			if(node->ty == ty) {
				discovery_list[*size] = (char*)malloc(MAX_URI_SIZE*sizeof(char));
				strcpy(discovery_list[(*size)++], node->uri);
			}
		} else {
			discovery_list[*size] = (char*)malloc(MAX_URI_SIZE*sizeof(char));
			strcpy(discovery_list[(*size)++], node->uri);
		}
		child[++index] = node->child;
		sibling[index] = node;
		node = node->sibling_right;
	}

	if((ty == -1 || ty == TY_CIN) && curr < level) {
		for(int i= 0; i <= index; i++) {
			RTNode *cin_list_head = db_get_cin_rtnode_list_by_pi(sibling[i]->ri);
			RTNode *p = cin_list_head;

			while(p) {
				discovery_list[*size] = (char*)malloc(MAX_URI_SIZE*sizeof(char));
				strcpy(discovery_list[*size], sibling[i]->uri);
				strcat(discovery_list[*size], "/");
				strcat(discovery_list[*size], p->ri);
				(*size)++;
				p = p->sibling_right;
			}

			free_rtnode_list(cin_list_head);
		}
	}

	for(int i = 0; i<=index; i++) {
		retrieve_filtercriteria_data(child[i], ty, discovery_list, size, level, curr+1, 0);
	} 
}

void retrieve_object_filtercriteria(RTNode *pnode) {
	char **discovery_list = (char**)malloc(65536*sizeof(char*));
	int size = 0;
	ObjectType ty = get_value_querystring_int("ty");
	int level = get_value_querystring_int("lvl");
	if(level == -1) level = INT32_MAX;

	retrieve_filtercriteria_data(pnode, ty, discovery_list, &size, level, 0, 1);

	response_payload = discovery_to_json(discovery_list, size);
	respond_to_client(200, NULL, "2000");
	for(int i=0; i<size; i++) free(discovery_list[i]);
	free(discovery_list);
	free(response_payload); response_payload = NULL;

	return;
}
*/