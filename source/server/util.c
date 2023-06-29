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
	logger("O2M", LOG_LEVEL_DEBUG, "Call parse_uri");
	
	char uri_array[MAX_URI_SIZE];
	char *uri_parse = uri_array;
	char *fopt_buf = NULL;
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
	RTNode* rtnode = find_rtnode_by_uri(cb, uri_array);
	
	if(rtnode && !o2pt->isFopt && latest_oldest_flag != -1) rtnode = find_latest_oldest(rtnode, latest_oldest_flag);

	if(index_start == 1) o2pt->op = OP_VIEWER;

	return rtnode;
}

RTNode *find_rtnode_by_uri(RTNode *cb, char *target_uri) {
	RTNode *rtnode = cb, *parent_rtnode = NULL;
	target_uri = strtok(target_uri, "/");

	if(!target_uri) return NULL;

	char ri[64];
	strcpy(ri, target_uri);

	char uri_array[64][MAX_URI_SIZE];
	int index = -1;

	while(target_uri) {
		strcpy(uri_array[++index], target_uri);
		target_uri = strtok(NULL, "/");
	}

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
		CIN *cin = db_get_cin(uri_array[index]);
		if(cin) {
			if(!strcmp(cin->pi, get_ri_rtnode(parent_rtnode))) {
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


RTNode *find_latest_oldest(RTNode* rtnode, int flag) {
	logger("UTL", LOG_LEVEL_DEBUG, "latest oldest");
	RTNode *cin_rtnode = NULL;
	#ifdef SQLITE_DB
	if(rtnode->ty == RT_CNT) {
		CIN *cin_object = db_get_cin_laol(rtnode, flag);
		cin_rtnode = create_rtnode(cin_object, RT_CIN);
		cin_rtnode->parent = rtnode;
	}
	#else 
	if(rtnode->ty == RT_CNT) {
		RTNode *head = db_get_cin_rtnode_list(rtnode);
		RTNode *cin_rtnode = head;

		if(cin_rtnode) {
			if(flag == 1) {
				head = head->sibling_right;
				cin_rtnode->sibling_right = NULL;			
			} else {
				while(cin_rtnode->sibling_right) cin_rtnode = cin_rtnode->sibling_right;
				if(cin_rtnode->sibling_left) {
					cin_rtnode->sibling_left->sibling_right = NULL;
					cin_rtnode->sibling_left = NULL;
				}
			}
			if(head != cin_rtnode) free_rtnode_list(head);
			if(cin_rtnode) {
				return cin_rtnode;
			} else {
				return NULL;
			}
		}
		return NULL;
	}
	#endif

	return cin_rtnode;
}

int net_to_bit(char *net) {
	int net_size = strlen(net);
	int ret = 0;

	for(int i=0; i<net_size; i++) {
		int exp = atoi(net+i);
		if(exp > 0) ret = (ret | (int)pow(2, exp - 1));
	}

	return ret;
}

int add_child_resource_tree(RTNode *parent, RTNode *child) {
	RTNode *node = parent->child;
	child->parent = parent;

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

	#ifdef BERKELEY_DB
	child->uri = malloc(strlen(parent->uri) + strlen(get_rn_rtnode(child)) + 2);
	strcpy(child->uri, parent->uri);
	strcat(child->uri, "/");
	strcat(child->uri, get_rn_rtnode(child));
	#endif

	return 1;
}

ResourceType http_parse_object_type() {
	char *content_type = request_header("Content-Type");
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
	CNT *cnt = (CNT *) rtnode->obj;
	if(cnt->cni <= cnt->mni && cnt->cbs <= cnt->mbs) return;

	RTNode *head = db_get_cin_rtnode_list(rtnode);
	RTNode *right;

	while((cnt->mni >= 0 && cnt->cni > cnt->mni) || (cnt->mbs >= 0 && cnt->cbs > cnt->mbs)) {
		if(head) {
			logger("UT", LOG_LEVEL_DEBUG, "%d", head->ty);
			right = head->sibling_right;
			db_delete_onem2m_resource(head);
			cnt->cbs -= ((CIN *)head->obj)->cs;
			cnt->cni--;
			free_rtnode(head);
			head = right;
		} else {
			break;
		}
	}

	if(head) free_rtnode_list(head);
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
	
	char *la = strstr(qs,"la=");
	if(la) cinSize = atoi(la+3);
	
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
	logger("UTIL", LOG_LEVEL_DEBUG, "Run time : %lf", end-start);
}


void init_server() {
	// if(SERVER_TYPE == MN_CSE){
	// 	logger("UTIL", LOG_LEVEL_DEBUG, "MN-CSE");
		
		
	// } else if(SERVER_TYPE == IN_CSE){
	// 	logger("UTIL", LOG_LEVEL_DEBUG, "IN-CSE");
	// }

	rt = (ResourceTree *)calloc(1, sizeof(rt));
	
	CSE *cse;

	cse = db_get_cse(CSE_BASE_RI);

	if(!cse){
		cse = calloc(1, sizeof(CSE));
		init_cse(cse);
		db_store_cse(cse);
	}

	rt->cb = create_rtnode(cse, RT_CSE);
	rt->cb->uri = CSE_BASE_NAME;

 	init_resource_tree();
}

void init_resource_tree(){
	RTNode *rtnode_list = (RTNode *)calloc(1,sizeof(RTNode));
	RTNode *tail = rtnode_list;
	
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
				logger("ACP", LOG_LEVEL_DEBUG, "getacop : %d, acop : %d", get_acop(o2pt, rtnode), acop);
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

int get_acop(oneM2MPrimitive *o2pt, RTNode *rtnode) {
	char origin[64];
	int acop = 0;
	
	if(o2pt->fr) {
		strcpy(origin, o2pt->fr);
	} else {
		strcpy(origin, "all");
	}

    if(!strcmp(origin, "CAdmin")) return ALL_ACOP;

	if(rtnode->ty == RT_ACP) {
		acop = (acop | get_acop_origin(origin, rtnode, 1));
		acop = (acop | get_acop_origin("all", rtnode, 1));
		return acop;
	}

	char *acpi = get_acpi_rtnode(rtnode);
    if(!acpi) return 0;

	RTNode *cb = rtnode;
	while(cb->parent) cb = cb->parent;
	
	int uri_cnt = 0;
	char arr_acp_uri[512][1024] = {"\0", }, arr_acpi[MAX_ATTRIBUTE_SIZE] = "\0";
	char *acp_uri = NULL;

	strcpy(arr_acpi, acpi);

	acp_uri = strtok(arr_acpi, ",");

	while(acp_uri) { 
		strcpy(arr_acp_uri[uri_cnt++],acp_uri);
		acp_uri = strtok(NULL, ",");
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

int get_acop_origin(char *origin, RTNode *acp_rtnode, int flag) {
	if(!origin) return 0;

	int ret_acop = 0, cnt = 0;
	char *acor, *acop, arr_acor[1024], arr_acop[1024];
	ACP *acp = (ACP *)acp_rtnode->obj;

	if(flag) {
		if(!acp->pvs_acor) {
			return 0;
		}
		strcpy(arr_acor, acp->pvs_acor);
		strcpy(arr_acop, acp->pvs_acop);
	} else {
		if(!acp->pv_acor) {
			return 0;
		}
		strcpy(arr_acor, acp->pv_acor);
		strcpy(arr_acop, acp->pv_acop);
	}

	int size = 0;
	acor = strtok(arr_acor, ",");

	while(acor) {
		if(!strcmp(acor, origin)) break;
		acor = strtok(NULL, ",");
        size++;
	}

	if(acor) {
		acop = strtok(arr_acop, ",");
		for(int i=0; i<size; i++) {
			acop = strtok(NULL, ",");
		}
	}
	logger("ACP", LOG_LEVEL_DEBUG, "acor :%s, origin : %s", acor, origin);
	//if(acop) logger("ACP", LOG_LEVEL_DEBUG, "acop : %s", acop);
	if(acor && acop) ret_acop = (ret_acop | atoi(acop));
	logger("ACP", LOG_LEVEL_DEBUG, "retacop : %d", ret_acop);
	return ret_acop;
}

int check_rn_duplicate(oneM2MPrimitive *o2pt, RTNode *rtnode) {
	if(!rtnode) return 0;
	cJSON *root = o2pt->cjson_pc;
	cJSON *resource, *rn;

	switch(o2pt->ty) {
		case RT_AE: resource = cJSON_GetObjectItem(root, "m2m:ae"); break;
		case RT_CNT: resource = cJSON_GetObjectItem(root, "m2m:cnt"); break;
		case RT_CIN: resource = cJSON_GetObjectItem(root, "m2m:cin"); break;
		case RT_SUB: resource = cJSON_GetObjectItem(root, "m2m:sub"); break;
		case RT_GRP: resource = cJSON_GetObjectItem(root, "m2m:grp"); break;
		case RT_ACP: resource = cJSON_GetObjectItem(root, "m2m:acp"); break;
	}

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
		if(!strcmp(get_ri_rtnode(child), aei)) {
			handle_error(o2pt, RSC_ORIGINATOR_HAS_ALREADY_REGISTERD, "attribute `aei` is duplicated");
			return -1;
		}
		child = child->sibling_right;
	}

	return 0;
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

	switch(ty) {
		case RT_AE: resource = cJSON_GetObjectItem(root, "m2m:ae"); break;
		case RT_CNT: resource = cJSON_GetObjectItem(root, "m2m:cnt"); break;
		case RT_SUB: resource = cJSON_GetObjectItem(root, "m2m:sub"); break;
		case RT_ACP: resource = cJSON_GetObjectItem(root, "m2m:acp"); break;
		case RT_GRP: resource = cJSON_GetObjectItem(root, "m2m:grp"); break;
	}

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

void handle_error(oneM2MPrimitive *o2pt, int rsc, char *err){
	logger("UTIL", LOG_LEVEL_ERROR, err);
	o2pt->rsc = rsc;
	o2pt->errFlag = true;
	char pc[MAX_PAYLOAD_SIZE];
	sprintf(pc, "{\"m2m:dbg\": \"%s\"}", err);
	set_o2pt_pc(o2pt, pc);
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

int validate_grp(GRP *grp){
	logger("UTIL", LOG_LEVEL_DEBUG, "Validating GRP");
	int rsc = 0;
	bool hasFopt = false;
	bool isLocalResource = true;
	char *mid = NULL;
	char *p = NULL;
	char *tStr = NULL;

	RTNode *rt_node;
	GRP *pGrp;

	if(grp->mtv) return 1;

	for(int i = 0 ; i < grp->mnm; i++){
		mid = grp->mid[i];
		if(!mid) break;

		// Check is local resource
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
				if(grp->mt != 0 && rt_node->ty != grp->mt)
					if(handle_csy(grp, i)) return RSC_GROUPMEMBER_TYPE_INCONSISTENT;
			}else{
				logger("UTIL", LOG_LEVEL_DEBUG, "GRP member %s not present", tStr);
				if(handle_csy(grp, i)) return RSC_GROUPMEMBER_TYPE_INCONSISTENT;
			}

			if(rt_node && rt_node->ty == RT_GRP){
				//pGrp = db_get_grp(rt_node->ri);
				pGrp = (GRP*)rt_node->obj;
				if((rsc = validate_grp(pGrp)) >= 4000 ){
					free_grp(pGrp);
					if(handle_csy(grp, i)) return RSC_GROUPMEMBER_TYPE_INCONSISTENT;
				}
				//free_grp(pGrp);
			}

			free(tStr);
			tStr = NULL;
		}else{
			return RSC_NOT_IMPLEMENTED;
		}


	}

	grp->mtv = true;
	return RSC_OK;
}

int handle_csy(GRP *grp, int i){
	switch(grp->csy){
		case CSY_ABANDON_MEMBER:
			remove_mid(grp->mid, i, grp->cnm);
			grp->cnm--;
			break;
		case CSY_ABANDON_GROUP:
			free_grp(grp);
			return RSC_GROUPMEMBER_TYPE_INCONSISTENT;

		case CSY_SET_MIXED:
			grp->mt = RT_MIXED;
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
		case RSC_MAX_NUMBER_OF_MEMBER_EXCEEDED:
			return 400;
		
		case RSC_OPERATION_NOT_ALLOWED:
			return 403;

		case RSC_INTERNAL_SERVER_ERROR:
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
		free_fc(o2pt->fc);
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
	(*dest)->prot = src->prot;
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
	char *pi = NULL;
	
	switch(rtnode->ty) {
		case RT_CSE: pi = ((CSE *)rtnode->obj)->pi; break;
		case RT_AE: pi = ((AE *)rtnode->obj)->pi; break;
		case RT_CNT: pi = ((CNT *)rtnode->obj)->pi; break;
		case RT_CIN: pi = ((CIN *)rtnode->obj)->pi; break;
		case RT_SUB: pi = ((SUB *)rtnode->obj)->pi; break;
		case RT_ACP: pi = ((ACP *)rtnode->obj)->pi; break;
		case RT_GRP: pi = ((GRP *)rtnode->obj)->pi; break;
	}

	return pi;
}

char *get_ri_rtnode(RTNode *rtnode) {
	char *ri = NULL;
	
	switch(rtnode->ty) {
		case RT_CSE: ri = ((CSE *)rtnode->obj)->ri; break;
		case RT_AE: ri = ((AE *)rtnode->obj)->ri; break;
		case RT_CNT: ri = ((CNT *)rtnode->obj)->ri; break;
		case RT_CIN: ri = ((CIN *)rtnode->obj)->ri; break;
		case RT_SUB: ri = ((SUB *)rtnode->obj)->ri; break;
		case RT_ACP: ri = ((ACP *)rtnode->obj)->ri; break;
		case RT_GRP: ri = ((GRP *)rtnode->obj)->ri; break;
	}

	return ri;
}

char *get_rn_rtnode(RTNode *rtnode) {
	char *rn = NULL;
	
	switch(rtnode->ty) {
		case RT_CSE: rn = ((CSE *)rtnode->obj)->rn; break;
		case RT_AE: rn = ((AE *)rtnode->obj)->rn; break;
		case RT_CNT: rn = ((CNT *)rtnode->obj)->rn; break;
		case RT_CIN: rn = ((CIN *)rtnode->obj)->rn; break;
		case RT_SUB: rn = ((SUB *)rtnode->obj)->rn; break;
		case RT_ACP: rn = ((ACP *)rtnode->obj)->rn; break;
		case RT_GRP: rn = ((GRP *)rtnode->obj)->rn; break;
	}

	return rn;
}

char *get_acpi_rtnode(RTNode *rtnode) {
	char *acpi = NULL;
	
	switch(rtnode->ty) {
		//case RT_CSE: acpi = ((CSE *)rtnode->obj)->acpi; break;
		case RT_AE: acpi = ((AE *)rtnode->obj)->acpi; break;
		case RT_CNT: acpi = ((CNT *)rtnode->obj)->acpi; break;
		// case RT_CIN: acpi = ((CIN *)rtnode->obj)->acpi; break;
		// case RT_SUB: acpi = ((SUB *)rtnode->obj)->acpi; break;
		// case RT_ACP: acpi = ((ACP *)rtnode->obj)->acpi; break;
		// case RT_GRP: acpi = ((GRP *)rtnode->obj)->acpi; break;
	}

	return acpi;
}

char *get_ct_rtnode(RTNode *rtnode){
	char *ct = NULL;
	switch((rtnode->ty)){
		case RT_CSE: ct = ((CSE *)rtnode->obj)->ct; break;
		case RT_AE: ct = ((AE *)rtnode->obj)->ct; break;
		case RT_CNT: ct = ((CNT *)rtnode->obj)->ct; break;
		case RT_CIN: ct = ((CIN *)rtnode->obj)->ct; break;
		case RT_SUB: ct = ((SUB *)rtnode->obj)->ct; break;
		case RT_ACP: ct = ((ACP *)rtnode->obj)->ct; break;
		case RT_GRP: ct = ((GRP *)rtnode->obj)->ct; break;
	}

	return ct;
}

char *get_et_rtnode(RTNode *rtnode){
	char *et = NULL;
	switch((rtnode->ty)){
		case RT_CSE: et = NULL; break;
		case RT_AE: et = ((AE *)rtnode->obj)->et; break;
		case RT_CNT: et = ((CNT *)rtnode->obj)->et; break;
		case RT_CIN: et = ((CIN *)rtnode->obj)->et; break;
		case RT_SUB: et = ((SUB *)rtnode->obj)->et; break;
		case RT_ACP: et = ((ACP *)rtnode->obj)->et; break;
		case RT_GRP: et = ((GRP *)rtnode->obj)->et; break;
	}

	return et;
}

char *get_lt_rtnode(RTNode *rtnode){
	char *lt = NULL;
	switch((rtnode->ty)){
		case RT_CSE: lt = ((CSE *)rtnode->obj)->lt; break;
		case RT_AE: lt = ((AE *)rtnode->obj)->lt; break;
		case RT_CNT: lt = ((CNT *)rtnode->obj)->lt; break;
		case RT_CIN: lt = ((CIN *)rtnode->obj)->lt; break;
		case RT_SUB: lt = ((SUB *)rtnode->obj)->lt; break;
		case RT_ACP: lt = ((ACP *)rtnode->obj)->lt; break;
		case RT_GRP: lt = ((GRP *)rtnode->obj)->lt; break;
	}

	return lt;
}

char *get_lbl_rtnode(RTNode *rtnode){
	char *lbl = NULL;
	switch ((rtnode->ty)){
		case RT_CSE:
		case RT_ACP:
		case RT_CIN:
		case RT_GRP:
			break;
		
		case RT_AE:
			lbl = ((AE *) rtnode->obj)->lbl;
			break;
		case RT_CNT:
			lbl = ((CNT *) rtnode->obj)->lbl;
			break;
		default:
			break;
	}
	return lbl;
}

int get_st_rtnode(RTNode *rtnode){
	if(rtnode->ty != RT_CNT)
		return -1;

	return ((CNT *) rtnode->obj)->st;
}

int get_cs_rtnode(RTNode *rtnode){
	if(rtnode->ty != RT_CIN)
		return -1;

	return ((CIN *) rtnode->obj)->cs;
}

char *get_uri_rtnode(RTNode *rtnode){
	switch(rtnode->ty){
		case RT_CSE:
			return ((CSE *) rtnode->obj)->uri;
		case RT_AE:
			return ((AE *) rtnode->obj)->uri;
		case RT_CNT:
			return ((CNT *) rtnode->obj)->uri;
		case RT_CIN:
			return ((CIN *) rtnode->obj)->uri;
		case RT_ACP:
			return ((ACP *) rtnode->obj)->uri;
		case RT_SUB:
			return ((SUB *) rtnode->obj)->uri;
		case RT_GRP:
			return ((GRP *) rtnode->obj)->uri;
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
			if((get_acop_origin(o2pt->fr, acp_rtnode, 1) & ACOP_UPDATE) != ACOP_UPDATE) {
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
	SUB *sub = (SUB *)sub_rtnode->obj;
	int uri_len = 0, index = 0;
	char *noti_json = cJSON_PrintUnformatted(noti_cjson);
	char *p = NULL;
	char port[10] = {'\0'};
	NotiTarget *nt = NULL;

	logger("NOTI", LOG_LEVEL_DEBUG, "%d, %s", sub->net_bit, sub->net);

	if(sub->net_bit <= 0) {
		sub->net_bit = net_to_bit(sub->net);
	}
	int net_bit = (int)pow(2, net-1);
	if(sub->net_bit & net_bit != net_bit) {
		return;
	}

	char nu[4096];
	strcpy(nu, sub->nu);

	char *noti_uri = strtok(nu, ",");

	while(noti_uri) {
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

#ifndef SQLITE_DB
cJSON *fc_scan_resource_tree(RTNode *rtnode, FilterCriteria *fc, int lvl){
	
	RTNode *prt = rtnode;
	RTNode *trt = NULL;
	RTNode *cinrtHead = NULL;
	cJSON *uril = cJSON_CreateArray();
	cJSON *curil = NULL;
	cJSON *pjson = NULL;
	char buf[512] = {0};
	int curilSize = 0;
	while(prt){
		if(prt->ty == RT_CIN){
			if(prt->sibling_right){
				prt->sibling_right->parent = prt->parent;
			}
		}
		// If resource is cnt, Construct RTNode of child cin and attach it
		if(prt->ty == RT_CNT){
			cinrtHead = trt = db_get_cin_rtnode_list(prt);
			prt->child = cinrtHead;
		}
		// Check if resource satisfies filter
		if(isResourceAptFC(prt, fc)){
			if(fc->arp){
				sprintf(buf, "%s/%s", get_rn_rtnode(prt), fc->arp);
				trt = find_rtnode_by_uri(prt, buf);
				if(trt) cJSON_AddItemToArray(uril, cJSON_CreateString(trt->uri));
				buf[0] = '\0';
			}else{
				cJSON_AddItemToArray(uril, cJSON_CreateString(prt->uri));
			}
			
		}

		// Check child resources if available
		if(fc->lvl - lvl > 0 && prt->child ){
			curil = fc_scan_resource_tree(prt->child, fc, lvl+1);

			curilSize = cJSON_GetArraySize(curil);
			for(int i = 0 ; i < curilSize ; i++){
				pjson = cJSON_GetArrayItem(curil, i);
				cJSON_AddItemToArray(uril, cJSON_CreateString(pjson->valuestring));
			}
			cJSON_Delete(curil);
			curil = NULL;
			
		}

		// Remove attatched rtnode of CIN when available
		if(cinrtHead){
			free_rtnode_list(cinrtHead);
			prt->child = NULL;
		}
		cinrtHead = NULL;
		prt = prt->sibling_right;
	}

	return uril;
}
#endif


#if SERVER_TYPE == MN_CSE
bool isCSEAvailable(){

	return true;
}

#endif