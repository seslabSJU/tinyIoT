#ifndef __ONEM2M_H__
#define __ONEM2M_H__

#include <stdbool.h>
#include "cJSON.h"
#include "onem2mTypes.h"

//enum
typedef enum {
	PROT_HTTP = 1,
	PROT_MQTT,
	PROT_COAP = 3
}Protocol;

typedef enum {
	OP_NONE = 0,
	OP_CREATE = 1,
	OP_RETRIEVE = 2,
	OP_UPDATE = 4,
	OP_DELETE = 8,
	OP_NOTIFY = 16,
	OP_DISCOVERY = 32,
	OP_VIEWER = 1000,
	OP_OPTIONS,
	OP_FORWARDING
}Operation;

typedef enum {
	ACOP_CREATE = 1,
	ACOP_RETRIEVE = 2,
	ACOP_UPDATE = 4,
	ACOP_DELETE = 8,
	ACOP_NOTIFY = 16,
	ACOP_DISCOVERY = 32
}ACOP;

typedef enum {
	CS_PARTIAL_CONTENT = 1,
	CS_FULL_CONTENT = 2
} ContentStatus;

//oneM2M Resource
typedef struct {
	char *ct;
	char *lt;
	char *rn;
	char *ri;
	char *pi;
	char *csi;
	char *srt;
	char *srv;

	ResourceType ty;
	char *uri;
} CSE;



//Resource Tree
typedef struct RTNode {
	struct RTNode *parent;
	struct RTNode *child;
	struct NodeList *subs;
	struct RTNode *sibling_left;
	struct RTNode *sibling_right;
	char *rn;
	char *uri;
	int sub_cnt;
	ResourceType ty;
	cJSON *obj;
} RTNode;

typedef struct NodeList {
	struct NodeList *next;
	RTNode *rtnode;
	char *uri;
} NodeList;

//Reachablity Resource
typedef struct RRNode {
	struct RRNode *next;
	char *uri;
	struct RTNode *rtnode;
} RRNode;
typedef struct {  
	RTNode *cb;
	RTNode *registrar_csr;
	NodeList *csr_list;
	RRNode *rr_list;
}ResourceTree;

typedef enum {
    FU_DISCOVERY                    = 1,
    FU_CONDITIONAL_OPERATION       = 2, // DEFAULT
    FU_IPE_ON_DEMAND_DISCOVERY     = 3,
    FU_DISCOVERY_BASED_OPERATION   = 4
} FilterUsage;


typedef enum{
    FO_AND = 1,    // DEFAULT
    FO_OR  = 2,
    FO_XOR = 3
} FilterOperation;

typedef struct _o{
	char *to;
	char *fr;
	char *rqi;
	char *rvi;
	char *pc;
	Operation op;
	cJSON *cjson_pc;
	int rcn;
	int rsc;
	int ty;
	char *origin;
	bool isFopt;
	bool isForwarding;
	char *fopt;
	bool errFlag;
	char *ip;
	ContentStatus cnst;
	int cnot;
	cJSON *fc;
	int drt;
}oneM2MPrimitive;

typedef struct _n{
	Protocol prot;
	char host[1024];
	int port;
	char target[256];
	char *noti_json;
} NotiTarget;

//onem2m resource
int create_onem2m_resource(oneM2MPrimitive *o2pt, RTNode* target_rtnode);
int handle_onem2m_request(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int retrieve_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int retrieve_object_filtercriteria(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int delete_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int notify_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int fopt_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
int discover_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int forwarding_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode);

int create_ae(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
int create_cnt(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
int create_cin(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
int create_sub(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
int create_acp(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
int create_grp(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
int create_csr(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
int create_cba(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
int create_aea(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);

int update_cse(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_ae(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_cnt(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_sub(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_acp(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_grp(oneM2MPrimitive *o2pt, RTNode *target_rtnode);

void init_cse(cJSON* cse);
void init_csr(cJSON *csr);

//resource tree
RTNode* create_rtnode(cJSON *resource, ResourceType ty);
int delete_process(oneM2MPrimitive *o2pt, RTNode *rtnode);
void free_rtnode(RTNode *rtnode);
void free_rtnode_list(RTNode *rtnode);

RTNode* restruct_resource_tree(RTNode *node, RTNode *list);
RTNode* latest_cin_list(RTNode *cinList, int num); // use in viewer API
RTNode* find_latest_oldest(RTNode* node, int flag);
bool isResourceAptFC(oneM2MPrimitive* o2pt, RTNode *rtnode, cJSON *fc);


//etc
int update_cnt_cin(RTNode *cnt_rtnode, RTNode *cin_rtnode, int sign);


// filter criteria
int validate_filter_criteria(oneM2MPrimitive *o2pt);
char *fc_to_qs(cJSON *fc);
bool FC_isAptCrb(char *fcCrb, RTNode *rtnode);
bool FC_isAptCra(char *fcCra, RTNode *rtnode);
bool FC_isAptMs(char *fcMs, RTNode *rtnode);
bool FC_isAptUs(char *fcUs, RTNode *rtnode);
bool FC_isAptStb(int fcStb, RTNode *rtnode);
bool FC_isAptSts(int fcSts, RTNode *rtnode);
bool FC_isAptExa(char *fcExa, RTNode *rtnode);
bool FC_isAptExb(char *fcExb, RTNode *rtnode);
bool FC_isAptLbl(cJSON *fcLbl, RTNode *rtnode);
bool FC_isAptPalb(cJSON *fcPalb, RTNode *rtnode);
bool FC_isAptClbl(cJSON *fcClbl, RTNode *rtnode);
bool FC_isAptTy(cJSON *ty_list, int ty);
bool FC_isAptChty(cJSON *chty_list, int ty);
bool FC_isAptPty(cJSON *fcPty, int ty);
bool FC_isAptSza(int fcSza, RTNode *rtnode);
bool FC_isAptSzb(int fcSzb, RTNode *rtnode);
bool FC_isAptOps(ACOP fcAcop, oneM2MPrimitive *o2pt, RTNode *rtnode);

bool do_uri_exist(cJSON* list, char *uri);
void cjson_merge_objs_by_operation(cJSON* obj1, cJSON* obj2, FilterOperation fo);

bool isValidFcAttr(char* attr);
void parse_filter_criteria(cJSON *fc);
void parse_qs(cJSON *qs);
void route(oneM2MPrimitive *o2pt);
void add_general_attribute(cJSON *root, RTNode *parent_rtnode, ResourceType ty);
char* create_remote_annc(RTNode *parent_rtnode, cJSON *obj, char *at, bool isParent);

#define ALL_ACOP ACOP_CREATE + ACOP_RETRIEVE + ACOP_UPDATE + ACOP_DELETE + ACOP_NOTIFY + ACOP_DISCOVERY

#endif