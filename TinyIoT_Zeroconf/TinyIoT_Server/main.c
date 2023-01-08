#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include "onem2m.h"

ResourceTree *rt;

extern char *response_headers;
char *response_json;

int main(int c, char **v) {
	init();
 	char *port = c == 1 ? "3000" : v[1];

	serve_forever(port); // main oneM2M operation logic in void route()    

	return 0;
}

void route() {
    double start, end;

    start = (double)clock() / CLOCKS_PER_SEC; // runtime check - start

	Operation op = OP_NONE;

	memset(response_headers, 0, sizeof(response_headers));

	char *header_value;
	if((header_value = request_header("X-M2M-RI"))) {
		set_response_header("X-M2M-RI", header_value);
	}
	if(header_value = request_header("X-M2M-RVI")) {
		set_response_header("X-M2M-RVI", header_value);
	} 

	Node* pnode = parse_uri(rt->cb, uri, &op); // return tree node by URI
	int e = result_parse_uri(pnode);

	if(e != -1) e = check_payload_size();
	if(e == -1) return;

	if(op == OP_NONE) op = parse_operation(); // parse operation by HTTP method

	switch(op) {
	
	case OP_CREATE:	
		create_object(pnode); break;
	
	case OP_RETRIEVE:
		retrieve_object(pnode);	break;
		
	case OP_UPDATE: 
		update_object(pnode); break;
		
	case OP_DELETE:
		delete_object(pnode); break;

	case OP_VIEWER:
		tree_viewer_api(pnode); break;
	
	case OP_OPTIONS:
		respond_to_client(200, "{\"m2m:dbg\": \"response about options method\"}");
		break;
	
	default:
		respond_to_client(500, "{\"m2m:dbg\": \"internal server error\"}");
	}
	if(pnode->ty == TY_CIN) free_node(pnode);

	end = (((double)clock()) / CLOCKS_PER_SEC); // runtime check - end
    fprintf(stderr,"Run time :%lf\n", (end-start));
}

void init() {
	response_headers = (char *)calloc(4096,sizeof(char));

	rt = (ResourceTree *)malloc(sizeof(rt));

	if(access("./RESOURCE.db", 0) == -1) {
		CSE* cse = (CSE*)malloc(sizeof(CSE));
		init_cse(cse);
		db_store_cse(cse);
		rt->cb = create_node(cse, TY_CSE);
		free_cse(cse); cse = NULL;
	} else {
		rt->cb = db_get_all_cse();
	}

 	restruct_resource_tree();
}

void create_object(Node *pnode) {
	ObjectType ty = parse_object_type(); 
	
	int e = check_privilege(pnode, ACOP_CREATE);
	if(e != -1) e = check_request_body_empty();
	if(e != -1) e = check_json_format();
	if(e != -1) e = check_resource_name_duplicate(pnode);
	//if(e != -1) e = check_resource_type_equal(ty, parse_object_type_in_request_body());
	if(e == -1) return;

	set_response_header("X-M2M-RSC", "2001");

	switch(ty) {	
	case TY_AE :
		fprintf(stderr,"\x1b[42mCreate AE\x1b[0m\n");
		create_ae(pnode);
		break;	
					
	case TY_CNT :
		fprintf(stderr,"\x1b[42mCreate CNT\x1b[0m\n");
		create_cnt(pnode);
		break;
			
	case TY_CIN :
		fprintf(stderr,"\x1b[42mCreate CIN\x1b[0m\n");
		create_cin(pnode);
		break;

	case TY_SUB :
		fprintf(stderr,"\x1b[42mCreate Sub\x1b[0m\n");
		create_sub(pnode);
		break;
	
	case TY_ACP :
		fprintf(stderr,"\x1b[42mCreate ACP\x1b[0m\n");
		create_acp(pnode);
		break;

	//Modify
	case TY_CSE :
		 if(request_header("X-fc") && !strcmp(request_header("X-fc"), "Create") && request_header("X-TinyIoT_ID") && !strcmp(request_header("X-TinyIoT_ID"), TINYIOT_ID)) {
            fprintf(stderr,"\x1b[43mDevice Registered by Zero-conf\x1b[0m\n");

            //add making AE, CNT
			AE *ae = json_to_ae(payload);
			CNT *cnt = json_to_cnt(payload);
			init_ae(ae, pnode->ri); init_cnt(cnt, ae->ri);
			db_store_ae(ae); db_store_cnt(cnt);
			Node *ae_node = create_node(ae, TY_AE);
			Node *cnt_node = create_node(cnt, TY_CNT);
			add_child_resource_tree(pnode, ae_node);
			add_child_resource_tree(ae_node, cnt_node);
			
            fprintf(stderr,"Regist Success\n"); 
            //Request Device
            Request_Device(ae->api, ae->rn, cnt->rn);

			free_ae(ae); free_cnt(cnt);
            respond_to_client(200, "{\"m2m:dbg\": \"device registered successfully\"}");

         }
         else{
            fprintf(stderr,"Not TinyIoT App\n");
            HTTP_400;
            printf("{\"m2m:dbg\": \"Not TinyIoT App\"}");
         }
         break;

	default :
		fprintf(stderr,"Resource type error (Content-Type Header Invalid)\n");
		respond_to_client(400, "{\"m2m:dbg\": \"resource type error (Content-Type header invalid)\"}");
	}	
}

void retrieve_object(Node *pnode) {
	int e = check_privilege(pnode, ACOP_RETRIEVE);

	if(e == -1) return;

	set_response_header("X-M2M-RSC", "2000");

	int fu = get_value_querystring_int("fu");

	if(fu == 1) {
		fprintf(stderr,"\x1b[43mRetrieve FilterCriteria\x1b[0m\n");
		retrieve_object_filtercriteria(pnode);
		return;
	}

	switch(pnode->ty) {
		
	case TY_CSE :
		//fc: ZeroconfDiscovery
    	if(request_header("X-fc") && !strcmp(request_header("X-fc"), "Zeroconf")) {
        	fprintf(stderr,"\x1b[43mRetrieve CSE Zero-conf\x1b[0m\n");
        	char* zeroconf_data = Zeroconf_Discovery();
			char Request_Message[MAX_PAYLOAD_SIZE]="Access-Control-Allow-Origin: *\nAccess-Control-Allow-Headers: Accept, Accept-Language, Content-Language, Content-Type\nAccess-Control-Allow-Methods: GET, PUT, POST, DELETE, OPTIONS\nAccess-Control-Request-Methods: GET, PUT, POST, DELETE, OPTIONS\nX-TinyIoT-ID:";
			strcat(Request_Message,TINYIOT_ID);
        	printf("%s 200 OK\n%s\n\n", RESPONSE_PROTOCOL, Request_Message);
        	printf("%s",zeroconf_data);
      	}
		else{
			fprintf(stderr,"\x1b[43mRetrieve CSE\x1b[0m\n");
        	retrieve_cse(pnode);
		}
      	break;

	case TY_AE : 
		fprintf(stderr,"\x1b[43mRetrieve AE\x1b[0m\n");
		retrieve_ae(pnode);	
		break;	
			
	case TY_CNT :
		fprintf(stderr,"\x1b[43mRetrieve CNT\x1b[0m\n");
		retrieve_cnt(pnode);			
		break;
			
	case TY_CIN :
		fprintf(stderr,"\x1b[43mRetrieve CIN\x1b[0m\n");
		retrieve_cin(pnode);			
		break;

	case TY_SUB :
		fprintf(stderr,"\x1b[43mRetrieve Sub\x1b[0m\n");
		retrieve_sub(pnode);			
		break;

	case TY_ACP :
		fprintf(stderr,"\x1b[43mRetrieve ACP\x1b[0m\n");
		retrieve_acp(pnode);			
		break;
	}	
}

void update_object(Node *pnode) {
	ObjectType ty = parse_object_type_in_request_body(); 

	int e = check_privilege(pnode, ACOP_UPDATE);
	if(e != -1) e = check_request_body_empty();
	if(e != -1) e = check_json_format();
	if(e != -1) e = check_resource_name_duplicate(pnode->parent);
	if(e != -1) e = check_resource_type_equal(ty, pnode->ty);
	if(e == -1) return;

	set_response_header("X-M2M-RSC", "2004");
	
	switch(ty) {
	case TY_AE :
		fprintf(stderr,"\x1b[45mUpdate AE\x1b[0m\n");
		update_ae(pnode);
		break;

	case TY_CNT :
		fprintf(stderr,"\x1b[45mUpdate CNT\x1b[0m\n");
		update_cnt(pnode);
		break;

	case TY_SUB :
		fprintf(stderr,"\x1b[45mUpdate Sub\x1b[0m\n");
		update_sub(pnode);
		break;
	
	case TY_ACP :
		fprintf(stderr,"\x1b[45mUpdate ACP\x1b[0m\n");
		update_acp(pnode);
		break;

	default :
		fprintf(stderr,"Resource type do not support PUT method\n");
		respond_to_client(400, "{\"m2m:dbg\": \"not support PUT method about resource type\"}");
	}
}

void create_ae(Node *pnode) {
	if(pnode->ty != TY_CSE) {
		parent_type_error();
		return;
	}
	AE* ae = json_to_ae(payload);
	if(!ae) {
		no_mandatory_error();
		return;
	}
	init_ae(ae,pnode->ri);
	
	int result = db_store_ae(ae);
	if(result != 1) { 
		respond_to_client(500, "{\"m2m:dbg\": \"DB store fail\"}");
		free_ae(ae); ae = NULL;
		return;
	}
	
	Node* node = create_node(ae, TY_AE);
	add_child_resource_tree(pnode,node);
	response_json = ae_to_json(ae);

	respond_to_client(201, NULL);
	notify_object(pnode->child, response_json, NOTIFICATION_EVENT_3);
	free(response_json); response_json = NULL;
	free_ae(ae); ae = NULL;
}

void create_cnt(Node *pnode) {
	if(pnode->ty != TY_CNT && pnode->ty != TY_AE) {
		parent_type_error();
		return;
	}
	CNT* cnt = json_to_cnt(payload);
	if(!cnt) {
		no_mandatory_error();
		return;
	}
	init_cnt(cnt,pnode->ri);

	int result = db_store_cnt(cnt);
	if(result != 1) { 
		respond_to_client(500, "{\"m2m:dbg\": \"DB store fail\"}");
		free_cnt(cnt); cnt = NULL;
		return;
	}
	if(!strcmp(cnt->acpi, " ")) cnt->acpi = NULL;
	
	Node* node = create_node(cnt, TY_CNT);
	add_child_resource_tree(pnode,node);

	response_json = cnt_to_json(cnt);
	respond_to_client(201, NULL);
	notify_object(pnode->child, response_json, NOTIFICATION_EVENT_3);
	free(response_json); response_json = NULL; 
	free_cnt(cnt); cnt = NULL;
}

void create_cin(Node *pnode) {
	if(pnode->ty != TY_CNT) {
		parent_type_error();
		return;
	}
	CIN* cin = json_to_cin(payload);
	if(!cin) {
		no_mandatory_error();
		return;
	}
	init_cin(cin,pnode->ri);

	int result = db_store_cin(cin);
	if(result != 1) { 
		respond_to_client(500, "{\"m2m:dbg\": \"DB store fail\"}");
		free_cin(cin);
		cin = NULL;
		return;
	}
	
	response_json = cin_to_json(cin);
	respond_to_client(201, NULL);
	notify_object(pnode->child, response_json, NOTIFICATION_EVENT_3);
	free(response_json); response_json = NULL; 
	free_cin(cin); cin = NULL;
}

void create_sub(Node *pnode) {
	if(pnode->ty == TY_CIN || pnode->ty == TY_SUB) {
		parent_type_error();
		return;
	}
	Sub* sub = json_to_sub(payload);
	if(!sub) {
		no_mandatory_error();
		return;
	}
	init_sub(sub, pnode->ri);
	
	int result = db_store_sub(sub);
	if(result != 1) { 
		respond_to_client(500, "{\"m2m:dbg\": \"DB store fail\"}");
		free_sub(sub); sub = NULL;
		return;
	}
	
	Node* node = create_node(sub, TY_SUB);
	add_child_resource_tree(pnode,node);
	
	response_json = sub_to_json(sub);
	respond_to_client(201, NULL);
	result = send_http_packet(sub->nu, response_json);
	notify_object(pnode->child, response_json, NOTIFICATION_EVENT_3);
	free(response_json); response_json = NULL;
	free_sub(sub); sub = NULL;
}

void create_acp(Node *pnode) {
	if(pnode->ty != TY_CSE && pnode->ty != TY_AE) {
		parent_type_error();
		return;
	}
	ACP* acp = json_to_acp(payload);
	if(!acp) {
		no_mandatory_error();
		return;
	}
	init_acp(acp, pnode->ri);
	
	int result = db_store_acp(acp);
	if(result != 1) { 
		respond_to_client(500, "{\"m2m:dbg\": \"DB store fail\"}");
		free_acp(acp); acp = NULL;
		return;
	}
	
	Node* node = create_node(acp, TY_ACP);
	add_child_resource_tree(pnode,node);
	
	response_json = acp_to_json(acp);
	respond_to_client(201, NULL);
	notify_object(pnode->child, response_json, NOTIFICATION_EVENT_3);
	free(response_json); response_json = NULL; 
	free_acp(acp); acp = NULL;
}

void retrieve_cse(Node *pnode){
	CSE* gcse = db_get_cse(pnode->ri);
	response_json = cse_to_json(gcse);
	respond_to_client(200, NULL);
	free(response_json); response_json = NULL; 
	free_cse(gcse); gcse = NULL;
}

void retrieve_ae(Node *pnode){
	AE* gae = db_get_ae(pnode->ri);
	response_json = ae_to_json(gae);
	respond_to_client(200, NULL);
	free(response_json); response_json = NULL;
	free_ae(gae); gae = NULL;
}

void retrieve_cnt(Node *pnode){
	CNT* gcnt = db_get_cnt(pnode->ri);
	response_json = cnt_to_json(gcnt);
	respond_to_client(200, NULL);
	free(response_json); response_json = NULL;
	free_cnt(gcnt); gcnt = NULL;
}

void retrieve_cin(Node *pnode){
	CIN* gcin = db_get_cin(pnode->ri);
	response_json = cin_to_json(gcin);
	respond_to_client(200, NULL);
	free(response_json); response_json = NULL; 
	free_cin(gcin); gcin = NULL;
}

void retrieve_sub(Node *pnode){
	Sub* gsub = db_get_sub(pnode->ri);
	response_json = sub_to_json(gsub);
	respond_to_client(200, NULL);
	free(response_json); response_json = NULL; 
	free_sub(gsub); gsub = NULL;
}

void retrieve_acp(Node *pnode){
	ACP* gacp = db_get_acp(pnode->ri);
	response_json = acp_to_json(gacp);
	respond_to_client(200, NULL);
	free(response_json); response_json = NULL; 
	free_acp(gacp); gacp = NULL;
}

void update_ae(Node *pnode) {
	AE* after = db_get_ae(pnode->ri);
	int result;

	set_ae_update(after);
	set_node_update(pnode, after);
	result = db_delete_object(after->ri);
	result = db_store_ae(after);
	
	response_json = ae_to_json(after);
	respond_to_client(200, NULL);
	notify_object(pnode->child, response_json, NOTIFICATION_EVENT_1);
	free(response_json); response_json = NULL; 
	free_ae(after); after = NULL;
}

void update_cnt(Node *pnode) {
	CNT* after = db_get_cnt(pnode->ri);
	int result;

	set_cnt_update(after);
	set_node_update(pnode, after);
	result = db_delete_object(after->ri);
	result = db_store_cnt(after);
	
	response_json = cnt_to_json(after);
	respond_to_client(200, NULL);
	notify_object(pnode->child, response_json, NOTIFICATION_EVENT_1);
	free(response_json); response_json = NULL;
	free_cnt(after); after = NULL;
}

void update_sub(Node *pnode) {
	Sub* after = db_get_sub(pnode->ri);
	int result;
	
	set_sub_update(after);
	set_node_update(pnode, after);
	result = db_delete_sub(after->ri);
	result = db_store_sub(after);
	
	response_json = sub_to_json(after);
	respond_to_client(200, NULL);
	free(response_json); response_json = NULL;
	free_sub(after); after = NULL;
}

void update_acp(Node *pnode) {
	ACP* after = db_get_acp(pnode->ri);
	int result;
	
	set_acp_update(after);
	set_node_update(pnode, after);
	result = db_delete_acp(after->ri);
	result = db_store_acp(after);
	
	response_json = acp_to_json(after);
	respond_to_client(200, NULL);
	notify_object(pnode->child, response_json, NOTIFICATION_EVENT_1);
	free(response_json); response_json = NULL;
	free_acp(after); after = NULL;
}

void delete_object(Node* pnode) {
	fprintf(stderr,"\x1b[41mDelete Object\x1b[0m\n");
	if(pnode->ty == TY_CSE) {
		respond_to_client(403, "{\"m2m:dbg\": \"CSE can not be deleted\"}");
		return;
	}
	strcat(response_headers,"\nX-M2M-RSC: 2002");
	delete_node_and_db_data(pnode,1);
	pnode = NULL;
	respond_to_client(200, "{\"m2m:dbg\": \"resource is deleted successfully\"}");
}

void restruct_resource_tree(){
	Node *node_list = (Node *)calloc(1,sizeof(Node));
	Node *tail = node_list;
	
	if(access("./RESOURCE.db", 0) != -1) {
		Node* ae_list = db_get_all_ae();
		tail->sibling_right = ae_list;
		if(ae_list) ae_list->sibling_left = tail;
		while(tail->sibling_right) tail = tail->sibling_right;

		Node* cnt_list = db_get_all_cnt();
		tail->sibling_right = cnt_list;
		if(cnt_list) cnt_list->sibling_left = tail;
		while(tail->sibling_right) tail = tail->sibling_right;
	} else {
		fprintf(stderr,"RESOURCE.db is not exist\n");
	}
	
	if(access("./SUB.db", 0) != -1) {
		Node* sub_list = db_get_all_sub();
		tail->sibling_right = sub_list;
		if(sub_list) sub_list->sibling_left = tail;
		while(tail->sibling_right) tail = tail->sibling_right;
	} else {
		fprintf(stderr,"SUB.db is not exist\n");
	}

	if(access("./ACP.db", 0) != -1) {
		Node* acp_list = db_get_all_acp();
		tail->sibling_right = acp_list;
		if(acp_list) acp_list->sibling_left = tail;
		while(tail->sibling_right) tail = tail->sibling_right;
	} else {
		fprintf(stderr,"ACP.db is not exist\n");
	}
	
	Node *fnode = node_list;
	node_list = node_list->sibling_right;
	if(node_list) node_list->sibling_left = NULL;
	free_node(fnode);
	
	if(node_list) restruct_resource_tree_child(rt->cb, node_list);
}

Node* restruct_resource_tree_child(Node *pnode, Node *list) {
	Node *node = list;
	
	while(node) {
		Node *right = node->sibling_right;

		if(!strcmp(pnode->ri, node->pi)) {
			Node *left = node->sibling_left;
			
			if(!left) {
				list = right;
			} else {
				left->sibling_right = right;
			}
			
			if(right) right->sibling_left = left;
			node->sibling_left = node->sibling_right = NULL;
			add_child_resource_tree(pnode, node);
		}
		node = right;
	}
	Node *child = pnode->child;
	
	while(child) {
		list = restruct_resource_tree_child(child, list);
		child = child->sibling_right;
	}
	
	return list;
}

void no_mandatory_error(){
	fprintf(stderr,"No Mandatory Error\n");
	respond_to_client(400, "{\"m2m:dbg\": \"insufficient mandatory attribute\"}");
}

void parent_type_error(){
	fprintf(stderr,"Parent Type Error\n");
	respond_to_client(403, "{\"m2m:dbg\": \"resource can not be created under type of parent\"}");
}

int check_json_format() {
	cJSON *json = cJSON_Parse(payload);
	if(json == NULL) {
		fprintf(stderr,"Body Format Invalid\n");
		respond_to_client(400,"{\"m2m:dbg\": \"body format invalid\"}");
		cJSON_Delete(json);
		return -1;
	}
	cJSON_Delete(json);
	return 0;
}

int check_privilege(Node *node, ACOP acop) {
	if(node->ty == TY_CIN) node = node->parent;
	if(node->ty != TY_CNT && node->ty != TY_ACP) return 0;

	if((get_acop(node) & acop) != acop) {
		fprintf(stderr,"X-M2M-Origin has no privilege\n");
		respond_to_client(403, "{\"m2m:dbg\": \"access denied\"}");
		return -1;
	}
	return 0;
}

int check_request_body_empty() {
	if(!payload) {
		fprintf(stderr,"Request body empty error\n");
		respond_to_client(500, "{\"m2m:dbg\": \"request body empty\"\n}");
		return -1;
	}
	return 0;
}

int check_resource_name_duplicate(Node *node) {
	if(!node) return 0;

	if(find_same_resource_name(node)) {
		fprintf(stderr,"Resource name duplicate error\n");
		respond_to_client(209, "{\"m2m:dbg\": \"attribute `rn` is duplicated\"}");
		return -1;
	}
	return 0;
}

int check_resource_type_equal(ObjectType ty1, ObjectType ty2) {	
	if(ty1 != ty2) {
		fprintf(stderr,"Resource type error\n");
		respond_to_client(400, "{\"m2m:dbg\": \"resource type error\"}");
		return -1;
	}
	return 0;
}

int result_parse_uri(Node *node) {
	if(!node) {
		fprintf(stderr,"Invalid\n");
		respond_to_client(404, "{\"m2m:dbg\": \"URI is invalid\"}");
		return -1;
	} else {
		fprintf(stderr,"OK\n");
		return 0;
	} 
}

int check_payload_size() {
	if(payload && payload_size > MAX_PAYLOAD_SIZE) {
		fprintf(stderr,"Request payload too large\n");
		respond_to_client(413, "{\"m2m:dbg\": \"payload is too large\"}");
		return -1;
	}
	return 0;
}

void retrieve_filtercriteria_data(Node *node, ObjectType ty, char **discovery_list, int *size, int level, int curr, int flag) {
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
		node = node->sibling_right;
	}

	if((ty == -1 || ty == TY_CIN) && curr < level) {
		for(int i= 0; i <= index; i++) {
			Node *cin_list_head = db_get_cin_list_by_pi(sibling[i]->ri);
			Node *p = cin_list_head;

			while(p) {
				discovery_list[*size] = (char*)malloc(MAX_URI_SIZE*sizeof(char));
				strcpy(discovery_list[*size], sibling[i]->uri);
				strcat(discovery_list[*size], "/");
				strcat(discovery_list[*size], p->ri);
				(*size)++;
				p = p->sibling_right;
			}

			free_node_list(cin_list_head);
		}
	}

	for(int i = 0; i<=index; i++) {
		retrieve_filtercriteria_data(child[i], ty, discovery_list, size, level, curr+1, 0);
	} 
}

void retrieve_object_filtercriteria(Node *pnode) {
	char **discovery_list = (char**)malloc(65536*sizeof(char*));
	int size = 0;
	ObjectType ty = get_value_querystring_int("ty");
	int level = get_value_querystring_int("lvl");
	if(level == -1) level = INT32_MAX;

	retrieve_filtercriteria_data(pnode, ty, discovery_list, &size, level, 0, 1);

	response_json = discovery_to_json(discovery_list, size);
	respond_to_client(200, NULL);
	for(int i=0; i<size; i++) free(discovery_list[i]);
	free(discovery_list);
	free(response_json); response_json = NULL;

	return;
}