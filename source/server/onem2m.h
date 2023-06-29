#ifndef __ONEM2M_H__
#define __ONEM2M_H__

#include <stdbool.h>
#include "cJSON.h"
#include "onem2mTypes.h"

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
	OP_OPTIONS,
	OP_DISCOVERY
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
	ResourceType ty;
	char *uri;
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
	char *acpi;
	char *origin;
	ResourceType ty;
	bool rr;
	char *uri;
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
	ResourceType ty;
	int st;
	int cni;
	int cbs;
	int mni;
	int mbs;
	char *uri;
} CNT;

typedef struct {
	char *et;
	char *ct;
	char *lt;
	char *rn;
	char *ri;
	char *pi;
	char *con;	
	ResourceType ty;
	int st;
	int cs;
	char *uri;
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
	ResourceType ty;
	int nct;
	char *uri;
	int net_bit;
} SUB;

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
	char *uri;
	char *lbl;
	ResourceType ty;
} ACP;

typedef struct {
	char *rn;
	char *pi;
	char *ri;
	char *ct;
	char *lt;
	char *et;
	char *acpi;
	char *macp;
	
	ResourceType mt;
	int mnm;
	int cnm;

	char **mid;
	bool mtv;
	ConsistencyStrategy csy;
	char *uri;
} GRP;

//Resource Tree
typedef struct RTNode {
	struct RTNode *parent;
	struct RTNode *child;
	struct RTNode *sibling_left;
	struct RTNode *sibling_right;

	char *uri;
	ResourceType ty;
	void *obj;
}RTNode;

typedef struct {  
	RTNode *cb;
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


typedef struct {
    /*Created Before*/
    char* crb;
    /*Created After*/
    char* cra;
    /*Modified Since*/
    char* ms;
    /*Unmodified Since*/
    char* us;
    /*stateTag Smaller*/
    int sts;
    /*stateTag Bigger*/
    int stb;
    /*Expire Before*/
    char* exb;
    /*Expire After*/
    char* exa;
    
    /*Label*/
    cJSON* lbl;
    /*Parent Label*/
    cJSON *palb;
    /*Child Label*/
    cJSON *clbl;

    /*Labels Query*/
    char* lbq;
    /*Resource Type*/
    int *ty;
    int tycnt;
    /*Child Resource Type*/
    int *chty;
    int chtycnt;
    /*Parent Resource Type*/
    int *pty;
    int ptycnt;
    /*Size Above*/
    int sza;
    /*Size Below*/
    int szb;

    /*Content Type*/
    char* cty;
    /*Attribute*/
    char* atr;
    /*Child Attribute*/
    char* catr;
    /*Parent Attribute*/
    char* patr;

    FilterUsage fu;
    /*Limit*/
    int lim;
    /*sememtics FIlter*/
    char* smf;
    FilterOperation fo;

    /*Content Filter Syntax*/
    char* cfs;
    /*Content Filter Query*/
    char* cfq;

    /*Level*/
    int lvl;
    /*Offset*/
    int ofst;
    /*apply Relative Path*/
    char* arp;
    /*GeoQuery*/
    char* gq;
    /*Operations*/
    ACOP ops;
    
    struct _o *o2pt;
} FilterCriteria;


typedef struct _o{
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
	bool isFopt;
	char *fopt;
	bool errFlag;
	ContentStatus cnst;
	int cnot;
	FilterCriteria *fc;
	int slotno;
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

int create_ae(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
int create_cnt(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
int create_cin(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
int create_sub(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
int create_acp(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);
int create_grp(oneM2MPrimitive *o2pt, RTNode *parent_rtnode);

int retrieve_cse(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int retrieve_ae(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int retrieve_cnt(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int retrieve_cin(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int retrieve_cin_latest(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int retrieve_cin_by_ri(char *ri);
int retrieve_sub(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int retrieve_acp(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int retrieve_grp(oneM2MPrimitive *o2pt, RTNode *target_rtnode);

int update_cse(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_ae(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_cnt(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_sub(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_acp(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int update_grp(oneM2MPrimitive *o2pt, RTNode *target_rtnode);

void init_cse(CSE* cse);
void init_ae(AE* ae, RTNode *parent_rtnode, char *origin);
void init_cnt(CNT* cnt, RTNode *parent_rtnode);
void init_cin(CIN* cin, RTNode *parent_rtnode);
void init_sub(SUB* sub, RTNode *parent_rtnode);
void init_acp(ACP* acp, RTNode *parent_rtnode);
void init_grp(GRP* grp, RTNode *parent_rtnode);
void set_rtnode_update(RTNode* rtnode, void *after);
int set_ae_update(oneM2MPrimitive *o2pt, cJSON *m2m_ae, AE* ae);
int set_cnt_update(oneM2MPrimitive *o2pt, cJSON *m2m_cnt, CNT* cnt);
int set_sub_update(oneM2MPrimitive *o2pt, cJSON *m2m_sub, SUB* sub);
int set_acp_update(oneM2MPrimitive *o2pt, cJSON *m2m_acp, ACP* acp);
int set_grp_update(oneM2MPrimitive *o2pt, cJSON *m2m_grp, GRP* grp);

void free_cse(CSE* cse);
void free_ae(AE* ae);
void free_cnt(CNT* cnt);
void free_cin(CIN* cin);
void free_sub(SUB* sub);
void free_acp(ACP *acp);
void free_grp(GRP *grp);

//resource tree
RTNode* create_rtnode(void *resource, ResourceType ty);
int delete_rtnode_and_db_data(oneM2MPrimitive *o2pt, RTNode *rtnode, int flag);
void free_rtnode(RTNode *rtnode);
void free_rtnode_list(RTNode *rtnode);

RTNode* restruct_resource_tree(RTNode *node, RTNode *list);
RTNode* latest_cin_list(RTNode *cinList, int num); // use in viewer API
RTNode* find_latest_oldest(RTNode* node, int flag);
void set_node_uri(RTNode* rtnode);

//etc
int update_cnt_cin(RTNode *cnt_rtnode, RTNode *cin_rtnode, int sign);

FilterCriteria *parseFilterCriteria(cJSON *fcjson);

void free_fc(FilterCriteria *fc);
bool do_uri_exist(cJSON* list, char *uri);
cJSON *cjson_merge_arrays_by_operation(cJSON* arr1, cJSON* arr2, FilterOperation fo);

bool isValidFcAttr(char* attr);

FilterCriteria *parseFilterCriteria(cJSON *fcjson);

void free_fc(FilterCriteria *fc);

#define ALL_ACOP ACOP_CREATE + ACOP_RETRIEVE + ACOP_UPDATE + ACOP_DELETE + ACOP_NOTIFY + ACOP_DISCOVERY

#endif