#include "onem2m.h"
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

extern char response_headers[1024];

Node* parse_uri(Node *cb, char *uri, Operation *op) {
	fprintf(stderr,"parse_uri \x1b[33m%s\x1b[0m...",uri);
	char uri_array[MAX_URI_SIZE];
	char *uri_parse = uri_array;
	strcpy(uri_array, uri);

	char uri_strtok[64][MAX_URI_SIZE] = {"\0", };
	int index_start = 0, index_end = -1;

	uri_parse = strtok(uri_array, "/");

	while(uri_parse) {
		strcpy(uri_strtok[++index_end], uri_parse);
		uri_parse = strtok(NULL, "/");
	}

	if(!strcmp(uri_strtok[0], "viewer")) index_start++;
	if(!strcmp(uri_strtok[index_end], "la") || !strcmp(uri_strtok[index_end], "latest")) {
		*op = OP_LATEST; index_end--;
	} else if(!strcmp(uri_strtok[index_end], "ol") || !strcmp(uri_strtok[index_end], "oldest")) {
		*op = OP_OLDEST; index_end--;
	}

	strcpy(uri_array, "\0");
	for(int i=index_start; i<=index_end; i++) {
		strcat(uri_array,"/"); strcat(uri_array,uri_strtok[i]);
	}
	Node* node = find_node_by_uri(cb, uri_array);
	
	if(node && (*op == OP_LATEST || *op == OP_OLDEST)) node = find_latest_oldest(node, op);

	if(index_start == 1) *op = OP_VIEWER;

	return node;
}

Node *find_latest_oldest(Node* node, Operation *op) {
	if(node->ty == TY_CNT) {
		Node *head = db_get_cin_list_by_pi(node->ri);
		Node *cin = head;

		if(cin) {
			if(*op == OP_OLDEST) {
				head = head->sibling_right;
				cin->sibling_right = NULL;
			} else {
				while(cin->sibling_right) cin = cin->sibling_right;
				if(cin->sibling_left) cin->sibling_left->sibling_right = NULL;
				cin->sibling_left = NULL;
			}
			if(head != cin) free_node_list(head);
			*op = OP_NONE;
			if(cin) cin->parent = node;
			return cin;
		}
	} else if(node->ty == TY_AE){
		node = node->child;
		while(node) {
			if(node->ty == TY_CNT) break;
			node = node->sibling_right;
		}
		if(node && *op == OP_LATEST) {
			while(node->sibling_right && node->sibling_right->ty == TY_CNT) {
				node = node->sibling_right;
			}
		}
		*op = OP_NONE;
		return node;
	}
	return NULL;
}

Operation parse_operation(){
	Operation op;

	if(strcmp(method, "POST") == 0) op = OP_CREATE;
	else if(strcmp(method, "GET") == 0) op = OP_RETRIEVE;
	else if (strcmp(method, "PUT") == 0) op = OP_UPDATE;
	else if (strcmp(method, "DELETE") == 0) op = OP_DELETE;
	else if (strcmp(method, "OPTIONS") == 0) op = OP_OPTIONS;

	return op;
}

int find_same_resource_name(Node *pnode) {
	Node* node = pnode->child;
	char* rn = get_json_value_char("rn",payload);
	if(!rn) return 0;

	while(node) {
		if(!strcmp(node->rn, rn)) return 1;
		node = node->sibling_right;
	}

	return 0;
}

void tree_viewer_api(Node *node) {
	fprintf(stderr,"\x1b[43mTree Viewer API\x1b[0m\n");
	char arr_viewer_data[MAX_TREE_VIEWER_SIZE] = "[";
	char *viewer_data = arr_viewer_data;
	
	Node *p = node;
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

	respond_to_client(200, res);
}

void tree_viewer_data(Node *node, char **viewer_data, int cin_size) {
	char *json = node_to_json(node);

	strcat(*viewer_data, ",");
	strcat(*viewer_data, json);
	free(json); json = NULL;

	Node *child = node->child;
	while(child) {
		tree_viewer_data(child, viewer_data, cin_size);
		child = child->sibling_right;
	}

	if(node->ty != TY_SUB && node->ty != TY_ACP) {
		Node *cin_list_head = db_get_cin_list_by_pi(node->ri);

		if(cin_list_head) cin_list_head = latest_cin_list(cin_list_head, cin_size);

		Node *p = cin_list_head;

		while(p) {
			json = node_to_json(p);
			strcat(*viewer_data, ",");
			strcat(*viewer_data, json);
			free(json); json = NULL;
			p = p->sibling_right;		
		}
		free_node_list(cin_list_head);
	}
}

Node *latest_cin_list(Node* cinList, int num) {
	Node *head, *tail;
	head = tail = cinList;
	int cnt = 1;
	
	while(tail->sibling_right) {
		tail = tail->sibling_right;
		cnt++;
	}
	
	for(int i=0; i < cnt-num; i++) {
		head = head->sibling_right;
		free_node(head->sibling_left); head->sibling_left = NULL;
	}
	
	return head;
}

void normalization_payload() {
	int index = 0;

	for(int i=0; i<payload_size; i++) {
		if(is_json_valid_char(payload[i])) {
			payload[index++] =  payload[i];
		}
	}

	payload[index] = '\0';
}

ObjectType parse_object_type() {
	char *ct = request_header("Content-Type");
	if(!ct) return 0;
	char *qs_ty = strstr(ct, "ty=");
	if(!qs_ty) return 0;
	int obj_ty = atoi(qs_ty+3);

	ObjectType ty;
	
	switch(obj_ty) {
	case 1 : ty = TY_ACP; break;
	case 2 : ty = TY_AE; break;
	case 3 : ty = TY_CNT; break;
	case 4 : ty = TY_CIN; break;
	case 5 : ty = TY_CSE; break;
	case 23 : ty = TY_SUB; break;
	}
	
	return ty;
}

ObjectType parse_object_type_in_request_body() {
	ObjectType ty;
	
	char *cse, *ae, *cnt, *cin, *sub, *acp;
	
	cse = strstr(payload, "m2m:cb");
	ae = strstr(payload, "m2m:ae");
	cnt = strstr(payload, "m2m:cnt");
	cin = strstr(payload, "m2m:cin");
	sub = strstr(payload, "m2m:sub");
	acp = strstr(payload, "m2m:acp");
	
	if(cse) ty = TY_CSE;
	else if(ae) ty = TY_AE;
	else if(cnt) ty = TY_CNT;
	else if(cin) ty = TY_CIN;
	else if(sub) ty = TY_SUB;
	else if(acp) ty = TY_ACP;
	
	return ty;
}

Node* create_node(void *obj, ObjectType ty){
	Node* node = NULL;

	switch(ty) {
	case TY_CSE: node = create_cse_node((CSE*)obj); break;
	case TY_AE: node = create_ae_node((AE*)obj); break;
	case TY_CNT: node = create_cnt_node((CNT*)obj); break;
	case TY_CIN: node = create_cin_node((CIN*)obj); break;
	case TY_SUB: node = create_sub_node((Sub*)obj); break;
	case TY_ACP: node = create_acp_node((ACP*)obj); break;
	}

	node->parent = NULL;
	node->child = NULL;
	node->sibling_left = NULL;
	node->sibling_right = NULL;
	
	fprintf(stderr,"OK\n");
	
	return node;
}

Node* create_cse_node(CSE *cse) {
	fprintf(stderr,"Create Tree Node\n[rn] %s\n[ri] %s...",cse->rn, cse->ri);

	Node* node = calloc(1, sizeof(Node));

	node->rn = (char*)malloc((strlen(cse->rn) + 1) * sizeof(char));
	node->ri = (char*)malloc((strlen(cse->ri) + 1) * sizeof(char));
	node->pi = (char*)malloc((strlen(cse->pi) + 1) * sizeof(char));
	
	strcpy(node->rn, cse->rn);
	strcpy(node->ri, cse->ri);
	strcpy(node->pi, cse->pi);

	node->ty = TY_CSE;

	return node;
}

Node* create_ae_node(AE *ae) {
	fprintf(stderr,"Create Tree Node\n[rn] %s\n[ri] %s...",ae->rn, ae->ri);

	Node* node = calloc(1, sizeof(Node));

	node->rn = (char*)malloc((strlen(ae->rn) + 1) * sizeof(char));
	node->ri = (char*)malloc((strlen(ae->ri) + 1) * sizeof(char));
	node->pi = (char*)malloc((strlen(ae->pi) + 1) * sizeof(char));
	
	strcpy(node->rn, ae->rn);
	strcpy(node->ri, ae->ri);
	strcpy(node->pi, ae->pi);

	node->ty = TY_AE;

	return node;
}

Node* create_cnt_node(CNT *cnt) {
	fprintf(stderr,"Create Tree Node\n[rn] %s\n[ri] %s...",cnt->rn, cnt->ri);

	Node* node = calloc(1, sizeof(Node));

	node->rn = (char*)malloc((strlen(cnt->rn) + 1) * sizeof(char));
	node->ri = (char*)malloc((strlen(cnt->ri) + 1) * sizeof(char));
	node->pi = (char*)malloc((strlen(cnt->pi) + 1) * sizeof(char));
	
	strcpy(node->rn, cnt->rn);
	strcpy(node->ri, cnt->ri);
	strcpy(node->pi, cnt->pi);

	if(cnt->acpi) {
		node->acpi = (char*)malloc((strlen(cnt->acpi) + 1) * sizeof(char));
		strcpy(node->acpi, cnt->acpi);
	}

	node->ty = TY_CNT;

	return node;
}

Node* create_cin_node(CIN *cin) {
	fprintf(stderr,"Create Tree Node\n[rn] %s\n[ri] %s...",cin->rn, cin->ri);

	Node* node = calloc(1, sizeof(Node));

	node->rn = (char*)malloc((strlen(cin->rn) + 1) * sizeof(char));
	node->ri = (char*)malloc((strlen(cin->ri) + 1) * sizeof(char));
	node->pi = (char*)malloc((strlen(cin->pi) + 1) * sizeof(char));
	
	strcpy(node->rn, cin->rn);
	strcpy(node->ri, cin->ri);
	strcpy(node->pi, cin->pi);

	node->ty = TY_CIN;

	return node;
}

Node* create_sub_node(Sub *sub) {
	fprintf(stderr,"Create Tree Node\n[rn] %s\n[ri] %s...",sub->rn, sub->ri);

	Node* node = calloc(1, sizeof(Node));

	node->rn = (char*)malloc((strlen(sub->rn) + 1) * sizeof(char));
	node->ri = (char*)malloc((strlen(sub->ri) + 1) * sizeof(char));
	node->pi = (char*)malloc((strlen(sub->pi) + 1) * sizeof(char));
	node->nu = (char*)malloc((strlen(sub->nu) + 1) * sizeof(char));
	node->sur = (char*)malloc((strlen(sub->sur) + 1) * sizeof(char));
	
	strcpy(node->rn, sub->rn);
	strcpy(node->ri, sub->ri);
	strcpy(node->pi, sub->pi);
	strcpy(node->nu, sub->nu);
	strcpy(node->sur, sub->sur);

	node->ty = TY_SUB;
	node->net = net_to_bit(sub->net);

	return node;
}

Node* create_acp_node(ACP *acp) {
	fprintf(stderr,"Create Tree Node\n[rn] %s\n[ri] %s...",acp->rn, acp->ri);

	Node* node = calloc(1, sizeof(Node));

	node->rn = (char*)malloc((strlen(acp->rn) + 1) * sizeof(char));
	node->ri = (char*)malloc((strlen(acp->ri) + 1) * sizeof(char));
	node->pi = (char*)malloc((strlen(acp->pi) + 1) * sizeof(char));
	node->pv_acor = (char*)malloc((strlen(acp->pv_acor) + 1) * sizeof(char));
	node->pv_acop = (char*)malloc((strlen(acp->pv_acop) + 1) * sizeof(char));
	node->pvs_acor = (char*)malloc((strlen(acp->pvs_acor) + 1) * sizeof(char));
	node->pvs_acop = (char*)malloc((strlen(acp->pvs_acop) + 1) * sizeof(char));

	strcpy(node->rn, acp->rn);
	strcpy(node->ri, acp->ri);
	strcpy(node->pi, acp->pi);
	strcpy(node->pv_acor, acp->pv_acor);
	strcpy(node->pv_acop, acp->pv_acop);
	strcpy(node->pvs_acor, acp->pvs_acor);
	strcpy(node->pvs_acop, acp->pvs_acop);

	node->ty = TY_ACP;

	return node;
}

int add_child_resource_tree(Node *parent, Node *child) {
	Node *node = parent->child;
	child->parent = parent;

	fprintf(stderr,"\nAdd Child\n[P] %s\n[C] %s...",parent->rn, child->rn);
	
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
	
	fprintf(stderr,"OK\n");
	
	return 1;
}

void delete_node_and_db_data(Node *node, int flag) {
	switch(node->ty) {
	case TY_AE : 
		db_delete_object(node->ri); 
		break;
	case TY_CNT : 
		db_delete_object(node->ri); 
		char *noti_json = (char*)malloc(sizeof("resource is deleted successfully") + 1);
		strcpy(noti_json, "resource is deleted successfully");
		notify_object(node->child,noti_json,NOTIFICATION_EVENT_2); 
		free(noti_json); noti_json = NULL;
		break;
	case TY_SUB :
		db_delete_sub(node->ri);
		break;
	case TY_ACP :
		db_delete_acp(node->ri);
		break;
	}

	Node *left = node->sibling_left;
	Node *right = node->sibling_right;
	
	if(flag == 1) {
		if(left) left->sibling_right = right;
		else node->parent->child = right;
		if(right) right->sibling_left = left;
	} else {
		if(right) delete_node_and_db_data(right, 0);
	}
	
	if(node->child) delete_node_and_db_data(node->child, 0);
	
	fprintf(stderr,"[free_node] %s...",node->rn);
	free_node(node); node = NULL;
	fprintf(stderr,"OK\n");
}

void free_node(Node *node) {
	free(node->ri);
	free(node->rn);
	free(node->pi);
	if(node->nu) free(node->nu);
	if(node->sur) free(node->sur);
	if(node->acpi) free(node->acpi);
	if(node->pv_acop) free(node->pv_acop);
	if(node->pv_acor) free(node->pv_acor);
	if(node->pvs_acor) free(node->pvs_acor);
	if(node->pvs_acop) free(node->pvs_acop);
	if(node->uri) free(node->uri);
	free(node); node = NULL;
}

void free_node_list(Node *node) {
	while(node) {
		Node *right = node->sibling_right;

		free_node(node);
		node = right;
	}
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
	
	char* now = (char*)malloc(16 * sizeof(char));
	
	*now = '\0';
	strcat(now,year);
	strcat(now,mon);
	strcat(now,day);
	strcat(now,"T");
	strcat(now,hour);
	strcat(now,minute);
	strcat(now,sec);
	
	return now;
}

void init_cse(CSE* cse) {
	char *ct = get_local_time(0);
	char *ri = resource_identifer(TY_CSE, ct);
	char rn[1024] = "TinyIoT";
	
	cse->ri = (char*)malloc((strlen(ri) + 1) * sizeof(char));
	cse->rn = (char*)malloc((strlen(rn) + 1) * sizeof(char));
	cse->ct = (char*)malloc((strlen(ct) + 1) * sizeof(char));
	cse->lt = (char*)malloc((strlen(ct) + 1) * sizeof(char));
	cse->csi = (char*)malloc((strlen(ct) + 1) * sizeof(char));
	cse->pi = (char*)malloc((strlen("NULL") + 1) * sizeof(char));
	
	strcpy(cse->ri, ri);
	strcpy(cse->rn, rn);
	strcpy(cse->ct, ct);
	strcpy(cse->lt, ct);
	strcpy(cse->csi,ct);
	strcpy(cse->pi,"NULL");
	
	cse->ty = TY_CSE;
	
	free(ct); ct = NULL;
	free(ri); ri = NULL;
}

void init_ae(AE* ae, char *pi) {
	char *ct = get_local_time(0);
	char *et = get_local_time(EXPIRE_TIME);
	char *aei = NULL; //request_header("X-M2M-Origin"); 
	char *ri = resource_identifer(TY_AE, ct);
	int m_aei = 0;
	
	if(!aei) {
		m_aei = 1;
		aei = (char*)malloc((strlen(ri) + 2) * sizeof(char));
		strcpy(aei, "C");
		strcat(aei, ri);
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
	ae->aei = (char*)malloc((strlen(aei) + 1) * sizeof(char));
	strcpy(ae->ri, ri);
	strcpy(ae->pi, pi);
	strcpy(ae->et, et);
	strcpy(ae->ct, ct);
	strcpy(ae->lt, ct);
	strcpy(ae->aei, aei);
	
	ae->ty = TY_AE;
	
	if(m_aei) {free(aei); aei = NULL;}
	free(ct); ct = NULL;
	free(et); et = NULL;
	free(ri); ri = NULL;
}

void init_cnt(CNT* cnt, char *pi) {
	char *ct = get_local_time(0);
	char *et = get_local_time(EXPIRE_TIME);
	char *ri = resource_identifer(TY_CNT, ct);
	
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
	char *ri = resource_identifer(TY_CIN, ct);
	
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

void init_sub(Sub* sub, char *pi) {
	char *ct = get_local_time(0);
	char *et = get_local_time(EXPIRE_TIME);
	char *ri = resource_identifer(TY_SUB, ct);

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

	strcpy(sub->sur, uri);
	strcat(sub->sur, "/");
	strcat(sub->sur, sub->rn);
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
	char *ri = resource_identifer(TY_ACP, ct);

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

void set_ae_update(AE* after) {
	char *rn = get_json_value_char("rn", payload);
	int rr = get_json_value_bool("rr", payload);

	if(rn) {
		free(after->rn);
		after->rn = (char*)malloc((strlen(rn) + 1) * sizeof(char));
		strcpy(after->rn, rn);
		free(rn);
	}

	switch(rr) {
		case 0: after->rr = false; break;
		case 1: after->rr = true; break;
		default: break;
	}

	if(after->lt) free(after->lt);
	after->lt = get_local_time(0);
}


void set_cnt_update(CNT* after) {
	char *rn = get_json_value_char("rn", payload);
	char *acpi = NULL;

	if(strstr(payload, "\"acpi\"") != NULL)
		acpi = get_json_value_list("acpi", payload);

	if(rn) {
		free(after->rn);
		after->rn = (char*)malloc((strlen(rn) + 1) * sizeof(char));
		strcpy(after->rn, rn);
	}

	if(acpi) {
		if(after->acpi) free(after->acpi);
		after->acpi = (char*)malloc((strlen(acpi) + 1) * sizeof(char)); 
		strcpy(after->acpi, acpi);
	}

	if(after->lt) free(after->lt);
	after->lt = get_local_time(0);
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

void set_node_update(Node *node, void *after) {
	ObjectType ty = node->ty;
	if(node->rn) {free(node->rn); node->rn = NULL;}
	if(node->uri) {free(node->uri); node->uri = NULL;}
	if(node->acpi) {free(node->acpi); node->acpi = NULL;}
	if(node->nu) {free(node->nu); node->nu = NULL;}
	if(node->pv_acor && node->pv_acop) {
		free(node->pv_acor); node->pv_acor = NULL; 
		free(node->pv_acop); node->pv_acop = NULL;
	}
	if(node->pvs_acor && node->pvs_acop) {
		free(node->pvs_acor); node->pvs_acor = NULL;
		free(node->pvs_acop); node->pvs_acop = NULL;
	}
	
	switch(ty) {
	case TY_AE:
		AE *ae = (AE*)after;
		node->rn = (char*)malloc((strlen(ae->rn) + 1)*sizeof(char));
		strcpy(node->rn, ae->rn);
		break;

	case TY_CNT:
		CNT *cnt = (CNT*)after;
		node->rn = (char*)malloc((strlen(cnt->rn) + 1)*sizeof(char));
		strcpy(node->rn, cnt->rn);
		if(cnt->acpi) {
			node->acpi = (char*)malloc((strlen(cnt->acpi) + 1)*sizeof(char));
			strcpy(node->acpi, cnt->acpi);
		}
		break;

	case TY_SUB:
		Sub *sub = (Sub*)after;
		node->rn = (char*)malloc((strlen(sub->rn) + 1)*sizeof(char));
		strcpy(node->rn, sub->rn);
		node->net = net_to_bit(sub->net);
		if(sub->nu) {
			node->nu = (char*)malloc((strlen(sub->nu) + 1)*sizeof(char));
			strcpy(node->nu, sub->nu);
		}
		break;

	case TY_ACP:
		ACP *acp = (ACP*)after;
		node->rn = (char*)malloc((strlen(acp->rn) + 1)*sizeof(char));
		strcpy(node->rn, acp->rn);
		if(acp->pv_acor && acp->pv_acop) {
			node->pv_acor = (char*)malloc((strlen(acp->pv_acor) + 1)*sizeof(char));
			node->pv_acop = (char*)malloc((strlen(acp->pv_acop) + 1)*sizeof(char));
			strcpy(node->pv_acor, acp->pv_acor);
			strcpy(node->pv_acop, acp->pv_acop);
		}
		if(acp->pvs_acor && acp->pvs_acop) {
			node->pvs_acor = (char*)malloc((strlen(acp->pvs_acor) + 1)*sizeof(char));
			node->pvs_acop = (char*)malloc((strlen(acp->pvs_acop) + 1)*sizeof(char));
			strcpy(node->pvs_acor, acp->pvs_acor);
			strcpy(node->pvs_acop, acp->pvs_acop);
		}
		break;
	}
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

void notify_object(Node *node, char *res_json, NET net) {
	remove_invalid_char_json(res_json);
	while(node) {
		if(node->ty == TY_SUB && (net & node->net) == net) {
			if(!node->uri) set_node_uri(node);
			char *notify_json = notification_to_json(node->uri, (int)log2((double)net ) + 1, res_json);
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

int is_json_valid_char(char c){
	return ('!' <= c && c <= '~');
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

char *resource_identifer(ObjectType ty, char *ct) {
	char *ri = (char *)malloc(24 * sizeof(char));

	switch(ty) {
		case TY_CSE : strcpy(ri, "5-"); break;
		case TY_AE : strcpy(ri, "2-"); break;
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


size_t write_data(void *ptr, size_t size, size_t nmemb, struct url_data *data) {
    size_t index = data->size;
    size_t n = (size * nmemb);
    char* tmp;

    data->size += (size * nmemb);
    tmp = realloc(data->data, data->size + 1); /* +1 for '\0' */

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
    data.data = malloc(4096 * sizeof(char)); /* reasonable size initial buffer */

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

int get_acop(Node *node) {
	char *requestheader_origin = request_header("X-M2M-Origin");
	char origin[128];
	int acop = 0;
	
	if(requestheader_origin) {
		strcpy(origin, requestheader_origin);
	} else {
		strcpy(origin, "all");
	}

	if(node->ty == TY_ACP) {
		acop = (acop | get_acop_origin(origin, node, 1));
		acop = (acop | get_acop_origin("all", node, 1));
		return acop;
	}

	if(!node->acpi || !strcmp(node->acpi, "") || !strcmp(node->acpi, " ")) return ALL_ACOP;

	Node *cb = node;
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
		Node *acp = find_node_by_uri(cb, arr_acp_uri[i]);

		if(acp) {
			acop = (acop | get_acop_origin(origin, acp, 0));
			acop = (acop | get_acop_origin("all", acp, 0));
		}
	}

	return acop;
}

int get_acop_origin(char *origin, Node *acp, int flag) {
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

Node *find_node_by_uri(Node *cb, char *node_uri) {
	Node *node = cb, *pnode = NULL;
	node_uri = strtok(node_uri, "/");

	if(!node_uri) return NULL;

	char uri_array[64][MAX_URI_SIZE];
	int index = -1;

	while(node_uri) {
		strcpy(uri_array[++index], node_uri);
		node_uri = strtok(NULL, "/");
	}

	for(int i=0; i<=index; i++) {
		while(node) {
			if(!strcmp(node->rn, uri_array[i])) break;
			node = node->sibling_right;
		}
		if(i == index-1) pnode = node;
		if(!node) break;
		if(i != index) node = node->child;
	}

	if(node) return node;

	Node *head;

	if(pnode) {
		head = db_get_cin_list_by_pi(pnode->ri);
		node = head;
		while(node) {
			if(!strcmp(node->rn, uri_array[index])) break;
			node = node->sibling_right;
		}
	}

	if(node) {
		if(node->sibling_left) node->sibling_left->sibling_right = node->sibling_right;
		if(node->sibling_right) node->sibling_right->sibling_left = node->sibling_left;
		node->parent = pnode;
	}

	if(head) free_node_list(head);

	return node;
}

int get_value_querystring_int(char *key) {
	char *value = strstr(qs, key);
	if(!value) return -1;

	value = value + strlen(key) + 1;

	return atoi(value);
}

void set_node_uri(Node* node) {
	if(!node->uri) node->uri = (char*)calloc(MAX_URI_SIZE,sizeof(char));

	Node *p = node;
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