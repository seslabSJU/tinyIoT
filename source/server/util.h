#ifndef __UTIL_H__
#define __UTIL_H__
#include "onem2mTypes.h"
#include "onem2m.h"
#include "config.h"
#include "cJSON.h"

void init_server();

void set_o2pt_pc(oneM2MPrimitive *o2pt, char *pc, ...);
void set_o2pt_rsc(oneM2MPrimitive *o2pt, int rsc);
void o2ptcpy(oneM2MPrimitive **dest, oneM2MPrimitive *src);
void free_o2pt(oneM2MPrimitive *o2pt);
void free_all_resource(RTNode *rtnode);
void log_runtime(double start);

RTNode* parse_uri(oneM2MPrimitive *o2pt, RTNode *cb);
int tree_viewer_api(oneM2MPrimitive *o2pt, RTNode *node);
void tree_viewer_data(RTNode *node, char **viewer_data, int cin_size) ;

//Resource Tree
void init_resource_tree();
int add_child_resource_tree(RTNode *parent, RTNode *child);
RTNode *find_rtnode_by_uri(RTNode *cb, char *node_uri);
RTNode *find_rtnode_by_ri(RTNode *cb, char *ri);
RTNode* find_latest_oldest(RTNode* node, int flag);
RTNode* latest_cin_list(RTNode *cinList, int num); // use in viewer API
char *get_ri_rtnode(RTNode *rtnode);
char *get_pi_rtnode(RTNode *rtnode);
char *get_rn_rtnode(RTNode *rtnode);
char *get_acpi_rtnode(RTNode *rtnode);
char *get_ct_rtnode(RTNode *rtnode);
char *get_et_rtnode(RTNode *rtnode);
char *get_lt_rtnode(RTNode *rtnode);
int get_st_rtnode(RTNode *rtnode);
int get_cs_rtnode(RTNode *rtnode);
char *get_lbl_rtnode(RTNode *rtnode);
char *get_uri_rtnode(RTNode *rtnode);

//error
int check_privilege(oneM2MPrimitive *o2pt, RTNode *target_rtnode, ACOP acop);
int check_payload_empty(oneM2MPrimitive *o2pt);
int check_rn_duplicate(oneM2MPrimitive *o2pt, RTNode *rtnode);
int check_aei_duplicate(oneM2MPrimitive *o2pt, RTNode *rtnode);
int check_resource_type_equal(oneM2MPrimitive *o2pt);
int check_resource_type_invalid(oneM2MPrimitive *o2pt);
int result_parse_uri(oneM2MPrimitive *o2pt, RTNode *target_rtnode);
int check_payload_size(oneM2MPrimitive *o2pt);
int check_payload_format(oneM2MPrimitive *o2pt);
int check_rn_invalid(oneM2MPrimitive *o2pt, ResourceType ty);
bool check_acpi_valid(oneM2MPrimitive *o2pt, cJSON *acpi);
void db_store_fail(oneM2MPrimitive *o2pt);

//etc
char* get_local_time(int diff);
char* resource_identifier(ResourceType ty, char *ct);
void delete_cin_under_cnt_mni_mbs(RTNode *rtnode);
int net_to_bit(char *net);
int get_acop(oneM2MPrimitive *o2pt, RTNode *node);
int get_acop_origin(char *origin, RTNode *acp, int flag);
int get_value_querystring_int(char *key);
void remove_invalid_char_json(char* json);
int is_json_valid_char(char c);
bool is_rn_valid_char(char c);

int validate_grp(GRP *grp);
bool isMinDup(char **mid, int idx, char *new_mid);

ResourceType http_parse_object_type();
ResourceType parse_object_type_cjson(cJSON *cjson);

bool isFopt(char *str);
bool endswith(char *str, char *match);

void handle_error(oneM2MPrimitive *o2pt, int rsc, char *err);

int rsc_to_http_status(int rsc);

cJSON *o2pt_to_json(oneM2MPrimitive *o2pt);
void remove_mid(char **mid, int idx, int cnm);
int handle_csy(GRP *grp, int i);
int get_number_from_cjson(cJSON *json);
cJSON *qs_to_json(char* qs);
cJSON *handle_uril(cJSON *uril, char *new_uri, FilterOperation fo);
void filterOptionStr(FilterOperation fo , char *sql);

#ifdef BERKELEY_DB
cJSON *fc_scan_resource_tree(RTNode *rtnode, FilterCriteria *fc, int lvl);
#endif

#endif