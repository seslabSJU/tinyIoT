#ifndef __ONEM2M_H__
#define __ONEM2M_H__

#include <stdbool.h>
#include "cJSON.h"

//enum
typedef enum {
	PROT_HTTP = 1,
	PROT_MQTT
}Protocol;

typedef enum {
	OP_NONE = 0,
	OP_CREATE,
	OP_RETRIEVE,
	OP_UPDATE,
	OP_DELETE,
	OP_VIEWER = 1000,
	OP_OPTIONS
}Operation;

typedef enum {
	TY_NONE = 0,
	TY_ACP,
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
	char *srv;
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
typedef struct RTNode {
	struct RTNode *parent;
	struct RTNode *child;
	struct RTNode *sibling_left;
	struct RTNode *sibling_right;
	
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
}RTNode;

typedef struct {  
	RTNode *cb;
}ResourceTree;

typedef struct {
	char *to;
	char *fr;
	char *rqi;
	char *rvi;
	char *pc;
	Operation op;
	Protocol prot;
	cJSON *cjson_pc;
	int rsc;
	int ty;
	char *origin;
	char *req_type;
}oneM2MPrimitive;

//http request
RTNode* parse_uri(oneM2MPrimitive *o2pt, RTNode *cb);
ObjectType http_parse_object_type();
ObjectType parse_object_type_cjson(cJSON *cjson);

//onem2m resource
void create_onem2m_resource(oneM2MPrimitive *o2pt, RTNode* target_rtnode);
void retrieve_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
void retrieve_object_filtercriteria(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
void update_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
void delete_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
void notify_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *node, char *response_payload, NET net);

void create_ae(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
void create_cnt(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
void create_cin(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
void create_sub(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
void create_acp(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);

void retrieve_cse(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
void retrieve_ae(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
void retrieve_cnt(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
void retrieve_cin(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
void retrieve_cin_latest(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
void retrieve_cin_by_ri(char *ri);
void retrieve_sub(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
void retrieve_acp(oneM2MPrimitive *o2pt, RTNode *target_rtnode);

void update_cse(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
void update_ae(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
void update_cnt(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
void update_sub(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
void update_acp(oneM2MPrimitive *o2pt, RTNode *target_rtnode);

void init_cse(CSE* cse);
void init_ae(AE* ae, char *pi, char *origin);
void init_cnt(CNT* cnt, char *pi);
void init_cin(CIN* cin, char *pi);
void init_sub(Sub* sub, char *pi);
void init_acp(ACP* acp, char *pi);
void set_ae_update(cJSON *m2m_ae, AE* after);
void set_cnt_update(cJSON *m2m_cnt, CNT* after);
void set_sub_update(cJSON *m2m_sub, Sub* after);
void set_acp_update(cJSON *m2m_acp, ACP* after);
void set_rtnode_update(RTNode* rtnode, void *after);

void free_cse(CSE* cse);
void free_ae(AE* ae);
void free_cnt(CNT* cnt);
void free_cin(CIN* cin);
void free_sub(Sub* sub);
void free_acp(ACP *acp);

//resource tree
RTNode* create_rtnode(void *resource, ObjectType ty);
RTNode* create_cse_rtnode(CSE *cse);
RTNode* create_ae_rtnode(AE *ae);
RTNode* create_cnt_rtnode(CNT *cnt);
RTNode* create_cin_rtnode(CIN *cin);
RTNode* create_sub_rtnode(Sub *sub);
RTNode* create_acp_rtnode(ACP *acp);
int add_child_resource_tree(RTNode *parent, RTNode *child);
RTNode *find_rtnode_by_uri(RTNode *cse, char *node_uri);
void delete_rtnode_and_db_data(RTNode *node, int flag);
void free_rtnode(RTNode *node);
void free_rtnode_list(RTNode *node);

void tree_viewer_api(RTNode *node);
void tree_viewer_data(RTNode *node, char **viewer_data, int cin_size);
void restruct_resource_tree();
RTNode* restruct_resource_tree_child(RTNode *node, RTNode *list);
RTNode* latest_cin_list(RTNode *cinList, int num); // use in viewer API
RTNode* find_latest_oldest(RTNode* node, int flag);
void set_node_uri(RTNode* node);

//json
void remove_invalid_char_json(char* json);
int is_json_valid_char(char c);
bool is_rn_valid_char(char c);

//http etc
struct url_data { size_t size; char* data;};
size_t write_data(void *ptr, size_t size, size_t nmemb, struct url_data *data);
int send_http_packet(char *target, char *post_data);

//exception
void no_mandatory_error(oneM2MPrimitive *o2pt);
void child_type_error(oneM2MPrimitive *o2pt);
int check_privilege(oneM2MPrimitive *o2pt, RTNode *target_rtnode, ACOP acop);
int check_payload_empty(oneM2MPrimitive *o2pt);
int check_rn_duplicate(oneM2MPrimitive *o2pt, RTNode *rtnode);
int check_aei_duplicate(oneM2MPrimitive *o2pt, RTNode *rtnode);
int check_resource_type_equal(oneM2MPrimitive *o2pt);
int check_resource_type_invalid(oneM2MPrimitive *o2pt);
int result_parse_uri(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int check_payload_size(oneM2MPrimitive *o2pt);
int check_payload_format(oneM2MPrimitive *o2pt);
int check_rn_invalid(oneM2MPrimitive *o2pt, ObjectType ty);
void api_prefix_invalid(oneM2MPrimitive *o2pt);

//etc
void init_server();
char* get_local_time(int diff);
char* resource_identifier(ObjectType ty, char *ct);
void cin_in_period(RTNode *pnode);
void object_test_api(RTNode *node);
char* json_label_value(char *json_payload);
int net_to_bit(char *net);
int get_acop(RTNode *node);
int get_acop_origin(char *origin, RTNode *acp, int flag);
int get_value_querystring_int(char *key);
void log_runtime(double start);
void set_o2pt_pc(oneM2MPrimitive *o2pt, char *pc);
void set_o2pt_rsc(oneM2MPrimitive *o2pt, int rsc);
void handle_http_request();
void respond_to_client(oneM2MPrimitive *o2pt, int status);

#define MAX_TREE_VIEWER_SIZE 65536
#define EXPIRE_TIME -3600*24*365*2
#define ALL_ACOP ACOP_CREATE + ACOP_RETRIEVE + ACOP_UPDATE + ACOP_DELETE + ACOP_NOTIFY + ACOP_DISCOVERY

#endif