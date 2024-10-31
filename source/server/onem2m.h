#ifndef __ONEM2M_H__
#define __ONEM2M_H__

#include <stdbool.h>
#include "cJSON.h"
#include "onem2mTypes.h"

// oneM2M Resource
typedef struct
{
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

// Resource Tree
typedef struct RTNode
{
	struct RTNode *parent;
	struct RTNode *child;
	struct NodeList *subs;
	cJSON *memberOf;
	struct RTNode *sibling_left;
	struct RTNode *sibling_right;
	char *rn;
	char *uri;
	int sub_cnt;

	ResourceType ty;
	cJSON *obj;
} RTNode;

typedef struct NodeList
{
	struct NodeList *next;
	RTNode *rtnode;
	char *uri;
} NodeList;

typedef struct
{
	RTNode *cb;
	RTNode *registrar_csr;
	NodeList *csr_list;
} ResourceTree;

typedef struct _o
{
	char *to;  // To
	char *fr;  // From
	char *rqi; // Request ID
	RVI rvi;   // Request Version Indicator
	// char *pc;
	Operation op;		// Operation
	cJSON *request_pc;	// Primitive Content
	cJSON *response_pc; // Primitive Content
	int rcn;			// Result Content
	int rsc;			// Result Code
	int coap_rsc;		// CoAP Result Code
	int ty;				// Resource Type
	bool isFopt;
	bool isForwarding;
	char *fopt;
	bool errFlag;
	char *ip;
	char *mqtt_origin;
	ContentStatus cnst; // Content Status
	int cnot;			// Content Offset
	cJSON *fc;			// Filter Criteria
	int drt;			// Discovery Result Type
} oneM2MPrimitive;

typedef struct _n
{
	Protocol prot;
	char host[1024];
	int port;
	char target[256];
	char *noti_json;
} NotiTarget;

// onem2m resource
int create_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int handle_onem2m_request(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int retrieve_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int retrieve_object_filtercriteria(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int delete_onem2m_resource(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int notify_via_sub(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
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
int update_aea(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_cnt(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_sub(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_acp(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_grp(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_csr(oneM2MPrimitive *o2pt, RTNode *target_rtnode);

void init_cse(cJSON *cse);
void init_acp(cJSON *acp);
void init_csr(cJSON *csr);

// resource tree
RTNode *create_rtnode(cJSON *resource, ResourceType ty);
int delete_process(oneM2MPrimitive *o2pt, RTNode *rtnode);
void free_rtnode(RTNode *rtnode);
void free_rtnode_list(RTNode *rtnode);

RTNode *restruct_resource_tree(RTNode *node, RTNode *list);
RTNode *latest_cin_list(RTNode *cinList, int num); // use in viewer API
RTNode *find_latest_oldest(RTNode *node, int flag);
bool isResourceAptFC(oneM2MPrimitive *o2pt, RTNode *rtnode, cJSON *fc);

// etc
int cJSON_getArrayIdx(cJSON *arr, char *value);
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

bool do_uri_exist(cJSON *list, char *uri);
void cjson_merge_objs_by_operation(cJSON *obj1, cJSON *obj2, FilterOperation fo);

bool isValidFcAttr(char *attr);
void parse_filter_criteria(cJSON *fc);
void parse_qs(cJSON *qs);
void route(oneM2MPrimitive *o2pt);
void add_general_attribute(cJSON *root, RTNode *parent_rtnode, ResourceType ty);
char *create_remote_annc(RTNode *parent_rtnode, cJSON *obj, char *at, bool isParent);
int create_annc(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
int update_annc(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);

#define ALL_ACOP ACOP_CREATE + ACOP_RETRIEVE + ACOP_UPDATE + ACOP_DELETE + ACOP_NOTIFY + ACOP_DISCOVERY

#endif