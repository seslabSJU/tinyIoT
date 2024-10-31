#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "onem2m.h"
#include "logger.h"
#include "dbmanager.h"
#include "util.h"
#include "config.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;
extern pthread_mutex_t main_lock;

/**
 * @brief get resource with oneM2MPrimitive
 * @param o2pt oneM2MPrimitive
 * @return resource node or NULL
 * @remark distinguish ResourceAddressingType
 */
RTNode *get_rtnode(oneM2MPrimitive *o2pt)
{
    char *ptr = NULL;
    if (!o2pt)
        return NULL;
    if (o2pt->to == NULL)
    {
        handle_error(o2pt, RSC_BAD_REQUEST, "Invalid uri");
        return NULL;
    }

    ResourceAddressingType RAT = checkResourceAddressingType(o2pt->to);
    RTNode *rtnode = NULL;
    char *target_uri = strdup(o2pt->to);
    if (RAT == CSE_RELATIVE)
    {
        logger("RTM", LOG_LEVEL_DEBUG, "CSE_RELATIVE");
        rtnode = find_rtnode(target_uri);
    }
    else if (RAT == SP_RELATIVE)
    {
        logger("RTM", LOG_LEVEL_DEBUG, "SP_RELATIVE");
        rtnode = parse_spr_uri(o2pt, target_uri);
    }
    else if (RAT == ABSOLUTE)
    {
        logger("RTM", LOG_LEVEL_DEBUG, "ABSOLUTE");
        rtnode = parse_abs_uri(o2pt, target_uri);
    }

    if (rtnode && rtnode->ty == RT_GRP)
    {
        if (strlen(target_uri) > strlen(get_uri_rtnode(rtnode)))
        {
            char *fopt_start = strstr(target_uri, "/fopt");
            if (fopt_start)
            {
                if (strlen(fopt_start) == 5)
                {
                    if (strncmp(fopt_start, "/fopt", 5) == 0)
                    {
                        o2pt->isFopt = true;
                        o2pt->fopt = NULL;
                    }
                }
                else if (strlen(fopt_start) > 5)
                {
                    if (fopt_start[5] == '/')
                    {
                        o2pt->fopt = strdup(fopt_start + 5);
                        o2pt->isFopt = true;
                    }
                }
            }
        }
    }

    free(target_uri);
    return rtnode;
}

/**
 *
 */

RTNode *parse_abs_uri(oneM2MPrimitive *o2pt, char *target_uri)
{
    RTNode *rtnode = NULL;
    char *ptr = NULL;
    if (isSPIDLocal(target_uri))
    {
        if (strcmp(target_uri + 2, CSE_BASE_SP_ID) == 0)
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "Invalid uri");
            free(target_uri);
            return NULL;
        }
        ptr = target_uri + strlen(CSE_BASE_SP_ID) + 2; // for first two /
        if (strlen(ptr) == 0)
        {
            logger("UTIL", LOG_LEVEL_DEBUG, "addr is empty");
            handle_error(o2pt, RSC_BAD_REQUEST, "Invalid uri");
            free(target_uri);
            return NULL;
        }
        rtnode = parse_spr_uri(o2pt, ptr);
    }
    else
    {
        int rsc = 0;
        char buffer[256];
        char *tmp = NULL;
        o2pt->isForwarding = true;
        RTNode *csr = find_csr_rtnode_by_uri(target_uri);

        // remote CSE not registered
        // forwarding TS 0004
    }
    return rtnode;
}

/**
 * @brief parse uri with SP RELATIVE addressing
 * @param o2pt oneM2MPrimitive
 * @param target_uri target uri
 * @return resource node or NULL
 */
RTNode *parse_spr_uri(oneM2MPrimitive *o2pt, char *target_uri)
{
    RTNode *rtnode = NULL;
    char *ptr = NULL;
    if (isSpRelativeLocal(target_uri))
    {
        if (strcmp(target_uri + 1, CSE_BASE_RI) == 0)
        {
            handle_error(o2pt, RSC_BAD_REQUEST, "Invalid uri");
            return NULL;
        }
        ptr = target_uri + strlen(CSE_BASE_RI) + 2; // for first / and end /
        if (strlen(ptr) == 0)
        {
            logger("RTM", LOG_LEVEL_DEBUG, "addr is empty");
            handle_error(o2pt, RSC_BAD_REQUEST, "Invalid uri");
            return NULL;
        }
        rtnode = find_rtnode(ptr);
    }
    else
    {
        int rsc = 0;
        char buffer[256];
        char *tmp = NULL;
        o2pt->isForwarding = true;
        RTNode *csr = find_csr_rtnode_by_uri(target_uri);

        // remote CSE not registered
        // forwarding TS 0004 7.3.2.6
        if (!csr)
        {
            if (SERVER_TYPE == IN_CSE)
            {
                handle_error(o2pt, RSC_NOT_FOUND, "Resource Not Found");
                return NULL;
            }
            else
            {
                cJSON *registrar_csr = rt->registrar_csr->obj;
                if (strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(registrar_csr, "csi")), target_uri) != 0)
                {
                    handle_error(o2pt, RSC_NOT_FOUND, "Resource Not Found");
                    return NULL;
                }
            }
        }

        rsc = forwarding_onem2m_resource(o2pt, csr);
        if (rsc >= 4000)
        {
            handle_error(o2pt, rsc, "Forwarding Failed");
        }
    }
    return rtnode;
}

/**
 * @brief get csr rtnode with uri
 * @param uri hierarchical uri (Ex. /tinyiot/CSE1)
 * @return resource node or NULL
 */
RTNode *find_csr_rtnode_by_uri(char *uri)
{
    char *uriPtr;
    // RTNode *rtnode = rt->cb->child, *parent_rtnode = NULL;
    if (!uri)
        return NULL;
    char *target_uri = strdup(uri); // remove second '/'
    char *ptr = strtok_r(target_uri + 1, "/", &uriPtr);
    if (!ptr)
        return NULL;

    NodeList *csrlist = rt->csr_list;

    while (csrlist)
    {
        // logger("UTIL", LOG_LEVEL_DEBUG, "csrlist->uri : %s", csrlist->uri);
        if (!strcmp(csrlist->uri + strlen(CSE_BASE_NAME), target_uri))
            break;

        cJSON *dcse_list = cJSON_GetObjectItem(csrlist->rtnode->obj, "dcse");
        cJSON *dcse = NULL;
        cJSON_ArrayForEach(dcse, dcse_list)
        {
            if (!strcmp(cJSON_GetStringValue(dcse), target_uri))
                break;
        }
        csrlist = csrlist->next;
    }
    return csrlist ? csrlist->rtnode : NULL;
}

/**
 * @brief get resource rtnode with CSE RELATIVE address(both heirarchical and non-heirarchical)
 * @param addr address of resource
 * @return resource node or NULL
 */
RTNode *find_rtnode(char *addr)
{
    if (!addr)
        return NULL;
    char *foptPtr = NULL;
    RTNode *rtnode = NULL;
    if (strcmp(addr, CSE_BASE_NAME) == 0 || strcmp(addr, "-") == 0)
    {
        return rt->cb;
    }
    if ((foptPtr = strstr(addr, "/fopt")))
    {
        foptPtr[0] = '\0';
    }
    logger("RTM", LOG_LEVEL_DEBUG, "find_rtnode [%s]", addr);

    if ((strncmp(addr, CSE_BASE_NAME, strlen(CSE_BASE_NAME)) == 0 && addr[strlen(CSE_BASE_NAME)] == '/') || (addr[0] == '-' && addr[1] == '/'))
    {
        logger("RTM", LOG_LEVEL_DEBUG, "Hierarchical Addressing");
        rtnode = find_rtnode_by_uri(addr);
    }
    else
    {
        logger("RTM", LOG_LEVEL_DEBUG, "Non-Hierarchical Addressing");
        rtnode = find_rtnode_by_ri(addr);
    }
    if (foptPtr)
    {
        foptPtr[0] = '/';
    }
    return rtnode;
}

/**
 * @brief get resource from remote cse with sp-relative uri
 * @param address sp-relative address
 * @return RTNode or NULL
 * @remark rtnode should be freed after use
 */
RTNode *get_remote_resource(char *address, int *rsc)
{
    if (!address)
        return NULL;
    if (address[0] != '/')
        return NULL;

    char *target_uri = strdup(address);
    RTNode *csr = find_csr_rtnode_by_uri(target_uri);

    if (!csr)
        return NULL;

    oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(1, sizeof(oneM2MPrimitive));
    o2pt->fr = strdup("/" CSE_BASE_RI);
    o2pt->to = strdup(target_uri);
    o2pt->op = OP_RETRIEVE;
    o2pt->rqi = strdup("retrieve remote resource");
    o2pt->rvi = CSE_RVI;

    forwarding_onem2m_resource(o2pt, csr);
    *rsc = o2pt->rsc;
    if (*rsc != RSC_OK)
    {
        free_o2pt(o2pt);
        return NULL;
    }
    ResourceType ty = parse_object_type_cjson(o2pt->response_pc);
    cJSON *resource = cJSON_GetObjectItem(o2pt->response_pc, get_resource_key(ty));
    RTNode *rtnode = create_rtnode(cJSON_Duplicate(resource, 1), ty);
    rtnode->uri = strdup(target_uri);

    free_o2pt(o2pt);
    return rtnode;
}

/**
 * @brief get resource node with uri
 * @param uri hierarchical uri
 * @return resource node or NULL
 */
RTNode *find_rtnode_by_uri(char *uri)
{
    RTNode *rtnode = rt->cb, *parent_rtnode = NULL;
    char *target_uri = strdup(uri);
    char *target_ptr;
    char *ptr = strtok_r(target_uri, "/", &target_ptr);
    if (!ptr)
        return NULL;
    if (!strcmp(ptr, "-"))
    {
        logger("RTM", LOG_LEVEL_DEBUG, "root node -");
        rtnode = rt->cb->child;
        ptr = strtok_r(NULL, "/", &target_ptr);
    }
#if MONO_THREAD == 0
    pthread_mutex_lock(&main_lock);
#endif
    while (ptr)
    {
        while (rtnode)
        {
            if (!strcmp(rtnode->rn, ptr))
                break;
            rtnode = rtnode->sibling_right;
        }
        if (!rtnode)
            break;
        ptr = strtok_r(NULL, "/", &target_ptr);
        if (ptr)
        {
            parent_rtnode = rtnode;
            rtnode = rtnode->child;
        }
    }

    if (rtnode)
    {
        free(target_uri);
#if MONO_THREAD == 0
        pthread_mutex_unlock(&main_lock);
#endif
        return rtnode;
    }
    int flag = -1;
    if (parent_rtnode)
    {
        if (parent_rtnode->ty == RT_CNT)
        {
            cJSON *cin = NULL;

            if (parent_rtnode->child)
            {
                if (strcmp(cJSON_GetObjectItem(parent_rtnode->child->obj, "rn")->valuestring, ptr) == 0)
                {
                    logger("RTM", LOG_LEVEL_DEBUG, "resource is latest");
                    rtnode = parent_rtnode->child;
                }
            }
            if (!rtnode && !strtok_r(NULL, "/", &target_ptr))
            { // if next '/' doesn't exist
                if (!strcmp(ptr, "ol") || !strcmp(ptr, "oldest"))
                {
                    flag = 1;
                }
                if (flag == 0 || flag == 1)
                {
                    cin = db_get_cin_laol(parent_rtnode, flag);
                }
                else
                {
                    cin = db_get_resource_by_uri(uri, RT_CIN);
                }
                if (cin)
                {
                    rtnode = create_rtnode(cin, RT_CIN);
                    rtnode->parent = parent_rtnode;
                }
            }
        }
    }
#if MONO_THREAD == 0
    pthread_mutex_unlock(&main_lock);
#endif

    free(target_uri);
    return rtnode;
}

/**
 * @brief get resource node with resource identifier
 * @param ri resource identifier
 * @return resource node or NULL
 */
RTNode *find_rtnode_by_ri(char *ri)
{
    logger("RTM", LOG_LEVEL_DEBUG, "find_rtnode_by_ri [%s]", ri);
    cJSON *resource = NULL;
    RTNode *rtnode = NULL;
    char *fopt = strstr(ri, "/fopt");
    // logger("UTIL", LOG_LEVEL_DEBUG, "RI : %s", ri);
    if (strncmp(ri, "4-", 2) == 0)
    {
        resource = db_get_resource(ri, RT_CIN);
        rtnode = create_rtnode(resource, RT_CIN);
        rtnode->parent = find_rtnode_by_uri(rtnode->uri);
        return rtnode;
    }
    else if (strncmp(ri, "9-", 2) == 0)
    {
        logger("RTM", LOG_LEVEL_DEBUG, "GRP");
        if (fopt)
        {
            *fopt = '\0';
        }
    }
#if MONO_THREAD == 0
    pthread_mutex_lock(&main_lock);
#endif

    rtnode = rt_search_ri(rt->cb, ri);

#if MONO_THREAD == 0
    pthread_mutex_unlock(&main_lock);
#endif
    if (fopt)
    {
        *fopt = '/';
    }
    return rtnode;
}

/**
 * @brief recursive search of resource tree
 * @param rtnode resource node
 * @param ri resource identifier
 * @return resource node or NULL
 */
RTNode *rt_search_ri(RTNode *rtnode, char *ri)
{
    RTNode *ret = NULL;
    if (!rtnode)
        return NULL;
    while (!ret && rtnode)
    {
        if (!strcmp(get_ri_rtnode(rtnode), ri))
        {
            ret = rtnode;
            break;
        }
        if (rtnode->child)
            ret = rt_search_ri(rtnode->child, ri);
        rtnode = rtnode->sibling_right;
    }

    return ret;
}

int add_child_resource_tree(RTNode *parent, RTNode *child)
{
    RTNode *node = parent->child;
    child->parent = parent;

    char *uri = malloc(strlen(parent->uri) + strlen(child->rn) + 2);
    strcpy(uri, parent->uri);
    strcat(uri, "/");
    strcat(uri, child->rn);
    if (child->uri)
        free(child->uri);
    child->uri = uri;

    logger("O2M", LOG_LEVEL_DEBUG, "Add Resource Tree Node [Parent-ID] : %s, [Child-ID] : %s", get_ri_rtnode(parent), get_ri_rtnode(child));

#if MONO_THREAD == 0
    pthread_mutex_lock(&main_lock);
#endif

    if (!node)
    {
        parent->child = child;
    }
    else if (node)
    {
        while (node->sibling_right && node->sibling_right->ty <= child->ty)
        {
            node = node->sibling_right;
        }

        if (parent->child == node && child->ty < node->ty)
        {
            parent->child = child;
            child->parent = parent;
            child->sibling_right = node;
            node->sibling_left = child;
        }
        else
        {
            if (node->sibling_right)
            {
                node->sibling_right->sibling_left = child;
                child->sibling_right = node->sibling_right;
            }

            node->sibling_right = child;
            child->sibling_left = node;
        }
    }
    if (child->ty == RT_CSR)
    {
        add_csrlist(child);
    }
    if (child->ty == RT_SUB)
    {
        add_subs(parent, child);
    }

#if MONO_THREAD == 0
    pthread_mutex_unlock(&main_lock);
#endif
    return 1;
}

void init_resource_tree()
{
    RTNode *temp = NULL;
    RTNode *rtnode_list = (RTNode *)calloc(1, sizeof(RTNode));
    RTNode *tail = rtnode_list;

    logger("UTIL", LOG_LEVEL_DEBUG, "init_resource_tree");
    RTNode *resource_list = db_get_all_resource_as_rtnode();
    tail->sibling_right = resource_list;
    if (resource_list)
        resource_list->sibling_left = tail;
    while (tail->sibling_right)
        tail = tail->sibling_right;

    temp = db_get_latest_cins();
    if (temp)
    {
        tail->sibling_right = temp;
        temp->sibling_left = tail;
        while (tail->sibling_right)
        {
            tail = tail->sibling_right;
            tail->rn = strdup("la");
        }
    }

    temp = rtnode_list;
    rtnode_list = rtnode_list->sibling_right;
    if (rtnode_list)
        rtnode_list->sibling_left = NULL;
    free_rtnode(temp);
    temp = NULL;

    temp = rtnode_list;

    if (rtnode_list)
        restruct_resource_tree(rt->cb, rtnode_list);
}

RTNode *restruct_resource_tree(RTNode *parent_rtnode, RTNode *list)
{
    // logger("UTIL", LOG_LEVEL_DEBUG, "restruct_resource_tree");
    RTNode *rtnode = list;
    while (rtnode)
    {
        RTNode *right = rtnode->sibling_right;
        // logger("UTIL", LOG_LEVEL_DEBUG, "restruct_resource_tree : %s, pi %s", get_ri_rtnode(rtnode), get_pi_rtnode(rtnode));
        if (!strcmp(get_ri_rtnode(parent_rtnode), get_pi_rtnode(rtnode)))
        {
            RTNode *left = rtnode->sibling_left;

            if (!left)
            {
                list = right;
            }
            else
            {
                left->sibling_right = right;
            }

            if (right)
                right->sibling_left = left;
            rtnode->sibling_left = rtnode->sibling_right = NULL;
            add_child_resource_tree(parent_rtnode, rtnode);
        }
        rtnode = right;
    }
    RTNode *child = parent_rtnode->child;

    while (child)
    {
        list = restruct_resource_tree(child, list);
        child = child->sibling_right;
    }

    return list;
}

int addNodeList(NodeList *target, RTNode *rtnode)
{
    if (!target || !rtnode)
        return -1;

    NodeList *newNode = (NodeList *)malloc(sizeof(NodeList));
    newNode->rtnode = rtnode;
    newNode->uri = strdup(rtnode->uri);
    newNode->next = NULL;

    if (target->next == NULL)
    {
        target->next = newNode;
    }
    else
    {
        NodeList *cur = target->next;
        while (cur->next != NULL)
        {
            cur = cur->next;
        }
        cur->next = newNode;
    }
    return 0;
}

int deleteNodeList(NodeList *target, RTNode *rtnode)
{
    if (!target || !rtnode)
        return -1;

    NodeList *cur = target;
    NodeList *prev = NULL;

    while (cur)
    {
        if (cur->rtnode == rtnode)
        {
            if (prev)
            {
                prev->next = cur->next;
            }
            else
            {
                target->next = cur->next;
            }
            free(cur->uri);
            free(cur);
            cur = NULL;
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }
    return 0;
}

void free_all_nodelist(NodeList *nl)
{
    NodeList *del;
    while (nl)
    {
        del = nl;
        nl = nl->next;
        free(del->uri);
        free(del);
        del = NULL;
    }
}

void free_nodelist(NodeList *nl)
{
    if (nl)
    {
        free(nl->uri);
        free(nl);
        nl = NULL;
    }
}

/**
 * @brief add Subscription List to resource
 * @param parent parent of subscription
 * @param sub subscription rtnode
 */
void add_subs(RTNode *parent, RTNode *sub)
{
    if (!parent->subs)
    {
        parent->subs = calloc(sizeof(NodeList), 1);
        parent->subs->rtnode = sub;
        parent->subs->uri = strdup(sub->uri);
        parent->subs->next = NULL;
        parent->sub_cnt++;
        return;
    }
    NodeList *psub = parent->subs;
    while (psub->next)
    {
        psub = psub->next;
    }
    psub->next = calloc(sizeof(NodeList), 1);
    psub->next->rtnode = sub;
    psub->next->uri = strdup(sub->uri);
    psub->next->next = NULL;
    parent->sub_cnt++;
}

/**
 * @brief detach subscription from resource
 * @param parent parent of subscription
 * @param sub subscription rtnode
 * @remark sub should be freed by caller
 */
void detach_subs(RTNode *parent, RTNode *sub)
{
    NodeList *psub = parent->subs;
    NodeList *del = NULL;
    if (!psub)
    {
        return;
    }
    if (psub->rtnode == sub)
    {
        del = psub;
        parent->subs = psub->next;
        parent->sub_cnt--;
        // free_nodelist(del);
        return;
    }
    while (psub->next)
    {
        if (psub->next->rtnode == sub)
        {
            del = psub->next;
            psub->next = psub->next->next;
            parent->sub_cnt--;
            // free_nodelist(del);
            return;
        }
        psub = psub->next;
    }
}

void add_csrlist(RTNode *csr)
{
    extern pthread_mutex_t csr_lock;
#if MONO_THREAD == 0
    pthread_mutex_lock(&csr_lock);
#endif
    NodeList *pnode = rt->csr_list;
    if (!pnode)
    {
        rt->csr_list = calloc(sizeof(NodeList), 1);
        rt->csr_list->rtnode = csr;
        rt->csr_list->uri = strdup(csr->uri);
        rt->csr_list->next = NULL;
#if MONO_THREAD == 0
        pthread_mutex_unlock(&csr_lock);
#endif
        return;
    }
    while (pnode->next)
    {
        pnode = pnode->next;
    }
    pnode->next = calloc(sizeof(NodeList), 1);
    pnode->next->rtnode = csr;
    pnode->next->uri = csr->uri;
    pnode->next->next = NULL;
#if MONO_THREAD == 0
    pthread_mutex_unlock(&csr_lock);
#endif
}

void detach_csrlist(RTNode *csr)
{
    NodeList *pnode = rt->csr_list;
    if (!pnode)
    {
        return;
    }
    if (pnode->rtnode == csr)
    {
        rt->csr_list = pnode->next;
        free_nodelist(pnode);
        return;
    }
    while (pnode->next)
    {
        if (pnode->next->rtnode == csr)
        {
            NodeList *temp = pnode->next;
            pnode->next = pnode->next->next;
            free_nodelist(temp);
            temp = NULL;
            return;
        }
        pnode = pnode->next;
    }
}

void free_all_resource(RTNode *rtnode)
{
    RTNode *del;
    while (rtnode)
    {
        del = rtnode;
        if (rtnode->child)
            free_all_resource(rtnode->child);
        rtnode = rtnode->sibling_right;
        free_rtnode(del);
        del = NULL;
    }
}