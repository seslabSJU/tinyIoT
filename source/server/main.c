#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include "onem2m.h"

#define CHUNK_SIZE 1024 // read 1024 bytes at a time

// Public directory settings
#define PUBLIC_DIR "./public"
#define INDEX_HTML "/index.html"
#define NOT_FOUND_HTML "/404.html"

ResourceTree *rt;

int main(int c, char **v) {
	init();
 	char *port = c == 1 ? "3000" : v[1];

	serve_forever(port); // main oneM2M operation logic in void route()    

	return 0;
}

void route() {
    double start, end;

    start = (double)clock() / CLOCKS_PER_SEC; // runtime check - start

	Operation op = o_NONE;

	Node* pnode = Parse_URI(rt->cb, uri, &op); // return tree node by URI
	int e = Result_Parse_URI(pnode);

	if(e != -1) e = Check_Payload_Size();

	if(e == -1) return;

	if(op == o_NONE) op = Parse_Operation(); // parse operation by HTTP method

	switch(op) {
	
	case o_CREATE:	
		Create_Object(pnode); break;
	
	case o_RETRIEVE:
		Retrieve_Object(pnode);	break;
		
	case o_UPDATE: 
		Update_Object(pnode); break;
		
	case o_DELETE:
		Delete_Object(pnode); break;

	case o_VIEWER:
		Tree_Viewer_API(pnode); break;
	
	case o_OPTIONS:
		HTTP_200_JSON;
		printf("{\"m2m:dbg\": \"respond options method request\"}");
		break;
	
	default:
		HTTP_500;
		printf("{\"m2m:dbg\": \"internal server error\"}");
	}
	if(pnode->ty == t_CIN) Free_Node(pnode);

	end = (((double)clock()) / CLOCKS_PER_SEC); // runtime check - end
    fprintf(stderr,"Run time :%lf\n", (end-start)); 
}

void init() {
	rt = (ResourceTree *)malloc(sizeof(rt));

	if(access("./RESOURCE.db", 0) == -1) {
		CSE* cse = (CSE*)malloc(sizeof(CSE));
		Init_CSE(cse);
		DB_Store_CSE(cse);
		rt->cb = Create_Node(cse, t_CSE);
		Free_CSE(cse); cse = NULL;
	} else {
		rt->cb = DB_Get_All_CSE();
	}

 	Restruct_ResourceTree();
}

void Create_Object(Node *pnode) {
	ObjectType ty = Parse_ObjectType(); 
	
	int e = Check_Privilege(pnode, acop_Create);
	if(e != -1) e = Check_Request_Body();
	if(e != -1) e = Check_Resource_Name_Duplicate(pnode);
	if(e != -1) e = Check_Resource_Type_Equal(ty, Parse_ObjectType_Body());

	if(e == -1) return;

	switch(ty) {

	case t_CSE :
		/*no such case*/
		break;
		
	case t_AE :
		fprintf(stderr,"\x1b[42mCreate AE\x1b[0m\n");
		Create_AE(pnode);
		break;	
					
	case t_CNT :
		fprintf(stderr,"\x1b[42mCreate CNT\x1b[0m\n");
		Create_CNT(pnode);
		break;
			
	case t_CIN :
		fprintf(stderr,"\x1b[42mCreate CIN\x1b[0m\n");
		Create_CIN(pnode);
		break;

	case t_Sub :
		fprintf(stderr,"\x1b[42mCreate Sub\x1b[0m\n");
		Create_Sub(pnode);
		break;
	
	case t_ACP :
		fprintf(stderr,"\x1b[42mCreate ACP\x1b[0m\n");
		Create_ACP(pnode);
		break;

	default :
		fprintf(stderr,"Resource type error (Content-Type Header Invalid)\n");
		HTTP_400;
		printf("{\"m2m:dbg\": \"resource type error (Content-Type header invalid)\"}");
	}	
}

void Retrieve_Object(Node *pnode) {
	int e = Check_Privilege(pnode, acop_Retrieve);

	if(e == -1) return;

	int fu = get_value_querystring_int("fu");

	if(fu == 1) {
		fprintf(stderr,"\x1b[43mRetrieve FilterCriteria\x1b[0m\n");
		Retrieve_Object_FilterCriteria(pnode);
		return;
	}

	switch(pnode->ty) {
		
	case t_CSE :
		/*
		//fc: ZeroconfDiscovery
    	if(request_header("X-fc") && !strcmp(request_header("X-fc"), "Zeroconf")) {
        	fprintf(stderr,"\x1b[43mRetrieve CSE Zero-conf\x1b[0m\n");
        	char* zeroconf_data = Zeroconf_Discovery();
        	//HTTP_200_CORS;
        	printf("%s 200 OK\n%s\n\n", RESPONSE_PROTOCOL, "Access-Control-Allow-Origin: *\nAccess-Control-Allow-Headers: Accept, Accept-Language, Content-Language, Content-Type\nAccess-Control-Allow-Methods: GET, PUT, POST, DELETE, OPTIONS\nAccess-Control-Request-Methods: GET, PUT, POST, DELETE, OPTIONS\nX-TinyIoT-ID:SejongTinyIoT");
        	printf("%s",zeroconf_data);
      	}*/
        fprintf(stderr,"\x1b[43mRetrieve CSE\x1b[0m\n");
        Retrieve_CSE(pnode);
      	break;
	case t_AE : 
		fprintf(stderr,"\x1b[43mRetrieve AE\x1b[0m\n");
		Retrieve_AE(pnode);	
		break;	
			
	case t_CNT :
		fprintf(stderr,"\x1b[43mRetrieve CNT\x1b[0m\n");
		Retrieve_CNT(pnode);			
		break;
			
	case t_CIN :
		fprintf(stderr,"\x1b[43mRetrieve CIN\x1b[0m\n");
		Retrieve_CIN(pnode);			
		break;

	case t_Sub :
		fprintf(stderr,"\x1b[43mRetrieve Sub\x1b[0m\n");
		Retrieve_Sub(pnode);			
		break;

	case t_ACP :
		fprintf(stderr,"\x1b[43mRetrieve ACP\x1b[0m\n");
		Retrieve_ACP(pnode);			
		break;
	}	
}

void Update_Object(Node *pnode) {
	ObjectType ty = Parse_ObjectType_Body(); 

	int e = Check_Privilege(pnode, acop_Update);
	if(e != -1) e = Check_Request_Body();
	if(e != -1) e = Check_Resource_Name_Duplicate(pnode->parent);
	if(e != -1) e = Check_Resource_Type_Equal(ty, pnode->ty);

	if(e == -1) return;
	
	switch(ty) {

	case t_CSE :
		/*No Definition such request*/
		break;

	case t_AE :
		fprintf(stderr,"\x1b[45mUpdate AE\x1b[0m\n");
		Update_AE(pnode);
		break;

	case t_CNT :
		fprintf(stderr,"\x1b[45mUpdate CNT\x1b[0m\n");
		Update_CNT(pnode);
		break;

	case t_Sub :
		fprintf(stderr,"\x1b[45mUpdate Sub\x1b[0m\n");
		Update_Sub(pnode);
		break;
	
	case t_ACP :
		fprintf(stderr,"\x1b[45mUpdate ACP\x1b[0m\n");
		Update_ACP(pnode);
		break;
	}
}

void Create_AE(Node *pnode) {
	if(pnode->ty != t_CSE) {
		HTTP_403;
		printf("{\"m2m:dbg\": \"AE can only be created under CSE\"}");
		return;
	}
	AE* ae = JSON_to_AE(payload);
	if(!ae) {
		JSON_Parse_Error();
		return;
	}
	Init_AE(ae,pnode->ri);
	
	int result = DB_Store_AE(ae);
	if(result != 1) { 
		HTTP_500;
		printf("{\"m2m:dbg\": \"DB store fail\"}");
		Free_AE(ae); ae = NULL;
		return;
	}
	
	Node* node = Create_Node(ae, t_AE);
	Add_child(pnode,node);
	
	char *res_json = AE_to_json(ae);
	HTTP_201_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL;
	Free_AE(ae); ae = NULL;
}

void Create_CNT(Node *pnode) {
	if(pnode->ty != t_CNT && pnode->ty != t_AE) {
		HTTP_403;
		printf("{\"m2m:dbg\": \"container can only be created under AE, container\"}");
		return;
	}
	CNT* cnt = JSON_to_CNT(payload);
	if(!cnt) {
		JSON_Parse_Error();
		return;
	}
	Init_CNT(cnt,pnode->ri);

	int result = DB_Store_CNT(cnt);
	if(result != 1) { 
		HTTP_500;
		printf("{\"m2m:dbg\": \"DB store fail\"}");
		Free_CNT(cnt); cnt = NULL;
		return;
	}
	
	Node* node = Create_Node(cnt, t_CNT);
	Add_child(pnode,node);

	char *res_json = CNT_to_json(cnt);
	HTTP_201_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL; 
	Free_CNT(cnt); cnt = NULL;
}

void Create_CIN(Node *pnode) {
	if(pnode->ty != t_CNT) {
		HTTP_403;
		printf("{\"m2m:dbg\": \"contentinstance can only be created under container\"}");
		return;
	}
	CIN* cin = JSON_to_CIN(payload);
	if(!cin) {
		JSON_Parse_Error();
		return;
	}
	Init_CIN(cin,pnode->ri);

	int result = DB_Store_CIN(cin);
	if(result != 1) { 
		HTTP_500;
		printf("{\"m2m:dbg\": \"DB store fail\"}");
		Free_CIN(cin);
		cin = NULL;
		return;
	}
	
	char *res_json = CIN_to_json(cin);
	HTTP_201_JSON;
	printf("%s", res_json);
	Notify_Object(pnode->child, res_json, noti_event_3);
	free(res_json); res_json = NULL; 
	Free_CIN(cin); cin = NULL;
	pnode->cinSize++;
}

void Create_Sub(Node *pnode) {
	if(pnode->ty == t_CIN) {
		HTTP_403;
		printf("{\"m2m:dbg\": \"subscription can not be created under contentinstance\"}");
		return;
	}
	Sub* sub = JSON_to_Sub(payload);
	if(!sub) {
		JSON_Parse_Error();
		return;
	}
	Init_Sub(sub, pnode->ri);
	
	int result = DB_Store_Sub(sub);
	if(result != 1) { 
		HTTP_500;
		printf("{\"m2m:dbg\": \"DB store fail\"}");
		Free_Sub(sub); sub = NULL;
		return;
	}
	
	Node* node = Create_Node(sub, t_Sub);
	Add_child(pnode,node);
	
	char *res_json = Sub_to_json(sub);
	HTTP_201_JSON;
	printf("%s", res_json);
	char *res = Send_HTTP_Packet(sub->nu, res_json);
	if(res) { free(res); res = NULL; }
	free(res_json); res_json = NULL;
	Free_Sub(sub); sub = NULL;
}

void Create_ACP(Node *pnode) {
	if(pnode->ty != t_CSE && pnode->ty != t_AE) {
		HTTP_403;
		printf("{\"m2m:dbg\": \"ACP can only be created under CSE, AE\"}");
		return;
	}
	ACP* acp = JSON_to_ACP(payload);
	if(!acp) {
		JSON_Parse_Error();
		return;
	}
	Init_ACP(acp, pnode->ri);
	
	int result = DB_Store_ACP(acp);
	if(result != 1) { 
		HTTP_500;
		printf("{\"m2m:dbg\": \"DB store fail\"}");
		Free_ACP(acp); acp = NULL;
		return;
	}
	
	Node* node = Create_Node(acp, t_ACP);
	Add_child(pnode,node);
	
	char *res_json = ACP_to_json(acp);
	HTTP_201_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL; 
	Free_ACP(acp); acp = NULL;
}

void Retrieve_CSE(Node *pnode){
	fprintf(stderr,"Child CIN Size : %d\n",pnode->cinSize);
	CSE* gcse = DB_Get_CSE(pnode->ri);
	char *res_json = CSE_to_json(gcse);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL; 
	Free_CSE(gcse); gcse = NULL;
}

void Retrieve_AE(Node *pnode){
	fprintf(stderr,"Child CIN Size : %d\n",pnode->cinSize);
	AE* gae = DB_Get_AE(pnode->ri);
	char *res_json = AE_to_json(gae);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL;
	Free_AE(gae); gae = NULL;
}

void Retrieve_CNT(Node *pnode){
	fprintf(stderr,"Child CIN Size : %d\n",pnode->cinSize);
	CNT* gcnt = DB_Get_CNT(pnode->ri);
	char *res_json = CNT_to_json(gcnt);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL;
	Free_CNT(gcnt); gcnt = NULL;
}

void Retrieve_CIN(Node *pnode){
	CIN* gcin = DB_Get_CIN(pnode->ri);
	char *res_json = CIN_to_json(gcin);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL; 
	Free_CIN(gcin); gcin = NULL;
}

void Retrieve_Sub(Node *pnode){
	Sub* gsub = DB_Get_Sub(pnode->ri);
	char *res_json = Sub_to_json(gsub);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL; 
	Free_Sub(gsub); gsub = NULL;
}

void Retrieve_ACP(Node *pnode){
	ACP* gacp = DB_Get_ACP(pnode->ri);
	char *res_json = ACP_to_json(gacp);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL; 
	Free_ACP(gacp); gacp = NULL;
}

void Update_AE(Node *pnode) {
	AE* after = DB_Get_AE(pnode->ri);
	int result;

	Set_AE_Update(after);
	result = DB_Delete_Object(after->ri);
	result = DB_Store_AE(after);
	
	free(pnode->rn);
	pnode->rn = (char *)malloc((strlen(after->rn) + 1) * sizeof(char));
	strcpy(pnode->rn, after->rn);
	
	char *res_json = AE_to_json(after);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL; 
	Free_AE(after); after = NULL;
}

void Update_CNT(Node *pnode) {
	CNT* after = DB_Get_CNT(pnode->ri);
	int result;

	Set_CNT_Update(after);
	result = DB_Delete_Object(after->ri);
	result = DB_Store_CNT(after);
	
	free(pnode->rn);
	pnode->rn = (char *)malloc((strlen(after->rn) + 1) * sizeof(char));
	strcpy(pnode->rn, after->rn);

	if(pnode->acpi) {
		free(pnode->acpi);
		pnode->acpi = (char *)malloc((strlen(after->acpi) + 1) * sizeof(char));
		strcpy(pnode->acpi, after->acpi);
	}
	
	char *res_json = CNT_to_json(after);
	HTTP_200_JSON;
	printf("%s", res_json);
	Notify_Object(pnode->child, res_json, noti_event_1);
	free(res_json); res_json = NULL;
	Free_CNT(after); after = NULL;
}

void Update_Sub(Node *pnode) {
	Sub* after = DB_Get_Sub(pnode->ri);
	int result;
	
	Set_Sub_Update(after);
	result = DB_Delete_Sub(after->ri);
	result = DB_Store_Sub(after);
	
	free(pnode->rn);
	pnode->rn = (char *)malloc((strlen(after->rn) + 1) * sizeof(char));
	strcpy(pnode->rn, after->rn);
	
	char *res_json = Sub_to_json(after);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL;
	Free_Sub(after); after = NULL;
}

void Update_ACP(Node *pnode) {
	ACP* after = DB_Get_ACP(pnode->ri);
	int result;
	
	Set_ACP_Update(after);
	result = DB_Delete_ACP(after->ri);
	result = DB_Store_ACP(after);
	
	free(pnode->rn);
	pnode->rn = (char *)malloc((strlen(after->rn) + 1) * sizeof(char));
	strcpy(pnode->rn, after->rn);
	
	char *res_json = ACP_to_json(after);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL;
	Free_ACP(after); after = NULL;
}

void Delete_Object(Node* pnode) {
	fprintf(stderr,"\x1b[41mDelete Object\x1b[0m\n");
	Delete_Node_and_DB_Data(pnode,1);
	pnode = NULL;
	HTTP_200_JSON;
	printf("{\"m2m:dbg\": \"resource is deleted successfully\"}");
}

void Restruct_ResourceTree(){
	Node *node_list = (Node *)calloc(1,sizeof(Node));
	Node *tail = node_list;
	
	if(access("./RESOURCE.db", 0) != -1) {
		Node* ae_list = DB_Get_All_AE();
		tail->siblingRight = ae_list;
		if(ae_list) ae_list->siblingLeft = tail;
		while(tail->siblingRight) tail = tail->siblingRight;

		Node* cnt_list = DB_Get_All_CNT();
		tail->siblingRight = cnt_list;
		if(cnt_list) cnt_list->siblingLeft = tail;
		while(tail->siblingRight) tail = tail->siblingRight;
	} else {
		fprintf(stderr,"RESOURCE.db is not exist\n");
	}
	
	if(access("./SUB.db", 0) != -1) {
		Node* sub_list = DB_Get_All_Sub();
		tail->siblingRight = sub_list;
		if(sub_list) sub_list->siblingLeft = tail;
		while(tail->siblingRight) tail = tail->siblingRight;
	} else {
		fprintf(stderr,"SUB.db is not exist\n");
	}

	if(access("./ACP.db", 0) != -1) {
		Node* acp_list = DB_Get_All_ACP();
		tail->siblingRight = acp_list;
		if(acp_list) acp_list->siblingLeft = tail;
		while(tail->siblingRight) tail = tail->siblingRight;
	} else {
		fprintf(stderr,"ACP.db is not exist\n");
	}
	
	Node *fnode = node_list;
	node_list = node_list->siblingRight;
	if(node_list) node_list->siblingLeft = NULL;
	Free_Node(fnode);
	
	if(node_list) Restruct_childs(rt->cb, node_list);
}

Node* Restruct_childs(Node *pnode, Node *list) {
	Node *node = list;
	
	while(node) {
		Node *right = node->siblingRight;

		if(!strcmp(pnode->ri, node->pi)) {
			Node *left = node->siblingLeft;
			
			if(!left) {
				list = right;
			} else {
				left->siblingRight = right;
			}
			
			if(right) right->siblingLeft = left;
			node->siblingLeft = node->siblingRight = NULL;
			Add_child(pnode, node);
		}
		node = right;
	}
	Node *child = pnode->child;
	
	while(child) {
		list = Restruct_childs(child, list);
		child = child->siblingRight;
	}
	
	return list;
}

void JSON_Parse_Error(){
	fprintf(stderr,"Request JSON Invalid\n");
	printf("{\"m2m:dbg\": \"request JSON invalid\"}");
}

int Check_Privilege(Node *node, ACOP acop) {
	if(node->ty == t_CIN) node = node->parent;
	if(node->ty != t_CNT) return 0;

	if((get_acop(node) & acop) != acop) {
		fprintf(stderr,"Origin has no privilege\n");
		HTTP_403;
		printf("{\"m2m:dbg\": \"access denied\"}");
		return -1;
	}
	return 0;
}

int Check_Request_Body() {
	if(!payload) {
		HTTP_500;
		fprintf(stderr,"Request body empty error\n");
		printf("{\"m2m:dbg\": \"request body empty\"\n}");
		return -1;
	}
	return 0;
}

int Check_Resource_Name_Duplicate(Node *node) {
	if(duplicate_resource_check(node)) {
		HTTP_209_JSON;
		fprintf(stderr,"Resource name duplicate error\n");
		printf("{\"m2m:dbg\": \"rn is duplicated\"}");
		return -1;
	}
	return 0;
}

int Check_Resource_Type_Equal(ObjectType ty1, ObjectType ty2) {	
	if(ty1 != ty2) {
		fprintf(stderr,"Resource type error\n");
		HTTP_400;
		printf("{\"m2m:dbg\": \"resource type error\"}");
		return -1;
	}
	return 0;
}

int Result_Parse_URI(Node *node) {
	if(!node) {
		fprintf(stderr,"Invalid\n");
		HTTP_404;
		printf("{\"m2m:dbg\": \"URI is invalid\"}");
		return -1;
	} else {
		fprintf(stderr,"OK\n");
		return 0;
	} 
}

int Check_Payload_Size() {
	if(payload && payload_size > MAX_PAYLOAD_SIZE) {
		fprintf(stderr,"Request payload too large\n");
		HTTP_413;
		printf("{\"m2m:dbg\": \"payload is too large\"}");
		return -1;
	}
	return 0;
}

void Retrieve_FilterCriteria_Data(Node *node, ObjectType ty, char **discovery_list, int *size, int level, int curr, int flag) {
	if(!node || curr > level) return;

	Node *child[1024];
	Node *sibling[1024];
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
		node = node->siblingRight;
	}

	if((ty == -1 || ty == t_CIN) && curr < level) {
		for(int i= 0; i <= index; i++) {
			Node *cin_list_head = DB_Get_CIN_Pi(sibling[i]->ri);
			Node *p = cin_list_head;

			while(p) {
				discovery_list[*size] = (char*)malloc(MAX_URI_SIZE*sizeof(char));
				strcpy(discovery_list[*size], sibling[i]->uri);
				strcat(discovery_list[*size], "/");
				strcat(discovery_list[*size], p->ri);
				(*size)++;
				p = p->siblingRight;
			}

			Free_Node_List(cin_list_head);
		}
	}

	for(int i = 0; i<=index; i++) {
		Retrieve_FilterCriteria_Data(child[i], ty, discovery_list, size, level, curr+1, 0);
	} 
}

void Retrieve_Object_FilterCriteria(Node *pnode) {
	char **discovery_list = (char**)malloc(65536*sizeof(char*));
	int size = 0;
	ObjectType ty = get_value_querystring_int("ty");
	int level = get_value_querystring_int("lvl");
	if(level == -1) level = 987654321;

	Retrieve_FilterCriteria_Data(pnode, ty, discovery_list, &size, level, 0, 1);

	char *res_json = Discovery_to_json(discovery_list, size);
	for(int i=0; i<size; i++) free(discovery_list[i]);
	free(discovery_list);

	HTTP_200_JSON;
	printf("%s",res_json);
	free(res_json); res_json = NULL;

	return;
}