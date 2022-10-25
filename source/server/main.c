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
#define MAX_PAYLOAD_SIZE 16384

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

	//if(Label_To_URI(uri)) { uri = Label_To_URI(uri);}

	Operation op = o_NONE;

	Node* pnode = Parse_URI(rt->cb, uri, &op); // return tree node by URI
	if(!pnode) {
		if(op != o_CIN_RI) { 
			fprintf(stderr,"Invalid\n");
			HTTP_404;
			printf("{\"m2m:dbg\": \"URI is invalid\"}");
		}
		return;
	} else {
		fprintf(stderr,"OK\n");
	} 

	if(payload && payload_size > MAX_PAYLOAD_SIZE) {
		fprintf(stderr,"Request payload too large\n");
		HTTP_413;
		printf("{\"m2m:dbg\": \"payload is too large\"}");
		return;
	}

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
		printf("{\"m2m:dbg\": \"unexpected server error\"}");
	}

	end = (((double)clock()) / CLOCKS_PER_SEC); // runtime check - end
    fprintf(stderr,"Run time :%lf\n", (end-start)); 
}

void init() {
	CSE *cse;
	
	if(access("./CSE.db", 0) == -1) {
		cse = (CSE*)malloc(sizeof(CSE));
		Init_CSE(cse);
		Store_CSE(cse);
	} else {
		cse = Get_CSE();
	}
	rt = (ResourceTree *)malloc(sizeof(rt));
 	rt->cb = Create_Node(cse, t_CSE);
 	Free_CSE(cse); cse = NULL;
 	Restruct_ResourceTree();
}

void Create_Object(Node *pnode) {
	if((Get_acop(pnode) & acop_Create) != acop_Create) {
		fprintf(stderr,"Originator has no privilege\n");
		HTTP_403;
		printf("{\"m2m:dbg\": \"originator has no privilege\"}");
		return;
	}

	if(!payload) {
		HTTP_500;
		fprintf(stderr,"Request body empty\n");
		printf("{\"m2m:dbg\": \"request body empty\"}"); 
		return;
	}

	if(duplicate_resource_check(pnode)) {
		HTTP_209_JSON;
		fprintf(stderr,"Resource duplicate error\n");
		printf("{\"m2m:dbg\": \"resource is already exist\"}");
		return;
	}

	ObjectType ty = Parse_ObjectType(); // parse object type by payload json key -> "m2m:??"
	switch(ty) {

	case t_CSE :
		/*No Definition such request*/
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
	if((Get_acop(pnode) & acop_Retrieve) != acop_Retrieve) {
		fprintf(stderr,"Originator has no privilege\n");
		HTTP_403;
		printf("{\"m2m:dbg\": \"originator has no privilege\"}");
		return;
	}

	switch(pnode->ty) {
		
	case t_CSE :
		if(request_header("X-fc") && !strcmp(request_header("X-fc"), "Zeroconf")) {
			fprintf(stderr,"\x1b[43mRetrieve CSE Zero-conf\x1b[0m\n");
		} else {
			fprintf(stderr,"\x1b[43mRetrieve CSE\x1b[0m\n");
			Retrieve_CSE(pnode);
		}
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
		//Retrieve_ACP(pnode);			
		break;
	}	
}

void Update_Object(Node *pnode) {
	if((Get_acop(pnode) & acop_Update) != acop_Update) {
		fprintf(stderr,"Originator has no privilege\n");
		HTTP_403;
		printf("{\"m2m:dbg\": \"originator has no privilege\"}");
		return;
	}

	if(!payload) {
		HTTP_500;
		fprintf(stderr,"Request body empty error\n");
		printf("{\"m2m:dbg\": \"request body empty\"\n}");
		return;
	}

	if(duplicate_resource_check(pnode->parent)) {
		HTTP_209_JSON;
		fprintf(stderr,"Resource duplicate error\n");
		printf("{\"m2m:dbg\": \"resource \"rn\" is duplicated\"}");
		return;
	}

	ObjectType ty = Parse_ObjectType_Body();
	
	if(ty != pnode->ty) {
		fprintf(stderr,"Update resource type error\n");
		HTTP_400;
		printf("{\"m2m:dbg\": \"resource type error\"}");
		return;
	}
	
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
		//Update_ACP(pnode);
		break;
	}
}

void Create_AE(Node *pnode) {
	AE* ae = JSON_to_AE(payload);
	if(!ae) {
		Response_JSON_Parse_Error();
		return;
	}
	Init_AE(ae,pnode->ri);
	
	int result = Store_AE(ae);
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
	CNT* cnt = JSON_to_CNT(payload);
	if(!cnt) {
		Response_JSON_Parse_Error();
		return;
	}
	Init_CNT(cnt,pnode->ri);

	int result = Store_CNT(cnt);
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
	CIN* cin = JSON_to_CIN(payload);
	if(!cin) {
		Response_JSON_Parse_Error();
		return;
	}
	Init_CIN(cin,pnode->ri);

	int result = Store_CIN(cin);
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
	Sub* sub = JSON_to_Sub(payload);
	if(!sub) {
		Response_JSON_Parse_Error();
		return;
	}
	Init_Sub(sub, pnode->ri);
	
	int result = Store_Sub(sub);
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
	free(res); res = NULL;
	free(res_json); res_json = NULL;
	Free_Sub(sub); sub = NULL;
}

void Create_ACP(Node *pnode) {
	ACP* acp = JSON_to_ACP(payload);
	if(!acp) {
		Response_JSON_Parse_Error();
		return;
	}
	Init_ACP(acp, pnode->ri);
	
	int result = Store_ACP(acp);
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
	CSE* gcse = Get_CSE(pnode->ri);
	char *res_json = CSE_to_json(gcse);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL; 
	Free_CSE(gcse); gcse = NULL;
}

void Retrieve_AE(Node *pnode){
	fprintf(stderr,"Child CIN Size : %d\n",pnode->cinSize);
	AE* gae = Get_AE(pnode->ri);
	char *res_json = AE_to_json(gae);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL;
	Free_AE(gae); gae = NULL;
}

void Retrieve_CNT(Node *pnode){
	fprintf(stderr,"Child CIN Size : %d\n",pnode->cinSize);
	CNT* gcnt = Get_CNT(pnode->ri);
	char *res_json = CNT_to_json(gcnt);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL;
	Free_CNT(gcnt); gcnt = NULL;
}

void Retrieve_CIN(Node *pnode){
	CIN* gcin = Get_CIN(pnode->ri);
	char *res_json = CIN_to_json(gcin);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL; 
	Free_CIN(gcin); gcin = NULL;
}

void Retrieve_Sub(Node *pnode){
	Sub* gsub = Get_Sub(pnode->ri);
	char *res_json = Sub_to_json(gsub);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL; 
	Free_Sub(gsub); gsub = NULL;
}

void Update_AE(Node *pnode) {
	AE* after = Get_AE(pnode->ri);
	
	Set_AE_Update(after);
	int result = Update_AE_DB(after);
	if(result != 1) { 
		HTTP_500;
		printf("{\"m2m:dbg\": \"DB update fail\"}");
		Free_AE(after); after = NULL;
		return;
	}
	
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
	CNT* after = Get_CNT(pnode->ri);

	Set_CNT_Update(after);
	int result = Update_CNT_DB(after);
	if(result != 1) { 
		HTTP_500;
		printf("{\"m2m:dbg\": \"DB update fail\"}");
		Free_CNT(after); after = NULL;
		return;
	}
	
	free(pnode->rn);
	pnode->rn = (char *)malloc((strlen(after->rn) + 1) * sizeof(char));
	strcpy(pnode->rn, after->rn);
	
	char *res_json = CNT_to_json(after);
	HTTP_200_JSON;
	printf("%s", res_json);
	Notify_Object(pnode->child, res_json, noti_event_1);
	free(res_json); res_json = NULL;
	Free_CNT(after); after = NULL;
}

void Update_Sub(Node *pnode) {
	Sub* after = Get_Sub(pnode->ri);
	
	Set_Sub_Update(after);
	int result = Update_Sub_DB(after);
	if(result != 1) { 
		HTTP_500;
		printf("{\"m2m:dbg\": \"DB update fail\"}");
		Free_Sub(after); after = NULL;
		return;
	}
	
	free(pnode->rn);
	pnode->rn = (char *)malloc((strlen(after->rn) + 1) * sizeof(char));
	strcpy(pnode->rn, after->rn);
	
	char *res_json = Sub_to_json(after);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json); res_json = NULL;
	Free_Sub(after); after = NULL;
}

void Delete_Object(Node* pnode) {
	fprintf(stderr,"\x1b[41mDelete Object\x1b[0m\n");
	Delete_Node_Object(pnode,1);
	pnode = NULL;
	HTTP_200_JSON;
	printf("{\"m2m:dbg\": \"resource is deleted successfully\"}");
}

void Restruct_ResourceTree(){
	Node *node_list = (Node *)calloc(1,sizeof(Node));
	Node *tail = node_list;
	
	if(access("./AE.db", 0) != -1) {
		Node* ae_list = Get_All_AE();
		tail->siblingRight = ae_list;
		if(ae_list) ae_list->siblingLeft = tail;
		while(tail->siblingRight) tail = tail->siblingRight;
	} else {
		fprintf(stderr,"AE.db is not exist\n");
	}
	
	if(access("./CNT.db", 0) != -1) {
		Node* cnt_list = Get_All_CNT();
		tail->siblingRight = cnt_list;
		if(cnt_list) cnt_list->siblingLeft = tail;
		while(tail->siblingRight) tail = tail->siblingRight;
	} else {
		fprintf(stderr,"CNT.db is not exist\n");
	}
	
	if(access("./SUB.db", 0) != -1) {
		Node* sub_list = Get_All_Sub();
		tail->siblingRight = sub_list;
		if(sub_list) sub_list->siblingLeft = tail;
		while(tail->siblingRight) tail = tail->siblingRight;
	} else {
		fprintf(stderr,"SUB.db is not exist\n");
	}
	
	/*
	if(access("./CIN.db", 0) != -1) {
		Node* cin_list = Get_All_CIN();
		tail->siblingRight = cin_list;
		if(cin_list) cin_list->siblingLeft = tail;
		while(tail->siblingRight) tail = tail->siblingRight;
	} else {
		fprintf(stderr,"CIN.db is not exist\n");
	}
	*/
	
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

void Response_JSON_Parse_Error(){
	fprintf(stderr,"Request JSON Invalid\n");
	printf("{\"m2m:dbg\": \"request JSON invalid\"}");
}


// httpd open source basic functions
int file_exists(const char *file_name) {
	struct stat buffer;
	int exists;

	exists = (stat(file_name, &buffer) == 0);

	return exists;
}

int read_file(const char *file_name) {
	char buf[CHUNK_SIZE];
	FILE *file;
	size_t nread;
	int err = 1;

	file = fopen(file_name, "r");

	if (file) {
		while ((nread = fread(buf, 1, sizeof buf, file)) > 0) {
			fwrite(buf, 1, nread, stdout);
		}

		err = ferror(file);
		fclose(file);
	}
	
	return err;
}
