#ifndef RT_MANAGER_H
#define RT_MANAGER_H

#include "onem2m.h"

RTNode *find_rtnode_by_uri(const char *node_uri);
RTNode *get_rtnode(oneM2MPrimitive *o2pt);
RTNode *find_csr_rtnode_by_uri(char *uri);
RTNode *find_rtnode(char *addr);
RTNode *get_remote_resource(char *address, int *rsc);
RTNode *find_rtnode_by_ri(char *ri);
RTNode *rt_search_ri(RTNode *rtnode, char *ri);
int add_child_resource_tree(RTNode *parent, RTNode *child);
void init_resource_tree();
RTNode *restruct_resource_tree(RTNode *parent_rtnode, RTNode *list);
int addNodeList(NodeList *target, RTNode *rtnode);
int deleteNodeList(NodeList *target, RTNode *rtnode);
void free_all_nodelist(NodeList *nl);
void free_nodelist(NodeList *nl);
void add_subs(RTNode *parent, RTNode *sub);
void detach_subs(RTNode *parent, RTNode *sub);
void add_csrlist(RTNode *csr);
void detach_csrlist(RTNode *csr);
void free_all_resource(RTNode *rtnode);

#endif // RT_MANAGER_H
