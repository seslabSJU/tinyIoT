#ifndef __UTIL_H__
#define __UTIL_H__
#include "onem2mTypes.h"
#include "onem2m.h"
#include "config.h"
#include "cJSON.h"
#include "httpd.h"

bool init_server();

void set_o2pt_pc(oneM2MPrimitive *o2pt, char *pc, ...);
void set_o2pt_rsc(oneM2MPrimitive *o2pt, int rsc);
void o2ptcpy(oneM2MPrimitive **dest, oneM2MPrimitive *src);
void free_o2pt(oneM2MPrimitive *o2pt);
void free_all_resource(RTNode *rtnode);
void log_runtime(double start);

void update_resource(cJSON *old, cJSON *new);
int parsePoa(char *poa_str, Protocol *prot, char **host, int *port, char **path);
RTNode *get_rtnode(oneM2MPrimitive *o2pt);
RTNode *parse_uri(oneM2MPrimitive *o2pt, RTNode *cb);
RTNode *get_remote_resource(char *address, int *rsc);

cJSON *getNonDiscoverableAcp(oneM2MPrimitive *o2pt, RTNode *rtnode);
cJSON *getNoPermAcopDiscovery(oneM2MPrimitive *o2pt, RTNode *rtnode, ACOP acop);

// Addressing
ResourceAddressingType checkResourceAddressingType(char *uri);
bool isSPIDLocal(char *address);
bool isSpRelativeLocal(char *address);

// Remote-CSE
int register_remote_cse();
int create_local_csr();
int update_remote_csr_dcse(RTNode *created_rtnode);
int deRegister_csr();
void add_csrlist(RTNode *csr_rtnode);
void detach_csrlist(RTNode *csr_rtnode);

// Subscription
void add_subs(RTNode *parent, RTNode *sub);
void detach_subs(RTNode *parent, RTNode *sub);

// Announcement
int handle_annc_create(RTNode *parent_rtnode, cJSON *resource_obj, cJSON *at_obj, cJSON *final_at);
int handle_annc_update(RTNode *target_rtnode, cJSON *at_obj, cJSON *final_at);

void announce_to_annc(RTNode *target_rtnode);
int create_remote_cba(char *poa, char **cbA_url);
int deregister_remote_cba(char *cbA_url);
int deregister_remote_annc(RTNode *target_rtnode, cJSON *delete_at_list);
void removeChildAnnc(RTNode *rtnode, char *at);

// Resource Tree
void init_resource_tree();
int add_child_resource_tree(RTNode *parent, RTNode *child);
RTNode *find_rtnode(char *addr);
RTNode *parse_spr_uri(oneM2MPrimitive *o2pt, char *target_uri);
RTNode *parse_abs_uri(oneM2MPrimitive *o2pt, char *target_uri);
RTNode *find_rtnode_by_uri(char *node_uri);
RTNode *find_rtnode_by_ri(char *ri);
RTNode *rt_search_ri(RTNode *rtnode, char *ri);
RTNode *find_csr_rtnode_by_uri(char *target_uri);
RTNode *find_latest_oldest(RTNode *node, int flag);
RTNode *latest_cin_list(RTNode *cinList, int num); // use in viewer API
char *get_ri_rtnode(RTNode *rtnode);
char *get_pi_rtnode(RTNode *rtnode);
char *get_rn_rtnode(RTNode *rtnode);
cJSON *get_acpi_rtnode(RTNode *rtnode);
char *get_ct_rtnode(RTNode *rtnode);
char *get_et_rtnode(RTNode *rtnode);
char *get_lt_rtnode(RTNode *rtnode);
int get_st_rtnode(RTNode *rtnode);
int get_cs_rtnode(RTNode *rtnode);
char *get_lbl_rtnode(RTNode *rtnode);
char *get_uri_rtnode(RTNode *rtnode);
char *ri_to_uri(char *ri);
cJSON *getResource(cJSON *root, ResourceType ty);

// Notification
int requestToResource(oneM2MPrimitive *o2pt, RTNode *rtnode);
int send_verification_request(char *noti_uri, cJSON *noti_cjson);
bool isAptEnc(oneM2MPrimitive *o2pt, RTNode *target_rtnode, RTNode *sub_rtnode);
int notify_to_nu(RTNode *sub_rtnode, cJSON *noti_cjson, int net);

// rcn
int make_response_body(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
void build_rcn4(oneM2MPrimitive *o2pt, RTNode *rtnode, cJSON *result_obj, int ofst, int lim, int level);
void build_rcn5(oneM2MPrimitive *o2pt, RTNode *rtnode, cJSON *result_obj, int ofst, int lim, int level);
void build_rcn6(oneM2MPrimitive *o2pt, RTNode *rtnode, cJSON *result_obj, int ofst, int lim, int level);
void build_rcn8(oneM2MPrimitive *o2pt, RTNode *rtnode, cJSON *result_obj, int ofst, int lim, int level);
void build_child_structure(oneM2MPrimitive *o2pt, RTNode *rtnode, cJSON *result_obj, int *ofst, int *lim, int level);
void get_child_references(oneM2MPrimitive *o2pt, RTNode *rtnode, cJSON *result_obj, int *ofst, int *lim, int level);

// validation
bool is_attr_valid(cJSON *obj, ResourceType ty, char *err_msg);
bool is_valid_acr(cJSON *acr);
int validate_ae(oneM2MPrimitive *o2pt, cJSON *ae, Operation op);
int validate_cnt(oneM2MPrimitive *o2pt, cJSON *cnt, Operation op);
int validate_cin(oneM2MPrimitive *o2pt, cJSON *parent_cnt, cJSON *cin, Operation op);
int validate_sub(oneM2MPrimitive *o2pt, cJSON *sub, Operation op);
int validate_acp(oneM2MPrimitive *o2pt, cJSON *acp, Operation op);
int validate_grp(oneM2MPrimitive *o2pt, cJSON *grp);
int validate_grp_update(oneM2MPrimitive *o2pt, cJSON *grp_old, cJSON *grp_new);
int validate_grp_member(cJSON *grp, cJSON *final_mid, int csy, int mt);
int validate_csr(oneM2MPrimitive *o2pt, RTNode *parent_rtnode, cJSON *csr, Operation op);
int validate_acpi(oneM2MPrimitive *o2pt, cJSON *acpiAttr, Operation op);
int validate_acr(oneM2MPrimitive *o2pt, cJSON *acr_attr);
bool checkResourceCseID(char *resourceUri, char *cseID);
bool isValidChildType(ResourceType parent, ResourceType child);

// error
int check_mandatory_attributes(oneM2MPrimitive *o2pt);
int check_privilege(oneM2MPrimitive *o2pt, RTNode *target_rtnode, ACOP acop);
int check_payload_empty(oneM2MPrimitive *o2pt);
int check_rn_duplicate(oneM2MPrimitive *o2pt, RTNode *rtnode);
int check_aei_duplicate(oneM2MPrimitive *o2pt, RTNode *rtnode);
int check_aei_invalid(oneM2MPrimitive *o2pt);
int check_resource_type_equal(oneM2MPrimitive *o2pt);
int check_resource_type_invalid(oneM2MPrimitive *o2pt);
int result_parse_uri(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int check_payload_format(oneM2MPrimitive *o2pt);
int check_rn_invalid(oneM2MPrimitive *o2pt, ResourceType ty);
bool check_acpi_valid(oneM2MPrimitive *o2pt, cJSON *acpi);
int check_csi_duplicate(char *new_csi, RTNode *rtnode);
int check_macp_privilege(oneM2MPrimitive *o2pt, RTNode *rtnode, ACOP acop);

// etc
char *get_local_time(int diff);
char *resource_identifier(ResourceType ty, char *ct);
void delete_cin_under_cnt_mni_mbs(RTNode *rtnode);
int net_to_bit(cJSON *net);
int get_value_querystring_int(char *key);
void remove_invalid_char_json(char *json);
int is_json_valid_char(char c);
bool is_rn_valid_char(char c);
int parsePoa(char *poa_str, Protocol *prot, char **host, int *port, char **path);
bool isMinDup(char **mid, int idx, char *new_mid);
int set_grp_member(RTNode *grp_rtnode);
int addNodeList(NodeList *target, RTNode *rtnode);
int deleteNodeList(NodeList *target, RTNode *rtnode);
void free_all_nodelist(NodeList *nl);
void free_nodelist(NodeList *nl);
RVI to_rvi(char *str);
char *from_rvi(RVI rvi);

// privilege
int get_acop(oneM2MPrimitive *o2pt, char *origin, RTNode *node);
int get_acop_macp(oneM2MPrimitive *o2pt, RTNode *rtnode);
int get_acop_origin(oneM2MPrimitive *o2pt, char *origin, RTNode *acp_rtnode, int flag);
int has_privilege(oneM2MPrimitive *o2pt, char *acpi, ACOP acop);
int has_acpi_update_privilege(oneM2MPrimitive *o2pt, char *acpi);

ResourceType http_parse_object_type(header_t *headers);
ResourceType coap_parse_object_type(int object_type);
ResourceType parse_object_type_cjson(cJSON *cjson);

bool isUriFopt(char *str);
bool endswith(char *str, char *match);

int handle_error(oneM2MPrimitive *o2pt, int rsc, char *err);

int rsc_to_http_status(int rsc, char **msg);
int rsc_to_coap_status(int rsc);

char *get_resource_key(ResourceType ty);

cJSON *o2pt_to_json(oneM2MPrimitive *o2pt);
void remove_mid(char **mid, int idx, int cnm);
int handle_csy(cJSON *grp, cJSON *mid_obj, int csy, int i);
int get_number_from_cjson(cJSON *json);
cJSON *qs_to_json(char *qs);
cJSON *handle_uril(cJSON *uril, char *new_uri, FilterOperation fo);
void filterOptionStr(FilterOperation fo, char *sql);
ACOP op_to_acop(Operation op);
#endif