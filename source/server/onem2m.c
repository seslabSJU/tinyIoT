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

int Validate_oneM2M_Standard() {
	int ret = 1;

	if(!request_header("X-M2M-RI")) {
		fprintf(stderr,"Request has no \"X-M2M-RI\" Header\n");
		ret = 0;
	} 
	if(!request_header("X-M2M-Origin")) {
		fprintf(stderr,"Request has no \"X-M2M-Origin\" Header\n");
		ret = 0;		
	}
	
	return ret;
}

Node* Parse_URI(Node *cb, char *uri, Operation *op) {
	fprintf(stderr,"Parse_URI \x1b[33m%s\x1b[0m...",uri);
	char uri_array[MAX_URI_SIZE];
	char *uri_parse = uri_array;
	strcpy(uri_array, uri);

	char uri_strtok[64][MAX_URI_SIZE] = {"\0", };
	int index_s = 0, index_end = 0;

	uri_parse = strtok(uri_array, "/");
	if(uri_parse) {
		strcpy(uri_strtok[index_end++], uri_parse);
	} else {
		return NULL;
	}

	while(uri_parse) {
		uri_parse = strtok(NULL, "/");
		if(uri_parse) {
			strcpy(uri_strtok[index_end++], uri_parse);
		}
	}

	index_end--;

	if(!strcmp(uri_strtok[0], "viewer")) {
		*op = o_VIEWER; index_s++;
	} else if(!strcmp(uri_strtok[index_end], "la") || !strcmp(uri_strtok[index_end], "latest")) {
		*op = o_LA; index_end--;
	} else if(!strcmp(uri_strtok[index_end], "ol") || !strcmp(uri_strtok[index_end], "oldest")) {
		*op = o_OL; index_end--;
	} else if(strstr(uri_strtok[index_end], "4-20")) {
		*op = o_CIN_RI; Retrieve_CIN_Ri(uri_strtok[index_end]); return NULL;
	}

	strcpy(uri_array,"/\0");

	for(int i=index_s; i<=index_end; i++) {
		strcat(uri_array,uri_strtok[i]);
		strcat(uri_array,"/");
	}

	return Find_Node_by_URI(cb, uri_array);
}

Operation Parse_Operation(){
	Operation op;

	if(strcmp(method, "POST") == 0) op = o_CREATE;
	else if(strcmp(method, "GET") == 0) op = o_RETRIEVE;
	else if (strcmp(method, "PUT") == 0) op = o_UPDATE;
	else if (strcmp(method, "DELETE") == 0) op = o_DELETE;
	else if (strcmp(method, "OPTIONS") == 0) op = o_OPTIONS;

	return op;
}

int duplicate_resource_check(Node *pnode) {
	Node* node = pnode->child;
	char* rn = Get_JSON_Value_char("rn",payload);
	if(!rn) return 0;

	while(node) {
		if(!strcmp(node->rn, rn)) return 1;
		node = node->siblingRight;
	}

	return 0;
}

void Retrieve_CIN_Ri(char *ri) {
	fprintf(stderr,"OK\n\x1b[43mRetrieve CIN By Ri\x1b[0m\n");
	CIN* gcin = Get_CIN(ri);
	
	if(gcin) {
		char *res_json = CIN_to_json(gcin);
		HTTP_200_JSON;
		printf("%s",res_json);
		free(res_json); res_json = NULL;
		Free_CIN(gcin); gcin = NULL;
	} else {
		fprintf(stderr,"There is no such CIN ri = %s\n",ri);
		HTTP_400;
		printf("{\"m2m:dbg\": \"invalid object\"}");
	}
}

void CIN_in_period(Node *pnode) {
	int period = 0;
	char key[8] = "period=";
	
	qs = strtok(qs, "&");
	
	while(qs != NULL) {
		int flag = 1;
		if(strlen(qs) >= 8) {
			for(int i=0; i<7; i++) {
				if(qs[i] != key[i]) flag = 0;
			}
		}
		if(flag) {
			period = atoi(qs+7);
			break;
		}
		qs = strtok(NULL, "&");
	}
	
	char *start = Get_LocalTime(period);
	char *end = Get_LocalTime(0);
	Node *cinList = Get_CIN_Period(start, end);
	
	fprintf(stderr,"period : %d seconds\n",period);
	
	Node *cin = cinList;
	
	HTTP_200_CORS;
	while(cin) {
		if(!strcmp(cin->pi, pnode->ri)) {
			CIN* gcin = Get_CIN(cin->ri);
			char *res_json = CIN_to_json(gcin);
			printf("%s\n",res_json);
			free(res_json); res_json = NULL;
			Free_CIN(gcin); gcin = NULL;
		}
		cin = cin->siblingRight;
	}
	
	while(cinList) {
		Node *r = cinList->siblingRight;
		Free_Node(cinList);
		cinList = r;
	}
}

void Tree_Viewer_API(Node *node) {
	fprintf(stderr,"\x1b[43mTree Viewer API\x1b[0m\n");
	char arr_viewer_data[MAX_TREE_VIEWER_SIZE] = "[";
	char *viewer_data = arr_viewer_data;
	
	Node *p = node;
	while(p = p->parent) {
		char *json = Node_to_json(p);
		strcat(viewer_data,",");
		strcat(viewer_data,json);
		free(json); json = NULL;
	}
	
	int cinSize = 1;
	
	char *la = strstr(qs,"la=");
	if(la) cinSize = atoi(la+3);
	
	fprintf(stderr,"Latest CIN Size : %d\n", cinSize);
	
	Tree_data(node, &viewer_data, cinSize);
	strcat(viewer_data,"]\0");
	char res[MAX_TREE_VIEWER_SIZE] = "";
	int index = 0;
	
	for(int i=0; i<MAX_TREE_VIEWER_SIZE; i++) {
		if(i == 1) continue;
		if(is_JSON_Valid_Char(viewer_data[i])) {
			res[index++] = viewer_data[i];
		}
	}
	
	fprintf(stderr,"Tree_Viewer_API Content-Size : %ld\n",strlen(res));

	HTTP_200_CORS;
	printf("%s",res);
}

void Tree_data(Node *node, char **viewer_data, int cin_size) {
	char *json = Node_to_json(node);

	strcat(*viewer_data, ",");
	strcat(*viewer_data, json);
	free(json); json = NULL;

	Node *child = node->child;
	while(child) {
		Tree_data(child, viewer_data, cin_size);
		child = child->siblingRight;
	}

	if(node->ty != t_Sub && node->ty != t_ACP) {
		Node *cin_list = Get_CIN_Pi(node->ri);

		if(cin_list) cin_list = Latest_CINs(cin_list, cin_size);

		Node *p = cin_list;

		while(p) {
			json = Node_to_json(p);
			strcat(*viewer_data, ",");
			strcat(*viewer_data, json);
			free(json); json = NULL;
			p = p->siblingRight;
			Free_Node(cin_list);
			cin_list = p;			
		}
	}
}

Node *Latest_CINs(Node* cinList, int num) {
	Node *head, *tail;
	head = tail = cinList;
	int cnt = 1;
	
	while(tail->siblingRight) {
		tail = tail->siblingRight;
		cnt++;
	}
	
	for(int i=0; i < cnt-num; i++) {
		head = head->siblingRight;
		Free_Node(head->siblingLeft); head->siblingLeft = NULL;
	}
	
	return head;
}

void Object_Test_API(Node *node) {
	HTTP_200_JSON;
	printf("{\"cin-size\": %d}",node->cinSize);
	return;
}

void Remove_Specific_Asterisk_Payload() {
	int index = 0;

	for(int i=0; i<payload_size; i++) {
		if(is_JSON_Valid_Char(payload[i])) {
			payload[index++] =  payload[i];
		}
	}

	payload[index] = '\0';
}

ObjectType Parse_ObjectType() {
	ObjectType ty;
	char *ct = request_header("Content-Type");
	if(!ct) return 0;
	ct = strstr(ct, "ty=");
	if(!ct) return 0;
	int objType = atoi(ct+3);
	
	switch(objType) {
	case 1 : ty = t_ACP; break;
	case 2 : ty = t_AE; break;
	case 3 : ty = t_CNT; break;
	case 4 : ty = t_CIN; break;
	case 5 : ty = t_CSE; break;
	case 23 : ty = t_Sub; break;
	}
	
	return ty;
}

ObjectType Parse_ObjectType_Body() {
	ObjectType ty;
	
	char *cse, *ae, *cnt, *sub, *acp;
	
	cse = strstr(payload, "m2m:cse");
	ae = strstr(payload, "m2m:ae");
	cnt = strstr(payload, "m2m:cnt");
	sub = strstr(payload, "m2m:sub");
	acp = strstr(payload, "m2m:acp");
	
	if(cse) ty = t_CSE;
	else if(ae) ty = t_AE;
	else if(cnt) ty = t_CNT;
	else if(sub) ty = t_Sub;
	else if(acp) ty = t_ACP;
	
	return ty;
}

Node* Create_Node(void *obj, ObjectType ty){
	Node* node = NULL;

	switch(ty) {
	case t_CSE: node = Create_CSE_Node((CSE*)obj); break;
	case t_AE: node = Create_AE_Node((AE*)obj); break;
	case t_CNT: node = Create_CNT_Node((CNT*)obj); break;
	case t_CIN: node = Create_CIN_Node((CIN*)obj); break;
	case t_Sub: node = Create_Sub_Node((Sub*)obj); break;
	case t_ACP: node = Create_ACP_Node((ACP*)obj); break;
	}

	node->parent = NULL;
	node->child = NULL;
	node->siblingLeft = NULL;
	node->siblingRight = NULL;
	
	fprintf(stderr,"OK\n");
	
	return node;
}

Node* Create_CSE_Node(CSE *cse) {
	fprintf(stderr,"Create Tree Node\n[rn] %s\n[ri] %s...",cse->rn, cse->ri);

	Node* node = calloc(1, sizeof(Node));

	node->rn = (char*)malloc((strlen(cse->rn) + 1) * sizeof(char));
	node->ri = (char*)malloc((strlen(cse->ri) + 1) * sizeof(char));
	node->pi = (char*)malloc((strlen(cse->pi) + 1) * sizeof(char));
	
	strcpy(node->rn, cse->rn);
	strcpy(node->ri, cse->ri);
	strcpy(node->pi, cse->pi);

	node->ty = t_CSE;
	node->cinSize = 0;

	return node;
}

Node* Create_AE_Node(AE *ae) {
	fprintf(stderr,"Create Tree Node\n[rn] %s\n[ri] %s...",ae->rn, ae->ri);

	Node* node = calloc(1, sizeof(Node));

	node->rn = (char*)malloc((strlen(ae->rn) + 1) * sizeof(char));
	node->ri = (char*)malloc((strlen(ae->ri) + 1) * sizeof(char));
	node->pi = (char*)malloc((strlen(ae->pi) + 1) * sizeof(char));
	
	strcpy(node->rn, ae->rn);
	strcpy(node->ri, ae->ri);
	strcpy(node->pi, ae->pi);

	node->ty = t_AE;
	node->cinSize = 0;

	return node;
}

Node* Create_CNT_Node(CNT *cnt) {
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
	

	node->ty = t_CNT;
	node->cinSize = 0;

	return node;
}

Node* Create_CIN_Node(CIN *cin) {
	fprintf(stderr,"Create Tree Node\n[rn] %s\n[ri] %s...",cin->rn, cin->ri);

	Node* node = calloc(1, sizeof(Node));

	node->rn = (char*)malloc((strlen(cin->rn) + 1) * sizeof(char));
	node->ri = (char*)malloc((strlen(cin->ri) + 1) * sizeof(char));
	node->pi = (char*)malloc((strlen(cin->pi) + 1) * sizeof(char));
	
	strcpy(node->rn, cin->rn);
	strcpy(node->ri, cin->ri);
	strcpy(node->pi, cin->pi);

	node->ty = t_CIN;
	node->cinSize = 0;

	return node;
}

Node* Create_Sub_Node(Sub *sub) {
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

	node->ty = t_Sub;
	node->cinSize = 0;
	node->net = net_to_bit(sub->net);

	return node;
}

Node* Create_ACP_Node(ACP *acp) {
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

	node->ty = t_ACP;
	node->cinSize = 0;

	return node;
}

int Add_child(Node *parent, Node *child) {
	Node *node = parent->child;
	child->parent = parent;

	fprintf(stderr,"\nAdd Child\n[P] %s\n[C] %s...",parent->rn, child->rn);
	
	if(!node) {
		parent->child = child;
	} else if(node) {
		while(node->siblingRight && node->siblingRight->ty <= child->ty) { 	
				node = node->siblingRight;
		}

		if(parent->child == node && child->ty < node->ty) {
			parent->child = child;
			child->siblingRight = node;
			node->siblingLeft = child;
		} else {
			if(node->siblingRight) {
				node->siblingRight->siblingLeft = child;
				child->siblingRight = node->siblingRight;
			}

			node->siblingRight = child;
			child->siblingLeft = node;
		}
	}
	
	fprintf(stderr,"OK\n");
	
	return 1;
}

void Delete_Node_Object(Node *node, int flag) {
	switch(node->ty) {
	case t_AE : 
		Delete_AE(node->ri); 
		break;
	case t_CNT : 
		Delete_CNT(node->ri); 
		char *noti_json = (char*)malloc(sizeof("resource is deleted successfully") + 1);
		strcpy(noti_json, "resource is deleted successfully");
		Notify_Object(node->child,noti_json,noti_event_2); 
		free(noti_json); noti_json = NULL;
		break;
	case t_Sub :
		Delete_Sub(node->ri);
		break;
	}

	Node *left = node->siblingLeft;
	Node *right = node->siblingRight;
	
	if(flag == 1) {
		if(left) left->siblingRight = right;
		else node->parent->child = right;
		if(right) right->siblingLeft = left;
	} else {
		if(right) Delete_Node_Object(right, 0);
	}
	
	if(node->child) Delete_Node_Object(node->child, 0);
	
	fprintf(stderr,"Free_Node : %s...",node->rn);
	Free_Node(node); node = NULL;
	fprintf(stderr,"OK\n");
}

void Free_Node(Node *node) {
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
	free(node); node = NULL;
}

char *Get_LocalTime(int diff) {
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

void Init_CSE(CSE* cse) {
	char *ct = Get_LocalTime(0);
	char *ri = resource_identifier(t_CSE, ct);
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
	
	cse->ty = t_CSE;
	
	free(ct); ct = NULL;
	free(ri); ri = NULL;
}

void Init_AE(AE* ae, char *pi) {
	char *ct = Get_LocalTime(0);
	char *et = Get_LocalTime(EXPIRE_TIME);
	char *aei = request_header("X-M2M-Origin"); 
	char *ri = resource_identifier(t_AE, ct);
	char tmp[MAX_PROPERTY_SIZE];
	int m_aei = 0;

	if(!aei) {
		m_aei = 1;
		aei = (char*)malloc((strlen(ri) + 1) * sizeof(char));
		strcpy(aei, ri);
	}
	
	strcpy(tmp,ae->api);
	ae->api = (char*)malloc((strlen(ae->api) + 1) * sizeof(char));
	strcpy(ae->api,tmp);
	
	strcpy(tmp,ae->rn);
	ae->rn = (char*)malloc((strlen(ae->rn) + 1) * sizeof(char));
	strcpy(ae->rn,tmp);
	
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
	
	ae->ty = t_AE;
	
	if(m_aei) {free(aei); aei = NULL;}
	free(ct); ct = NULL;
	free(et); et = NULL;
	free(ri); ri = NULL;
}

void Init_CNT(CNT* cnt, char *pi) {
	char *ct = Get_LocalTime(0);
	char *et = Get_LocalTime(EXPIRE_TIME);
	char *ri = resource_identifier(t_CNT, ct);
	char tmp[MAX_PROPERTY_SIZE];
	
	if(cnt->rn) {
		strcpy(tmp,cnt->rn);
		cnt->rn = (char*)malloc((strlen(cnt->rn) + 1) * sizeof(char));
		strcpy(cnt->rn,tmp);
	}

	if(cnt->acpi) {
		strcpy(tmp,cnt->acpi);
		cnt->acpi = (char*)malloc((strlen(cnt->acpi) + 1) * sizeof(char));
		strcpy(cnt->acpi,tmp);
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
	
	cnt->ty = t_CNT;
	cnt->st = 0;
	cnt->cni = 0;
	cnt->cbs = 0;
	
	free(ct); ct = NULL;
	free(et); et = NULL;
	free(ri); ri = NULL;
}

void Init_CIN(CIN* cin, char *pi) {
	char *ct = Get_LocalTime(0);
	char *et = Get_LocalTime(EXPIRE_TIME);
	char *ri = resource_identifier(t_CIN, ct);
	char tmp[MAX_PROPERTY_SIZE];
	
	strcpy(tmp,cin->con);
	cin->con = (char*)malloc((strlen(cin->con) + 1) * sizeof(char));
	strcpy(cin->con,tmp);
	
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
	
	cin->ty = t_CIN;
	cin->st = 0;
	cin->cs = strlen(cin->con);
	
	free(ct); ct = NULL;
	free(et); et = NULL;
	free(ri); ri = NULL;
}

void Init_Sub(Sub* sub, char *pi) {
	char *ct = Get_LocalTime(0);
	char *et = Get_LocalTime(EXPIRE_TIME);
	char *ri = resource_identifier(t_Sub, ct);
	char tmp[MAX_PROPERTY_SIZE];

	strcpy(tmp,sub->rn);
	sub->rn = (char*)malloc((strlen(sub->rn) + 1) * sizeof(char));
	strcpy(sub->rn,tmp);

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
	sub->ty = t_Sub;
	sub->nct = 0;

	free(ct); ct = NULL;
	free(et); et = NULL;
	free(ri); ri = NULL;
}

void Init_ACP(ACP* acp, char *pi) {
	char *ct = Get_LocalTime(0);
	char *et = Get_LocalTime(EXPIRE_TIME);
	char *ri = resource_identifier(t_ACP, ct);
	char tmp[MAX_PROPERTY_SIZE];
	
	strcpy(tmp,acp->rn);
	acp->rn = (char*)malloc((strlen(acp->rn) + 1) * sizeof(char));
	strcpy(acp->rn,tmp);
	
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
	
	acp->ty = t_ACP;
	
	free(ct); ct = NULL;
	free(et); et = NULL;
	free(ri); ri = NULL;
}

void Set_AE_Update(AE* after) {
	char *rn = Get_JSON_Value_char("rn", payload);
	char *api = Get_JSON_Value_char("api", payload);
	int rr = Get_JSON_Value_bool("rr", payload);

	if(rn) {
		free(after->rn);
		after->rn = (char*)malloc((strlen(rn) + 1) * sizeof(char));
		strcpy(after->rn, rn);
	}

	if(api) {
		free(after->api);
		after->api = (char*)malloc((strlen(api) + 1) * sizeof(char));
		strcpy(after->api, api);
	}

	if(rr != -1) {
		after->rr = rr;
	}
}


void Set_CNT_Update(CNT* after) {
	char *rn = Get_JSON_Value_char("rn", payload);
	char *acpi = Get_JSON_Value_char("acpi", payload);

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
}

void Set_Sub_Update(Sub* after) {
	char *rn = Get_JSON_Value_char("rn", payload);

	if(rn) {
		free(after->rn);
		after->rn = (char*)malloc((strlen(rn) + 1) * sizeof(char));
		strcpy(after->rn, rn);
	}
}

void Free_CSE(CSE *cse) {
	if(cse->ct) free(cse->ct);
	if(cse->lt) free(cse->lt);
	if(cse->rn) free(cse->rn);
	if(cse->ri) free(cse->ri);
	if(cse->csi) free(cse->csi);
	if(cse->pi) free(cse->pi);
	free(cse); cse = NULL;
}

void Free_AE(AE *ae) {
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

void Free_CNT(CNT *cnt) {
	if(cnt->et) free(cnt->et);
	if(cnt->ct) free(cnt->ct);
	if(cnt->lt) free(cnt->lt);
	if(cnt->rn) free(cnt->rn);
	if(cnt->ri) free(cnt->ri);
	if(cnt->pi) free(cnt->pi);
	free(cnt); cnt = NULL;
}

void Free_CIN(CIN* cin) {
	if(cin->et) free(cin->et);
	if(cin->ct) free(cin->ct);
	if(cin->lt) free(cin->lt);
	if(cin->rn) free(cin->rn);
	if(cin->ri) free(cin->ri);
	if(cin->pi) free(cin->pi);
	if(cin->con) free(cin->con);
	free(cin); cin = NULL;
}

void Free_Sub(Sub* sub) {
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

void Free_ACP(ACP* acp) {
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

void Notify_Object(Node *node, char *res_json, Net net) {
	Remove_Invalid_Char_JSON(res_json);
	while(node) {
		if(node->ty == t_Sub && (net & node->net) == net) {
			char *noti_json = Noti_to_json(node->sur, (int)log2((double)net ) + 1, res_json);
			char *res = Send_HTTP_Packet(node->nu, noti_json);
			free(noti_json); noti_json = NULL;
			free(res); res = NULL;
		}
		node = node->siblingRight;
	}
}

void Remove_Invalid_Char_JSON(char* json) { 
	int size = (int)malloc_usable_size(json); // segmentation fault if json memory not in heap (malloc)
	int index = 0;

	for(int i=0; i<size; i++) {
		if(is_JSON_Valid_Char(json[i]) && json[i] != '\\') {
			json[index++] = json[i];
		}
	}

	json[index] = '\0';
}

int is_JSON_Valid_Char(char c){
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

char *resource_identifier(ObjectType ty, char *ct) {
	char *ri = (char *)malloc(24 * sizeof(char));

	switch(ty) {
		case t_CSE : strcpy(ri, "5-"); break;
		case t_AE : strcpy(ri, "2-"); break;
		case t_CNT : strcpy(ri, "3-"); break;
		case t_CIN : strcpy(ri, "4-"); break;
		case t_Sub : strcpy(ri, "23-"); break;
		case t_ACP : strcpy(ri, "1-"); break;
	}

	strcat(ri, ct);

	srand(time(NULL));

	int r = 1000 + rand()%9000;

	char ran[5] = "\0\0\0\0\0";
	for(int i=0; i<4; i++) {
		ran[i] = r % 10 + '0';
		r /= 10;
	}

	strcat(ri, ran);

	return ri;
}


size_t write_data(void *ptr, size_t size, size_t nmemb, struct url_data *data) {
    size_t index = data->size;
    size_t n = (size * nmemb);
    char* tmp;

    data->size += (size * nmemb);

#ifdef DEBUG
    fprintf(stderr, "data at %p size=%ld nmemb=%ld\n", ptr, size, nmemb);
#endif
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

char *Send_HTTP_Packet(char* target, char *post_data) {
    CURL *curl;
    struct url_data data;

    data.size = 0;
    data.data = malloc(4096 * sizeof(char)); /* reasonable size initial buffer */

    if(NULL == data.data) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return NULL;
    }

    data.data[0] = '\0';

    CURLcode res;

    curl = curl_easy_init();

    if (curl) {

        curl_easy_setopt(curl, CURLOPT_URL, target);
		if(post_data){
			Remove_Invalid_Char_JSON(post_data);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
		}
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

        res = curl_easy_perform(curl);
		/*
        if(res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n",
                        curl_easy_strerror(res));
        }
		*/
        curl_easy_cleanup(curl);
    }

    return data.data;
}

int get_acop(Node *node) {
	char *origin = request_header("X-M2M-Origin");

	if(node->ty == t_ACP) return get_acop_origin(origin, node, 1);

	if(!node->acpi || !strcmp(node->acpi, "")) return ALL_ACOP;

	if(!origin) return 0;

	Node *cb = node;
	while(cb->parent) cb = cb->parent;
	
	int acop = 0, uri_cnt = 0;
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
		Node *acp = Find_Node_by_URI(cb, arr_acp_uri[i]);

		if(acp) acop = (acop | get_acop_origin(origin, acp, 0));
	}

	return acop;
}

int get_acop_origin(char *origin, Node *acp, int flag) {
	if(!origin) return 0;

	int ret_acop = 0, cnt = 0;
	char *acor, *acop, arr_acor[1024], arr_acop[1024];

	if(flag) {
		strcpy(arr_acor, acp->pvs_acor);
		strcpy(arr_acop, acp->pvs_acop);
	} else {
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

Node *Find_Node_by_URI(Node *cse, char *node_uri) {
	Node *node = cse;

	node_uri = strtok(node_uri, "/");

	if(!node_uri) return NULL;

	while(node) {
		while(node) {
			if(!strcmp(node->rn, node_uri)) break;
			node = node->siblingRight;
		}
		
		node_uri = strtok(NULL, "/");
		if(!node_uri) break;
		node = node->child;
	}

	return node;
}