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
#define TREE_VIEWER_DATASIZE 65536
#define MAX_PROPERTY_SIZE 32768
#define MAX_URI_SIZE 256
#define EXPIRE_TIME -3600*24*365*2

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

Node* Parse_URI(RT *rt) {
	fprintf(stderr,"Parse_URI \x1b[33m%s\x1b[0m...",uri);
	char uri_array[MAX_URI_SIZE];
	char *uri_parse = uri_array;
	Node *node = NULL;

	strcpy(uri_array, uri);
	
	uri_parse = strtok(uri_parse, "/");
	
	int viewer = 0, test = 0, la_ol = 0;
	
	if(uri_parse != NULL) {
		if(!strcmp("test", uri_parse)) test = 1;
		else if(!strcmp("viewer", uri_parse)) viewer = 1;
		
		if(test || viewer) uri_parse = strtok(NULL, "/");
	}
	
	if(uri_parse != NULL && !strcmp("TinyIoT", uri_parse)) node = rt->root;
	
	while(uri_parse != NULL && node) {
		char *cin_ri;
		if((cin_ri = strstr(uri_parse, "4-20")) != NULL) {
			fprintf(stderr,"OK\n\x1b[43mRetrieve CIN By Ri\x1b[0m\n");
			Retrieve_CIN_Ri(cin_ri);
			return NULL;
		}
	
		if(!strcmp("la", uri_parse) || !strcmp("latest", uri_parse)) {
			while(node->siblingRight) {
				if(node->ty == t_CIN && node->siblingRight->ty != t_CIN) break;
				node = node->siblingRight;
			}
			la_ol = 1;
		} else if(!strcmp("ol", uri_parse) || !strcmp("oldest", uri_parse)) {
			while(node) {
				if(node->ty == t_CIN) break;
				node = node->siblingRight;
			}
			la_ol = 1;
		}
		
		if(!la_ol)
		{
			while(node) {
				if(!strcmp(node->rn,uri_parse)) break;
				node = node->siblingRight;
			}
		}

		la_ol = 0;
		
		uri_parse = strtok(NULL, "/");
		
		if(uri_parse == NULL) break;
		
		if(!strcmp(uri_parse, "cinperiod")) {
			fprintf(stderr,"OK\n\x1b[43mRetrieve CIN in Period\x1b[0m\n");
			CIN_in_period(node);
			return NULL;
		}
		
		if(node) node = node->child;
	}
	
	if(node) {
		if(viewer) {
			fprintf(stderr,"OK\n\x1b[43mTree Viewer API\x1b[0m\n");
			Tree_Viewer_API(node);
			return NULL;
		} else if(test) {
			fprintf(stderr,"OK\n\x1b[43mObject Test API\x1b[0m\n");
			Object_Test_API(node);
			return NULL;
		}
	} else if(!node) {
		fprintf(stderr,"Invalid\n");
		HTTP_400;
		printf("{\"m2m:dbg\": \"invalid object\"}");
		return NULL;
	}
	
	fprintf(stderr,"OK\n");

	return node;
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
	CIN* gcin = Get_CIN(ri);
	
	if(gcin) {
		char *res_json = CIN_to_json(gcin);
		HTTP_200_JSON;
		printf("%s",res_json);
		free(res_json);
		Free_CIN(gcin);
		res_json = NULL;
		gcin = NULL;
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
			free(res_json);
			Free_CIN(gcin);
			res_json = NULL;
			gcin = NULL;
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
	char *viewer_data = (char *)calloc(TREE_VIEWER_DATASIZE, sizeof(char));
	strcpy(viewer_data,"[");
	
	Node *p = node;
	while(p = p->parent) {
		char *json = Node_to_json(p);
		strcat(viewer_data,",");
		strcat(viewer_data,json);
	}
	
	int cinSize = 1;
	
	char *la = strstr(qs,"la=");
	if(la) cinSize = atoi(la+3);
	
	fprintf(stderr,"Latest CIN Size : %d\n", cinSize);
	
	Tree_data(node, &viewer_data, cinSize);
	strcat(viewer_data,"]\0");
	char res[TREE_VIEWER_DATASIZE] = "";
	int index = 0;
	
	for(int i=0; i<TREE_VIEWER_DATASIZE; i++) {
		if(i == 1) continue;
		if(is_JSON_Valid_Char(viewer_data[i])) {
			res[index++] = viewer_data[i];
		}
	}
	
	fprintf(stderr,"TreeViewerAPI Content-Size : %ld\n",strlen(res));

	HTTP_200_CORS;
	printf("%s",res);
	free(viewer_data);
	viewer_data = NULL;
}

void Tree_data(Node *node, char **viewer_data, int cin_num) {
	if(node->ty == t_CIN) {
		Node *cinLatest = Get_CIN_Pi(node->pi);
		
		Node *p = cinLatest;
		
		cinLatest = Latest_CINs(cinLatest, cin_num);
		
		while(cinLatest) {
			char *json = Node_to_json(cinLatest);
			strcat(*viewer_data, ",");
			strcat(*viewer_data, json);
			Node *right = cinLatest->siblingRight;
			Free_Node(cinLatest);
			cinLatest = right;
		}
		return;
	}
	
	char *json = Node_to_json(node);
	strcat(*viewer_data, ",");
	strcat(*viewer_data, json);
	
	node = node->child;
	
	while(node) {
		Tree_data(node, viewer_data, cin_num);
		if(node->ty == t_CIN) {
			while(node->siblingRight && node->siblingRight->ty != t_Sub) {
				node = node->siblingRight;
			}
		}
		node = node->siblingRight;
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
		Free_Node(head->siblingLeft);
		head->siblingLeft = NULL;
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
	
	char *cse, *ae, *cnt, *sub;
	
	cse = strstr(payload, "m2m:cse");
	ae = strstr(payload, "m2m:ae");
	cnt = strstr(payload, "m2m:cnt");
	sub = strstr(payload, "m2m:sub");
	
	if(cse) ty = t_CSE;
	else if(ae) ty = t_AE;
	else if(cnt) ty = t_CNT;
	else if(sub) ty = t_Sub;
	
	return ty;
}

Node* Create_Node(char *ri, char *rn, char *pi, char *nu, char *sur, int net, ObjectType ty){
	Node* node = (Node*)malloc(sizeof(Node));
	
	if(strcmp(rn,"") && strcmp(rn,"TinyIoT")) {
		fprintf(stderr,"\nCreate Tree Node\n[rn] %s\n[ri] %s...", rn, ri);
	}
	
	node->rn = (char*)malloc((strlen(rn) + 1) * sizeof(char));
	node->ri = (char*)malloc((strlen(ri) + 1) * sizeof(char));
	node->pi = (char*)malloc((strlen(pi) + 1) * sizeof(char));
	node->nu = (char*)malloc((strlen(nu) + 1) * sizeof(char));
	node->sur = (char*)calloc((strlen(sur) + 1), sizeof(char));
	

	strcpy(node->sur, sur);
	strcpy(node->rn, rn);
	strcpy(node->ri, ri);
	strcpy(node->pi, pi);
	strcpy(node->nu, nu);
	
	node->net = net;
	node->ty = ty;
	node->cinSize = 0;

	node->parent = NULL;
	node->child = NULL;
	node->siblingLeft = NULL;
	node->siblingRight = NULL;
	
	if(strcmp(rn,"") && strcmp(rn,"TinyIoT")) fprintf(stderr,"OK\n");
	
	return node;
}

int Add_child(Node *parent, Node *child) {
	Node *node = parent->child;
	child->parent = parent;
	
	if(child->ty == t_CIN) parent->cinSize++;
	
	if(child->ty != t_CIN) fprintf(stderr,"\nAdd Child\n[P] %s\n[C] %s...",parent->rn, child->rn);
	
	if(!node) {
		parent->child = child;
	} else if(node) {
		if(child->ty < node->ty) {
			parent->child = child;
			child->siblingRight = node;
			node->siblingLeft = child;
		} else {
			while(node->siblingRight && node->siblingRight->ty <= child->ty) { 
				if(node->ty == t_CIN && node->siblingRight->ty == t_CIN && child->ty == t_CIN) {
					Node *right = node->siblingRight->siblingRight;
					Free_Node(node->siblingRight);
					node->siblingRight = right;
					break;
				}
				node = node->siblingRight;
			}
			
			if(node->siblingRight) {
				node->siblingRight->siblingLeft = child;
				child->siblingRight = node->siblingRight;
			}
			node->siblingRight = child;
			child->siblingLeft = node;
		}
	}
	
	if(child->ty != t_CIN) fprintf(stderr,"OK\n");
	
	return 1;
}

void Delete_Node_Object(Node *node, int flag) {
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
	
	switch(node->ty) {
	case t_AE : 
		Delete_AE(node->ri); 
		break;
	case t_CNT : 
		Delete_CNT(node->ri); 
		char *noti_json = (char*)malloc(sizeof("Deleted") + 1);
		strcpy(noti_json, "Deleted");
		Notify_Object(node->child,noti_json,sub_2); 
		free(noti_json);
		break;
	case t_Sub :
		Delete_Sub(node->ri);
		break;
	}
	
	fprintf(stderr,"Free_Node : %s...",node->rn);
	Free_Node(node);
	fprintf(stderr,"OK\n");
	node = NULL;
}

void Free_Node(Node *node) {
	free(node->ri);
	free(node->rn);
	free(node->pi);
	free(node);
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
	
	free(ct);
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
	
	if(m_aei) free(aei);
	free(ct);
	free(et);
	free(ri);
}

void Init_CNT(CNT* cnt, char *pi) {
	char *ct = Get_LocalTime(0);
	char *et = Get_LocalTime(EXPIRE_TIME);
	char *ri = resource_identifier(t_CNT, ct);
	char tmp[MAX_PROPERTY_SIZE];
	
	strcpy(tmp,cnt->rn);
	cnt->rn = (char*)malloc((strlen(cnt->rn) + 1) * sizeof(char));
	strcpy(cnt->rn,tmp);
	
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
	
	free(ct);
	free(et);
	free(ri);
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
	
	free(ct);
	free(et);
	free(ri);
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

	free(ct);
	free(et);
	free(ri);
}

void Set_AE_Update(AE* after) {
	char *rn = Get_JSON_Value_char("rn", payload);
	char *api = Get_JSON_Value_char("api", payload);
	bool rr = Get_JSON_Value_bool("rr", payload);

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

	if(rr) {
		after->rr = rr;
	}
}


void Set_CNT_Update(CNT* after) {
	char *rn = Get_JSON_Value_char("rn", payload);

	if(rn) {
		free(after->rn);
		after->rn = (char*)malloc((strlen(rn) + 1) * sizeof(char));
		strcpy(after->rn, rn);
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
	free(cse->ct);
	free(cse->lt);
	free(cse->rn);
	free(cse->ri);
	free(cse->csi);
	free(cse->pi);
	free(cse);
}

void Free_AE(AE *ae) {
	free(ae->et);
	free(ae->ct);
	free(ae->lt);
	free(ae->rn);
	free(ae->ri);
	free(ae->pi);
	free(ae->api);
	free(ae->aei);
	free(ae);
}

void Free_CNT(CNT *cnt) {
	free(cnt->et);
	free(cnt->ct);
	free(cnt->lt);
	free(cnt->rn);
	free(cnt->ri);
	free(cnt->pi);
	free(cnt);
}

void Free_CIN(CIN* cin) {
	free(cin->et);
	free(cin->ct);
	free(cin->lt);
	free(cin->rn);
	free(cin->ri);
	free(cin->pi);
	free(cin->con);
	free(cin);
}

void Free_Sub(Sub* sub) {
	free(sub->et);
	free(sub->ct);
	free(sub->lt);
	free(sub->rn);
	free(sub->ri);
	free(sub->pi);
	free(sub->nu);
	free(sub->net);
	free(sub);
}

void Notify_Object(Node *node, char *res_json, Net net) {
	Remove_Invalid_Char_JSON(res_json);
	while(node) {
		if(node->ty == t_Sub && (net & node->net) == net) {
			char *noti_json = Noti_to_json(node->sur, (int)log2((double)net ) + 1, res_json);
			char *res = Send_HTTP_Packet(node->nu, noti_json);
			free(noti_json);
			free(res);
		}
		node = node->siblingRight;
	}
}

void Remove_Invalid_Char_JSON(char* json) {
	int size = (int)malloc_usable_size(json);
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