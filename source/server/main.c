#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include "zeroconf.h"

#define CHUNK_SIZE 1024 // read 1024 bytes at a time

// Public directory settings
#define PUBLIC_DIR "./public"
#define INDEX_HTML "/index.html"
#define NOT_FOUND_HTML "/404.html"
#define MAX_PAYLOAD_SIZE 16384

RT *rt;

int main(int c, char **v) {
	fprintf(stderr,"PID : %d\n",getpid());
	init();
 	char *port = c == 1 ? "3000" : v[1];

	serve_forever(port);
    
	return 0;
}

void route() {
	if(Label_To_URI(uri)) {
		uri = Label_To_URI(uri);
	}

	Node* pnode = Parse_URI(rt);
	if(!pnode) {
		return;
	}

	fprintf(stderr,"rn : %s\n",pnode->rn);

	if(payload && payload_size > MAX_PAYLOAD_SIZE) {
		HTTP_406;
		fprintf(stderr,"Request Payload Too Large\n");
		printf("{\"m2m:dbg\": \"request data is too large\"}");
		return;
	}

	Operation op = Parse_Operation();
	
	switch(op) {
	
	case o_CREATE:	
		Create_Object(pnode); break;
	
	case o_RETRIEVE:
		Retrieve_Object(pnode);	break;
		
	case o_UPDATE: 
		Update_Object(pnode); break;
		
	case o_DELETE:
		Delete_Object(pnode); break;
	
	case o_OPTIONS:
		HTTP_200_JSON;
		printf("{\"m2m:dbg\": \"respond options method request\"}");
		break;
	
	default:
		HTTP_500;
	}
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
	rt = (RT *)malloc(sizeof(rt));
 	rt->root = Create_Node(cse->ri, cse->rn, cse->pi, "\0", "\0", 0, t_CSE);
 	Free_CSE(cse);
 	cse = NULL;
 	Restruct_ResourceTree();
}

void Create_Object(Node *pnode) {
	if(!payload) {
		HTTP_500;
		fprintf(stderr,"Request Empty\n");
		printf("{\"m2m:dbg\": \"request body empty\"}"); // Need oneM2M WireShark packet Check
		return;
	}

	if(duplicate_resource_check(pnode)) {
		HTTP_209_JSON;
		fprintf(stderr,"Resource Duplicate Error\n");
		printf("{\"m2m:dbg\": \"resource is already exist\"}");
		return;
	}

	ObjectType ty = Parse_ObjectType();
	switch(ty) {
		
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

	case t_CSE :
		/*No Definition such request*/

	default :
		fprintf(stderr,"Resource Type Error (Content-Type Header Invalid)\n");
		HTTP_400;
		printf("{\"m2m:dbg\": \"resource type error (Content-Type header invalid)\"}");
	}	
}

void Retrieve_Object(Node *pnode) {
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
	}	
}

void Update_Object(Node *pnode) {
	if(!payload) {
		HTTP_500;
		fprintf(stderr,"Request Empty Error\n");
		printf("{\"m2m:dbg\": \"request body empty\"\n}");
		return;
	}

	ObjectType ty = Parse_ObjectType_Body();
	
	if(ty != pnode->ty) {
		fprintf(stderr,"Update Resource Type Error\n");
		HTTP_400;
		printf("{\"m2m:dbg\": \"resource type error\"}");
		return;
	}
	
	switch(ty) {
	
	case t_CSE :
		break;
	case t_AE :
		fprintf(stderr,"\x1b[45mUpdate AE\x1b[0m\n");
		Update_AE(pnode);
		break;
	case t_CNT :
		fprintf(stderr,"\x1b[45mUpdate CNT\x1b[0m\n");
		Update_CNT(pnode);
	case t_Sub :
		fprintf(stderr,"\x1b[45mUpdate Sub\x1b[0m\n");
		Update_Sub(pnode);
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
		Free_AE(ae);
		ae = NULL;
		return;
	}
	
	Node* node = Create_Node(ae->ri, ae->rn, ae->pi, "\0", "\0", 0, t_AE);
	Add_child(pnode,node);
	
	char *res_json = AE_to_json(ae);
	HTTP_201_JSON;
	printf("%s", res_json);
	free(res_json);
	Free_AE(ae);
	res_json = NULL;
	ae = NULL;
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
		Free_CNT(cnt);
		cnt = NULL;
		return;
	}
	
	Node* node = Create_Node(cnt->ri, cnt->rn, cnt->pi, "\0", "\0", 0, t_CNT);
	Add_child(pnode,node);
	
	char *res_json = CNT_to_json(cnt);
	HTTP_201_JSON;
	printf("%s", res_json);
	free(res_json);
	Free_CNT(cnt);
	res_json = NULL;
	cnt = NULL;
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
	
	Node* node = Create_Node(cin->ri, cin->rn, cin->pi, "\0", "\0", 0, t_CIN);
	Add_child(pnode,node);
	char *res_json = CIN_to_json(cin);
	HTTP_201_JSON;
	printf("%s", res_json);
	Notify_Object(pnode->child, res_json, sub_3);
	free(res_json);
	Free_CIN(cin);
	res_json = NULL;
	cin = NULL;
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
		Free_Sub(sub);
		sub = NULL;
		return;
	}
	
	Node* node = Create_Node(sub->ri, sub->rn, sub->pi, sub->nu, sub->sur, net_to_bit(sub->net), t_Sub);
	Add_child(pnode,node);
	
	char *res_json = Sub_to_json(sub);
	HTTP_201_JSON;
	printf("%s", res_json);
	char *res = Send_HTTP_Packet(sub->nu, res_json);
	free(res);
	free(res_json);
	Free_Sub(sub);
	res_json = NULL;
	sub = NULL;
}

void Retrieve_CSE(Node *pnode){
	fprintf(stderr,"Child CIN Size : %d\n",pnode->cinSize);
	CSE* gcse = Get_CSE(pnode->ri);
	char *res_json = CSE_to_json(gcse);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json);
	Free_CSE(gcse);
	res_json = NULL;
	gcse = NULL;
}

void Retrieve_AE(Node *pnode){
	fprintf(stderr,"Child CIN Size : %d\n",pnode->cinSize);
	AE* gae = Get_AE(pnode->ri);
	char *res_json = AE_to_json(gae);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json);
	Free_AE(gae);
	res_json = NULL;
	gae = NULL;
}

void Retrieve_CNT(Node *pnode){
	fprintf(stderr,"Child CIN Size : %d\n",pnode->cinSize);
	CNT* gcnt = Get_CNT(pnode->ri);
	char *res_json = CNT_to_json(gcnt);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json);
	Free_CNT(gcnt);
	res_json = NULL;
	gcnt = NULL;
}

void Retrieve_CIN(Node *pnode){
	CIN* gcin = Get_CIN(pnode->ri);
	char *res_json = CIN_to_json(gcin);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json);
	Free_CIN(gcin);
	res_json = NULL;
	gcin = NULL;
}

void Retrieve_Sub(Node *pnode){
	Sub* gsub = Get_Sub(pnode->ri);
	char *res_json = Sub_to_json(gsub);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json);
	Free_Sub(gsub);
	res_json = NULL;
	gsub = NULL;
}

void Update_AE(Node *pnode) {
	AE* after = Get_AE(pnode->ri);
	
	Set_AE_Update(after);
	int result = Update_AE_DB(after);
	if(result != 1) { 
		HTTP_500;
		printf("{\"m2m:dbg\": \"DB update fail\"}");
		Free_AE(after);
		after = NULL;
		return;
	}
	
	free(pnode->rn);
	pnode->rn = (char *)malloc((strlen(after->rn) + 1) * sizeof(char));
	strcpy(pnode->rn, after->rn);
	
	char *res_json = AE_to_json(after);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json);
	Free_AE(after);
	res_json = NULL;
	after = NULL;
}

void Update_CNT(Node *pnode) {
	CNT* after = Get_CNT(pnode->ri);

	Set_CNT_Update(after);
	int result = Update_CNT_DB(after);
	if(result != 1) { 
		HTTP_500;
		printf("{\"m2m:dbg\": \"DB update fail\"}");
		Free_CNT(after);
		after = NULL;
		return;
	}
	
	free(pnode->rn);
	pnode->rn = (char *)malloc((strlen(after->rn) + 1) * sizeof(char));
	strcpy(pnode->rn, after->rn);
	
	char *res_json = CNT_to_json(after);
	HTTP_200_JSON;
	printf("%s", res_json);
	Notify_Object(pnode->child, res_json, sub_1);
	free(res_json);
	Free_CNT(after);
	res_json = NULL;
	after = NULL;
}

void Update_Sub(Node *pnode) {
	Sub* after = Get_Sub(pnode->ri);
	
	Set_Sub_Update(after);
	int result = Update_Sub_DB(after);
	if(result != 1) { 
		HTTP_500;
		printf("{\"m2m:dbg\": \"DB update fail\"}");
		Free_Sub(after);
		after = NULL;
		return;
	}
	
	free(pnode->rn);
	pnode->rn = (char *)malloc((strlen(after->rn) + 1) * sizeof(char));
	strcpy(pnode->rn, after->rn);
	
	char *res_json = Sub_to_json(after);
	HTTP_200_JSON;
	printf("%s", res_json);
	free(res_json);
	Free_Sub(after);
	res_json = NULL;
	after = NULL;
}

void Delete_Object(Node* pnode) {
	fprintf(stderr,"\x1b[41mDelete Object\x1b[0m\n");
	Delete_Node_Object(pnode,1);
	pnode = NULL;
	HTTP_200_JSON;
	printf("{\"m2m:dbg\": \"resource deleted succesessfully\"}");
}

void Restruct_ResourceTree(){
	Node *node_list = (Node *)calloc(1,sizeof(Node));
	Node *tail = node_list;
	
	if(access("./AE.db", 0) != -1) {
		Node* ae_list = Get_All_AE();
		tail->siblingRight = ae_list;
		ae_list->siblingLeft = tail;
		while(tail->siblingRight) tail = tail->siblingRight;
	} else {
		fprintf(stderr,"AE.db is not exist\n");
	}
	
	if(access("./CNT.db", 0) != -1) {
		Node* cnt_list = Get_All_CNT();
		tail->siblingRight = cnt_list;
		cnt_list->siblingLeft = tail;
		while(tail->siblingRight) tail = tail->siblingRight;
	} else {
		fprintf(stderr,"CNT.db is not exist\n");
	}
	
	if(access("./CIN.db", 0) != -1) {
		Node* cin_list = Get_All_CIN();
		tail->siblingRight = cin_list;
		cin_list->siblingLeft = tail;
		while(tail->siblingRight) tail = tail->siblingRight;
	} else {
		fprintf(stderr,"CIN.db is not exist\n");
	}
	
	Node *temp = node_list;
	node_list = node_list->siblingRight;
	if(node_list) node_list->siblingLeft = NULL;
	Free_Node(temp);
	
	if(node_list) Restruct_childs(rt->root, node_list);
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


// httpd open source default functions
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
