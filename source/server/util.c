#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <malloc.h>
#include <sys/timeb.h>
#include "onem2m.h"
#include "util.h"
#include "httpd.h"
#include "logger.h"
#include "onem2mTypes.h"
#include "config.h"
#include "dbmanager.h"
#include "jsonparser.h"

extern ResourceTree *rt;


RTNode* parse_uri(oneM2MPrimitive *o2pt, RTNode *cb) {
	logger("O2M", LOG_LEVEL_DEBUG, "Call parse_uri %s", o2pt->to);
	
	char uri_array[MAX_URI_SIZE];
	char *uri_parse = uri_array;
	char *fopt_buf = NULL;

	if(!strncmp(o2pt->to, "~/", 2)){
        if(!strncmp(o2pt->to + 2, CSE_BASE_RI, strlen(CSE_BASE_RI))){
            char *temp = strdup(o2pt->to + 2 + strlen(CSE_BASE_RI) + 1);
            free(o2pt->to);
            o2pt->to = temp;
        }else{
            o2pt->isForwarding = true;
        }
    }

	strcpy(uri_array, o2pt->to);

	char uri_strtok[64][MAX_URI_SIZE] = {"\0", };
	int index_start = 0, index_end = -1, fopt_cnt = -1;

	uri_parse = strtok(uri_array, "/");

	while(uri_parse) {
		strcpy(uri_strtok[++index_end], uri_parse);
		if(!o2pt->isFopt && uri_parse && !strcmp(uri_parse, "fopt")){
			o2pt->isFopt = true;
		}
		if(uri_parse && o2pt->isFopt){
			fopt_cnt++;
		}
		uri_parse = strtok(NULL, "/");
		
	}

	int latest_oldest_flag = -1;
	if(o2pt->isFopt)
	{	
		index_end = index_end - 1 - fopt_cnt;
		if(fopt_cnt){

			fopt_buf = calloc(1, 256);
			for(int i = 0 ; i < fopt_cnt ; i++){
				strcat(fopt_buf, "/");
				strcat(fopt_buf, uri_strtok[i+index_end + 2]); //index end before fopt so +2
			}
			o2pt->fopt = strdup(fopt_buf);
			
			free(fopt_buf);
		}
	}else{

		if(!strcmp(uri_strtok[0], "viewer")) index_start++;
		if(!strcmp(uri_strtok[index_end], "la") || !strcmp(uri_strtok[index_end], "latest")) {
			latest_oldest_flag = 0; index_end--;
			
		} else if(!strcmp(uri_strtok[index_end], "ol") || !strcmp(uri_strtok[index_end], "oldest")) {
			latest_oldest_flag = 1; index_end--;
			
		}
	}

	strcpy(uri_array, "\0");
	for(int i=index_start; i<=index_end; i++) {
		strcat(uri_array,"/"); 
		strcat(uri_array,uri_strtok[i]);
	}

	RTNode * rtnode = NULL;
	if(o2pt->isForwarding){
		rtnode = find_csr_rtnode_by_uri(cb, uri_array);
	}else{
		rtnode = find_rtnode_by_uri(cb, uri_array);
	}

	
	if(rtnode && !o2pt->isFopt && latest_oldest_flag != -1) rtnode = find_latest_oldest(rtnode, latest_oldest_flag);

	if(index_start == 1) o2pt->op = OP_VIEWER;

	return rtnode;
}

RTNode *find_csr_rtnode_by_uri(RTNode *cb, char *uri){
	RTNode *rtnode = cb->child, *parent_rtnode = NULL;
	char *target_uri = strtok(uri+3, "/");
	target_uri -= 1;

	if(!target_uri) return NULL;


	logger("O2M", LOG_LEVEL_DEBUG, "target_uri : %s", target_uri);


	while(rtnode) {
		if(rtnode->ty != RT_CSR){
			rtnode = rtnode->sibling_right;
			continue;
		}
		cJSON *csi = cJSON_GetObjectItem(rtnode->obj, "csi");
		if(rtnode->obj && !strcmp(cJSON_GetStringValue(csi), target_uri)) break;
		rtnode = rtnode->sibling_right;
	}
	
	return rtnode;
}

RTNode *find_rtnode_by_uri(RTNode *cb, char *uri) {
	RTNode *rtnode = cb, *parent_rtnode = NULL;
	char *target_uri = strdup(uri);
	char *ptr = strtok(target_uri, "/");
	if(!ptr) return NULL;

	char ri[64];
	strcpy(ri, ptr);

	char uri_array[64][MAX_URI_SIZE];
	int index = -1;

	while(ptr) {
		strcpy(uri_array[++index], ptr);
		ptr = strtok(NULL, "/");
	}

	free(target_uri);

	for(int i=0; i<=index; i++) {
		while(rtnode) {
			if(!strcmp(get_rn_rtnode(rtnode), uri_array[i])) break;
			rtnode = rtnode->sibling_right;
		}
		if(i == index-1) parent_rtnode = rtnode;
		if(!rtnode) break;
		if(i != index) rtnode = rtnode->child;
	}
	if(rtnode) return rtnode;

	if(parent_rtnode) {
		cJSON *cin = db_get_resource(uri_array[index], RT_CIN);
		//CIN *cin = db_get_cin(uri_array[index]);
		if(cin) {
			cJSON *pi = cJSON_GetObjectItem(cin, "pi");
			if(!strcmp(cJSON_GetStringValue(pi), get_ri_rtnode(parent_rtnode))) {
				rtnode = create_rtnode(cin, RT_CIN);
				rtnode->parent = parent_rtnode;
			}
		}
	}

	if(!rtnode) {
		rtnode = find_rtnode_by_ri(cb, ri);
	}

	return rtnode;
}

RTNode *find_rtnode_by_ri(RTNode *cb, char *ri){
	RTNode *rtnode = cb;
	RTNode *ret = NULL;

	while(rtnode) {
		if(!strcmp(get_ri_rtnode(rtnode), ri)) {
			return rtnode;
		}
		
		ret = find_rtnode_by_ri(rtnode->child, ri);
		if(ret) {
			return ret;
		}
		rtnode = rtnode->sibling_right;
	}

	return ret;
}

char *ri_to_uri(char *ri){
	char *uri = NULL;
	RTNode *rtnode = find_rtnode_by_ri(rt->cb, ri);

	if(rtnode){
		uri = rtnode->uri;
	}
	
	return uri;
}


RTNode *find_latest_oldest(RTNode* rtnode, int flag) {
	logger("UTL", LOG_LEVEL_DEBUG, "latest oldest");
	RTNode *cin_rtnode = NULL;

	if(rtnode->ty == RT_CNT) {
		cJSON *cin_object = db_get_cin_laol(rtnode, flag);
		cin_rtnode = create_rtnode(cin_object, RT_CIN);
		cin_rtnode->parent = rtnode;
	}

	return cin_rtnode;
}

int net_to_bit(cJSON *net) {
	int ret = 0;
	cJSON *pjson = NULL;

	cJSON_ArrayForEach(pjson, net){
		int exp = atoi(pjson->valuestring);
		if(exp > 0) ret = (ret | (int)pow(2, exp - 1));
	}

	return ret;
}

int add_child_resource_tree(RTNode *parent, RTNode *child) {
	RTNode *node = parent->child;
	child->parent = parent;

	char *uri = malloc(strlen(parent->uri) + strlen(get_rn_rtnode(child)) + 2);
	strcpy(uri, parent->uri);
	strcat(uri, "/");
	strcat(uri, get_rn_rtnode(child));
	child->uri = uri;

	logger("O2M", LOG_LEVEL_DEBUG, "Add Resource Tree Node [Parent-ID] : %s, [Child-ID] : %s",get_ri_rtnode(parent), get_ri_rtnode(child));
	
	if(!node) {
		parent->child = child;
	} else if(node) {
		while(node->sibling_right && node->sibling_right->ty <= child->ty) { 	
				node = node->sibling_right;
		}

		if(parent->child == node && child->ty < node->ty) {
			parent->child = child;
			child->parent = parent;
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

ResourceType http_parse_object_type(header_t *headers, int cnt) {
	char *content_type = request_header(headers, cnt, "Content-Type");
	if(!content_type) return RT_MIXED;
	char *str_ty = strstr(content_type, "ty=");
	if(!str_ty) return RT_MIXED;
	int object_type = atoi(str_ty+3);

	ResourceType ty;
	
	switch(object_type) {
	case 1 : ty = RT_ACP; break;
	case 2 : ty = RT_AE; break;
	case 3 : ty = RT_CNT; break;
	case 4 : ty = RT_CIN; break;
	case 5 : ty = RT_CSE; break;
	case 9 : ty = RT_GRP; break;
	case 16: ty = RT_CSR; break;
	case 23 : ty = RT_SUB; break;
	default : ty = RT_MIXED; break;
	}
	
	return ty;
}


char *get_local_time(int diff) {
	time_t t = time(NULL) - diff;
	struct tm tm = *localtime(&t);
	struct timespec specific_time;
    //int millsec;
    clock_gettime(0, &specific_time);
	
	char year[5], mon[3], day[3], hour[3], minute[3], sec[3], millsec[7]; 
	
	sprintf(year,"%d", tm.tm_year+1900);
	sprintf(mon,"%02d",tm.tm_mon+1);
	sprintf(day,"%02d",tm.tm_mday);
	sprintf(hour,"%02d",tm.tm_hour);
	sprintf(minute,"%02d",tm.tm_min);
	sprintf(sec,"%02d",tm.tm_sec);
	sprintf(millsec, "%06d", (int) floor(specific_time.tv_nsec/1.0e6));
	
	char* local_time = (char*)malloc(24 * sizeof(char));
	
	*local_time = '\0';
	strcat(local_time,year);
	strcat(local_time,mon);
	strcat(local_time,day);
	strcat(local_time,"T");
	strcat(local_time,hour);
	strcat(local_time,minute);
	strcat(local_time,sec);
	strcat(local_time,millsec);
	
	return local_time;
}

char *get_resource_key(ResourceType ty){
	char *key = NULL;
	switch(ty){
		case RT_CSE:
			key = "m2m:cb";
			break;
		case RT_AE:
			key = "m2m:ae";
			break;
		case RT_CNT:
			key = "m2m:cnt";
			break;
		case RT_CIN:
			key = "m2m:cin";
			break;
		case RT_SUB:
			key = "m2m:sub";
			break;
		case RT_GRP:
			key = "m2m:grp";
			break;
		case RT_ACP:
			key = "m2m:acp";
			break;
		case RT_CSR:
			key = "m2m:csr";
			break;
		default:
			key = "general";
			break;
	}
	return key;
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

ResourceType parse_object_type_cjson(cJSON *cjson) {
	ResourceType ty;

	if(!cjson) return RT_MIXED;
	
	if(cJSON_GetObjectItem(cjson, "m2m:cb") || cJSON_GetObjectItem(cjson, "m2m:cse")) ty = RT_CSE;
	else if(cJSON_GetObjectItem(cjson, "m2m:ae")) ty = RT_AE;
	else if(cJSON_GetObjectItem(cjson, "m2m:cnt")) ty = RT_CNT;
	else if(cJSON_GetObjectItem(cjson, "m2m:cin")) ty = RT_CIN;
	else if(cJSON_GetObjectItem(cjson, "m2m:sub")) ty = RT_SUB;
	else if(cJSON_GetObjectItem(cjson, "m2m:grp")) ty = RT_GRP;
	else if(cJSON_GetObjectItem(cjson, "m2m:acp")) ty = RT_ACP;
	else if(cJSON_GetObjectItem(cjson, "m2m:csr")) ty = RT_CSR;
	else ty = RT_MIXED;
	
	return ty;
}

char *resource_identifier(ResourceType ty, char *ct) {
	char *ri = (char *)calloc(32, sizeof(char));

	switch(ty) {
		case RT_AE : strcpy(ri, "CAE"); break;
		case RT_CNT : strcpy(ri, "3-"); break;
		case RT_CIN : strcpy(ri, "4-"); break;
		case RT_SUB : strcpy(ri, "23-"); break;
		case RT_ACP : strcpy(ri, "1-"); break;
		case RT_GRP : strcpy(ri, "9-"); break;
		case RT_CSR : strcpy(ri, "16-"); break; 
	}

	// struct timespec specific_time;
    // int millsec;
	static int n = 0;

	char buf[32] = "\0";

    // clock_gettime(CLOCK_REALTIME, &specific_time);
    // millsec = floor(specific_time.tv_nsec/1.0e6);

	sprintf(buf, "%s%04d",ct, n);
	n = (n + 1) % 10000;

	strcat(ri, buf);

	return ri;
}

void delete_cin_under_cnt_mni_mbs(RTNode *rtnode) {
	logger("UTIL", LOG_LEVEL_DEBUG, "call delete_cin_under_cnt_mni_mbs");
	cJSON *cnt = rtnode->obj;
	cJSON *cni_obj = NULL;
	cJSON *cbs_obj = NULL;
	cJSON *mni_obj = NULL;
	cJSON *mbs_obj = NULL;
	int cni, mni, cbs, mbs;

	if(cni_obj = cJSON_GetObjectItem(cnt, "cni")) {
		cni = cni_obj->valueint;
	}
	if(mni_obj = cJSON_GetObjectItem(cnt, "mni")) {
		mni = mni_obj->valueint;
	}else{
		mni = DEFAULT_MAX_NR_INSTANCES;
	}
	if(cbs_obj = cJSON_GetObjectItem(cnt, "cbs")) {
		cbs = cbs_obj->valueint;
	}
	if(mbs_obj = cJSON_GetObjectItem(cnt, "mbs")) {
		mbs = mbs_obj->valueint;
	}else{
		mbs = DEFAULT_MAX_BYTE_SIZE;
	}

	if(cni <= mni && cbs <= mbs) return;

	RTNode *head = db_get_cin_rtnode_list(rtnode);
	RTNode *right;

	while((mni >= 0 && cni > mni) || (mbs >= 0 && cbs > mbs)) {
		if(head) {
			logger("UTI", LOG_LEVEL_DEBUG, "%s", cJSON_GetObjectItem(head->obj, "con")->valuestring);
			right = head->sibling_right;
			db_delete_onem2m_resource(head);
			cbs -= cJSON_GetObjectItem(head->obj, "cs")->valueint;
			cni--;
			free_rtnode(head);
			head = right;
		} else {
			break;
		}
	}

	if(head) free_rtnode_list(head);

	if( cni_obj->valueint != cni){
		cJSON_SetIntValue(cni_obj, cni);
	}
	if( cbs_obj->valueint != cbs ){
		cJSON_SetIntValue(cbs_obj, cbs);
	}
}

int tree_viewer_api(oneM2MPrimitive *o2pt, RTNode *node) {
	logger("O2M", LOG_LEVEL_DEBUG, "\x1b[43mTree Viewer API\x1b[0m\n");
	char arr_viewer_data[MAX_TREE_VIEWER_SIZE] = "[";
	char *viewer_data = arr_viewer_data;
	
	RTNode *p = node;
	while(p = p->parent) {
		char *json = rtnode_to_json(p);
		strcat(viewer_data,",");
		strcat(viewer_data,json);
		free(json); json = NULL;
	}
	
	int cinSize = 1;
	
	//char *la = strstr(qs,"la=");
	//if(la) cinSize = atoi(la+3);

	cinSize = cJSON_GetNumberValue(cJSON_GetObjectItem(o2pt->fc, "la"));
	
	logger("O2M", LOG_LEVEL_DEBUG,"Latest CIN Size : %d\n", cinSize);
	
	tree_viewer_data(node, &viewer_data, cinSize);
	strcat(viewer_data,"]\0");
	char *res;
	res = calloc(1, MAX_TREE_VIEWER_SIZE);
	int index = 0;

	for(int i=0; i<MAX_TREE_VIEWER_SIZE; i++) {
		if(i == 1) continue;
		if(is_json_valid_char(viewer_data[i])) {
			res[index++] = viewer_data[i];
		}
	}
	
	fprintf(stderr,"Content-Size : %ld\n",strlen(res));
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = res;
}

void tree_viewer_data(RTNode *rtnode, char **viewer_data, int cin_size) {
	char *json = rtnode_to_json(rtnode);

	strcat(*viewer_data, ",");
	strcat(*viewer_data, json);
	free(json); json = NULL;

	RTNode *child = rtnode->child;
	while(child) {
		tree_viewer_data(child, viewer_data, cin_size);
		child = child->sibling_right;
	}

	if(rtnode->ty == RT_CNT) {
		RTNode *cin_list_head = db_get_cin_rtnode_list(rtnode);

		if(cin_list_head) cin_list_head = latest_cin_list(cin_list_head, cin_size);

		RTNode *cin = cin_list_head;

		while(cin) {
			json = rtnode_to_json(cin);
			strcat(*viewer_data, ",");
			strcat(*viewer_data, json);
			free(json); json = NULL;
			cin = cin->sibling_right;		
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

void log_runtime(double start) {
	double end = (((double)clock()) / CLOCKS_PER_SEC); // runtime check - end
	logger("UTIL", LOG_LEVEL_INFO, "Run time : %lf", end-start);
}


void init_server() {
	char poa[128] = {0};
	if(SERVER_TYPE == MN_CSE){
		logger("UTIL", LOG_LEVEL_DEBUG, "MN-CSE");
		// oneM2MPrimitive csr;
		// create_csr(&csr);
		
		// http_send_get_request(REMOTE_CSE_HOST, REMOTE_CSE_PORT, "/", DEFAULT_REQUEST_HEADERS, "", "");

	} else if(SERVER_TYPE == IN_CSE){
		logger("UTIL", LOG_LEVEL_DEBUG, "IN-CSE");
	}

	rt = (ResourceTree *)calloc(1, sizeof(rt));
	
	cJSON *cse;

	cse = db_get_resource(CSE_BASE_RI, RT_CSE);

	if(!cse){
		cse = cJSON_CreateObject();
		init_cse(cse);
		db_store_resource(cse, CSE_BASE_NAME);
	}
	sprintf(poa, "http://%s:%s", SERVER_HOST, SERVER_PORT);

	cJSON *poa_obj = cJSON_CreateArray();
	cJSON_AddItemToArray(poa_obj, cJSON_CreateString(poa));
	cJSON_AddItemToObject(cse, "poa", poa_obj);

	rt->cb = create_rtnode(cse, RT_CSE);
	rt->cb->uri = CSE_BASE_NAME;

 	init_resource_tree();
}

void init_resource_tree(){
	RTNode *rtnode_list = (RTNode *)calloc(1,sizeof(RTNode));
	RTNode *tail = rtnode_list;

	RTNode* csr_list = db_get_all_csr_rtnode();
	tail->sibling_right = csr_list;
	if(csr_list) csr_list->sibling_left = tail;
	while(tail->sibling_right) tail = tail->sibling_right;
	
	RTNode* ae_list = db_get_all_ae_rtnode();
	tail->sibling_right = ae_list;
	if(ae_list) ae_list->sibling_left = tail;
	while(tail->sibling_right) tail = tail->sibling_right;

	RTNode* cnt_list = db_get_all_cnt_rtnode();
	tail->sibling_right = cnt_list;
	if(cnt_list) cnt_list->sibling_left = tail;
	while(tail->sibling_right) tail = tail->sibling_right;
	

	RTNode* sub_list = db_get_all_sub_rtnode();
	tail->sibling_right = sub_list;
	if(sub_list) sub_list->sibling_left = tail;
	while(tail->sibling_right) tail = tail->sibling_right;


	RTNode* acp_list = db_get_all_acp_rtnode();
	tail->sibling_right = acp_list;
	if(acp_list) acp_list->sibling_left = tail;
	while(tail->sibling_right) tail = tail->sibling_right;

	RTNode* grp_list = db_get_all_grp_rtnode();
	tail->sibling_right = grp_list;
	if(grp_list) grp_list->sibling_left = tail;
	while(tail->sibling_right) tail = tail->sibling_right;
	
	RTNode *temp = rtnode_list;
	rtnode_list = rtnode_list->sibling_right;
	if(rtnode_list) rtnode_list->sibling_left = NULL;
	free_rtnode(temp);
	temp = NULL;
	if(rtnode_list) restruct_resource_tree(rt->cb, rtnode_list);
}

RTNode* restruct_resource_tree(RTNode *parent_rtnode, RTNode *list) {
	RTNode *rtnode = list;
	while(rtnode) {
		logger("UTIL", LOG_LEVEL_DEBUG, "restruct_resource_tree : %s", get_ri_rtnode(rtnode));
		RTNode *right = rtnode->sibling_right;
		if(!strcmp(get_ri_rtnode(parent_rtnode), get_pi_rtnode(rtnode))) {
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
		list = restruct_resource_tree(child, list);
		child = child->sibling_right;
	}
	
	return list;
}

int check_payload_size(oneM2MPrimitive *o2pt) {
	if(o2pt->pc && strlen(o2pt->pc) > MAX_PAYLOAD_SIZE) {
		handle_error(o2pt, RSC_BAD_REQUEST, "payload is too large");
		return -1;
	}
	return 0;
}

int result_parse_uri(oneM2MPrimitive *o2pt, RTNode *rtnode) {
	if(!rtnode) {
		handle_error(o2pt, RSC_NOT_FOUND, "URI is invalid");
		return -1;
	} else {
		return 0;
	} 
}

int check_privilege(oneM2MPrimitive *o2pt, RTNode *rtnode, ACOP acop) {
	bool deny = false;

	RTNode *parent_rtnode = rtnode;

	while(parent_rtnode && parent_rtnode->ty != RT_AE) {
		parent_rtnode = parent_rtnode->parent;
	}


	if(!o2pt->fr) {
		if(!(o2pt->op == OP_CREATE && o2pt->ty == RT_AE)) {
			deny = true;
		}
	} else if(!strcmp(o2pt->fr, "CAdmin")) {
		return false;
	} else if((parent_rtnode && strcmp(o2pt->fr, get_ri_rtnode(parent_rtnode)))) {
		deny = true;
	}
  
	if(rtnode->ty == RT_CIN) rtnode = rtnode->parent;

	if(rtnode->ty != RT_CSE) {
		if(get_acpi_rtnode(rtnode) || rtnode->ty == RT_ACP) {
			deny = true;
			if((get_acop(o2pt, rtnode) & acop) == acop) {
				deny = false;
			}
		}
	}

	if(deny) {
		handle_error(o2pt, RSC_ORIGINATOR_HAS_NO_PRIVILEGE, "originator has no privilege");
		return -1;
	}

	return false;
}

int check_macp_privilege(oneM2MPrimitive *o2pt, RTNode *rtnode, ACOP acop){
	bool deny = false;
	if(!o2pt->fr) {
		deny = true;
	} else if(!strcmp(o2pt->fr, "CAdmin")) {
		return false;
	} else if(strcmp(o2pt->fr, get_ri_rtnode(rtnode))) {
		deny = true;
	}

	cJSON *macp = cJSON_GetObjectItem(rtnode->obj, "macp");
	if(macp && cJSON_GetArraySize(macp) > 0) {
		deny = true;
		if((get_acop_macp(o2pt, rtnode) & acop) == acop) {
			deny = false;
		}
	}else{
		deny = false;
	}

	if(deny) {
		handle_error(o2pt, RSC_ORIGINATOR_HAS_NO_PRIVILEGE, "originator has no privilege");
		return -1;
	}

	return 0;
}

int get_acop(oneM2MPrimitive *o2pt, RTNode *rtnode) {
	int acop = 0;
	logger("UTIL", LOG_LEVEL_DEBUG, "get_acop : %s", o2pt->fr);
	char *origin = NULL;
	if(!o2pt->fr) {
		origin = strdup("all");
	}else{
		origin = strdup(o2pt->fr);
	}

    if(!strcmp(origin, "CAdmin")) return ALL_ACOP;

	if(rtnode->ty == RT_ACP) {
		acop = (acop | get_acop_origin(o2pt, rtnode, 1));
		return acop;
	}

	cJSON *acpiArr = get_acpi_rtnode(rtnode);
    if(!acpiArr) return 0;
	
	RTNode *cb = rtnode;
	while(cb->parent) cb = cb->parent;
	cJSON *acpi = NULL;
	cJSON_ArrayForEach(acpi, acpiArr) {
		RTNode *acp = find_rtnode_by_uri(cb, acpi->valuestring);
		if(acp) {
			acop = (acop | get_acop_origin(o2pt, acp, 0));
		}
	}
	return acop;
}

int get_acop_macp(oneM2MPrimitive *o2pt, RTNode *rtnode){
	int acop = 0;
	logger("UTIL", LOG_LEVEL_DEBUG, "get_acop_macp : %s", o2pt->fr);
	char *origin = NULL;
	if(!o2pt->fr) {
		origin = strdup("all");
	}else{
		origin = strdup(o2pt->fr);
	}

	if(!strcmp(origin, "CAdmin")) return ALL_ACOP;

	if(!strcmp(origin, "CAdmin")) return ALL_ACOP;

	cJSON *macp = cJSON_GetObjectItem(rtnode->obj, "macp");
	if(!macp) return 0;

	RTNode *cb = rtnode;
	while(cb->parent) cb = cb->parent;
	cJSON *acpi = NULL;
	cJSON_ArrayForEach(acpi, macp) {
		
		RTNode *acp = find_rtnode_by_uri(cb, acpi->valuestring);
		if(acp) {
			acop = (acop | get_acop_origin(o2pt, acp, 0));
		}
	}

	return acop;
}

int check_acco(cJSON *acco, char *ip){
	logger("UTIL", LOG_LEVEL_DEBUG, "check_acco : %s", ip);
	if(!acco) return 1;
	if(!ip) return 1;

	cJSON *acip = cJSON_GetObjectItem(acco, "acip");
	cJSON *ipv4 = cJSON_GetObjectItem(acip, "ipv4");
	cJSON *ipv6 = cJSON_GetObjectItem(acip, "ipv6");
	cJSON *pjson = NULL;

	cJSON_ArrayForEach(pjson, ipv4){
		logger("UTIL", LOG_LEVEL_DEBUG, "check_acco/ipv4 : %s", pjson->valuestring);
		if(!strcmp(pjson->valuestring, ip)) 
			return 1;
	}
	return 0;
}

int get_acop_origin(oneM2MPrimitive *o2pt, RTNode *acp_rtnode, int flag) {
	int ret_acop = 0, cnt = 0;
	cJSON *acp = acp_rtnode->obj;
	
	cJSON *privilege = NULL;
	cJSON *acr = NULL;
	cJSON *acor = NULL;
	bool found = false;
	char *origin = NULL;

	if(!o2pt->fr) {
		origin = strdup("all");
	}else{
		origin = strdup(o2pt->fr);
	}

	if(flag){
		privilege = cJSON_GetObjectItem(acp, "pvs");
	}else{
		privilege = cJSON_GetObjectItem(acp, "pv");
	}

	cJSON_ArrayForEach(acr, cJSON_GetObjectItem(privilege, "acr")){
		cJSON_ArrayForEach(acor, cJSON_GetObjectItem(acr, "acor")){
			if(acor->valuestring[strlen(acor->valuestring)-1] == '*') {
				if(!strncmp(acor->valuestring, origin, strlen(acor->valuestring)-1)) {
					if(check_acco(cJSON_GetObjectItem(acr, "acco"), o2pt->ip)){
						ret_acop = (ret_acop | cJSON_GetObjectItem(acr, "acop")->valueint);
					}
					break;
				}
			} else if(!strcmp(acor->valuestring, origin)) {
				if(check_acco(cJSON_GetObjectItem(acr, "acco"), o2pt->ip)){
					ret_acop = (ret_acop | cJSON_GetObjectItem(acr, "acop")->valueint);
				}
				break;
			}else if(!strcmp(acor->valuestring, "all")){
				if(check_acco(cJSON_GetObjectItem(acr, "acco"), o2pt->ip)){
					ret_acop = (ret_acop | cJSON_GetObjectItem(acr, "acop")->valueint);
				}
				break;
			}
		}
	}
	return ret_acop;
}

int has_privilege(oneM2MPrimitive *o2pt, char *acpi, ACOP acop) {
	char *origin = o2pt->fr;
	if(!origin) return 0;
	if(!acpi) return 1;

	RTNode *acp = find_rtnode_by_uri(rt->cb, acpi);
	int result = get_acop_origin(o2pt, acp, 0);
	if( (result & acop) == acop) {
		return 1;
	}
	return 0;
}

int check_rn_duplicate(oneM2MPrimitive *o2pt, RTNode *rtnode) {
	if(!rtnode) return 0;
	cJSON *root = o2pt->cjson_pc;
	cJSON *resource, *rn;

	resource = getResource(root, o2pt->ty);

	RTNode *child = rtnode->child;
    bool flag = false;

	rn = cJSON_GetObjectItem(resource, "rn");
	if(rn) {
		char *resource_name = rn->valuestring;
		while(child) {
			if(get_rn_rtnode(child) && !strcmp(get_rn_rtnode(child), resource_name)) {
                flag = true; 
                break;
			}
			child = child->sibling_right;
		}
        if(rtnode->ty == RT_CNT) {
            char invalid_rn[][8] = {"la", "latest", "ol", "oldest"};
            int invalid_rn_size = sizeof(invalid_rn)/(8*sizeof(char));
            for(int i=0; i<invalid_rn_size; i++) {
                if(!strcmp(resource_name, invalid_rn[i])) {
                    flag = true;
                }
            }
        }
	}

    if(flag) {
		handle_error(o2pt, RSC_CONFLICT, "attribute `rn` is duplicated");
		return -1;
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
		if(child->ty != RT_AE){
			child = child->sibling_right;
			continue;
		}
		if(!strcmp(get_ri_rtnode(child), aei)) {
			handle_error(o2pt, RSC_ORIGINATOR_HAS_ALREADY_REGISTERD, "attribute `aei` is duplicated");
			return -1;
		}
		child = child->sibling_right;
	}

	return 0;
}

int check_csi_duplicate(char *new_csi, RTNode *rtnode) {
	if(!rtnode || new_csi == NULL) return 0;

	RTNode *child = rtnode->child;

	while(child) {
		if(!strcmp(get_ri_rtnode(child), new_csi)) {
			return -1;
		}
		child = child->sibling_right;
	}

	return 0;
}

int check_aei_invalid(oneM2MPrimitive *o2pt) {
	char *aei = o2pt->fr;
	if(!aei || strlen(aei) == 0) return 0;
	cJSON *cjson = string_to_cjson_string_list_item(ALLOW_AE_ORIGIN);

	int size = cJSON_GetArraySize(cjson);
	for(int i=0; i<size; i++) {
		cJSON *item = cJSON_GetArrayItem(cjson, i);
		char *origin = strdup(item->valuestring);
		if(origin[strlen(origin)-1] == '*') {
			if(!strncmp(aei, origin, strlen(origin)-1)) return 0;
		} else if(!strcmp(origin, aei)) return 0;

		free(origin); origin = NULL;
	}
	o2pt->rsc = RSC_BAD_REQUEST;
	if(o2pt->pc) free(o2pt->pc);
	o2pt->pc = strdup("{\"m2m:dbg\":\"originator is invalid\"}");
	return -1;
}

int check_payload_format(oneM2MPrimitive *o2pt) {
	cJSON *cjson = o2pt->cjson_pc;
	
	if(cjson == NULL) {
		handle_error(o2pt, RSC_BAD_REQUEST, "payload format is invalid");
		return -1;
	}
	return 0;
}

int check_payload_empty(oneM2MPrimitive *o2pt) {
	if(!o2pt->pc) {
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "payload is empty");
		return -1;
	}
	return 0;
}

int check_rn_invalid(oneM2MPrimitive *o2pt, ResourceType ty) {
	cJSON *root = o2pt->cjson_pc;
	cJSON *resource, *rn;

	resource = getResource(root, ty);

	rn = cJSON_GetObjectItem(resource, "rn");
	if(!rn) return 0;
	char *resource_name = rn->valuestring;
	int len_resource_name = strlen(resource_name);

	for(int i=0; i<len_resource_name; i++) {
		if(!is_rn_valid_char(resource_name[i])) {
			handle_error(o2pt, RSC_BAD_REQUEST, "attribute `rn` is invalid");
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
		handle_error(o2pt, RSC_BAD_REQUEST, "resource type is invalid");
		return -1;
	}
	return 0;
}

int check_resource_type_invalid(oneM2MPrimitive *o2pt) {
	if(o2pt->ty == RT_MIXED) {
		handle_error(o2pt, RSC_BAD_REQUEST, "resource type is invalid");
		return -1;
	}
	return 0;
}

/**
 * @brief set rsc and error msg to oneM2MPrimitive
 * @param o2pt oneM2MPrimitive
 * @param rsc error code
 * @param err error message
 * @return error code
*/
int handle_error(oneM2MPrimitive *o2pt, int rsc, char *err){
	logger("UTIL", LOG_LEVEL_ERROR, err);
	o2pt->rsc = rsc;
	o2pt->errFlag = true;
	char pc[MAX_PAYLOAD_SIZE];
	sprintf(pc, "{\"m2m:dbg\": \"%s\"}", err);
	set_o2pt_pc(o2pt, pc);
	return rsc;
}

bool isUriFopt(char *str){
	char *s;
	if(!str) return false;
	s = strrchr(str, '/');
	if(!s) return false;
	return !strcmp(s, "/fopt");
}

bool endswith(char *str, char *match){
	size_t str_len = 0;
	size_t match_len = 0;
	if(!str || !match) 
		return false;

	str_len = strlen(str);
	match_len = strlen(match);

	if(!str_len || !match_len)
		return false;

	return strncmp(str + str_len - match_len, match, match_len);
}

int validate_grp(oneM2MPrimitive *o2pt, cJSON *grp){
	logger("UTIL", LOG_LEVEL_DEBUG, "Validating GRP");
	int rsc = 0;
	bool hasFopt = false;
	bool isLocalResource = true;
	char *p = NULL;
	char *tStr = NULL;
	cJSON *pjson = NULL;

	int mt = 0;
	int csy = DEFAULT_CONSISTENCY_POLICY;
	
	if(pjson = cJSON_GetObjectItem(grp, "mt")){
		mt = pjson->valueint;
	}else{
		handle_error(o2pt, RSC_BAD_REQUEST, "`mt` is mandatory");
		return RSC_BAD_REQUEST;
	}


	if(pjson = cJSON_GetObjectItem(grp, "csy")){
		csy = pjson->valueint;
		if(csy < 0 || csy > 3){
			handle_error(o2pt, RSC_BAD_REQUEST, "`csy` is invalid");
			return RSC_BAD_REQUEST;
		}
	}

	RTNode *rt_node;

	if(pjson = cJSON_GetObjectItem(grp, "mtv")){
		if(pjson->valueint == 1)
			return RSC_OK;
	}



	cJSON *midArr = cJSON_GetObjectItem(grp, "mid");
	cJSON *mid_obj = NULL;
	
	if(!midArr){
		handle_error(o2pt, RSC_BAD_REQUEST, "`mid` is mandatory");
		return RSC_BAD_REQUEST;
	}

	if(midArr && !cJSON_IsArray(midArr)){
		handle_error(o2pt, RSC_BAD_REQUEST, "`mid` should be array");
		return RSC_BAD_REQUEST;
	}

	if(pjson = cJSON_GetObjectItem(grp, "mnm")){
		
		if(pjson->valueint < cJSON_GetArraySize(midArr)){
			handle_error(o2pt, RSC_BAD_REQUEST, "`mnm` is less than `mid` size");
			return RSC_BAD_REQUEST;
		}
	}

	// validate macp
	if(pjson = cJSON_GetObjectItem(grp, "macp")){
		if(!cJSON_IsArray(pjson)){
			handle_error(o2pt, RSC_BAD_REQUEST, "`macp` should be array");
			return RSC_BAD_REQUEST;
		}else if(cJSON_GetArraySize(pjson) == 0){
			handle_error(o2pt, RSC_BAD_REQUEST, "`macp` should not be empty");
		}else{
			if( validate_acpi(o2pt, pjson, OP_CREATE)  != RSC_OK )
				return;
		}
	}

	// member id check
	int i = 0;
	bool dup = false;
	cJSON_ArrayForEach(mid_obj, midArr){
		char *mid = cJSON_GetStringValue(mid_obj);

		for(int j = 0 ; j < i ; j++){
			if(cJSON_GetArrayItem(midArr, j) && !strcmp(mid, cJSON_GetStringValue(cJSON_GetArrayItem(midArr, j)))){
				dup = true;
				cJSON_DeleteItemFromArray(midArr, i);
				break;
			}
		}

		if(dup){ 
			dup = false;
			continue;
		}

		isLocalResource = true;
		if(strlen(mid) >= 2 && mid[0] == '/' && mid[1] != '/'){
			tStr = strdup(mid);
			strtok(tStr, "/");
			p = strtok(NULL, "/");
			if( strcmp(p, CSE_BASE_NAME) != 0){
				isLocalResource = false;
			}

			free(tStr); tStr = NULL;
			p = NULL;
		}

		// resource check
		if(isLocalResource) {
			hasFopt = isUriFopt(mid);
			tStr = strdup(mid);
			if(hasFopt && strlen(mid) > 5)  // remove fopt 
				tStr[strlen(mid) - 5] = '\0';
			logger("util-t", LOG_LEVEL_DEBUG, "%s",tStr);

			if((rt_node = find_rtnode_by_uri(rt->cb, tStr))){
				if(mt != 0 && rt_node->ty != mt)
					if(handle_csy(grp, mid_obj, csy, i)) return RSC_GROUPMEMBER_TYPE_INCONSISTENT;
			}else{
				logger("UTIL", LOG_LEVEL_DEBUG, "GRP member %s not present", tStr);
				if(handle_csy(grp, mid_obj, csy, i)) return RSC_GROUPMEMBER_TYPE_INCONSISTENT;
			}

			if(rt_node && rt_node->ty == RT_GRP){
				//pGrp = db_get_grp(rt_node->ri);
				cJSON *pGrp = rt_node->obj;
				if((rsc = validate_grp(o2pt, pGrp)) >= 4000 ){
					if(handle_csy(grp, mid_obj, csy, i)) return RSC_GROUPMEMBER_TYPE_INCONSISTENT;
				}
				//free_grp(pGrp);
			}

			free(tStr);
			tStr = NULL;
		}else{
			return RSC_NOT_IMPLEMENTED;
		}
		i++;
	}
	return RSC_OK;
}

int validate_grp_update(oneM2MPrimitive *o2pt, cJSON *grp_old, cJSON *grp_new){
	logger("UTIL", LOG_LEVEL_DEBUG, "Validating GRP Update");
	int rsc = 0;
	bool hasFopt = false;
	bool isLocalResource = true;
	char *p = NULL;
	char *tStr = NULL;
	cJSON *pjson = NULL;

	int mt = 0;
	int csy = DEFAULT_CONSISTENCY_POLICY;
	logger("UTIL", LOG_LEVEL_DEBUG, "m2m : %s", cJSON_Print(grp_new));

	if(pjson = cJSON_GetObjectItem(grp_new, "mt")){
		mt = pjson->valueint;
	}

	if(pjson = cJSON_GetObjectItem(grp_new, "csy")){
		csy = pjson->valueint;
		if(csy < 0 || csy > 3){
			handle_error(o2pt, RSC_BAD_REQUEST, "`csy` is invalid");
			return RSC_BAD_REQUEST;
		}
	}

	cJSON *midArr = cJSON_GetObjectItem(grp_new, "mid");
	cJSON *mid_obj = NULL;
	if(midArr && !cJSON_IsArray(midArr)){
		handle_error(o2pt, RSC_BAD_REQUEST, "`mid` should be array");
		return RSC_BAD_REQUEST;
	}


	if(pjson = cJSON_GetObjectItem(grp_new, "mnm")){
		if(pjson->valueint < cJSON_GetArraySize(midArr)){
			handle_error(o2pt, RSC_BAD_REQUEST, "`mnm` is less than `mid` size");
			return RSC_BAD_REQUEST;
		}

		if(pjson->valueint < cJSON_GetObjectItem(grp_old, "cnm")->valueint){
			handle_error(o2pt, RSC_BAD_REQUEST, "`mnm` can't be smaller than `cnm` size");
			return RSC_BAD_REQUEST;
		}
	}
	else if(pjson = cJSON_GetObjectItem(grp_old, "mnm")){
		if(pjson->valueint < cJSON_GetArraySize(midArr)){
			handle_error(o2pt, RSC_MAX_NUMBER_OF_MEMBER_EXCEEDED, "`mnm` is less than `mid` size");
			return RSC_MAX_NUMBER_OF_MEMBER_EXCEEDED;
		}
	}

	if(pjson = cJSON_GetObjectItem(grp_new, "macp")){
		if(validate_acpi(o2pt, pjson, OP_UPDATE) != RSC_OK)
		return RSC_BAD_REQUEST;

		if(cJSON_GetArraySize(pjson) == 0){
			cJSON_DeleteItemFromObject(grp_new, "macp");
		}
	}

	RTNode *rt_node;

	// member id check
	int i = 0;
	bool dup = false;
	cJSON_ArrayForEach(mid_obj, midArr){
		char *mid = cJSON_GetStringValue(mid_obj);
		logger("UTIL", LOG_LEVEL_DEBUG, "mid : %s, %d", mid, i);

		for(int j = 0 ; j < i ; j++){
			if(!strcmp(mid, cJSON_GetStringValue(cJSON_GetArrayItem(midArr, j)))){
				logger("UTIL", LOG_LEVEL_DEBUG, "Duplicated");
				cJSON_DeleteItemFromArray(midArr, i);
				dup = true;
				break;
			}
		}

		if(dup){ 
			dup = false;
			if(i == cJSON_GetArraySize(midArr)) break;
			else continue;
		}

		isLocalResource = true;
		if(strlen(mid) >= 2 && mid[0] == '/' && mid[1] != '/'){
			tStr = strdup(mid);
			strtok(tStr, "/");
			p = strtok(NULL, "/");
			if( strcmp(p, CSE_BASE_NAME) != 0){
				isLocalResource = false;
			}

			free(tStr); tStr = NULL;
			p = NULL;
		}

		// resource check
		if(isLocalResource) {
			hasFopt = isUriFopt(mid);
			tStr = strdup(mid);
			if(hasFopt && strlen(mid) > 5)  // remove fopt 
				tStr[strlen(mid) - 5] = '\0';

			if((rt_node = find_rtnode_by_uri(rt->cb, tStr))){
				if(mt != 0 && rt_node->ty != mt)
					if(handle_csy(grp_old, mid_obj, csy, i)) return RSC_GROUPMEMBER_TYPE_INCONSISTENT;
			}else{
				logger("UTIL", LOG_LEVEL_DEBUG, "GRP member %s not present", tStr);
				if(handle_csy(grp_old, mid_obj, csy, i)) return RSC_GROUPMEMBER_TYPE_INCONSISTENT;
			}

			if(rt_node && rt_node->ty == RT_GRP){
				//pGrp = db_get_grp(rt_node->ri);
				cJSON *pGrp = rt_node->obj;
				if((rsc = validate_grp(o2pt, pGrp)) >= 4000 ){
					if(handle_csy(grp_old, mid_obj, csy, i)) return RSC_GROUPMEMBER_TYPE_INCONSISTENT;
				}
				//free_grp(pGrp);
			}

			free(tStr);
			tStr = NULL;
		}else{
			return handle_error(o2pt, RSC_NOT_IMPLEMENTED, "remote resource is not supported");
		}
		i++;
	}
	return RSC_OK;
}

int handle_csy(cJSON *grp, cJSON *mid, int csy, int i){
	cJSON *mt = cJSON_GetObjectItem(grp, "mt");
	cJSON *cnm = cJSON_GetObjectItem(grp, "cnm");

	switch(csy){
		case CSY_ABANDON_MEMBER:
			cJSON_DeleteItemFromArray(mid, i);
			cJSON_SetNumberValue(cnm, cnm->valueint - 1);
			break;
		case CSY_ABANDON_GROUP:
			return RSC_GROUPMEMBER_TYPE_INCONSISTENT;

		case CSY_SET_MIXED:
			cJSON_SetNumberValue(mt, RT_MIXED);
			break;
	}
	return 0;
}

bool isMinDup(char **mid, int idx, char *new_mid){
	if(!mid) return true;
	if(!new_mid) return true;

	for(int i = 0 ; i < idx ; i++){
		if( mid[i] != 0 && !strcmp(mid[i], new_mid))
			return true;
	}
	return false;
}

void remove_mid(char **mid, int idx, int cnm){
	char *del = mid[idx];
	for(int i = idx ; i < cnm-1; i++){
		mid[i] = mid[i+1];
	}
	//if(mid[cnm-1]) free(mid[cnm-1]);
	mid[cnm-1] = NULL;
	if(idx != cnm-1) free(del);
	del = NULL;
}

int rsc_to_http_status(int rsc){
	switch(rsc){
		case RSC_OK:
		case RSC_UPDATED:
			return 200;

		case RSC_CREATED:
			return 201;

		case RSC_BAD_REQUEST:
		case RSC_NOT_FOUND:
		case RSC_MAX_NUMBER_OF_MEMBER_EXCEEDED:
			return 400;
		
		case RSC_OPERATION_NOT_ALLOWED:
			return 403;

		case RSC_INTERNAL_SERVER_ERROR:
		case RSC_TARGET_NOT_REACHABLE:
			return 500;

		default:
			return 200;
	}
}


cJSON *o2pt_to_json(oneM2MPrimitive *o2pt){
	cJSON *json = cJSON_CreateObject();

    cJSON_AddNumberToObject(json, "rsc", o2pt->rsc);
    cJSON_AddStringToObject(json, "rqi", o2pt->rqi);
    cJSON_AddStringToObject(json, "to", o2pt->to);    
    cJSON_AddStringToObject(json, "fr", o2pt->fr);
    cJSON_AddStringToObject(json, "rvi", o2pt->rvi);
    if(o2pt->pc) cJSON_AddStringToObject(json, "pc", o2pt->pc);
	if(o2pt->cnot){
		cJSON_AddNumberToObject(json, "cnst", CS_PARTIAL_CONTENT);
		cJSON_AddNumberToObject(json, "cnot", o2pt->cnot);
	}
    //if(o2pt->ty >= 0) cJSON_AddNumberToObject(json, "ty", o2pt->ty);
	
	return json;
}

void free_o2pt(oneM2MPrimitive *o2pt){
	if(o2pt->rqi)
		free(o2pt->rqi);
	if(o2pt->origin)
		free(o2pt->origin);
	if(o2pt->pc)
		free(o2pt->pc);
	if(o2pt->fr)
		free(o2pt->fr);
	if(o2pt->to)
		free(o2pt->to);
	if(o2pt->fc)
		cJSON_Delete(o2pt->fc);
	if(o2pt->fopt)
		free(o2pt->fopt);
	if(o2pt->rvi)
		free(o2pt->rvi);
	if(o2pt->cjson_pc)
		cJSON_Delete(o2pt->cjson_pc);
	free(o2pt);
	o2pt = NULL;
}

void o2ptcpy(oneM2MPrimitive **dest, oneM2MPrimitive *src){
	if(src == NULL) return;

	(*dest) = (oneM2MPrimitive *) calloc(1, sizeof(oneM2MPrimitive));

	(*dest)->fr = strdup(src->fr);
	(*dest)->to = strdup(src->to);
	if(src->rqi) (*dest)->rqi = strdup(src->rqi);
	if(src->origin) (*dest)->origin = strdup(src->origin);
	if(src->pc) (*dest)->pc = strdup(src->pc);
	if(src->cjson_pc) (*dest)->cjson_pc = cJSON_Parse((*dest)->pc);
	if(src->rvi) (*dest)->rvi = strdup(src->rvi);
	if(src->fopt) (*dest)->fopt = strdup(src->fopt);
	(*dest)->ty = src->ty;
	(*dest)->op = src->op;
	(*dest)->isFopt = src->isFopt;
	(*dest)->rsc = src->rsc;
	(*dest)->cnot = src->cnot;
	(*dest)->fc = src->fc;
}


void free_all_resource(RTNode *rtnode){
	RTNode *del;
	while(rtnode) {
		del = rtnode;
		if(rtnode->child) free_all_resource(rtnode->child);
		rtnode = rtnode->sibling_right;
		free_rtnode(del);
		del = NULL;
	}
}

char *get_pi_rtnode(RTNode *rtnode) {
	cJSON *pi = cJSON_GetObjectItem(rtnode->obj, "pi");
	if(pi){
		return pi->valuestring;
	}else{
		return "";
	}
}

char *get_ri_rtnode(RTNode *rtnode) {
	cJSON *ri = cJSON_GetObjectItem(rtnode->obj, "ri");
	if(ri){
		return ri->valuestring;
	}else{
		return NULL;
	}
}

char *get_rn_rtnode(RTNode *rtnode) {
	cJSON *rn = cJSON_GetObjectItem(rtnode->obj, "rn");
	if(rn){
		return rn->valuestring;
	}else{
		return NULL;
	}
}

cJSON *get_acpi_rtnode(RTNode *rtnode) {
	cJSON *acpi = cJSON_GetObjectItem(rtnode->obj, "acpi");
	if(acpi){
		return acpi;
	}else{
		return NULL;
	}
}

char *get_ct_rtnode(RTNode *rtnode){
	cJSON *ct = cJSON_GetObjectItem(rtnode->obj, "ct");
	if(ct){
		return ct->valuestring;
	}else{
		return NULL;
	}
}

char *get_et_rtnode(RTNode *rtnode){
	cJSON *et = cJSON_GetObjectItem(rtnode->obj, "et");
	if(et){
		return et->valuestring;
	}else{
		return NULL;
	}
}

char *get_lt_rtnode(RTNode *rtnode){
	cJSON *lt = cJSON_GetObjectItem(rtnode->obj, "lt");
	if(lt){
		return lt->valuestring;
	}else{
		return NULL;
	}
}

char *get_uri_rtnode(RTNode *rtnode){
	return rtnode->uri;
}

cJSON *getResource(cJSON *root, ResourceType ty){
	switch(ty){
		case RT_CSE:
			return cJSON_GetObjectItem(root, "m2m:cse");
		case RT_AE:
			return cJSON_GetObjectItem(root, "m2m:ae");
		case RT_CNT:
			return cJSON_GetObjectItem(root, "m2m:cnt");
		case RT_CIN:
			return cJSON_GetObjectItem(root, "m2m:cin");
		case RT_ACP:
			return cJSON_GetObjectItem(root, "m2m:acp");
		case RT_SUB:
			return cJSON_GetObjectItem(root, "m2m:sub");
		case RT_GRP:
			return cJSON_GetObjectItem(root, "m2m:grp");
		case RT_CSR:
			return cJSON_GetObjectItem(root, "m2m:csr");
	}
}

int get_number_from_cjson(cJSON *json){
	if(!json) return 0;

	if(json->valueint)
		return json->valueint;
	if(json->valuestring)
		return atoi(json->valuestring);
}

cJSON *qs_to_json(char* qs){
	if(!qs) return NULL;

	int prevb = 0;
	char *qStr = strdup(qs);
	char *buf = calloc(1, 256);
	char *temp = calloc(1, 256);
	char *key = NULL, *value = NULL;

	size_t qslen = strlen(qs);
	cJSON *json;

	buf[0] = '{';

	for(int i = 0 ; i <= qslen; i++){
		if(qStr[i] == '='){
			key = qStr + prevb;
			qStr[i] = '\0';
			prevb = i+1;
		}
		else if(qStr[i] == '&' || i == qslen){
			value = qStr + prevb;
			qStr[i] = '\0';
			prevb = i+1;
		}

		if(key != NULL && value != NULL){
			if(value[0] == '['){
				sprintf(temp, "\"%s\":%s,", key, value);
			}else{
				sprintf(temp, "\"%s\":\"%s\",", key, value);
			}

			strcat(buf, temp);
			key = NULL;
			value = NULL;
		}

	}
	buf[strlen(buf)-1] = '}';
	json = cJSON_Parse(buf);
	if(!json){
		logger("UTIL", LOG_LEVEL_DEBUG, "ERROR before %10s\n", cJSON_GetErrorPtr());
		return NULL;
	}
	free(temp);
	free(buf);
	free(qStr);

	return json;
}


int is_in_uril(cJSON *uril, char* new){
	if(!uril) return false;
	if(!new) return false;
	int result = -1;
	cJSON *pjson = NULL;

	int urilSize = cJSON_GetArraySize(uril);

	for(int i = 0 ; i < urilSize ; i++){
		pjson = cJSON_GetArrayItem(uril, i);
		if(!strcmp(pjson->valuestring, new)){
			result = i;
			break;
		}
	}

	return result;
}


void filterOptionStr(FilterOperation fo , char *sql){
	switch(fo){
		case FO_AND:
			strcat(sql, " AND ");
			break;
		case FO_OR:
			strcat(sql, " OR ");
			break;
	}
}

bool check_acpi_valid(oneM2MPrimitive *o2pt, cJSON *acpi) {
	bool ret = true;

	int acpi_size = cJSON_GetArraySize(acpi);

	for(int i=0; i<acpi_size; i++) {
		char *acp_uri = cJSON_GetArrayItem(acpi, i)->valuestring;
		RTNode *acp_rtnode = find_rtnode_by_uri(rt->cb, acp_uri);
		if(!acp_rtnode) {
			ret = false;
			handle_error(o2pt, RSC_NOT_FOUND, "resource `acp` does not found");
			break;
		} else {
			if((get_acop_origin(o2pt, acp_rtnode, 1) & ACOP_UPDATE) != ACOP_UPDATE) {
				ret = false;
				handle_error(o2pt, RSC_ORIGINATOR_HAS_NO_PRIVILEGE, "originator has no privilege");
				break;
			}
		}
	}

	return ret;
}

char** http_split_uri(char *uri){
	char **ret = (char **)malloc(3 * sizeof(char *));
	char host[MAX_URI_SIZE] = {'\0'};
	char remain[MAX_URI_SIZE] = {'\0'};
	char port[8];

	int index = 0;
	int uri_size = strlen(uri);

	char *p = strstr(uri, "http://");

	if(p) {
		p = p+7;
		while(p < uri + uri_size && p != ':') {
			host[index++] = *(p++);
		}
		p = p+1;
		index = 0;
		while(p < uri + uri_size && p != '/' && p != '?') {
			port[index++] = *(p++);
		}
	}
}

void notify_to_nu(oneM2MPrimitive *o2pt, RTNode *sub_rtnode, cJSON *noti_cjson, int net) {
	logger("UTIL", LOG_LEVEL_DEBUG, "notify_to_nu");
	cJSON *sub = sub_rtnode->obj;
	int uri_len = 0, index = 0;
	char *noti_json = cJSON_PrintUnformatted(noti_cjson);
	char *p = NULL;
	char port[10] = {'\0'};
	bool isNoti = false;
	NotiTarget *nt = NULL;
	cJSON *pjson = NULL;

	cJSON *nu = cJSON_GetObjectItem(sub, "nu");
	if(!nu) return;

	cJSON *net_obj = cJSON_GetObjectItem(sub, "net");
	if(!net_obj) return;

	cJSON_ArrayForEach(pjson, net_obj){
		if(pjson->valueint == net) {
			isNoti = true;
			break;
		}
	}

	cJSON_ArrayForEach(pjson, nu){
		char *noti_uri = pjson->valuestring;
		logger("UTIL", LOG_LEVEL_DEBUG, "noti_uri : %s", noti_uri);
		index = 0;
		nt = calloc(1, sizeof(NotiTarget));
		uri_len = strlen(noti_uri);
		p = noti_uri+7;

		if(!strncmp(noti_uri, "http://", 7)) {
			nt->prot = PROT_HTTP;
		}else if(!strncmp(noti_uri, "mqtt://", 7)) {
			nt->prot = PROT_MQTT;
		}

		while(noti_uri + uri_len > p && *p != ':' && *p != '/' && *p != '?'){
			nt->host[index++] = *(p++);
		}
		if(noti_uri + uri_len > p && *p == ':') {
			p++;
			index = 0;
			while(noti_uri + uri_len > p && *p != '/' && *p != '?'){
				port[index++] = *(p++);
			}
			nt->port = atoi(port);
		}
		if(noti_uri + uri_len > p) {
			strcpy(nt->target, p);
			logger("UTIL", LOG_LEVEL_DEBUG, "%s", nt->target);
			p = strchr(nt->target, '?');
			if(p)
				memset(p, 0, strlen(p));
			// if(*p == '?') {
			// 	sprintf(nt->target, "/%s", p);
			// } else if(*p == '/') {
			// 	strcpy(nt->target, p);
			// }
		} 

		switch(nt->prot){
			case PROT_HTTP:
				if(!nt->port)
					nt->port = 80;
				if(nt->target[0] == '\0')
					nt->target[0] = '/';
				http_notify(o2pt, noti_json, nt);
				break;
			#ifdef ENABLE_MQTT
			case PROT_MQTT:
				if(!nt->port)
					nt->port = 1883;
				mqtt_notify(o2pt, noti_json, nt);
				break;
			#endif
		}
		noti_uri = strtok(NULL, ",");
		free(nt);
		nt = NULL;
	}

	free(noti_json);
}

void update_resource(cJSON *old, cJSON *new){
	cJSON *pjson = new->child;
	while(pjson){	
		if(cJSON_GetObjectItem(old, pjson->string)){
			cJSON_ReplaceItemInObject(old, pjson->string, cJSON_Duplicate(pjson, 1));
		}else{
			cJSON_AddItemToObject(old, pjson->string, cJSON_Duplicate(pjson, 1));
		}
		pjson = pjson->next;
	}
}

/**
 * @brief validate sub attribute
 * @param obj attribute supported
 * @param attr attribute to validate
 * @param err_msg error message
 * @return true if valid, false if invalid
 * */
bool validate_sub_attr(cJSON *obj, cJSON *attr, char* err_msg){
	if(!attr) return false;
	if(!obj) return false;
	cJSON *verifier = NULL;
	cJSON *verifiee = NULL;

	verifier = cJSON_GetObjectItem(obj, attr->string);
	if(!verifier && obj->type == cJSON_Array){
		verifier = obj;
	}
	
	if(!verifier){
		if(err_msg){
			sprintf(err_msg, "invalid attribute : %s", attr->string);
		}
		return false;
	}
	if(attr->type != cJSON_NULL && verifier->type != attr->type){ // if attribute type is null it is deleting attribute(on update)
		if(verifier->type == cJSON_True || verifier->type == cJSON_False){
			if(attr->type == cJSON_True || attr->type == cJSON_False){
			}else{
				if(err_msg){
					sprintf(err_msg, "invalid attribute type : %s", attr->string);
				}
				return false;
			}
		}else{
			if(err_msg){
				sprintf(err_msg, "invalid attribute type : %s", attr->string);
			}
			return false;
		}
		
	} 
	if(attr->type == cJSON_Object){
		cJSON_ArrayForEach(verifiee, attr){
			if(cJSON_GetArraySize(verifiee) > 0){
				if(!validate_sub_attr(verifier, verifiee, err_msg)){
					return false;
				}
			}
		}
	}else if(attr->type == cJSON_Array){
		cJSON_ArrayForEach(verifiee, attr){
			if(verifiee->type != verifier->child->type){
				if(err_msg){
					sprintf(err_msg, "invalid attribute type : %s", attr->string);
				}
				return false;
			}
			if(verifiee->type == cJSON_Object){
				cJSON *verifiee_child = NULL;
				cJSON_ArrayForEach(verifiee_child, verifiee){
					if(!validate_sub_attr(verifier->child, verifiee_child, err_msg)){
						return false;
					}
				}
			}
		}
	}
	
	return true;
}

/**
 * @brief validate requested attribute
 * @param obj attribute received
 * @param ty resource type
 * @param err_msg buffer for error message
 * @return true if valid, false if invalid
*/
bool is_attr_valid(cJSON *obj, ResourceType ty, char *err_msg){
	extern cJSON* ATTRIBUTES;
	cJSON *attrs = NULL;
	cJSON *general_attrs = NULL;
	bool flag = false;
	attrs = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(ty));
	general_attrs = cJSON_GetObjectItem(ATTRIBUTES, "general");
	if(!attrs) return false;
	if(!general_attrs) return false;

	cJSON *pjson = cJSON_GetObjectItem(obj, get_resource_key(ty));
	cJSON *attr = NULL;
	if(!pjson) return false;
	pjson = pjson->child;
	while(pjson){
		if(validate_sub_attr(attrs, pjson, err_msg)){
			flag = true;
		}
		if(flag){
			pjson = pjson->next;
			flag = false;
			continue;
		}
		if(validate_sub_attr(general_attrs, pjson, err_msg)){
			flag = true;
		}
		if(!flag){
			return false;
		}
		pjson = pjson->next;
		flag = false;
	}

	return true;
}

/**
 * Get Request Primitive and acpi attribute and validate it.
 * @param o2pt oneM2M request primitive
 * @param acpiAttr acpi attribute cJSON object
 * @param op operation type
 * @return 0 if valid, -1 if invalid
*/
int validate_acpi(oneM2MPrimitive *o2pt, cJSON *acpiAttr, Operation op){
	if(!acpiAttr) {
		return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
	}
	if( !cJSON_IsArray(acpiAttr) && !cJSON_IsNull(acpiAttr)){
		return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `acpi` is in invalid form");
	}
	if(cJSON_IsArray(acpiAttr) && cJSON_GetArraySize(acpiAttr) == 0){
		return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `acpi` is empty");
	}


	cJSON *acpi = NULL;
	cJSON_ArrayForEach(acpi, acpiAttr){
		RTNode *acp = NULL;
		acp = find_rtnode_by_uri(rt->cb, acpi->valuestring);
		if(!acp){
			return handle_error(o2pt, RSC_BAD_REQUEST, "resource `acp` is not found");
		}else if(op == OP_UPDATE){
			int acop = 0;
			acop = (acop | get_acop_origin(o2pt, acp, 1));
			logger("UTIL", LOG_LEVEL_DEBUG, "acop-pvs : %d, op : %d", acop, op);
			if(!strcmp(o2pt->fr, "CAdmin")){

			}
			else if((acop & op) != op){
				handle_error(o2pt, RSC_ORIGINATOR_HAS_NO_PRIVILEGE, "originator has no privilege");
				return RSC_ORIGINATOR_HAS_NO_PRIVILEGE;
			}
		}
	}
	

	return RSC_OK;
}

/**
 * @brief validate acp resource
 * @param o2pt oneM2M request primitive
 * @param acp acp attribute cJSON object
 * @param op operation type
 * @return 0 if valid, -1 if invalid
*/
int validate_acp(oneM2MPrimitive *o2pt, cJSON *acp, Operation op){
	cJSON *pjson = NULL;
	char *ptr = NULL;
	if(!acp) {
		return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
	}
	if(op == OP_CREATE){
		pjson = cJSON_GetObjectItem(acp, "pv");
		if(!pjson){
			return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
		}else if(pjson->type == cJSON_NULL || !cJSON_GetObjectItem(pjson, "acr")){
			return handle_error(o2pt, RSC_BAD_REQUEST, "empty `pv` is not allowed");
		}

		pjson = cJSON_GetObjectItem(acp, "pvs");
		if(!pjson){
			return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
		}else if( cJSON_IsNull(pjson) || !cJSON_GetObjectItem(pjson, "acr")){
			return handle_error(o2pt, RSC_BAD_REQUEST, "empty `pvs` is not allowed");
		}
	}
	if(op == OP_UPDATE){
		pjson = cJSON_GetObjectItem(acp, "pv");
		if(pjson){
			if( cJSON_IsNull(pjson) || !cJSON_GetObjectItem(pjson, "acr")){
				return handle_error(o2pt, RSC_BAD_REQUEST, "empty `pv` is not allowed");
			}
		}
		pjson = cJSON_GetObjectItem(acp, "pvs");
		if (pjson){
			if( cJSON_IsNull(pjson) || !cJSON_GetObjectItem(pjson, "acr")){
				return handle_error(o2pt, RSC_BAD_REQUEST, "empty `pvs` is not allowed");
			}
		}
	}

	return RSC_OK;
}

int validate_ae(oneM2MPrimitive *o2pt, cJSON *ae, Operation op){
	cJSON *pjson = NULL;
	char *ptr = NULL;
	if(!ae) {
		handle_error(o2pt, RSC_CONTENTS_UNACCEPTABLE, "insufficient mandatory attribute(s)");
		return RSC_CONTENTS_UNACCEPTABLE;
	}
	if(op == OP_CREATE){
		pjson = cJSON_GetObjectItem(ae, "api");
		if(!pjson){
			handle_error(o2pt, RSC_CONTENTS_UNACCEPTABLE, "insufficient mandatory attribute(s)");
			return RSC_CONTENTS_UNACCEPTABLE;
		}
		ptr = pjson->valuestring;
		if(ptr[0] != 'R' && ptr[0] != 'N') {
			handle_error(o2pt, RSC_BAD_REQUEST, "attribute `api` prefix is invalid");
			return RSC_BAD_REQUEST;
		}
	}
	pjson = cJSON_GetObjectItem(ae, "acpi");
	if(pjson){
		int result = validate_acpi(o2pt, pjson, op);
		if(result != RSC_OK) return result;
	}

	if(op == OP_UPDATE){
		if(pjson && cJSON_GetArraySize(pjson) > 1){
			handle_error(o2pt, RSC_BAD_REQUEST, "only attribute `acpi` is allowed when updating `acpi`");
			return RSC_BAD_REQUEST;
		}
	}

	return RSC_OK;
}

int validate_cnt(oneM2MPrimitive *o2pt, cJSON *cnt, Operation op){
	cJSON *pjson = NULL;
	char *ptr = NULL;
	if(!cnt) {
		handle_error(o2pt, RSC_CONTENTS_UNACCEPTABLE, "insufficient mandatory attribute(s)");
		return RSC_CONTENTS_UNACCEPTABLE;
	}
	
	pjson = cJSON_GetObjectItem(cnt, "acpi");
	if(pjson){
		int result = validate_acpi(o2pt, pjson, op);
		if(result != RSC_OK) return result;
	}

	if(op == OP_UPDATE){
		if(pjson && cJSON_GetArraySize(pjson) > 1){
			handle_error(o2pt, RSC_BAD_REQUEST, "only attribute `acpi` is allowed when updating `acpi`");
			return RSC_BAD_REQUEST;
		}
	}
	
	pjson = cJSON_GetObjectItem(cnt, "mni");
	if(pjson && pjson->valueint < 0){
		handle_error(o2pt, RSC_BAD_REQUEST, "attribute `mni` is invalid");
		return RSC_BAD_REQUEST;
	}
	
	pjson = cJSON_GetObjectItem(cnt, "mbs");
	if(pjson && pjson->valueint < 0) {
		handle_error(o2pt, RSC_BAD_REQUEST, "attribute `mbs` is invalid");
		return RSC_BAD_REQUEST;
	}

	return RSC_OK;
}

int validate_cin(oneM2MPrimitive *o2pt, cJSON *parent_cnt, cJSON *cin, Operation op){
	cJSON *pjson = NULL, *pjson2 = NULL;
	char *ptr = NULL;

	if(!cin) {
		handle_error(o2pt, RSC_CONTENTS_UNACCEPTABLE, "insufficient mandatory attribute(s)");
		return RSC_CONTENTS_UNACCEPTABLE;
	}

	pjson = cJSON_GetObjectItem(cin, "con");
	if(pjson && cJSON_IsNumber(pjson)){
		handle_error(o2pt, RSC_BAD_REQUEST, "attribute `con` cannot be number");
		return RSC_BAD_REQUEST;
	}

	cJSON *mbs = NULL;
	cJSON *cs = NULL;
	if(mbs = cJSON_GetObjectItem(parent_cnt, "mbs")){
		logger("UTIL", LOG_LEVEL_DEBUG, "mbs %d", mbs->valueint);
		if(cs = cJSON_GetObjectItem(cin, "cs")){
			logger("UTIL", LOG_LEVEL_DEBUG, "cs %d", cs->valueint);
			if(mbs->valueint >= 0 && cs->valueint > mbs->valueint) {
				handle_error(o2pt, RSC_NOT_ACCEPTABLE, "contentInstance size exceed `mbs`");
				return RSC_NOT_ACCEPTABLE;
			}
		}
	}
	

	return RSC_OK;
}

int validate_sub(oneM2MPrimitive *o2pt, cJSON *sub, Operation op){
	cJSON *pjson = NULL;
	char *ptr = NULL;

	if(!sub) {
		handle_error(o2pt, RSC_CONTENTS_UNACCEPTABLE, "insufficient mandatory attribute(s)");
		return RSC_CONTENTS_UNACCEPTABLE;
	}

	pjson = cJSON_GetObjectItem(sub, "exc");
	if(pjson && pjson->type != cJSON_Number){
		handle_error(o2pt, RSC_BAD_REQUEST, "attribute `exc` should be number");
		return RSC_BAD_REQUEST;
	}

	pjson = cJSON_GetObjectItem(sub, "nu");
	if(pjson && pjson->type != cJSON_Array){
		handle_error(o2pt, RSC_BAD_REQUEST, "attribute `nu` should be Array");
		return RSC_BAD_REQUEST;
	}

	pjson = cJSON_GetObjectItem(sub, "nct");
	if(pjson && pjson->type != cJSON_Number){
		handle_error(o2pt, RSC_BAD_REQUEST, "attribute `nct` should be number");
		return RSC_BAD_REQUEST;
	}


	return RSC_OK;
}

int validate_csr(oneM2MPrimitive *o2pt, RTNode *parent_rtnode, cJSON *csr, Operation op){
	char *mandatory[4] = {""}; // TODO - Add mandatory check
	char *optional[5] = {""}; // TODO - Add optional check
	cJSON *pjson = NULL;
	char *ptr = NULL;

	char *csi = NULL;

	if(pjson = cJSON_GetObjectItem(csr, "csi")){
		csi = pjson->valuestring;
		if(check_csi_duplicate(csi, parent_rtnode) == -1){
			handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "originator has already registered");
			return o2pt->rsc;
		}
	}else{
		return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
	}

	if(pjson = cJSON_GetObjectItem(csr, "cb")){
		// TODO - check cb
	}else{
		return handle_error(o2pt, RSC_BAD_REQUEST, "insufficient mandatory attribute(s)");
	}
	

	return RSC_OK;
}

#if SERVER_TYPE == MN_CSE
bool isCSEAvailable(){

	return true;
}

#endif