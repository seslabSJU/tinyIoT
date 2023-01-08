#include <stdio.h>
#include <stdbool.h>
#include <db.h>
#include "cJSON.h"
#include "httpd.h"
#include "zeroconf.h"

//enum
typedef enum {
	OP_NONE = 0,
	OP_CREATE,
	OP_RETRIEVE,
	OP_UPDATE,
	OP_DELETE,
	OP_OPTIONS,
	OP_VIEWER,
	OP_TEST,
	OP_LATEST,
	OP_OLDEST
}Operation;

typedef enum {
	TY_ACP = 1,
	TY_AE,
	TY_CNT,
	TY_CIN,
	TY_CSE,
	TY_SUB = 23
}ObjectType;

typedef enum {
	NOTIFICATION_EVENT_1 = 1,
	NOTIFICATION_EVENT_2 = 2,
	NOTIFICATION_EVENT_3 = 4,
	NOTIFICATION_EVENT_4 = 8
}NET;

typedef enum {
	ACOP_CREATE = 1,
	ACOP_RETRIEVE = 2,
	ACOP_UPDATE = 4,
	ACOP_DELETE = 8,
	ACOP_NOTIFY = 16,
	ACOP_DISCOVERY = 32
}ACOP;

//oneM2M Resource
typedef struct {
	char *ct;
	char *lt;
	char *rn;
	char *ri;
	char *pi;
	char *csi;
	int ty;
} CSE;

typedef struct {
	char *et;
	char *ct;
	char *lt;
	char *rn;
	char *ri;
	char *pi;
	char *api;
	char *aei;
	char *lbl;
	int ty;
	bool rr;
} AE;

typedef struct {
	char *et;
	char *ct;
	char *lt;
	char *rn;
	char *ri;
	char *pi;
	char *lbl;
	char *acpi;
	int ty;
	int st;
	int cni;
	int cbs;
} CNT;

typedef struct {
	char *et;
	char *ct;
	char *lt;
	char *rn;
	char *ri;
	char *pi;
	char *con;	
	int ty;
	int st;
	int cs;
} CIN;

typedef struct {
	char *et;
	char *ct;
	char *lt;
	char *rn;
	char *ri;
	char *pi;
	char *nu;
	char *net;
	char *sur;
	int ty;
	int nct;
} Sub;

typedef struct {
	char *rn;
	char *pi;
	char *ri;
	char *ct;
	char *lt;
	char *et;
	char *pv_acor;
	char *pv_acop;
	char *pvs_acor;
	char *pvs_acop;
	int ty;
} ACP;

//Resource Tree
typedef struct Node {
	struct Node *parent;
	struct Node *child;
	struct Node *sibling_left;
	struct Node *sibling_right;
	
	char *rn;
	char *ri;
	char *pi;
	char *nu;
	char *sur;
	char *acpi;
	char *pv_acor;
	char *pv_acop;
	char *pvs_acor;
	char *pvs_acop;
	char *uri;
	ObjectType ty;

	int net;
}Node;

typedef struct {  
	Node *cb;
}ResourceTree;

//http request
Node* parse_uri(Node *cb, char *uri_array, Operation *op);
Operation parse_operation();
ObjectType parse_object_type();
ObjectType parse_object_type_in_request_body();
void normalization_payload();

//onem2m resource
void create_object(Node* pnode);
void retrieve_object(Node *pnode);
void retrieve_object_filtercriteria(Node *pnode);
void update_object(Node *pnode);
void delete_object(Node *pnode);
void notify_object(Node *node, char *res_json, NET net);

void create_ae(Node *pnode);
void create_cnt(Node *pnode);
void create_cin(Node *pnode);
void create_sub(Node *pnode);
void create_acp(Node *pnode);

void retrieve_cse(Node *pnode);
void retrieve_ae(Node *pnode);
void retrieve_cnt(Node *pnode);
void retrieve_cin(Node *pnode);
void retrieve_cin_latest(Node *pnode);
void retrieve_cin_by_ri(char *ri);
void retrieve_sub(Node *pnode);
void retrieve_acp(Node *pnode);

void update_cse(Node *pnode);
void update_ae(Node *pnode);
void update_cnt(Node *pnode);
void update_sub(Node *pnode);
void update_acp(Node *pnode);

void init_cse(CSE* cse);
void init_ae(AE* ae, char *pi);
void init_cnt(CNT* cnt, char *pi);
void init_cin(CIN* cin, char *pi);
void init_sub(Sub* sub, char *pi);
void init_acp(ACP* acp, char *pi);
void set_ae_update(AE* after);
void set_cnt_update(CNT* after);
void set_sub_update(Sub* after);
void set_acp_update(ACP* after);
void set_node_update(Node* node, void *after);

void free_cse(CSE* cse);
void free_ae(AE* ae);
void free_cnt(CNT* cnt);
void free_cin(CIN* cin);
void free_sub(Sub* sub);
void free_acp(ACP *acp);

//database
int db_display(char* database);

int db_store_cse(CSE* cse_object);
int db_store_ae(AE* ae_object);
int db_store_cnt(CNT* cnt_object);
int db_store_cin(CIN* cin_object);
int db_store_sub(Sub *sub_object);
int db_store_acp(ACP *acp_object);

CSE* db_get_cse();
AE* db_get_ae(char *ri);
CNT* db_get_cnt(char *ri);
CIN* db_get_cin(char *ri);
Sub* db_get_sub(char* ri);
ACP* db_get_acp(char* ri);

int db_delete_object(char *ri);
int db_delete_sub(char* ri);
int db_delete_acp(char* ri);

Node* db_get_all_cse();
Node* db_get_all_ae();
Node* db_get_all_cnt();
Node* db_get_all_cin();
Node* db_get_all_sub();
Node* db_get_all_acp();

Node* db_get_cin_list_by_pi(char* pi);

//resource tree
Node* create_node(void *obj, ObjectType ty);
Node* create_cse_node(CSE *cse);
Node* create_ae_node(AE *ae);
Node* create_cnt_node(CNT *cnt);
Node* create_cin_node(CIN *cin);
Node* create_sub_node(Sub *sub);
Node* create_acp_node(ACP *acp);
int add_child_resource_tree(Node *parent, Node *child);
char* node_to_json(Node *node);
Node *find_node_by_uri(Node *cse, char *node_uri);
void delete_node_and_db_data(Node *node, int flag);
void free_node(Node *node);
void free_node_list(Node *node);

void tree_viewer_api(Node *node);
void tree_viewer_data(Node *node, char **viewer_data, int cin_size);
void restruct_resource_tree();
Node* restruct_resource_tree_child(Node *node, Node *list);
Node* latest_cin_list(Node *cinList, int num); // use in viewer API
Node *find_latest_oldest(Node* node, Operation *op);
int find_same_resource_name(Node *pnode);

//json parser
CSE* json_to_cse(char *json_payload);
AE* json_to_ae(char *json_payload);
CNT* json_to_cnt(char *json_payload);
CIN* json_to_cin(char *json_payload);
Sub* json_to_sub(char *json_payload);
ACP* json_to_acp(char *json_payload);

char* cse_to_json(CSE* cse_object);
char* ae_to_json(AE* ae_object);
char* cnt_to_json(CNT* cnt_object);
char* cin_to_json(CIN* cin_object);
char* sub_to_json(Sub *sub_object);
char* notification_to_json(char *sur, int net, char *rep);
char* acp_to_json(ACP *acp_object);
char* discovery_to_json(char **result, int size);

char* get_json_value_char(char *key, char *json);
int get_json_value_int(char *key, char *json);
int get_json_value_bool(char *key, char *json);
char *get_json_value_list(char *key, char *json);
void remove_invalid_char_json(char* json);
int is_json_valid_char(char c);

//http etc
struct url_data { size_t size; char* data;};
size_t write_data(void *ptr, size_t size, size_t nmemb, struct url_data *data);
int send_http_packet(char *target, char *post_data);

//exception
void no_mandatory_error();
void parent_type_error();
int check_privilege(Node *node, ACOP acop);
int check_request_body_empty();
int check_resource_name_duplicate(Node *node);
int check_resource_type_equal(ObjectType ty1, ObjectType ty2);
int result_parse_uri(Node *node);
int check_payload_size();
int check_json_format();

//etc
void init();
char* get_local_time(int diff);
char* resource_identifer(ObjectType ty, char *ct);
void cin_in_period(Node *pnode);
void object_test_api(Node *node);
char* json_label_value(char *json_payload);
int net_to_bit(char *net);
int get_acop(Node *node);
int get_acop_origin(char *origin, Node *acp, int flag);
int get_value_querystring_int(char *key);
void set_node_uri(Node* node);

#define MAX_TREE_VIEWER_SIZE 65536
#define MAX_PROPERTY_SIZE 16384
#define MAX_URI_SIZE 1024
#define EXPIRE_TIME -3600*24*365*2
#define ALL_ACOP ACOP_CREATE + ACOP_RETRIEVE + ACOP_UPDATE + ACOP_DELETE + ACOP_NOTIFY + ACOP_DISCOVERY
#define MAX_PAYLOAD_SIZE 16384