#include <stdio.h>
#include <stdbool.h>
#include <db.h>
#include "cJSON.h"
#include "httpd.h"

//enum
typedef enum {
	o_NONE = 0,
	o_CREATE,
	o_RETRIEVE,
	o_UPDATE,
	o_DELETE,
	o_OPTIONS,
	o_VIEWER,
	o_TEST,
	o_LA,
	o_OL,
	o_CIN_RI
}Operation;

typedef enum {
	t_ACP = 1,
	t_AE,
	t_CNT,
	t_CIN,
	t_CSE,
	t_Sub = 23
}ObjectType;

typedef enum {
	noti_event_1 = 1,
	noti_event_2 = 2,
	noti_event_3 = 4,
	noti_event_4 = 8
}Net;

typedef enum {
	acop_Create = 1,
	acop_Retrieve = 2,
	acop_Update = 4,
	acop_Delete = 8,
	acop_Notify = 16,
	acop_Discovery = 32
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
	struct Node *siblingLeft;
	struct Node *siblingRight;
	
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
	ObjectType ty;
	
	int cinSize;
	int net;
}Node;

typedef struct {  
	Node *cb;
}ResourceTree;

//HTTP Request
int Validate_oneM2M_Standard();
int duplicate_resource_check(Node *pnode);
Node* Parse_URI(Node *cb, char *uri_array, Operation *op);
Operation Parse_Operation();
ObjectType Parse_ObjectType();
ObjectType Parse_ObjectType_Body();
void Remove_Specific_Asterisk_Payload();

//oneM2M Resource
void Create_Object(Node* pnode);
void Retrieve_Object(Node *pnode);
void Update_Object(Node *pnode);
void Delete_Object();
void Notify_Object(Node *node, char *resjson, Net net);

void Create_AE(Node *pnode);
void Create_CNT(Node *pnode);
void Create_CIN(Node *pnode);
void Create_Sub(Node *pnode);
void Create_ACP(Node *pnode);

void Retrieve_CSE(Node *pnode);
void Retrieve_AE(Node *pnode);
void Retrieve_CNT(Node *pnode);
void Retrieve_CIN(Node *pnode);
void Retrieve_CIN_Ri(char *ri);
void Retrieve_Sub(Node *pnode);

void Update_CSE(Node *pnode);
void Update_AE(Node *pnode);
void Update_CNT(Node *pnode);
void Update_Sub(Node *pnode);

void Init_CSE(CSE* cse);
void Init_AE(AE* ae, char *pi);
void Init_CNT(CNT* cnt, char *pi);
void Init_CIN(CIN* cin, char *pi);
void Init_Sub(Sub* sub, char *pi);
void Init_ACP(ACP* acp, char *pi);
void Set_AE_Update(AE* after);
void Set_CNT_Update(CNT* after);
void Set_Sub_Update(Sub* after);

CSE* JSON_to_CSE(char *json_payload);
AE* JSON_to_AE(char *json_payload);
CNT* JSON_to_CNT(char *json_payload);
CIN* JSON_to_CIN(char *json_payload);
Sub* JSON_to_Sub(char *json_payload);
ACP* JSON_to_ACP(char *json_payload);

char* CSE_to_json(CSE* cse_object);
char* AE_to_json(AE* ae_object);
char* CNT_to_json(CNT* cnt_object);
char* CIN_to_json(CIN* cin_object);
char* Sub_to_json(Sub *sub_object);
char* Noti_to_json(char *sur, int net, char *rep);
char* ACP_to_json(ACP *acp_object);

void Free_CSE(CSE* cse);
void Free_AE(AE* ae);
void Free_CNT(CNT* cnt);
void Free_CIN(CIN* cin);
void Free_Sub(Sub* sub);
void Free_ACP(ACP *acp);

//DB
int display(char* database);

int Store_CSE(CSE* cse_object);
int Store_AE(AE* ae_object);
int Store_CNT(CNT* cnt_object);
int Store_CIN(CIN* cin_object);
int Store_Sub(Sub *sub_object);
int Store_ACP(ACP *acp_object);

CSE* Get_CSE();
AE* Get_AE(char *ri);
CNT* Get_CNT(char *ri);
CIN* Get_CIN(char *ri);
Sub* Get_Sub(char* ri);

int Update_AE_DB(AE* ae_object);
int Update_CNT_DB(CNT* cnt_object);
int Update_Sub_DB(Sub *sub_object);

int Delete_CSE(char *ri);
int Delete_AE(char *ri);
int Delete_CNT(char *ri);
int Delete_CIN(char *ri);
int Delete_Sub(char* ri);

Node* Get_All_AE();
Node* Get_All_CNT();
Node* Get_All_CIN();
Node* Get_All_Sub();

Node* Get_CIN_Period(char *start_time, char *end_time);
Node* Get_CIN_Pi(char* pi);

char* Label_To_URI(char* label);
char* URI_To_Label(char* uri);
int Store_Label(char* label, char* uri);

//Resource Tree
Node* Create_Node(void *obj, ObjectType ty);
Node* Create_CSE_Node(CSE *cse);
Node* Create_AE_Node(AE *ae);
Node* Create_CNT_Node(CNT *cnt);
Node* Create_CIN_Node(CIN *cin);
Node* Create_Sub_Node(Sub *sub);
Node* Create_ACP_Node(ACP *acp);
int Add_child(Node *parent, Node *child);
char* Node_to_json(Node *node);
Node *Find_Node_by_URI(Node *cse, char *node_uri);
void Delete_Node_Object(Node *node, int flag);
void Free_Node(Node *node);

void Tree_Viewer_API(Node *node);
void Tree_data(Node *node, char **viewer_data, int cin_size);
void Restruct_ResourceTree();
Node* Restruct_childs(Node *node, Node *list);
Node* Latest_CINs(Node *cinList, int num); // use in viewer API

//JSON
char* Get_JSON_Value_char(char *key, char *json);
int Get_JSON_Value_int(char *key, char *json);
int Get_JSON_Value_bool(char *key, char *json);
void Remove_Invalid_Char_JSON(char* json);
int is_JSON_Valid_Char(char c);

//HTTP etc
struct url_data { size_t size; char* data;};
size_t write_data(void *ptr, size_t size, size_t nmemb, struct url_data *data);
char* Send_HTTP_Packet(char *target, char *post_data);
void Response_JSON_Parse_Error();

//etc
void init();
char* Get_LocalTime(int diff);
char* resource_identifier(ObjectType ty, char *ct);
void CIN_in_period(Node *pnode);
void Object_Test_API(Node *node);
char* JSON_label_value(char *json_payload);
int net_to_bit(char *net);
int Get_acop(Node *node);

#define MAX_TREE_VIEWER_SIZE 65536
#define MAX_PROPERTY_SIZE 32768
#define MAX_URI_SIZE 1024
#define EXPIRE_TIME -3600*24*365*2
#define ALL_ACOP acop_Create + acop_Retrieve + acop_Update + acop_Delete + acop_Notify + acop_Discovery