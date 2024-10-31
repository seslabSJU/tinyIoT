#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <sys/timeb.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "onem2m.h"
#include "util.h"
#include "logger.h"
#include "httpd.h"
#include "onem2mTypes.h"
#include "config.h"
#include "dbmanager.h"
#include "jsonparser.h"
#include "mqttClient.h"
#include "coap.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;
extern pthread_mutex_t main_lock;

/**
 * @brief get uri of resource with ri
 * @param ri resource identifier
 * @return uri or NULL
 */
char *ri_to_uri(char *ri)
{
	char *uri = NULL;
	RTNode *rtnode = find_rtnode_by_ri(ri);

	if (rtnode)
	{
		uri = rtnode->uri;
	}

	return uri;
}

int net_to_bit(cJSON *net)
{
	int ret = 0;
	cJSON *pjson = NULL;

	cJSON_ArrayForEach(pjson, net)
	{
		int exp = atoi(pjson->valuestring);
		if (exp > 0)
			ret = (ret | (int)pow(2, exp - 1));
	}

	return ret;
}

// TODO - move to http.c
ResourceType http_parse_object_type(header_t *headers)
{
	char *content_type = search_header(headers, "content-type");
	if (!content_type)
		return RT_MIXED;
	char *str_ty = strstr(content_type, "ty=");
	if (!str_ty)
		return RT_MIXED;
	int object_type = atoi(str_ty + 3);

	ResourceType ty;

	switch (object_type)
	{
	case 1:
		ty = RT_ACP;
		break;
	case 2:
		ty = RT_AE;
		break;
	case 3:
		ty = RT_CNT;
		break;
	case 4:
		ty = RT_CIN;
		break;
	case 5:
		ty = RT_CSE;
		break;
	case 9:
		ty = RT_GRP;
		break;
	case 16:
		ty = RT_CSR;
		break;
	case 23:
		ty = RT_SUB;
		break;
	case 10002:
		ty = RT_AEA;
		break;
	case 10005:
		ty = RT_CBA;
		break;
	default:
		ty = RT_MIXED;
		break;
	}

	return ty;
}

// TODO - move to coap?
ResourceType coap_parse_object_type(int object_type)
{
	ResourceType ty;

	switch (object_type)
	{
	case 1:
	case 49:
		ty = RT_ACP;
		break;
	case 2:
	case 50:
		ty = RT_AE;
		break;
	case 3:
	case 51:
		ty = RT_CNT;
		break;
	case 4:
	case 52:
		ty = RT_CIN;
		break;
	case 5:
	case 53:
		ty = RT_CSE;
		break;
	case 9:
	case 57:
		ty = RT_GRP;
		break;
	case 16:
	case 64:
		ty = RT_CSR;
		break;
	case 23:
	case 71:
		ty = RT_SUB;
		break;
	default:
		ty = RT_MIXED;
		break;
	}

	return ty;
}

char *get_local_time(int diff)
{
	time_t t = time(NULL) - diff;
	struct tm tm = *localtime(&t);
	struct timespec specific_time;
	// int millsec;
	clock_gettime(0, &specific_time);

	char year[5], mon[3], day[3], hour[3], minute[3], sec[3], millsec[7];

	sprintf(year, "%d", tm.tm_year + 1900);
	sprintf(mon, "%02d", tm.tm_mon + 1);
	sprintf(day, "%02d", tm.tm_mday);
	sprintf(hour, "%02d", tm.tm_hour);
	sprintf(minute, "%02d", tm.tm_min);
	sprintf(sec, "%02d", tm.tm_sec);
	// sprintf(millsec, "%03d", (int) floor(specific_time.tv_nsec/1.0e6));

	char *local_time = (char *)malloc(25 * sizeof(char));

	*local_time = '\0';
	strcat(local_time, year);
	strcat(local_time, mon);
	strcat(local_time, day);
	strcat(local_time, "T");
	strcat(local_time, hour);
	strcat(local_time, minute);
	strcat(local_time, sec);
	// strcat(local_time,",");
	// strcat(local_time,millsec);

	return local_time;
}

char *get_resource_key(ResourceType ty)
{
	char *key = NULL;
	switch (ty)
	{
	case RT_CSE:
		key = "m2m:cb";
		break;
	case RT_AE:
		key = "m2m:ae";
		break;
	case RT_CNT:
		key = "m2m:cnt";
		break;
	case RT_CIN:
		key = "m2m:cin";
		break;
	case RT_SUB:
		key = "m2m:sub";
		break;
	case RT_GRP:
		key = "m2m:grp";
		break;
	case RT_ACP:
		key = "m2m:acp";
		break;
	case RT_CSR:
		key = "m2m:csr";
		break;
	case RT_ACPA:
		key = "m2m:acpA";
		break;
	case RT_AEA:
		key = "m2m:aeA";
		break;
	case RT_CNTA:
		key = "m2m:cntA";
		break;
	case RT_CINA:
		key = "m2m:cinA";
		break;
	case RT_CBA:
		key = "m2m:cbA";
		break;
	default:
		key = "general";
		break;
	}
	return key;
}

void set_o2pt_rsc(oneM2MPrimitive *o2pt, int rsc)
{
	o2pt->rsc = rsc;
}

int is_json_valid_char(char c)
{
	return (('!' <= c && c <= '~') || c == ' ');
}

ResourceType parse_object_type_cjson(cJSON *cjson)
{
	ResourceType ty;

	if (!cjson)
		return RT_MIXED;

	if (cJSON_GetObjectItem(cjson, "m2m:cb") || cJSON_GetObjectItem(cjson, "m2m:cse"))
		ty = RT_CSE;
	else if (cJSON_GetObjectItem(cjson, "m2m:ae"))
		ty = RT_AE;
	else if (cJSON_GetObjectItem(cjson, "m2m:cnt"))
		ty = RT_CNT;
	else if (cJSON_GetObjectItem(cjson, "m2m:cin"))
		ty = RT_CIN;
	else if (cJSON_GetObjectItem(cjson, "m2m:sub"))
		ty = RT_SUB;
	else if (cJSON_GetObjectItem(cjson, "m2m:grp"))
		ty = RT_GRP;
	else if (cJSON_GetObjectItem(cjson, "m2m:acp"))
		ty = RT_ACP;
	else if (cJSON_GetObjectItem(cjson, "m2m:csr"))
		ty = RT_CSR;
	else if (cJSON_GetObjectItem(cjson, "m2m:cba"))
		ty = RT_CBA;
	else if (cJSON_GetObjectItem(cjson, "m2m:aea"))
		ty = RT_AEA;
	else
		ty = RT_MIXED;

	return ty;
}

char *resource_identifier(ResourceType ty, char *ct)
{
	char *ri = (char *)calloc(32, sizeof(char));

	switch (ty)
	{
	case RT_AE:
		strcpy(ri, "CAE");
		break;
	case RT_CNT:
		strcpy(ri, "3-");
		break;
	case RT_CIN:
		strcpy(ri, "4-");
		break;
	case RT_SUB:
		strcpy(ri, "23-");
		break;
	case RT_ACP:
		strcpy(ri, "1-");
		break;
	case RT_GRP:
		strcpy(ri, "9-");
		break;
	case RT_CSR:
		strcpy(ri, "16-");
		break;
	case RT_CBA:
		strcpy(ri, "10005-");
		break;
	case RT_AEA:
		strcpy(ri, "10002-");
		break;
	case RT_CNTA:
		strcpy(ri, "10003-");
		break;
	case RT_CINA:
		strcpy(ri, "10004-");
		break;
	}

	// struct timespec specific_time;
	// int millsec;
	static int n = 0;

	char buf[32] = "\0";

	// clock_gettime(CLOCK_REALTIME, &specific_time);
	// millsec = floor(specific_time.tv_nsec/1.0e6);

	sprintf(buf, "%s%04d", ct, n);
	n = (n + 1) % 10000;

	strcat(ri, buf);

	return ri;
}

void delete_cin_under_cnt_mni_mbs(RTNode *rtnode)
{
	logger("UTIL", LOG_LEVEL_DEBUG, "call delete_cin_under_cnt_mni_mbs");
	cJSON *cnt = rtnode->obj;
	cJSON *cni_obj = NULL;
	cJSON *cbs_obj = NULL;
	cJSON *mni_obj = NULL;
	cJSON *mbs_obj = NULL;
	int cni, mni, cbs, mbs, tmp = 0;

	if ((cni_obj = cJSON_GetObjectItem(cnt, "cni")))
	{
		cni = cni_obj->valueint;
	}
	if ((mni_obj = cJSON_GetObjectItem(cnt, "mni")))
	{
		mni = mni_obj->valueint;
	}
	else
	{
		mni = DEFAULT_MAX_NR_INSTANCES;
	}
	if ((cbs_obj = cJSON_GetObjectItem(cnt, "cbs")))
	{
		cbs = cbs_obj->valueint;
	}
	if ((mbs_obj = cJSON_GetObjectItem(cnt, "mbs")))
	{
		mbs = mbs_obj->valueint;
	}
	else
	{
		mbs = DEFAULT_MAX_BYTE_SIZE;
	}
	if (cni <= mni && cbs <= mbs)
		return;

	if (cni == mni + 1)
	{
		tmp = db_delete_one_cin_mni(rtnode);
		if (tmp == -1)
		{
			logger("UTIL", LOG_LEVEL_ERROR, "delete_cin_under_cnt_mni_mbs error");
		}
		cbs -= tmp;
		cni--;
	}

	if (cni > mni || cbs > mbs)
	{
		RTNode *head = db_get_cin_rtnode_list(rtnode);
		RTNode *right;

		while ((mni >= 0 && cni > mni) || (mbs >= 0 && cbs > mbs))
		{
			if (head)
			{
				right = head->sibling_right;
				db_delete_onem2m_resource(head);
				cbs -= cJSON_GetObjectItem(head->obj, "cs")->valueint;
				cni--;
				free_rtnode(head);
				head = right;
			}
			else
			{
				break;
			}
		}
		if (head)
			free_rtnode_list(head);
	}

	if (cni_obj->valueint != cni)
	{
		cJSON_SetIntValue(cni_obj, cni);
	}
	if (cbs_obj->valueint != cbs)
	{
		cJSON_SetIntValue(cbs_obj, cbs);
	}
}

/**
 * @brief build rcn RCN_CHILD_RESOURCES
 * @param o2pt oneM2MPrimitive
 * @param rtnode target resource tree node
 * @param result_obj cJSON object
 * @remark Internally call rcn4 with offset 1
 */
void build_rcn8(oneM2MPrimitive *o2pt, RTNode *rtnode, cJSON *result_obj, int ofst, int lim, int level)
{
	RTNode *child = rtnode->child;
	cJSON *target = cJSON_GetObjectItem(result_obj, get_resource_key(rtnode->ty));
	if (!target)
		target = cJSON_AddObjectToObject(result_obj, get_resource_key(rtnode->ty));
	while (child)
	{
		if (child->ty != RT_CIN)
		{
			build_child_structure(o2pt, child, target, &ofst, &lim, level - 1);
		}
		child = child->sibling_right;
	}
	logger("UTIL", LOG_LEVEL_DEBUG, "ofst : %d, lim : %d", ofst, lim);
	if (lim < 0)
	{
		o2pt->cnst = CS_PARTIAL_CONTENT;
	}
}

void build_rcn6(oneM2MPrimitive *o2pt, RTNode *rtnode, cJSON *result_obj, int ofst, int lim, int level)
{
	cJSON *root = cJSON_AddObjectToObject(result_obj, "m2m:rrl");
	cJSON *target = cJSON_AddArrayToObject(root, "rrf");
	get_child_references(o2pt, rtnode, target, &ofst, &lim, level);
}

/**
 * @brief build rcn RCN_ATTRIBTUES_AND_CHILD_RESOURCE_REFERENCE
 * @param o2pt oneM2MPrimitive
 * @param rtnode target resource tree node
 * @param result_obj cJSON object
 * @param offset offset
 * @param limit limit
 * @remark build json with rcn 5
 */
void build_rcn5(oneM2MPrimitive *o2pt, RTNode *rtnode, cJSON *result_obj, int ofst, int lim, int level)
{
	cJSON_AddItemToObject(result_obj, get_resource_key(rtnode->ty), cJSON_Duplicate(rtnode->obj, true));
	cJSON *target = cJSON_GetObjectItem(result_obj, get_resource_key(rtnode->ty));
	target = cJSON_AddArrayToObject(target, "ch");
	get_child_references(o2pt, rtnode, target, &ofst, &lim, level - 1);
}

/**
 * @brief build rcn RCN_ATTRIBUTE_AND_CHILD_RESOURCES
 * @param o2pt oneM2MPrimitive
 * @param rtnode target resource tree node
 * @param result_obj cJSON object
 * @param offset offset
 * @param limit limit
 * @remark build json with rcn 4
 */
void build_rcn4(oneM2MPrimitive *o2pt, RTNode *rtnode, cJSON *result_obj, int ofst, int lim, int level)
{
	cJSON_AddItemReferenceToObject(result_obj, get_resource_key(rtnode->ty), cJSON_Duplicate(rtnode->obj, true));
	build_rcn8(o2pt, rtnode, result_obj, ofst, lim, level - 1);
}

/**
 * @brief get child references
 * @param o2pt oneM2MPrimitive
 * @param rtnode target resource tree node
 * @param result_obj cJSON Array to store child references
 * @param offset offset
 * @param limit limit
 * @param level level
 */
void get_child_references(oneM2MPrimitive *o2pt, RTNode *rtnode, cJSON *result_obj, int *ofst, int *lim, int level)
{
	RTNode *child = rtnode->child;
	cJSON *root = NULL;
	logger("UTIL", LOG_LEVEL_DEBUG, "get_child_references %s", rtnode->uri);
	while (child)
	{
		if (child->ty != RT_CIN)
		{
			if (*ofst <= 0 && *lim > 0)
			{
				if (isResourceAptFC(o2pt, child, o2pt->fc))
				{
					root = cJSON_CreateObject();
					cJSON_AddStringToObject(root, "nm", child->rn);
					cJSON_AddNumberToObject(root, "typ", child->ty);
					if (o2pt->drt == DRT_STRUCTURED)
					{
						cJSON_AddStringToObject(root, "val", child->uri);
					}
					else
					{
						cJSON_AddStringToObject(root, "val", cJSON_GetObjectItem(child->obj, "ri")->valuestring);
					}
					cJSON_AddItemToArray(result_obj, root);
				}
				else
				{
					*ofst -= 1;
				}
			}
		}
		if (child->ty == RT_CNT)
		{
			RTNode *cin_list_head = db_get_cin_rtnode_list(child);

			RTNode *cin = cin_list_head;

			while (cin)
			{
				if (*ofst <= 0 && *lim > 0)
				{
					if (isResourceAptFC(o2pt, cin, o2pt->fc))
					{
						root = cJSON_CreateObject();
						cJSON_AddStringToObject(root, "nm", cin->rn);
						cJSON_AddNumberToObject(root, "typ", cin->ty);
						if (o2pt->drt == DRT_STRUCTURED)
						{
							cJSON_AddStringToObject(root, "val", child->uri);
						}
						else
						{
							cJSON_AddStringToObject(root, "val", cJSON_GetObjectItem(child->obj, "ri")->valuestring);
						}
						cJSON_AddItemToArray(result_obj, root);
					}
					else
					{
						*ofst -= 1;
					}
				}
				cin = cin->sibling_right;
			}
			free_rtnode_list(cin_list_head);
		}
		get_child_references(o2pt, child, result_obj, ofst, lim, level - 1);
		child = child->sibling_right;
	}
}

void build_child_structure(oneM2MPrimitive *o2pt, RTNode *rtnode, cJSON *result_obj, int *ofst, int *lim, int level)
{
	// logger("UTIL", LOG_LEVEL_DEBUG, "build_child_structure %s", rtnode->uri);
	cJSON *pjson = NULL, *target = NULL;
	if (level <= 0)
		return;
	if (*lim <= 0)
	{
		*lim = -1;
		return;
	}
	if (*lim > 0)
	{
		if (isResourceAptFC(o2pt, rtnode, o2pt->fc))
		{
			if (*ofst > 0)
			{
				*ofst -= 1;
			}
			else
			{
				// logger("UTIL", LOG_LEVEL_DEBUG, "isResourceAptFC : %s", rtnode->uri);
				target = cJSON_Duplicate(rtnode->obj, true);
				if (pjson = cJSON_GetObjectItem(result_obj, get_resource_key(rtnode->ty)))
				{
					cJSON_AddItemToArray(pjson, target);
				}
				else
				{
					pjson = cJSON_CreateArray();
					cJSON_AddItemToArray(pjson, target);
					cJSON_AddItemToObject(result_obj, get_resource_key(rtnode->ty), pjson);
				}
				*lim -= 1;
			}
		}
	}
	if (*lim > 0 && level > 0 && rtnode->ty == RT_CNT)
	{
		RTNode *cin_list_head = db_get_cin_rtnode_list(rtnode);

		RTNode *cin = cin_list_head;

		while (cin)
		{
			if (*ofst <= 0 && *lim > 0)
			{
				if (isResourceAptFC(o2pt, cin, o2pt->fc))
				{
					if (*ofst > 0)
					{
						*ofst -= 1;
					}
					else
					{
						if (pjson = cJSON_GetObjectItem(target ? target : result_obj, get_resource_key(RT_CIN)))
						{
							cJSON_AddItemToArray(pjson, cJSON_Duplicate(cin->obj, true));
						}
						else
						{
							pjson = cJSON_CreateArray();
							cJSON_AddItemToArray(pjson, cJSON_Duplicate(cin->obj, true));
							cJSON_AddItemToObject(target ? target : result_obj, get_resource_key(cin->ty), pjson);
						}
						*lim -= 1;
					}
				}
			}
			cin = cin->sibling_right;
		}
		free_rtnode_list(cin_list_head);
	}

	RTNode *child = rtnode->child;
	while (child)
	{
		if (child->ty == RT_CIN)
		{
			child = child->sibling_right;
			continue;
		}
		if (target)
		{
			build_child_structure(o2pt, child, target, ofst, lim, level - 1);
		}
		else
		{
			build_child_structure(o2pt, child, result_obj, ofst, lim, level - 1);
		}
		child = child->sibling_right;
	}
}

RTNode *latest_cin_list(RTNode *cinList, int num)
{
	RTNode *head, *tail;
	head = tail = cinList;
	int cnt = 1;

	while (tail->sibling_right)
	{
		tail = tail->sibling_right;
		cnt++;
	}

	for (int i = 0; i < cnt - num; i++)
	{
		head = head->sibling_right;
		free_rtnode(head->sibling_left);
		head->sibling_left = NULL;
	}

	return head;
}

void log_runtime(double start)
{
	double end = (((double)clock()) / CLOCKS_PER_SEC); // runtime check - end
	logger("UTIL", LOG_LEVEL_INFO, "Run time : %lf", end - start);
}

bool init_server()
{
	bool setup = false;
	char poa[128] = {0};

	rt = (ResourceTree *)calloc(1, sizeof(ResourceTree));

	cJSON *cse, *acp;

	cse = db_get_resource(CSE_BASE_RI, RT_CSE);

	if (!cse)
	{
		setup = true;
		cse = cJSON_CreateObject();
		init_cse(cse);
		db_store_resource(cse, CSE_BASE_NAME);
	}
	else
	{
		cJSON *rr = cJSON_GetObjectItem(cse, "rr");
		if (rr->valueint == 1)
			rr->type = cJSON_True;
		else
			rr->type = cJSON_False;
		// cJSON_SetBoolValue(cJSON_GetObjectItem(cse, "rr"), false);
	}
	cJSON *poa_obj = cJSON_CreateArray();

#ifdef NIC_NAME
	struct ifreq ifr;
	char ipstr[40];
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	strncpy(ifr.ifr_name, NIC_NAME, IFNAMSIZ);

	if (ioctl(s, SIOCGIFADDR, &ifr) < 0)
	{
		logger("UTIL", LOG_LEVEL_ERROR, "NIC_NAME is invalid, using SERVER_IP");
		sprintf(ipstr, "%s", SERVER_IP);
	}
	else
	{
		inet_ntop(AF_INET, ifr.ifr_addr.sa_data + 2, ipstr, sizeof(struct sockaddr));
		logger("UTIL", LOG_LEVEL_INFO, "NIC IP : %s", ipstr);
	}
	close(s);
	sprintf(poa, "http://%s:%s", ipstr, SERVER_PORT);
	cJSON_AddItemToArray(poa_obj, cJSON_CreateString(poa));
#endif
	sprintf(poa, "http://%s:%s", SERVER_IP, SERVER_PORT);
	cJSON_AddItemToArray(poa_obj, cJSON_CreateString(poa));

#ifdef ENABLE_MQTT
	sprintf(poa, "mqtt://%s:%d", SERVER_IP, MQTT_PORT);
	cJSON_AddItemToArray(poa_obj, cJSON_CreateString(poa));
#endif

#ifdef ENABLE_COAP
#ifdef ENABLE_COAP_DTLS
	sprintf(poa, "coaps://%s:%d", SERVER_IP, COAP_PORT);
#else
	sprintf(poa, "coap://%s:%d", SERVER_IP, COAP_PORT);
#endif
	cJSON_AddItemToArray(poa_obj, cJSON_CreateString(poa));
#endif

	cJSON_AddItemToObject(cse, "poa", poa_obj);

	rt->cb = create_rtnode(cse, RT_CSE);
	return setup;
}

int result_parse_uri(oneM2MPrimitive *o2pt, RTNode *rtnode)
{
	if (!rtnode)
	{
		handle_error(o2pt, RSC_NOT_FOUND, "URI is invalid");
		return -1;
	}
	else
	{
		return 0;
	}
}

int check_mandatory_attributes(oneM2MPrimitive *o2pt)
{
	if (o2pt->rvi == RVI_NONE)
	{
		handle_error(o2pt, RSC_BAD_REQUEST, "rvi is mandatory");
		return -1;
	}
	if (!o2pt->fr && o2pt->ty != RT_AE)
	{
		handle_error(o2pt, RSC_BAD_REQUEST, "originator is mandatory");
		return -1;
	}
	return 0;
}

/**
 * @brief check privilege of originator
 * @param o2pt oneM2MPrimitive
 * @param rtnode resource node
 * @param acop access control operation
 * @return 0 if success, -1 if fail
 */
int check_privilege(oneM2MPrimitive *o2pt, RTNode *rtnode, ACOP acop)
{
	logger("UTIL", LOG_LEVEL_DEBUG, "check_privilege : %s : %d", o2pt->fr, acop);
	bool deny = true;
	char *origin = NULL;
	cJSON *acpi = NULL;

	RTNode *target_rtnode = rtnode;
#ifdef ADMIN_AE_ID
	if (o2pt->fr && !strcmp(o2pt->fr, ADMIN_AE_ID))
	{
		logger("UTIL", LOG_LEVEL_DEBUG, "ADMIN_AE_ID : %s", o2pt->fr);
		return 0;
	}
#endif

	if (isSPIDLocal(o2pt->fr))
	{
		origin = get_ri_rtnode(find_rtnode(o2pt->fr));
		if (!origin)
			origin = o2pt->fr;
	}
	else
	{
		origin = o2pt->fr;
	}
	// if target is AE, check ri of resource
	if (rtnode->ty == RT_AE)
	{
		if (!strcmp(origin, get_ri_rtnode(target_rtnode)))
		{
			logger("UTIL", LOG_LEVEL_DEBUG, "originator is the owner");
			return 0;
		}
	}
	// if target is CSR, check csi of resource
	if (rtnode->ty == RT_CSR)
	{
		if (!strcmp(origin, cJSON_GetObjectItem(target_rtnode->obj, "csi")->valuestring))
		{
			logger("UTIL", LOG_LEVEL_DEBUG, "originator is the owner");
			return 0;
		}
	}

	// if resource is not an AE|CSR, find acpi for all parent nodes
	while (target_rtnode->parent && cJSON_GetObjectItem(target_rtnode->obj, "acpi") == NULL)
	{
		target_rtnode = target_rtnode->parent;
	}

	if (o2pt->fr == NULL || strlen(o2pt->fr) == 0 || strcmp(o2pt->fr, "C") == 0)
	{
		if ((o2pt->op == OP_CREATE && o2pt->ty == RT_AE))
		{
			return 0;
		}
	}

	if ((get_acop(o2pt, origin, target_rtnode) & acop) == acop)
	{
		deny = false;
	}

	if (deny)
	{
		handle_error(o2pt, RSC_ORIGINATOR_HAS_NO_PRIVILEGE, "originator has no privilege");
		return -1;
	}

	return false;
}

int check_macp_privilege(oneM2MPrimitive *o2pt, RTNode *rtnode, ACOP acop)
{
	bool deny = false;
	if (!o2pt->fr)
	{
		deny = true;
	}
#ifdef ADMIN_AE_ID
	else if (!strcmp(o2pt->fr, ADMIN_AE_ID))
	{
		return false;
	}
#endif
	else if (strcmp(o2pt->fr, get_ri_rtnode(rtnode)))
	{
		deny = true;
	}

	cJSON *macp = cJSON_GetObjectItem(rtnode->obj, "macp");
	if (macp && cJSON_GetArraySize(macp) > 0)
	{
		deny = true;
		if ((get_acop_macp(o2pt, rtnode) & acop) == acop)
		{
			deny = false;
		}
	}
	else
	{
		deny = false;
	}

	if (deny)
	{
		handle_error(o2pt, RSC_ORIGINATOR_HAS_NO_PRIVILEGE, "originator has no privilege");
		return -1;
	}

	return 0;
}

int get_acop(oneM2MPrimitive *o2pt, char *corigin, RTNode *rtnode)
{
	int acop = 0;

#ifdef ADMIN_AE_ID
	if (!strcmp(corigin, ADMIN_AE_ID))
	{
		return ALL_ACOP;
	}
#endif

	if (rtnode->ty == RT_ACP)
	{
		acop = (acop | get_acop_origin(o2pt, corigin, rtnode, 1));
		return acop;
	}

	cJSON *acpiArr = get_acpi_rtnode(rtnode);
	if (!acpiArr)
		return 0;
	logger("UTIL", LOG_LEVEL_DEBUG, "get_acop : %s", rtnode->uri);

	cJSON *acpi = NULL;
	cJSON_ArrayForEach(acpi, acpiArr)
	{
		RTNode *acp = find_rtnode(acpi->valuestring);
		if (acp)
		{
			acop = (acop | get_acop_origin(o2pt, corigin, acp, 0));
		}
	}
	return acop;
}

int get_acop_macp(oneM2MPrimitive *o2pt, RTNode *rtnode)
{
	int acop = 0;
	logger("UTIL", LOG_LEVEL_DEBUG, "get_acop_macp : %s", o2pt->fr);
	char *origin = NULL;
	if (!o2pt->fr)
	{
		origin = strdup("all");
	}
	else
	{
		origin = strdup(o2pt->fr);
	}
#ifdef ADMIN_AE_ID
	if (!strcmp(origin, ADMIN_AE_ID))
	{
		free(origin);
		return ALL_ACOP;
	}
#endif

	cJSON *macp = cJSON_GetObjectItem(rtnode->obj, "macp");
	if (!macp)
		return 0;

	RTNode *cb = rtnode;
	while (cb->parent)
		cb = cb->parent;
	cJSON *acpi = NULL;
	cJSON_ArrayForEach(acpi, macp)
	{

		RTNode *acp = find_rtnode(acpi->valuestring);
		if (acp)
		{
			acop = (acop | get_acop_origin(o2pt, origin, acp, 0));
		}
	}

	free(origin);
	return acop;
}

int check_acco(cJSON *accos, char *ip)
{
	cJSON *acco = NULL;
	if (!accos)
		return 1;
	if (cJSON_GetArraySize(accos) == 0)
		return 1;
	if (!ip)
		return 1;
	cJSON *pjson = NULL;
	char *ip_str = NULL;
	int res = 0;
	struct in_addr addr, addr2;
	char *subnet_ptr;
	int mask = 0xFFFFFFFF;
	cJSON *acip;
	cJSON *ipv4;
	cJSON_ArrayForEach(acco, accos)
	{
		acip = cJSON_GetObjectItem(acco, "acip");
		ipv4 = cJSON_GetObjectItem(acip, "ipv4");

		cJSON_ArrayForEach(pjson, ipv4)
		{
			ip_str = strdup(pjson->valuestring);
			mask = 1;

			subnet_ptr = strchr(ip_str, '/');

			if (subnet_ptr)
			{
				subnet_ptr[0] = '\0';
				subnet_ptr++;
				for (int i = 0; i < atoi(subnet_ptr) - 1; i++)
				{
					mask = mask << 1;
					mask = mask | 1;
				}

				res = inet_pton(AF_INET, ip_str, &addr);
				if (res == 0)
				{
					logger("UTIL", LOG_LEVEL_DEBUG, "check_acco/ipv4 : %s is not valid ipv4 address", ip_str);
					free(ip_str);
					ip_str = NULL;
					continue;
				}
				else if (res == -1)
				{
					logger("UTIL", LOG_LEVEL_DEBUG, "check_acco/ipv4 : inet_pton error");
					free(ip_str);
					ip_str = NULL;
					continue;
				}

				res = inet_pton(AF_INET, ip, &addr2);
				if (res == 0)
				{
					logger("UTIL", LOG_LEVEL_DEBUG, "check_acco/ipv4 : %s is not valid ipv4 address", ip);
					free(ip_str);
					ip_str = NULL;
					continue;
				}
				else if (res == -1)
				{
					logger("UTIL", LOG_LEVEL_DEBUG, "check_acco/ipv4 : inet_pton error");
					free(ip_str);
					ip_str = NULL;
					continue;
				}
				logger("UTIL", LOG_LEVEL_DEBUG, "addr & mask : %X, addr2 & mask : %X", (addr.s_addr & mask), (addr2.s_addr & mask));

				if ((addr.s_addr & mask) == (addr2.s_addr & mask))
				{
					free(ip_str);
					return 1;
				}
			}
			else
			{
				if (!strcmp(ip_str, ip))
				{
					free(ip_str);
					return 1;
				}
			}

			free(ip_str);
		}
	}
	return 0;
}

/**
 * @brief get acop from acp
 * @param o2pt oneM2MPrimitive
 * @param acp_rtnode acp resource node
 * @param flag 1 : pvs, 0 : pv
 * @return acop
 */
int get_acop_origin(oneM2MPrimitive *o2pt, char *origin, RTNode *acp_rtnode, int flag)
{
	int ret_acop = 0, cnt = 0;
	cJSON *acp = acp_rtnode->obj;

	cJSON *privilege = NULL;
	cJSON *acr = NULL;
	cJSON *acor = NULL;
	bool found = false;
	char *asterisk = NULL;

#ifdef ADMIN_AE_ID
	if (origin && !strcmp(origin, ADMIN_AE_ID))
	{
		return ALL_ACOP;
	}
#endif

	if (flag)
	{
		privilege = cJSON_GetObjectItem(acp, "pvs");
		logger("UTIL", LOG_LEVEL_DEBUG, "pvs");
	}
	else
	{
		privilege = cJSON_GetObjectItem(acp, "pv");
		logger("UTIL", LOG_LEVEL_DEBUG, "pv");
	}

	char *acor_ptr = NULL;

	cJSON_ArrayForEach(acr, cJSON_GetObjectItem(privilege, "acr"))
	{
		cJSON_ArrayForEach(acor, cJSON_GetObjectItem(acr, "acor"))
		{
			acor_ptr = acor->valuestring;
			if (strncmp(acor_ptr, "/" CSE_BASE_RI "/", strlen(CSE_BASE_RI) + 2) == 0)
			{
				acor_ptr += strlen(CSE_BASE_RI) + 2;
			}
			if ((asterisk = strchr(acor_ptr, '*')))
			{
				if (!strncmp(acor_ptr, origin, asterisk - acor_ptr) && !strcmp(asterisk + 1, origin + strlen(origin) - strlen(asterisk + 1)))
				{
					if (check_acco(cJSON_GetObjectItem(acr, "acco"), o2pt->ip))
					{
						ret_acop = (ret_acop | cJSON_GetObjectItem(acr, "acop")->valueint);
					}
					break;
				}
			}
			else if (!strcmp(acor_ptr, origin))
			{
				if (check_acco(cJSON_GetObjectItem(acr, "acco"), o2pt->ip))
				{
					ret_acop = (ret_acop | cJSON_GetObjectItem(acr, "acop")->valueint);
				}
				break;
			}
			else if (!strcmp(acor_ptr, "all"))
			{
				if (check_acco(cJSON_GetObjectItem(acr, "acco"), o2pt->ip))
				{
					ret_acop = (ret_acop | cJSON_GetObjectItem(acr, "acop")->valueint);
				}
				break;
			}
		}
	}
	logger("UTIL", LOG_LEVEL_DEBUG, "get_acop_origin [%s]: %d", origin, ret_acop);
	return ret_acop;
}

int has_privilege(oneM2MPrimitive *o2pt, char *acpi, ACOP acop)
{
	char *origin = o2pt->fr;
	if (!origin)
		return 0;
	if (!acpi)
		return 1;

	RTNode *acp = find_rtnode(acpi);
	int result = get_acop_origin(o2pt, origin, acp, 0);
	if ((result & acop) == acop)
	{
		return 1;
	}
	return 0;
}

int has_acpi_update_privilege(oneM2MPrimitive *o2pt, char *acpi)
{
	logger("UTIL", LOG_LEVEL_DEBUG, "has_acpi_update_privilege : %s", acpi);
	char *origin = o2pt->fr;
	if (!origin)
		return 0;
	if (!acpi)
		return 1;

	RTNode *acp = find_rtnode(acpi);
	int result = get_acop_origin(o2pt, origin, acp, 1);
	if ((result & ACOP_UPDATE) == ACOP_UPDATE)
	{
		return 1;
	}
	return 0;
}

int check_rn_duplicate(oneM2MPrimitive *o2pt, RTNode *rtnode)
{
	if (!rtnode)
		return 0;
	cJSON *root = o2pt->request_pc;
	cJSON *resource, *rn;

	resource = getResource(root, o2pt->ty);

#if MONO_THREAD == 0
	pthread_mutex_lock(&main_lock);
#endif
	RTNode *child = rtnode->child;
	bool flag = false;

	rn = cJSON_GetObjectItem(resource, "rn");
	if (rn)
	{
		char *resource_name = rn->valuestring;
		while (child)
		{
			if (!strcmp(child->rn, resource_name))
			{
				flag = true;
				break;
			}
			child = child->sibling_right;
		}
#if ALLOW_CIN_RN
		if (o2pt->ty == RT_CIN)
		{
			if (db_check_cin_rn_dup(rn->valuestring, cJSON_GetObjectItem(rtnode->obj, "ri")->valuestring))
			{
				flag = true;
			}
		}
#endif
	}
#if MONO_THREAD == 0
	pthread_mutex_unlock(&main_lock);
#endif

	if (flag)
	{
		handle_error(o2pt, RSC_CONFLICT, "attribute `rn` is duplicated");
		return -1;
	}

	return 0;
}

int check_aei_duplicate(oneM2MPrimitive *o2pt, RTNode *rtnode)
{
	if (!rtnode)
		return 0;

	char aei[1024] = {
		'\0',
	};
	if (!o2pt->fr)
	{
		return 0;
	}

	strcat(aei, o2pt->fr);
	logger("UTIL", LOG_LEVEL_DEBUG, "check_aei_duplicate : %s", aei);
#if MONO_THREAD == 0
	pthread_mutex_lock(&main_lock);
#endif
	RTNode *child = rtnode->child;

	while (child)
	{
		if (child->ty != RT_AE)
		{
			child = child->sibling_right;
			continue;
		}
		if (!strcmp(get_ri_rtnode(child), aei))
		{
			handle_error(o2pt, RSC_ORIGINATOR_HAS_ALREADY_REGISTERD, "attribute `aei` is duplicated");
#if MONO_THREAD == 0
			pthread_mutex_unlock(&main_lock);
#endif
			return -1;
		}
		child = child->sibling_right;
	}
#if MONO_THREAD == 0
	pthread_mutex_unlock(&main_lock);
#endif

	return 0;
}

int check_csi_duplicate(char *new_csi, RTNode *rtnode)
{
	if (!rtnode || new_csi == NULL)
		return 0;

#if MONO_THREAD == 0
	pthread_mutex_lock(&main_lock);
#endif
	RTNode *child = rtnode->child;

	while (child)
	{
		if (!strcmp(get_ri_rtnode(child), new_csi))
		{
#if MONO_THREAD == 0
			pthread_mutex_unlock(&main_lock);
#endif
			return -1;
		}
		child = child->sibling_right;
	}
#if MONO_THREAD == 0
	pthread_mutex_unlock(&main_lock);
#endif

	return 0;
}

int check_payload_empty(oneM2MPrimitive *o2pt)
{
	if (!o2pt->request_pc)
	{
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "payload is empty");
		return -1;
	}
	return 0;
}

int check_rn_invalid(oneM2MPrimitive *o2pt, ResourceType ty)
{
	cJSON *root = o2pt->request_pc;
	cJSON *resource, *rn;

	resource = getResource(root, ty);

	rn = cJSON_GetObjectItem(resource, "rn");
	if (!rn)
		return 0;
	char *resource_name = rn->valuestring;
	int len_resource_name = strlen(resource_name);

	for (int i = 0; i < len_resource_name; i++)
	{
		if (!is_rn_valid_char(resource_name[i]))
		{
			handle_error(o2pt, RSC_BAD_REQUEST, "attribute `rn` is invalid");
			return -1;
		}
	}

	return 0;
}

bool is_rn_valid_char(char c)
{
	return ((48 <= c && c <= 57) || (65 <= c && c <= 90) || (97 <= c && c <= 122) || (c == '_' || c == '-'));
}

int check_resource_type_equal(oneM2MPrimitive *o2pt)
{
	if (o2pt->ty != parse_object_type_cjson(o2pt->request_pc))
	{
		handle_error(o2pt, RSC_BAD_REQUEST, "resource type is invalid");
		return -1;
	}
	return 0;
}

int check_resource_type_invalid(oneM2MPrimitive *o2pt)
{
	if (o2pt->ty == RT_MIXED)
	{
		handle_error(o2pt, RSC_BAD_REQUEST, "resource type can't be 0(Mixed)");
		return -1;
	}
	else if (o2pt->ty == RT_CSE)
	{
		handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "resource type can't be 5(CSEBase)");
		return -1;
	}
	return 0;
}

ACOP op_to_acop(Operation op)
{
	switch (op)
	{
	case OP_CREATE:
		return ACOP_CREATE;
	case OP_RETRIEVE:
		return ACOP_RETRIEVE;
	case OP_UPDATE:
		return ACOP_UPDATE;
	case OP_DELETE:
		return ACOP_DELETE;
	case OP_DISCOVERY:
		return ACOP_DISCOVERY;
	case OP_NOTIFY:
		return ACOP_NOTIFY;
	default:
		return 0;
	}
}

RVI to_rvi(char *str)
{
	if (!str)
		return RVI_NONE;
	if (!strcmp(str, "2a"))
	{
		return RVI_2;
	}
	if (!strcmp(str, "3"))
	{
		return RVI_3;
	}
	if (!strcmp(str, "1"))
	{
		return RVI_1;
	}
	if (!strcmp(str, "2"))
	{
		return RVI_2;
	}
	if (!strcmp(str, "4"))
	{
		return RVI_4;
	}
	if (!strcmp(str, "5"))
	{
		return RVI_5;
	}

	return RVI_NONE;
}

char *from_rvi(RVI rvi)
{
	switch (rvi)
	{
	case RVI_1:
		return "1";
	case RVI_2:
		return "2";
	case RVI_2a:
		return "2a";
	case RVI_3:
		return "3";
	case RVI_4:
		return "4";
	case RVI_5:
		return "5";
	default:
		return NULL;
	}
}

/**
 * @brief set rsc and error msg to oneM2MPrimitive
 * @param o2pt oneM2MPrimitive
 * @param rsc error code
 * @param err error message
 * @return error code
 */
int handle_error(oneM2MPrimitive *o2pt, int rsc, char *err)
{
	logger("UTIL", LOG_LEVEL_INFO, err);
	cJSON *root = cJSON_CreateObject();
	o2pt->rsc = rsc;
	o2pt->errFlag = true;
	cJSON_AddItemToObject(root, "m2m:dbg", cJSON_CreateString(err));
	if (o2pt->response_pc)
		cJSON_Delete(o2pt->response_pc);
	o2pt->response_pc = root;
	return rsc;
}

bool isUriFopt(char *str)
{
	char *s;
	if (!str)
		return false;
	s = strrchr(str, '/');
	if (!s)
		return false;
	return !strcmp(s, "/fopt");
}

bool endswith(char *str, char *match)
{
	size_t str_len = 0;
	size_t match_len = 0;
	if (!str || !match)
		return false;

	str_len = strlen(str);
	match_len = strlen(match);

	if (!str_len || !match_len)
		return false;

	return strncmp(str + str_len - match_len, match, match_len);
}

/**
 * @brief make response for each rcn
 * @param o2pt oneM2MPrimitive
 * @param target_rtnode target resource node
 * @return 0 if success, -1 if fail
 */
int make_response_body(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{
	bool limited = false;
	if (o2pt->rcn == RCN_NOTHING)
	{
		return 0;
	}

	cJSON *root = NULL;
	cJSON *pjson = NULL, *pjson2 = NULL, *pjson3 = NULL;
	int lim = DEFAULT_DISCOVERY_LIMIT;
	int ofst = 0;
	int lvl = 99999;
	int rsc = 0;
	RTNode *remote = NULL;

	if ((pjson = cJSON_GetObjectItem(o2pt->fc, "lim")))
	{
		lim = pjson->valueint;
		limited = true;
	}
	if ((pjson = cJSON_GetObjectItem(o2pt->fc, "ofst")))
	{
		ofst = pjson->valueint;
	}
	if ((pjson = cJSON_GetObjectItem(o2pt->fc, "lvl")))
	{
		lvl = pjson->valueint;
	}

	root = cJSON_CreateObject();
	switch (o2pt->rcn)
	{
	case RCN_NOTHING:
		cJSON_Delete(root);
		root = NULL;
		break;
	case RCN_ATTRIBUTES:
		cJSON_AddItemToObject(root, get_resource_key(target_rtnode->ty), cJSON_Duplicate(target_rtnode->obj, true));
		break;
	case RCN_HIERARCHICAL_ADDRESS:
		cJSON_AddItemToObject(root, "m2m:uri", cJSON_CreateString(target_rtnode->uri));
		break;
	case RCN_HIERARCHICAL_ADDRESS_ATTRIBUTES:
		pjson = cJSON_CreateObject();
		cJSON_AddItemReferenceToObject(pjson, get_resource_key(target_rtnode->ty), target_rtnode->obj);
		cJSON_AddItemToObject(pjson, "uri", cJSON_CreateString(target_rtnode->uri));
		cJSON_AddItemToObject(root, "m2m:rce", pjson);
		break;
	case RCN_ATTRIBUTES_AND_CHILD_RESOURCES:
#if MONO_THREAD == 0
		pthread_mutex_lock(&main_lock);
#endif
		build_rcn4(o2pt, target_rtnode, root, ofst, lim, lvl);
#if MONO_THREAD == 0
		pthread_mutex_unlock(&main_lock);
#endif
		break;
	case RCN_ATTRIBUTES_AND_CHILD_RESOURCE_REFERENCES:
		build_rcn5(o2pt, target_rtnode, root, ofst, lim, lvl);
		break;
	case RCN_CHILD_RESOURCE_REFERENCES:
#if MONO_THREAD == 0
		pthread_mutex_lock(&main_lock);
#endif
		build_rcn6(o2pt, target_rtnode, root, ofst, lim, lvl);
#if MONO_THREAD == 0
		pthread_mutex_unlock(&main_lock);
#endif
		break;
	case RCN_ORIGINAL_RESOURCE:
		if (target_rtnode->ty < 10000)
		{
			handle_error(o2pt, RSC_BAD_REQUEST, "rcn 7 is not supported for non anncounced resources");
			break;
		}

		remote = get_remote_resource(cJSON_GetObjectItem(target_rtnode->obj, "lnk")->valuestring, &rsc);
		// logger("UTIL", LOG_LEVEL_DEBUG, "make_response_body : %s", cJSON_GetObjectItem(target_rtnode->obj, "lnk")->valuestring);
		if (remote && rsc == RSC_OK)
		{
			// logger("UTIL", LOG_LEVEL_DEBUG, "make_response_body : %s", remote->uri);
			cJSON_AddItemToObject(root, get_resource_key(remote->ty), cJSON_Duplicate(remote->obj, true));
			free_rtnode(remote);
		}
		else
		{
			cJSON_Delete(root);
			return handle_error(o2pt, rsc, "original resource is not reachable");
		}
		break;
	case RCN_CHILD_RESOURCES:
#if MONO_THREAD == 0
		pthread_mutex_lock(&main_lock);
#endif
		build_rcn8(o2pt, target_rtnode, root, ofst, lim, lvl);
#if MONO_THREAD == 0
		pthread_mutex_unlock(&main_lock);
#endif
		break;
	case RCN_MODIFIED_ATTRIBUTES:
		if (!o2pt->request_pc)
		{
			cJSON_Delete(root);
			return handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "Internal Server Error");
		}
		if (o2pt->op == OP_CREATE)
		{
			cJSON_AddItemReferenceToObject(root, get_resource_key(target_rtnode->ty), target_rtnode->obj);
			pjson2 = cJSON_GetObjectItem(o2pt->request_pc, get_resource_key(target_rtnode->ty));
			pjson3 = cJSON_GetObjectItem(root, get_resource_key(target_rtnode->ty));
			cJSON_ArrayForEach(pjson, pjson2)
			{
				cJSON_DeleteItemFromObject(pjson3, pjson->string);
			}
		}
		else
		{
			cJSON_AddItemReferenceToObject(root, get_resource_key(target_rtnode->ty), o2pt->request_pc);
		}
		break;
	case RCN_SEMANTIC_CONTENT:
		break;
	case RCN_DISCOVERY_RESULT_REFERENCES:
		break;
	case RCN_PERMISSIONS:
		break;
	}

	if (o2pt->response_pc)
	{
		cJSON_Delete(o2pt->response_pc);
	}

	if (root)
	{

		o2pt->response_pc = root;
	}

	if (o2pt->cnst == CS_PARTIAL_CONTENT)
	{
		o2pt->cnot = ofst + lim;
	}
	else
	{
		if (limited)
			o2pt->cnst = CS_FULL_CONTENT;
	}
	return 0;
}

int handle_csy(cJSON *grp, cJSON *mid, int csy, int i)
{
	cJSON *mt = cJSON_GetObjectItem(grp, "mt");
	cJSON *cnm = cJSON_GetObjectItem(grp, "cnm");

	switch (csy)
	{
	case CSY_ABANDON_MEMBER:
		cJSON_DeleteItemFromArray(mid, i);
		cJSON_SetNumberValue(cnm, cnm->valueint - 1);
		break;
	case CSY_ABANDON_GROUP:
		return RSC_GROUPMEMBER_TYPE_INCONSISTENT;

	case CSY_SET_MIXED:
		cJSON_SetNumberValue(mt, RT_MIXED);
		break;
	}
	return 0;
}

bool isMinDup(char **mid, int idx, char *new_mid)
{
	if (!mid)
		return true;
	if (!new_mid)
		return true;

	for (int i = 0; i < idx; i++)
	{
		if (mid[i] != 0 && !strcmp(mid[i], new_mid))
			return true;
	}
	return false;
}

void remove_mid(char **mid, int idx, int cnm)
{
	char *del = mid[idx];
	for (int i = idx; i < cnm - 1; i++)
	{
		mid[i] = mid[i + 1];
	}
	// if(mid[cnm-1]) free(mid[cnm-1]);
	mid[cnm - 1] = NULL;
	if (idx != cnm - 1)
		free(del);
	del = NULL;
}

bool isSPIDLocal(char *address)
{
	if (!address)
		return false;
	if (address[0] != '/' && address[1] != '/')
		return false;
	char *ptr = strchr(address + 2, '/');
	if (ptr)
		*ptr = '\0';
	if (strcmp(address + 2, CSE_BASE_SP_ID) == 0)
	{
		if (ptr)
			*ptr = '/';
		return true;
	}
	if (ptr)
		*ptr = '/';
	return false;
}

bool isSpRelativeLocal(char *address)
{
	if (!address)
		return false;
	if (address[0] != '/')
		return false;
	char *ptr = strchr(address + 1, '/');
	if (ptr)
		*ptr = '\0';
	if (strcmp(address + 1, CSE_BASE_RI) == 0)
	{
		if (ptr)
			*ptr = '/';
		return true;
	}
	if (ptr)
		*ptr = '/';
	return false;
}

int rsc_to_http_status(int rsc, char **msg)
{
	switch (rsc)
	{
	case RSC_OK:
	case RSC_DELETED:
	case RSC_UPDATED:
		*msg = "OK";
		return 200;

	case RSC_CREATED:
		*msg = "Created";
		return 201;

	case RSC_ACCEPTED:
	case RSC_ACCEPTED_NONBLOCKING_REQUEST_SYNCH:
	case RSC_ACCEPTED_NONBLOCKING_REQUEST_ASYNCH:
		*msg = "Accepted";
		return 202;

	case RSC_BAD_REQUEST:
	case RSC_CONTENTS_UNACCEPTABLE:
	case RSC_GROUPMEMBER_TYPE_INCONSISTENT:
	case RSC_INVALID_SEMANTICS:
	case RSC_INVALID_TRIGGER_PURPOSE:
	case RSC_ILLEGAL_TRANSACTION_STATE_TRANSITION_ATTEMPTED:
	case RSC_ONTOLOGY_NOT_AVAILABLE:
	case RSC_INVALID_SPARQL_QUERY:
	case RSC_MAX_NUMBER_OF_MEMBER_EXCEEDED:
	case RSC_INVALID_ARGUMENTS:
	case RSC_INSUFFICIENT_ARGUMENTS:
		*msg = "Bad Request";
		return 400;

	case RSC_SUBSCRIPTION_CREATOR_HAS_NO_PRIVILEGE:
	case RSC_ORIGINATOR_HAS_NO_PRIVILEGE:
	case RSC_RECEIVER_HAS_NO_PRIVILEGES:
	case RSC_TARGET_NOT_SUBSCRIBABLE:
	case RSC_SUBSCRIPTION_HOST_HAS_NO_PRIVILEGE:
	case RSC_ORIGINATOR_HAS_NOT_REGISTERED:
	case RSC_SECURITY_ASSOCIATION_REQUIRED:
	case RSC_INVALID_CHILD_RESOURCETYPE:
	case RSC_NO_MEMBERS:
	case RSC_ESPRIM_UNSUPPORTED_OPTION:
	case RSC_ESPRIM_UNKNOWN_KEY_ID:
	case RSC_ESPRIM_UNKNOWN_ORIG_RAND_ID:
	case RSC_ESPRIM_UNKNOWN_RECV_RAND_ID:
	case RSC_ESPRIM_BAD_MAC:
	case RSC_ESPRIM_IMPERSONATION_ERROR:
	case RSC_ORIGINATOR_HAS_ALREADY_REGISTERD:
	case RSC_APP_RULE_VALIDATION_FAILED:
	case RSC_OPERATION_DENIED_BY_REMOTE_ENTITY:
	case RSC_SERVICE_SUBSCRIPTION_NOT_ESTABLISHED:
	case RSC_DISCOVERY_DENIED_BY_IPE:
		*msg = "Forbidden";
		return 403;

	case RSC_NOT_FOUND:
	case RSC_LINKED_SEMANTICS_NOT_AVAILABLE:
	case RSC_MASHUP_MEMBER_NOT_FOUND:
	case RSC_TARGET_NOT_REACHABLE:
		*msg = "Not Found";
		return 404;

	case RSC_OPERATION_NOT_ALLOWED:
		*msg = "Method Not Allowed";
		return 405;

	case RSC_NOT_ACCEPTABLE:
		*msg = "Not Acceptable";
		return 406;

	case RSC_GROUP_REQUEST_IDENTIFIER_EXISTS:
	case RSC_CONFLICT:
	case RSC_BLOCKING_SUBSCRIPTION_ALREADY_EXISTS:
	// case RSC_SOFTWARE_CAMPAIGN_CONFLICT:
	case RSC_ALREADY_EXISTS:
		// case RSC_UNABLE_TO_REPLACE_REQUEST:
		// case RSC_UNABLE_TO_RECALL_REQUEST:
		// case RSC_ALREADY_COMPLETE:
		// case RSC_MGMT_COMMAND_NOT_CANCELLABLE:
		*msg = "Conflict";
		return 409;

	case RSC_UNSUPPORTED_MEDIATYPE:
		*msg = "Unsupported Media Type";
		return 415;

	case RSC_INTERNAL_SERVER_ERROR:
	case RSC_SUBSCRIPTION_VERIFICATION_INITIATION_FAILED:
	case RSC_GROUP_MEMBERS_NOT_RESPONDED:
	// case RSC_ESPRIM_DECRYPTION_ERROR:
	// case RSC_ESPRIM_ENCRYPTION_ERROR:
	// case RSC_SPARQL_UPDATE_ERROR:
	// case RSC_JOIN_MULTICAST_GROUP_FAILED:
	// case RSC_LEAVE_MULTICAST_GROUP_FAILED:
	case RSC_CROSS_RESOURCE_OPERATION_FAILURE:
		// case RSC_ONTOLOGY_MAPPING_ALGORITHM_FAILED:
		// case RSC_ONTOLOGY_CONVERSION_FAILED:
		// case RSC_REASONING_PROCESSING_FAILED:
		// case RSC_MGMT_SESSION_CANNOT_BE_ESTABLISHED:
		// case RSC_MGMT_SESSION_ESTABLISHMENT_TIMEOUT:
		// case RSC_MGMT_CONVERSION_ERROR:
		// case RSC_MGMT_CANCELLATION_FAILED:
		// case RSC_NETWORK_QOS_CONFIG_ERROR:
		*msg = "Internal Server Error";
		return 500;

	case RSC_RELEASE_VERSION_NOT_SUPPORTED:
	case RSC_NOT_IMPLEMENTED:
	case RSC_SPECIALIZATION_SCHEMA_NOT_FOUND:
	case RSC_NON_BLOCKING_SYNCH_REQUEST_NOT_SUPPORTED:
		*msg = "Not Implemented";
		return 501;

	case RSC_REQUEST_TIMEOUT:
		// case RSC_EXTERNAL_OBJECT_NOT_REACHABLE_BEFORE_RQET_TIMEOUT:
		// case RSC_EXTERNAL_OBJECT_NOT_REACHABLE_BEFORE_OET_TIMEOUT:
		*msg = "Gateway Timeout";
		return 504;

	default:
		return 200;
	}
}

int rsc_to_coap_status(int rsc)
{
	switch (rsc)
	{
	// case RSC_ACCEPTED: // Not used
	case RSC_ACCEPTED_NONBLOCKING_REQUEST_SYNCH:
	case RSC_ACCEPTED_NONBLOCKING_REQUEST_ASYNCH:
	case RSC_CREATED:
		return 201; // 2.01 Created
	case RSC_DELETED:
		return 202; // 2.02 Deleted
					// case 2000: // OK for NOTIFY operation
	case RSC_UPDATED:
		return 204; // 2.04 Changed
	case RSC_OK:
		return 205; // 2.05 Content
	case RSC_BAD_REQUEST:
	case RSC_CONTENTS_UNACCEPTABLE:
	case RSC_GROUP_REQUEST_IDENTIFIER_EXISTS:
	case RSC_GROUPMEMBER_TYPE_INCONSISTENT:
	case RSC_ILLEGAL_TRANSACTION_STATE_TRANSITION_ATTEMPTED:
	case RSC_BLOCKING_SUBSCRIPTION_ALREADY_EXISTS:
	case RSC_ONTOLOGY_MAPPING_POLICY_NOT_MATCHED:
	case RSC_BAD_FACT_INPUTS_FOR_REASONING:
	case RSC_BAD_RULE_INPUTS_FOR_REASONING:
	case RSC_PRIMITIVE_PROFILE_BAD_REQUEST:
	case RSC_INVALID_SPARQL_QUERY:
	case RSC_ALREADY_EXISTS:
	case RSC_MAX_NUMBER_OF_MEMBER_EXCEEDED:
	case RSC_INVALID_CMDTYPE:
	case RSC_INVALID_ARGUMENTS:
	case RSC_INSUFFICIENT_ARGUMENTS:
	case RSC_ALREADY_COMPLETE:
	case RSC_MGMT_COMMAND_NOT_CANCELLABLE:
		return 400; // 4.00 Bad Request
	case RSC_INVALID_SEMANTICS:
	case RSC_INVALID_TRIGGER_PURPOSE:
		return 402; // 4.02 Bad Option
	case RSC_SUBSCRIPTION_CREATOR_HAS_NO_PRIVILEGE:
	case RSC_ORIGINATOR_HAS_NO_PRIVILEGE:
	case RSC_CONFLICT:
	case RSC_ORIGINATOR_HAS_NOT_REGISTERED:
	case RSC_SECURITY_ASSOCIATION_REQUIRED:
	case RSC_INVALID_CHILD_RESOURCETYPE:
	case RSC_NO_MEMBERS:
	case RSC_ESPRIM_UNSUPPORTED_OPTION:
	case RSC_ESPRIM_UNKNOWN_KEY_ID:
	case RSC_ESPRIM_UNKNOWN_ORIG_RAND_ID:
	case RSC_ESPRIM_UNKNOWN_RECV_RAND_ID:
	case RSC_ESPRIM_BAD_MAC:
	case RSC_ESPRIM_IMPERSONATION_ERROR:
	case RSC_ORIGINATOR_HAS_ALREADY_REGISTERD:
	case RSC_APP_RULE_VALIDATION_FAILED:
	case RSC_OPERATION_DENIED_BY_REMOTE_ENTITY:
	case RSC_SERVICE_SUBSCRIPTION_NOT_ESTABLISHED:
	case RSC_DISCOVERY_LIMIT_EXCEEDED:
	case RSC_PRIMITIVE_PROFILE_NOT_ACCESSIBLE:
	case RSC_UNAUTHORIZED_USER:
	case RSC_SERVICE_SUBSCRIPTION_NOT_ACTIVE:
	case RSC_SOFTWARE_CAMPAIGN_CONFLICT:
	case RSC_RECEIVER_HAS_NO_PRIVILEGES:
	case RSC_TARGET_NOT_SUBSCRIBABLE:
	case RSC_SUBSCRIPTION_HOST_HAS_NO_PRIVILEGE:
	case RSC_DISCOVERY_DENIED_BY_IPE:
	case RSC_TARGET_HAS_NO_SESSION_CAPABILITY:
	case RSC_SESSION_IS_ONLINE:
	case RSC_TRANSACTION_PROCESSING_IS_INCOMPLETE:
	case RSC_REQUESTED_ACTIVITY_PATTERN_NOT_PERMITTED:
		return 403; // 4.03 Forbidden
	case RSC_NOT_FOUND:
	case RSC_ONTOLOGY_NOT_AVAILABLE:
	case RSC_LINKED_SEMANTICS_NOT_AVAILABLE:
	case RSC_MASHUP_MEMBER_NOT_FOUND:
	case RSC_ONTOLOGY_MAPPING_ALGORITHM_NOT_AVAILABLE:
	case RSC_ONTOLOGY_MAPPING_NOT_AVAILABLE:
	case RSC_TARGET_NOT_REACHABLE:
	case RSC_REMOTE_ENTITY_NOT_REACHABLE:
	case RSC_EXTERNAL_OBJECT_NOT_REACHABLE:
	case RSC_EXTERNAL_OBJECT_NOT_FOUND:
		return 404; // 4.04 Not Found
	case RSC_OPERATION_NOT_ALLOWED:
		return 405; // 4.05 Method Not Allowed
	case RSC_NOT_ACCEPTABLE:
		return 406; // 4.06 Not Acceptable
	case RSC_UNSUPPORTED_MEDIATYPE:
		return 415; // 4.15 Unsupported Content-Format
	case RSC_INTERNAL_SERVER_ERROR:
	case RSC_SUBSCRIPTION_VERIFICATION_INITIATION_FAILED:
	case RSC_GROUP_MEMBERS_NOT_RESPONDED:
	case RSC_ESPRIM_DECRYPTION_ERROR:
	case RSC_ESPRIM_ENCRYPTION_ERROR:
	case RSC_SPARQL_UPDATE_ERROR:
	case RSC_JOIN_MULTICAST_GROUP_FAILED:
	case RSC_LEAVE_MULTICAST_GROUP_FAILED:
	case RSC_CROSS_RESOURCE_OPERATION_FAILURE:
	case RSC_ONTOLOGY_MAPPING_ALGORITHM_FAILED:
	case RSC_ONTOLOGY_CONVERSION_FAILED:
	case RSC_REASONING_PROCESSING_FAILED:
	case RSC_MGMT_SESSION_CANNOT_BE_ESTABLISHED:
	case RSC_MGMT_SESSION_ESTABLISHMENT_TIMEOUT:
	case RSC_MGMT_CONVERSION_ERROR:
	case RSC_MGMT_CANCELLATION_FAILED:
		return 500; // 5.00 Internal Server Error
	case RSC_RELEASE_VERSION_NOT_SUPPORTED:
	case RSC_SPECIALIZATION_SCHEMA_NOT_FOUND:
	case RSC_NOT_IMPLEMENTED:
	case RSC_NON_BLOCKING_SYNCH_REQUEST_NOT_SUPPORTED:
		return 501; // 5.01 Not Implemented
	case RSC_TRIGGERING_DISABLED_FOR_RECIPIENT:
	case RSC_UNABLE_TO_REPLACE_GROUP_MEMBER:
	case RSC_UNABLE_TO_RECALL_REQUEST:
	case RSC_NETWORK_QOS_CONFIG_ERROR:
		return 503; // 5.03 Service Unavailable
	case RSC_REQUEST_TIMEOUT:
	case RSC_EXTERNAL_OBJECT_NOT_REACHABLE_BEFORE_RQET_TIMEOUT:
	case RSC_EXTERNAL_OBJECT_NOT_REACHABLE_BEFORE_OET_TIMEOUT:
		return 504; // 5.04 Gateway Timeout
	default:
		return 0;
	}
}

cJSON *o2pt_to_json(oneM2MPrimitive *o2pt)
{
	cJSON *json = cJSON_CreateObject();

	cJSON_AddNumberToObject(json, "rsc", o2pt->rsc);
	cJSON_AddStringToObject(json, "rqi", o2pt->rqi);
	cJSON_AddStringToObject(json, "to", o2pt->to);
	cJSON_AddStringToObject(json, "fr", o2pt->fr);
	cJSON_AddStringToObject(json, "rvi", from_rvi(o2pt->rvi));
	if (o2pt->response_pc)
		cJSON_AddItemToObject(json, "pc", cJSON_Duplicate(o2pt->response_pc, true));
	if (o2pt->cnst)
		cJSON_AddNumberToObject(json, "cnst", o2pt->cnst);
	if (o2pt->cnot)
		cJSON_AddNumberToObject(json, "cnot", o2pt->cnot);
	// if(o2pt->ty >= 0) cJSON_AddNumberToObject(json, "ty", o2pt->ty);

	return json;
}

void free_o2pt(oneM2MPrimitive *o2pt)
{
	if (o2pt->rqi)
		free(o2pt->rqi);
	if (o2pt->fr)
		free(o2pt->fr);
	if (o2pt->to)
		free(o2pt->to);
	if (o2pt->fc)
		cJSON_Delete(o2pt->fc);
	if (o2pt->fopt)
		free(o2pt->fopt);
	if (o2pt->ip)
		free(o2pt->ip);
	if (o2pt->request_pc)
		cJSON_Delete(o2pt->request_pc);
	if (o2pt->response_pc)
		cJSON_Delete(o2pt->response_pc);
	if (o2pt->mqtt_origin)
		free(o2pt->mqtt_origin);

	memset(o2pt, 0, sizeof(oneM2MPrimitive));
	free(o2pt);
	o2pt = NULL;
}

void o2ptcpy(oneM2MPrimitive **dest, oneM2MPrimitive *src)
{
	if (src == NULL)
		return;

	(*dest) = (oneM2MPrimitive *)calloc(1, sizeof(oneM2MPrimitive));

	(*dest)->fr = strdup(src->fr);
	(*dest)->to = strdup(src->to);
	if (src->rqi)
		(*dest)->rqi = strdup(src->rqi);
	if (src->request_pc)
		(*dest)->request_pc = cJSON_Duplicate(src->request_pc, true);
	if (src->fopt)
		(*dest)->fopt = strdup(src->fopt);
	if (src->ip)
		(*dest)->ip = strdup(src->ip);
	(*dest)->rvi = src->rvi;
	(*dest)->rcn = src->rcn;
	(*dest)->ty = src->ty;
	(*dest)->op = src->op;
	(*dest)->isFopt = src->isFopt;
	(*dest)->rsc = src->rsc;
	(*dest)->cnot = src->cnot;
	(*dest)->fc = cJSON_Duplicate(src->fc, true);
}

char *get_pi_rtnode(RTNode *rtnode)
{
	cJSON *pi = cJSON_GetObjectItem(rtnode->obj, "pi");
	if (pi)
	{
		return pi->valuestring;
	}
	else
	{
		return "";
	}
}

char *get_ri_rtnode(RTNode *rtnode)
{
	cJSON *ri = cJSON_GetObjectItem(rtnode->obj, "ri");
	if (ri)
	{
		return ri->valuestring;
	}
	else
	{
		return NULL;
	}
}

char *get_rn_rtnode(RTNode *rtnode)
{
	cJSON *rn = cJSON_GetObjectItem(rtnode->obj, "rn");
	if (rn)
	{
		return rn->valuestring;
	}
	else
	{
		return NULL;
	}
}

cJSON *get_acpi_rtnode(RTNode *rtnode)
{
	cJSON *acpi = cJSON_GetObjectItem(rtnode->obj, "acpi");
	if (acpi)
	{
		return acpi;
	}
	else
	{
		return NULL;
	}
}

char *get_ct_rtnode(RTNode *rtnode)
{
	cJSON *ct = cJSON_GetObjectItem(rtnode->obj, "ct");
	if (ct)
	{
		return ct->valuestring;
	}
	else
	{
		return NULL;
	}
}

char *get_et_rtnode(RTNode *rtnode)
{
	cJSON *et = cJSON_GetObjectItem(rtnode->obj, "et");
	if (et)
	{
		return et->valuestring;
	}
	else
	{
		return NULL;
	}
}

char *get_lt_rtnode(RTNode *rtnode)
{
	cJSON *lt = cJSON_GetObjectItem(rtnode->obj, "lt");
	if (lt)
	{
		return lt->valuestring;
	}
	else
	{
		return NULL;
	}
}

char *get_uri_rtnode(RTNode *rtnode)
{
	return rtnode->uri;
}

cJSON *getResource(cJSON *root, ResourceType ty)
{
	switch (ty)
	{
	case RT_CSE:
		return cJSON_GetObjectItem(root, "m2m:cse");
	case RT_AE:
		return cJSON_GetObjectItem(root, "m2m:ae");
	case RT_CNT:
		return cJSON_GetObjectItem(root, "m2m:cnt");
	case RT_CIN:
		return cJSON_GetObjectItem(root, "m2m:cin");
	case RT_ACP:
		return cJSON_GetObjectItem(root, "m2m:acp");
	case RT_SUB:
		return cJSON_GetObjectItem(root, "m2m:sub");
	case RT_GRP:
		return cJSON_GetObjectItem(root, "m2m:grp");
	case RT_CSR:
		return cJSON_GetObjectItem(root, "m2m:csr");
	case RT_ACPA:
		return cJSON_GetObjectItem(root, "m2m:acpA");
	case RT_AEA:
		return cJSON_GetObjectItem(root, "m2m:aeA");
	case RT_CBA:
		return cJSON_GetObjectItem(root, "m2m:cbA");
	case RT_CINA:
		return cJSON_GetObjectItem(root, "m2m:cinA");
	case RT_CNTA:
		return cJSON_GetObjectItem(root, "m2m:cntA");
	}
	return NULL;
}

int get_number_from_cjson(cJSON *json)
{
	if (!json)
		return 0;

	if (json->valueint)
		return json->valueint;
	if (json->valuestring)
		return atoi(json->valuestring);
}

cJSON *qs_to_json(char *qs)
{
	if (!qs)
		return NULL;

	int prevb = 0;
	char *qStr = strdup(qs);
	char *buf = calloc(1, 256);
	char *temp = calloc(1, 256);
	char *key = NULL, *value = NULL;
	char *ptr = NULL;
	char *tokPtr = NULL;

	size_t qslen = strlen(qs);
	cJSON *json;

	buf[0] = '{';

	for (int i = 0; i <= qslen; i++)
	{
		if (qStr[i] == '=')
		{
			key = qStr + prevb;
			qStr[i] = '\0';
			prevb = i + 1;
		}
		else if (qStr[i] == '&' || i == qslen)
		{
			value = qStr + prevb;
			qStr[i] = '\0';
			prevb = i + 1;
		}

		if (key != NULL && value != NULL)
		{
			// logger("UTIL", LOG_LEVEL_DEBUG, "key = %s, value = %s", key, value);
			if (strstr(value, "+") != NULL)
			{
				ptr = strtok_r(value, "+", &tokPtr);
				sprintf(temp, "\"%s\":[\"", key);

				while (ptr != NULL)
				{
					strcat(temp, ptr);
					strcat(temp, "\",\"");
					ptr = strtok_r(NULL, "+", &tokPtr);
				}
				temp[strlen(temp) - 2] = ']';
				temp[strlen(temp) - 1] = ',';
			}
			else
			{
				sprintf(temp, "\"%s\":\"%s\",", key, value);
			}

			strcat(buf, temp);
			key = NULL;
			value = NULL;
		}
	}
	buf[strlen(buf) - 1] = '}';
	json = cJSON_Parse(buf);
	if (!json)
	{
		logger("UTIL", LOG_LEVEL_DEBUG, "ERROR before %10s\n", cJSON_GetErrorPtr());
		return NULL;
	}
	free(temp);
	free(buf);
	free(qStr);

	return json;
}

void filterOptionStr(FilterOperation fo, char *sql)
{
	switch (fo)
	{
	case FO_AND:
		strcat(sql, " AND ");
		break;
	case FO_OR:
		strcat(sql, " OR ");
		break;
	case FO_XOR:
		break;
	}
}

// TBD : not used
bool check_acpi_valid(oneM2MPrimitive *o2pt, cJSON *acpi)
{
	logger("UTIL", LOG_LEVEL_DEBUG, "check_acpi_valid");
	bool ret = true;

	int acpi_size = cJSON_GetArraySize(acpi);

	for (int i = 0; i < acpi_size; i++)
	{
		char *acp_uri = cJSON_GetArrayItem(acpi, i)->valuestring;
		RTNode *acp_rtnode = find_rtnode(acp_uri);
		if (!acp_rtnode)
		{
			ret = false;
			handle_error(o2pt, RSC_NOT_FOUND, "resource `acp` does not found");
			break;
		}
		else
		{
			if ((get_acop_origin(o2pt, o2pt->fr, acp_rtnode, 1) & ACOP_UPDATE) != ACOP_UPDATE)
			{
				ret = false;
				handle_error(o2pt, RSC_ORIGINATOR_HAS_NO_PRIVILEGE, "originator has no privilege");
				break;
			}
		}
	}

	return ret;
}

/**
 * @brief get the list of acps that originator has no discovery privilege
 * @param o2pt oneM2MPrimitive
 * @param rtnode target resource node
 * @return list of acps
 * @note for Thread-Safe behavior, lock mutex outside initial call as the function is recursive.
 */
cJSON *getNonDiscoverableAcp(oneM2MPrimitive *o2pt, RTNode *rtnode)
{
	cJSON *acp_list = cJSON_CreateArray();
	while (rtnode)
	{
		if (rtnode->ty == RT_ACP)
		{
			if ((get_acop_origin(o2pt, o2pt->fr, rtnode, 0) & ACOP_DISCOVERY) != ACOP_DISCOVERY)
			{
				cJSON_AddItemToArray(acp_list, cJSON_CreateString(get_uri_rtnode(rtnode)));
			}
		}
		if (rtnode->child)
		{
			cJSON *pjson = NULL;
			cJSON *child_acp_list = getNonDiscoverableAcp(o2pt, rtnode->child);
			cJSON_ArrayForEach(pjson, child_acp_list)
			{
				cJSON_AddItemToArray(acp_list, cJSON_CreateString(pjson->valuestring));
			}
			cJSON_Delete(child_acp_list);
		}
		rtnode = rtnode->sibling_right;
	}
	return acp_list;
}

cJSON *getNoPermAcopDiscovery(oneM2MPrimitive *o2pt, RTNode *rtnode, ACOP acop)
{
	cJSON *acp_list = cJSON_CreateArray();

	while (rtnode)
	{
		if (rtnode->ty == RT_ACP)
		{
			if ((get_acop_origin(o2pt, o2pt->fr, rtnode, 0) & acop) != acop)
			{
				cJSON_AddItemToArray(acp_list, cJSON_CreateString(get_uri_rtnode(rtnode)));
			}
		}
		if (rtnode->child)
		{
			cJSON *pjson = NULL;
			cJSON *child_acp_list = getNoPermAcopDiscovery(o2pt, rtnode->child, acop);
			cJSON_ArrayForEach(pjson, child_acp_list)
			{
				cJSON_AddItemToArray(acp_list, cJSON_CreateString(pjson->valuestring));
			}
			cJSON_Delete(child_acp_list);
		}
		rtnode = rtnode->sibling_right;
	}

	return acp_list;
}

int requestToResource(oneM2MPrimitive *o2pt, RTNode *rtnode)
{

	cJSON *pjson = NULL;
	char *ptr = NULL;
	int rsc = 0;
	if (!rtnode)
		return RSC_NOT_FOUND;
	logger("UTIL", LOG_LEVEL_DEBUG, "requestToResource [%s]", get_uri_rtnode(rtnode));
	if (rtnode->ty == RT_AE)
	{
		logger("UTIL", LOG_LEVEL_DEBUG, "requestToResource AE [%s]", rtnode->rn);
		cJSON *ae = rtnode->obj;
		cJSON *poa = cJSON_GetObjectItem(ae, "poa");
		cJSON *rr = cJSON_GetObjectItem(ae, "rr");

		if (!rr || rr->type == cJSON_False)
		{
			logger("UTIL", LOG_LEVEL_DEBUG, "requestToResource AE [%s] rr is false", rtnode->rn);
			return RSC_TARGET_NOT_REACHABLE;
		}
		if (!poa)
		{
			logger("UTIL", LOG_LEVEL_DEBUG, "requestToResource AE [%s] poa is NULL", rtnode->rn);
			return RSC_BAD_REQUEST;
		}
		Protocol prot;
		HTTPRequest *req = NULL;
		HTTPResponse *res = NULL;
		char *host, *path;
		int port;

		cJSON_ArrayForEach(pjson, poa)
		{
			if (parsePoa(pjson->valuestring, &prot, &host, &port, &path) == -1)
			{
				logger("UTIL", LOG_LEVEL_DEBUG, "poa parse error %d", 0);
				continue;
			}
			switch (prot)
			{
			case PROT_HTTP:
				logger("UTIL", LOG_LEVEL_DEBUG, "requestToResource HTTP [%s]", pjson->valuestring);
				req = (HTTPRequest *)calloc(1, sizeof(HTTPRequest));
				res = (HTTPResponse *)calloc(1, sizeof(HTTPResponse));
				req->method = op_to_method(o2pt->op);
				req->uri = strdup(path);
				req->payload = cJSON_PrintUnformatted(o2pt->request_pc);
				req->payload_size = strlen(req->payload);
				req->qs = NULL;
				req->headers = calloc(1, sizeof(header_t));
				add_header("X-M2M-Origin", o2pt->fr, req->headers);
				add_header("X-M2M-RI", o2pt->rqi, req->headers);
				add_header("Content-Type", "application/json", req->headers);
				if (o2pt->rvi != RVI_NONE)
				{
					add_header("X-M2M-RVI", from_rvi(o2pt->rvi), req->headers);
				}

				send_http_request(host, port, req, res);
				ptr = search_header(res->headers, "X-M2M-RSC");
				if (ptr)
				{
					rsc = atoi(ptr);
				}
				else
				{
					rsc = RSC_BAD_REQUEST;
				}
				free_HTTPResponse(res);
				free_HTTPRequest(req);
				break;
			case PROT_MQTT:
#ifdef ENABLE_MQTT // TODO
// mqtt_request(o2pt, host, port, path);
#endif
				break;
			}
			if (host)
				free(host);
			if (path)
				free(path);
		}
	}

	return rsc;
}

int send_verification_request(char *noti_uri, cJSON *noti_cjson)
{
	logger("UTIL", LOG_LEVEL_DEBUG, "send_verification_request");
	oneM2MPrimitive *o2pt = calloc(1, sizeof(oneM2MPrimitive));
	NotiTarget *nt;
	RTNode *rtnode = NULL;
	int rsc = 0;

	o2pt->op = OP_NOTIFY;
	o2pt->fr = strdup("/" CSE_BASE_RI);
	o2pt->rvi = CSE_RVI;
	o2pt->rqi = strdup("notify");
	o2pt->request_pc = cJSON_Duplicate(noti_cjson, true);

	ResourceAddressingType rat = checkResourceAddressingType(noti_uri);

	if (rat == CSE_RELATIVE)
	{
		logger("UTIL", LOG_LEVEL_DEBUG, "CSE_RELATIVE");
		rtnode = find_rtnode(noti_uri);
		if (!rtnode)
			return RSC_NOT_FOUND;
		rsc = requestToResource(o2pt, rtnode);
	}
	else if (rat == SP_RELATIVE)
	{
		logger("UTIL", LOG_LEVEL_DEBUG, "SP_RELATIVE");
		o2pt->to = strdup(noti_uri);

		forwarding_onem2m_resource(o2pt, find_csr_rtnode_by_uri(noti_uri));
		rsc = o2pt->rsc;
	}
	else if (rat == PROTOCOL_BINDING)
	{
		logger("UTIL", LOG_LEVEL_DEBUG, "protocol binding");
		Protocol prot;
		char *host, *path;
		int port;
		nt = calloc(1, sizeof(NotiTarget));

		if (parsePoa(noti_uri, &prot, &host, &port, &path) == -1)
		{
			logger("UTIL", LOG_LEVEL_DEBUG, "poa parse error");
			free(nt);
			return RSC_BAD_REQUEST;
		}
		o2pt->to = strdup(path);
		strncpy(nt->host, host, 1024);
		nt->port = port;
		nt->noti_json = cJSON_PrintUnformatted(noti_cjson);
		strncpy(nt->target, path, 256);
		nt->prot = prot;

		switch (prot)
		{
		case PROT_HTTP:
			rsc = http_notify(o2pt, host, port, nt);
			break;
		case PROT_MQTT:
#ifdef ENABLE_MQTT
			mqtt_notify(o2pt, noti_cjson, nt);
#endif
			break;
#ifdef ENABLE_COAP
		case PROT_COAP:
			coap_notify(o2pt, noti_cjson, nt);
#endif
			break;
		}

		free(nt->noti_json);
		free(nt);
		if (host)
			free(host);
		if (path)
			free(path);
	}

	// o2pt->fr = NULL;
	free_o2pt(o2pt);
	return rsc;
}

int notify_to_nu(RTNode *sub_rtnode, cJSON *noti_cjson, int net)
{
	logger("UTIL", LOG_LEVEL_DEBUG, "notify_to_nu");
	cJSON *sub = sub_rtnode->obj;
	int uri_len = 0, index = 0;
	char *noti_json = NULL;
	char *p = NULL;
	char port[10] = {'\0'};
	bool isNoti = false;
	NotiTarget *nt = NULL;
	cJSON *pjson = NULL;
	RTNode *rtnode = NULL;

	int rsc = 0;
	cJSON *nu = cJSON_GetObjectItem(sub, "nu");
	if (!nu)
		return RSC_OK;

	oneM2MPrimitive *o2pt = calloc(1, sizeof(oneM2MPrimitive));
	o2pt->op = OP_NOTIFY;
	o2pt->fr = strdup("/" CSE_BASE_RI);
	o2pt->rvi = CSE_RVI;
	o2pt->rqi = strdup("notify");
	o2pt->request_pc = cJSON_Duplicate(noti_cjson, true);

	cJSON_ArrayForEach(pjson, nu)
	{
		char *noti_uri = strdup(pjson->valuestring);
		logger("UTIL", LOG_LEVEL_DEBUG, "noti_uri : %s", noti_uri);
		index = 0;
		ResourceAddressingType rat = checkResourceAddressingType(noti_uri);

		if (rat == CSE_RELATIVE)
		{
			logger("UTIL", LOG_LEVEL_DEBUG, "CSE_RELATIVE");
			cJSON_DeleteItemFromObject(o2pt->request_pc, "vrq");
			rtnode = find_rtnode(noti_uri);
			requestToResource(o2pt, rtnode);
			rsc = RSC_OK;
		}
		else if (rat == SP_RELATIVE)
		{
			logger("UTIL", LOG_LEVEL_DEBUG, "SP_RELATIVE");
			oneM2MPrimitive noti_o2pt;
			memset(&noti_o2pt, 0, sizeof(oneM2MPrimitive));
			noti_o2pt.op = OP_NOTIFY;
			noti_o2pt.fr = strdup("/" CSE_BASE_RI);
			noti_o2pt.to = strdup(noti_uri);
			noti_o2pt.rqi = strdup("notify");
			noti_o2pt.rvi = CSE_RVI;
			noti_o2pt.request_pc = cJSON_Duplicate(noti_cjson, true);

			forwarding_onem2m_resource(&noti_o2pt, find_csr_rtnode_by_uri(noti_uri));
			// rtnode = get_remote_resource(noti_uri, &rsc);
			// if (rtnode)
			// {
			// 	rsc = requestToResource(o2pt, rtnode);
			// 	free_rtnode(rtnode);
			// 	rtnode = NULL;
			// }
			// else
			// {
			// 	logger("UTIL", LOG_LEVEL_DEBUG, "SP_RELATIVE resource not found");
			// }
		}
		else if (rat == PROTOCOL_BINDING)
		{
			logger("UTIL", LOG_LEVEL_DEBUG, "protocol binding");
			Protocol prot;
			char *host, *path;
			int port;
			nt = calloc(1, sizeof(NotiTarget));

			if (parsePoa(noti_uri, &prot, &host, &port, &path) == -1)
			{
				logger("UTIL", LOG_LEVEL_DEBUG, "poa parse error");
				free(noti_uri);
				continue;
			}
			o2pt->to = strdup(path);
			strncpy(nt->host, host, 1024);
			nt->port = port;
			nt->noti_json = cJSON_PrintUnformatted(noti_cjson);
			strncpy(nt->target, path, 256);
			nt->prot = prot;
			switch (prot)
			{
			case PROT_HTTP:
				rsc = http_notify(o2pt, host, port, nt);
				break;
			case PROT_MQTT:
#ifdef ENABLE_MQTT
				mqtt_notify(o2pt, noti_json, nt);
#endif
#ifdef ENABLE_COAP
			case PROT_COAP:
				coap_notify(o2pt, noti_json, nt);
				break;
#endif
				break;
			}
			free(nt->noti_json);
			free(nt);
			if (host)
				free(host);
			if (path)
				free(path);
		}
		free(noti_uri);
	}

	free_o2pt(o2pt);
	return RSC_OK;
}

void update_resource(cJSON *old_obj, cJSON *new_obj)
{
	cJSON *pjson = new_obj->child;
	while (pjson)
	{
		if (cJSON_GetObjectItem(old_obj, pjson->string))
		{
			if (pjson->type == cJSON_NULL)
			{
				cJSON_DeleteItemFromObject(old_obj, pjson->string);
			}
			else
			{
				cJSON_ReplaceItemInObject(old_obj, pjson->string, cJSON_Duplicate(pjson, 1));
			}
		}
		else
		{
			cJSON_AddItemToObject(old_obj, pjson->string, cJSON_Duplicate(pjson, 1));
		}
		pjson = pjson->next;
	}
}

/**
 * @brief validate sub attribute
 * @param obj attribute supported
 * @param attr attribute to validate
 * @param err_msg error message
 * @return true if valid, false if invalid
 * */
bool validate_sub_attr(cJSON *obj, cJSON *attr, char *err_msg)
{
	if (!attr)
		return false;
	if (!obj)
		return false;
	cJSON *verifier = NULL;
	cJSON *verifiee = NULL;

	verifier = cJSON_GetObjectItem(obj, attr->string);
	if (!verifier && obj->type == cJSON_Array)
	{
		verifier = obj;
	}

	if (!verifier)
	{
		if (err_msg)
		{
			sprintf(err_msg, "invalid attribute : %s", attr->string);
		}
		return false;
	}
	if (attr->type != cJSON_NULL && verifier->type != attr->type)
	{ // if attribute type is null it is deleting attribute(on update)
		if (verifier->type == cJSON_True || verifier->type == cJSON_False)
		{
			if (attr->type == cJSON_True || attr->type == cJSON_False)
			{
			}
			else
			{
				if (err_msg)
				{
					sprintf(err_msg, "invalid attribute type : %s", attr->string);
				}
				return false;
			}
		}
		else
		{
			if (err_msg)
			{
				sprintf(err_msg, "invalid attribute type : %s", attr->string);
			}
			return false;
		}
	}
	if (attr->type == cJSON_Object)
	{
		cJSON_ArrayForEach(verifiee, attr)
		{
			if (cJSON_GetArraySize(verifiee) > 0)
			{
				if (!validate_sub_attr(verifier, verifiee, err_msg))
				{
					return false;
				}
			}
		}
	}
	else if (attr->type == cJSON_Array)
	{
		cJSON_ArrayForEach(verifiee, attr)
		{
			if (verifiee->type != verifier->child->type)
			{
				if (err_msg)
				{
					sprintf(err_msg, "invalid attribute type : %s", attr->string);
				}
				return false;
			}
			if (verifiee->type == cJSON_Object)
			{
				cJSON *verifiee_child = NULL;
				cJSON_ArrayForEach(verifiee_child, verifiee)
				{
					if (!validate_sub_attr(verifier->child, verifiee_child, err_msg))
					{
						return false;
					}
				}
			}
		}
	}

	return true;
}

/**
 * @brief validate requested attribute
 * @param obj attribute received
 * @param ty resource type
 * @param err_msg buffer for error message
 * @return true if valid, false if invalid
 */
bool is_attr_valid(cJSON *obj, ResourceType ty, char *err_msg)
{
	extern cJSON *ATTRIBUTES;
	cJSON *attrs = NULL;
	cJSON *general_attrs = NULL;
	bool flag = false;
	attrs = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(ty));
	general_attrs = cJSON_GetObjectItem(ATTRIBUTES, "general");
	if (!attrs)
		return false;
	if (!general_attrs)
		return false;
	if (!cJSON_IsObject(attrs))
		return false;

	cJSON *pjson = cJSON_GetObjectItem(obj, get_resource_key(ty));
	cJSON *attr = NULL;
	if (!pjson)
		return false;
	pjson = pjson->child;
	while (pjson)
	{
		// if (!strcmp(pjson->string, "memberOf"))
		// {
		// 	return false;
		// }
		if (validate_sub_attr(attrs, pjson, err_msg))
		{
			flag = true;
		}
		if (flag)
		{
			pjson = pjson->next;
			flag = false;
			continue;
		}
		if (validate_sub_attr(general_attrs, pjson, err_msg))
		{
			flag = true;
		}
		if (!flag)
		{
			return false;
		}
		pjson = pjson->next;
		flag = false;
	}

	return true;
}

/**
 * Get Request Primitive and acpi attribute and validate it.
 * @param o2pt oneM2M request primitive
 * @param acpiAttr acpi attribute cJSON object
 * @param op operation type
 * @return 0 if valid, -1 if invalid
 */
int validate_acpi(oneM2MPrimitive *o2pt, cJSON *acpiAttr, Operation op)
{
	logger("UTIL", LOG_LEVEL_DEBUG, "validate_acpi %d", op);
	if (!acpiAttr)
	{
		return RSC_OK;
	}
	if (!cJSON_IsArray(acpiAttr) || cJSON_IsNull(acpiAttr))
	{
		return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `acpi` is in invalid form");
	}
	if (cJSON_IsArray(acpiAttr) && cJSON_GetArraySize(acpiAttr) == 0)
	{
		return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `acpi` is empty");
	}

	cJSON *acpi = NULL;
	int acop = 0;
	cJSON_ArrayForEach(acpi, acpiAttr)
	{
		RTNode *acp = NULL;
		acp = find_rtnode(acpi->valuestring);
		if (!acp)
		{
			return handle_error(o2pt, RSC_BAD_REQUEST, "resource `acp` is not found");
		}

		if (op == OP_UPDATE)
		{
			acop = (acop | get_acop_origin(o2pt, o2pt->fr, acp, 1));
			if ((acop & op_to_acop(op)) == op_to_acop(op))
			{
				break;
			}
		}
		else if (op == OP_CREATE)
		{
			acop = ACOP_CREATE;
			// Create does not need to check acop
			// acop = (acop | get_acop_origin(o2pt, acp, 0));
			// logger("UTIL", LOG_LEVEL_DEBUG, "acop-pv : %d, op : %d", acop, op);
			// if((acop & op) == op){
			// 	break;
			// }
		}
	}
#ifdef ADMIN_AE_ID
	if (o2pt->fr && !strcmp(o2pt->fr, ADMIN_AE_ID))
	{
		return RSC_OK;
	}
#endif
	if ((acop & op_to_acop(op)) != op_to_acop(op))
	{
		return handle_error(o2pt, RSC_ORIGINATOR_HAS_NO_PRIVILEGE, "originator has no privilege");
	}

	return RSC_OK;
}

/**
 * @brief validate acr attribute especially acip
 * @param o2pt oneM2M request primitive
 * @param acr_attr acr attribute cJSON object
 * @return RSC_OK if valid, else if invalid
 */
int validate_acr(oneM2MPrimitive *o2pt, cJSON *acr_attr)
{
	cJSON *acr = NULL;
	cJSON *acop = NULL;
	cJSON *acco = NULL;
	cJSON *acip = NULL;
	cJSON *ipv4 = NULL;
	char *ptr = NULL;

	int mask = 0;

	cJSON_ArrayForEach(acr, acr_attr)
	{
		acop = cJSON_GetObjectItem(acr, "acop");
		if (acop->valueint > 63 || acop->valueint < 0)
		{
			return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `acop` is invalid");
		}
		acco = cJSON_GetObjectItem(acr, "acco");

		if (acco)
		{
			if ((acip = cJSON_GetObjectItem(acco, "acip")))
			{
				cJSON_ArrayForEach(ipv4, cJSON_GetObjectItem(acip, "ipv4"))
				{
					if ((ptr = strchr(ipv4->valuestring, '/')))
					{
						mask = atoi(ptr + 1);
						if (mask > 32 || mask < 0)
						{
							return handle_error(o2pt, RSC_BAD_REQUEST, "ip in attribute `acip` is invalid");
						}
						*ptr = '\0';
					}
					struct sockaddr_in sa;
					if (inet_pton(AF_INET, ipv4->valuestring, &(sa.sin_addr)) != 1)
					{
						*ptr = '/';
						return handle_error(o2pt, RSC_BAD_REQUEST, "ip in attribute `acip` is invalid");
					}
					if (ptr)
						*ptr = '/';
				}
			}
		}
	}

	return RSC_OK;
}

int register_remote_cse()
{
	char buf[1024];
	HTTPRequest *req = (HTTPRequest *)calloc(sizeof(HTTPRequest), 1);
	HTTPResponse *res = (HTTPResponse *)calloc(sizeof(HTTPResponse), 1);
	int status_code = 0;
	sprintf(buf, "/%s/%s", REMOTE_CSE_NAME, CSE_BASE_RI);

	req->method = "GET";
	req->uri = strdup(buf);
	req->qs = NULL;
	req->prot = strdup("HTTP/1.1");
	req->payload = NULL;
	req->payload_size = 0;
	req->headers = calloc(sizeof(header_t), 1);
	add_header("X-M2M-Origin", "/" CSE_BASE_RI, req->headers);
	add_header("X-M2M-RI", "check-cse-registered", req->headers);
	add_header("Accept", "application/json", req->headers);
	// add_header("Content-Type", "application/json", req->headers);
	add_header("X-M2M-RVI", from_rvi(CSE_RVI), req->headers);

	send_http_request(REMOTE_CSE_HOST, REMOTE_CSE_PORT, req, res);
	logger("UTIL", LOG_LEVEL_DEBUG, "Remote CSE registration check: %d", res->status_code);
	status_code = res->status_code;

	if (status_code == 999 || status_code == 500)
	{
		logger("UTIL", LOG_LEVEL_ERROR, "Remote CSE is not running");
		free_HTTPRequest(req);
		free_HTTPResponse(res);
		return status_code;
	}

	if (res->status_code != 200)
	{
		free_HTTPRequest(req);
		free_HTTPResponse(res);
		logger("UTIL", LOG_LEVEL_DEBUG, "Remote CSE is not registered");
		req = (HTTPRequest *)calloc(sizeof(HTTPRequest), 1);
		res = (HTTPResponse *)calloc(sizeof(HTTPResponse), 1);
		// register MN-CSE
		req->method = "POST";
		sprintf(buf, "/%s", REMOTE_CSE_NAME);
		req->uri = strdup(buf);
		req->qs = NULL;
		req->prot = strdup("HTTP/1.1");

		req->headers = calloc(sizeof(header_t), 1);
		add_header("X-M2M-Origin", "/" CSE_BASE_RI, req->headers);
		add_header("X-M2M-RI", "register-cse", req->headers);
		add_header("Accept", "application/json", req->headers);
		add_header("Content-Type", "application/json;ty=16", req->headers);
		add_header("X-M2M-RVI", from_rvi(CSE_RVI), req->headers);

		cJSON *root = cJSON_CreateObject();
		cJSON *csr = cJSON_Duplicate(rt->cb->obj, 1);
		init_csr(csr);
		cJSON_AddItemToObject(root, get_resource_key(RT_CSR), csr);
		req->payload = cJSON_PrintUnformatted(root);
		cJSON_Delete(root);

		req->payload_size = strlen(req->payload);
		send_http_request(REMOTE_CSE_HOST, REMOTE_CSE_PORT, req, res);
		logger("UTIL", LOG_LEVEL_DEBUG, "Remote CSE registration: %d", res->status_code);
		logger("UTIL", LOG_LEVEL_DEBUG, "Remote CSE registration: %s", res->payload);
		char *rsc = 0;
		if ((rsc = search_header(res->headers, "x-m2m-rsc")))
		{
			logger("UTIL", LOG_LEVEL_DEBUG, "Remote CSE registration: %s", rsc);
			if (atoi(rsc) != RSC_CREATED)
			{
				logger("UTIL", LOG_LEVEL_ERROR, "Remote CSE registration failed");
				free_HTTPRequest(req);
				free_HTTPResponse(res);
				return atoi(rsc);
			}
		}
		else
		{
			logger("UTIL", LOG_LEVEL_ERROR, "Remote CSE registration failed");
			free_HTTPRequest(req);
			free_HTTPResponse(res);
			return -1;
		}
	}

	free_HTTPRequest(req);
	free_HTTPResponse(res);

	return 0;
}

int create_local_csr()
{
	char buf[256] = {0};

	HTTPRequest *req = (HTTPRequest *)malloc(sizeof(HTTPRequest));
	HTTPResponse *res = (HTTPResponse *)malloc(sizeof(HTTPResponse));

	req->method = "GET";
	req->uri = strdup("/" REMOTE_CSE_NAME);
	req->qs = NULL;
	req->prot = strdup("HTTP/1.1");
	req->payload = NULL;
	req->payload_size = 0;
	req->headers = calloc(sizeof(header_t), 1);
	add_header("X-M2M-Origin", "/" CSE_BASE_RI, req->headers);
	add_header("X-M2M-RI", "retrieve-cb", req->headers);
	add_header("Accept", "application/json", req->headers);
	// add_header("Content-Type", "application/json", req->headers);
	add_header("X-M2M-RVI", "2a", req->headers);

	send_http_request(REMOTE_CSE_HOST, REMOTE_CSE_PORT, req, res);

	if (res->status_code != 200)
	{
		logger("MAIN", LOG_LEVEL_ERROR, "Remote CSE is not online : %d", res->status_code);
		free_HTTPRequest(req);
		free_HTTPResponse(res);
		return res->status_code;
	}

	cJSON *root = cJSON_Parse(res->payload);
	cJSON *remote_cb = cJSON_GetObjectItem(root, get_resource_key(RT_CSE));

	if (!remote_cb)
	{
		logger("MAIN", LOG_LEVEL_ERROR, "Remote CSE not valid");
		free_HTTPRequest(req);
		free_HTTPResponse(res);
		return -1;
	}

	cJSON *pjson = NULL;
	cJSON *remote_ri = cJSON_GetObjectItem(remote_cb, "ri");
	cJSON *remote_rn = cJSON_GetObjectItem(remote_cb, "rn");
	cJSON *remote_csi = cJSON_GetObjectItem(remote_cb, "csi");
	if (!remote_rn || !remote_csi)
	{
		logger("UTIL", LOG_LEVEL_ERROR, "Remote CSE not valid");
		free_HTTPRequest(req);
		free_HTTPResponse(res);
		return -1;
	}

	cJSON *csr = cJSON_CreateObject();
	add_general_attribute(csr, rt->cb, RT_CSR);
	cJSON_DeleteItemFromObject(csr, "ri");
	cJSON_DeleteItemFromObject(csr, "rn");

	if ((pjson = cJSON_GetObjectItem(remote_cb, "cst")))
	{
		cJSON_AddItemToObject(csr, "cst", cJSON_Duplicate(pjson, 1));
	}
	if ((pjson = cJSON_GetObjectItem(remote_cb, "ri")))
	{
		cJSON_AddItemToObject(csr, "ri", cJSON_Duplicate(pjson, 1));
		cJSON_AddItemToObject(csr, "rn", cJSON_Duplicate(pjson, 1));
	}
	// if(pjson = cJSON_GetObjectItem(remote_cb, "rn")){
	// }
	if ((pjson = cJSON_GetObjectItem(remote_cb, "dcse")))
	{
		cJSON_AddItemToObject(csr, "dcse", cJSON_Duplicate(pjson, 1));
	}
	if ((pjson = cJSON_GetObjectItem(remote_cb, "poa")))
	{
		cJSON_AddItemToObject(csr, "poa", cJSON_Duplicate(pjson, 1));
	}
	if ((pjson = cJSON_GetObjectItem(remote_cb, "rr")))
	{
		cJSON_AddItemToObject(csr, "rr", cJSON_Duplicate(pjson, 1));
	}
	if ((pjson = cJSON_GetObjectItem(remote_cb, "srv")))
	{
		cJSON_AddItemToObject(csr, "srv", cJSON_Duplicate(pjson, 1));
	}
	cJSON_AddItemToObject(csr, "csi", cJSON_CreateString(remote_csi->valuestring));

	sprintf(buf, "%s/%s", remote_csi->valuestring, remote_rn->valuestring);
	cJSON_AddItemToObject(csr, "cb", cJSON_CreateString(buf));

	cJSON_Delete(root);

	// int rsc = validate_csr(o2pt, parent_rtnode, csr, OP_CREATE);
	// if(rsc != RSC_OK){
	// 	cJSON_Delete(root);
	// 	return rsc;
	// }

	char *ptr = malloc(1024);
	sprintf(ptr, "%s/%s", CSE_BASE_NAME, cJSON_GetObjectItem(csr, "rn")->valuestring);
	cJSON_AddItemToObject(csr, "uri", cJSON_CreateString(ptr));
	int result = db_store_resource(csr, ptr);
	if (result == -1)
	{
		cJSON_Delete(root);
		free(ptr);
		ptr = NULL;
		return RSC_INTERNAL_SERVER_ERROR;
	}
	free(ptr);
	ptr = NULL;

	RTNode *rtnode = create_rtnode(csr, RT_CSR);
	add_child_resource_tree(rt->cb, rtnode);

	rt->registrar_csr = rtnode;

	free_HTTPRequest(req);
	return 0;
}

int deRegister_csr()
{
	RTNode *rtnode = rt->cb->child;
	RTNode *next_rt = NULL;
	Protocol prot = PROT_HTTP;
	char *path = NULL;
	char *host = NULL;
	int port = 0;

	while (rtnode)
	{
		next_rt = rtnode->sibling_right;
		if (rtnode->ty == RT_CSR)
		{
			char buf[1024] = {0};
			cJSON *csr = rtnode->obj;
			cJSON *csi = cJSON_GetObjectItem(csr, "csi");
			cJSON *cb = cJSON_GetObjectItem(csr, "cb");

			cJSON *poa = cJSON_GetObjectItem(csr, "poa");
			cJSON *pjson = NULL;
			oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(sizeof(oneM2MPrimitive), 1);
			o2pt->op = OP_DELETE;
			o2pt->fr = strdup("/" CSE_BASE_RI);
			sprintf(buf, "%s/%s", csi->valuestring, CSE_BASE_RI);
			o2pt->to = strdup(buf);
			o2pt->rqi = strdup("delete-csr");
			o2pt->rvi = CSE_RVI;

			forwarding_onem2m_resource(o2pt, rtnode);
			free_o2pt(o2pt);

			db_delete_onem2m_resource(rtnode);
		}

		rtnode = next_rt;
	}
	return 0;
}

/**
 * @brief update dcse of remote cse
 * @param created_rtnode created remote cse\nIt will be excluded from update
 * @return 0 if success, -1 if failed
 */
int update_remote_csr_dcse(RTNode *skip_rtnode)
{
	NodeList *node = rt->csr_list;
	cJSON *root = NULL;
	cJSON *dcse = cJSON_GetObjectItem(rt->cb->obj, "dcse");
	if (!dcse)
	{
		logger("UTIL", LOG_LEVEL_DEBUG, "dcse not found");
		return -1;
	}
	root = cJSON_CreateObject();
	dcse = cJSON_Duplicate(dcse, 1);
	cJSON *csr = cJSON_CreateObject();
	cJSON_AddItemToObject(root, get_resource_key(RT_CSR), csr);
	cJSON_AddItemToObject(csr, "dcse", dcse);

	RTNode *rtnode = NULL;
	oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(1, sizeof(oneM2MPrimitive));
	o2pt->op = OP_UPDATE;
	o2pt->fr = strdup("/" CSE_BASE_RI);
	o2pt->rqi = strdup("update-csr");
	o2pt->rvi = CSE_RVI;
	o2pt->request_pc = root;
	logger("UTIL", LOG_LEVEL_DEBUG, "skip_rtnode: %s", skip_rtnode->uri);
	while (node)
	{
		rtnode = node->rtnode;
		node = node->next;
		char buf[1024] = {0};
		logger("UTIL", LOG_LEVEL_DEBUG, "rtnode: %s", rtnode->uri);
		if (rtnode == skip_rtnode)
			continue;

		sprintf(buf, "%s/%s", cJSON_GetObjectItem(rtnode->obj, "csi")->valuestring, CSE_BASE_RI);
		o2pt->to = strdup(buf);

		forwarding_onem2m_resource(o2pt, rtnode);
	}

	free_o2pt(o2pt);
	return 0;
}

/**
 * @brief create remote cse
 * @param poa poa of remote cse(SP_RELATIVE)
 * @param cse_name name of remote cse
 */
int create_remote_cba(char *poa, char **cbA_url)
{
	logger("UTIL", LOG_LEVEL_DEBUG, "create_remote_cba");
	Protocol prot = 0;
	char *host = NULL;
	int port = 0;
	char *path = NULL;
	char buf[1024] = {0};
	ResourceAddressingType rat = checkResourceAddressingType(poa);
	if (rat == SP_RELATIVE)
	{
		RTNode *csr = find_csr_rtnode_by_uri(poa);
		if (!csr)
		{
			logger("UTIL", LOG_LEVEL_ERROR, "csr not found");
			return -1;
		}
		oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(sizeof(oneM2MPrimitive), 1);
		o2pt->fr = strdup("/" CSE_BASE_RI);
		o2pt->to = strdup(cJSON_GetObjectItem(csr->obj, "cb")->valuestring);
		o2pt->op = OP_CREATE;
		o2pt->ty = RT_CBA;
		o2pt->rqi = strdup("create-cba");
		o2pt->rvi = CSE_RVI;

		cJSON *root = cJSON_CreateObject();
		cJSON *cba = cJSON_CreateObject();
		cJSON_AddItemToObject(root, get_resource_key(RT_CBA), cba);
		cJSON_AddItemToObject(cba, "lnk", cJSON_CreateString("/" CSE_BASE_RI "/" CSE_BASE_NAME));
		cJSON *srv = cJSON_Duplicate(cJSON_GetObjectItem(rt->cb->obj, "srv"), true);
		cJSON_AddItemToObject(cba, "srv", srv);
		// cJSON_AddItemToObject(cba, "ty", cJSON_CreateNumber(RT_CBA));

		o2pt->request_pc = root;
		if (forwarding_onem2m_resource(o2pt, csr) >= 4000)
		{
			free_o2pt(o2pt);
			logger("UTIL", LOG_LEVEL_ERROR, "Creation failed");
			return -1;
		}
		if (o2pt->response_pc)
		{
			// logger("UTIL", LOG_LEVEL_DEBUG, "cba_target: %s", cJSON_PrintUnformatted(result));
			root = cJSON_GetObjectItem(o2pt->response_pc, get_resource_key(RT_CBA));
			sprintf(buf, "%s/%s", poa, cJSON_GetObjectItem(root, "ri")->valuestring);
			*cbA_url = strdup(buf);
		}
		else
		{
			logger("UTIL", LOG_LEVEL_ERROR, "%s", cJSON_GetErrorPtr());
		}
		free_o2pt(o2pt);
	}
	else if (rat == ABSOLUTE)
	{
		// ABSOLUTE
	}
	else if (rat == -1)
	{
		logger("UTIL", LOG_LEVEL_ERROR, "poa is invalid");
	}

	cJSON *at = cJSON_GetObjectItem(rt->cb->obj, "at");
	if (!at)
	{
		at = cJSON_CreateArray();
		cJSON_AddItemToObject(rt->cb->obj, "at", at);
	}
	cJSON_AddItemToArray(at, cJSON_CreateString(*cbA_url));
	db_update_resource(rt->cb->obj, CSE_BASE_RI, RT_CSE);
	logger("UTIL", LOG_LEVEL_DEBUG, "cbA Created/ target: %s", *cbA_url);
	return 0;
}

int handle_annc_create(RTNode *parent_rtnode, cJSON *resource_obj, cJSON *at_obj, cJSON *final_at)
{
	if (!parent_rtnode)
		return 1;
	if (!final_at)
		return 1;
	if (at_obj == NULL)
		return 0;
	cJSON *at = NULL;
	char *at_str = NULL;
	cJSON_ArrayForEach(at, cJSON_GetObjectItem(resource_obj, "at"))
	{
		at_str = create_remote_annc(parent_rtnode, resource_obj, at->valuestring, false);
		if (!at_str)
		{
			continue;
		}
		cJSON_AddItemToArray(final_at, cJSON_CreateString(at_str));
	}

	return 0;
}

int handle_annc_update(RTNode *target_rtnode, cJSON *at_obj, cJSON *final_at)
{
	if (at_obj == NULL)
		return 0;

	cJSON *original_at_list = cJSON_GetObjectItem(target_rtnode->obj, "at");
	cJSON *register_at_list = cJSON_CreateArray();
	cJSON *delete_at_list = cJSON_CreateArray();
	cJSON *new_at = NULL;
	cJSON *at = NULL;
	// remove at which was already there
	cJSON_ArrayForEach(new_at, at_obj)
	{
		cJSON_ArrayForEach(at, original_at_list)
		{
			logger("UTIL", LOG_LEVEL_DEBUG, "new_at: %s, at: %s", new_at->valuestring, at->valuestring);
			if (checkResourceCseID(at->valuestring, new_at->valuestring))
			{
				// if already registered
				break;
			}
		}
		if (at == NULL)
		{
			cJSON_AddItemToArray(register_at_list, cJSON_CreateString(new_at->valuestring));
		}
	}
	// remove at which was not in new at list
	cJSON_ArrayForEach(at, original_at_list)
	{
		cJSON_ArrayForEach(new_at, at_obj)
		{
			if (checkResourceCseID(at->valuestring, new_at->valuestring))
			{
				break;
			}
		}
		if (new_at == NULL)
		{
			cJSON_AddItemToArray(delete_at_list, cJSON_CreateString(at->valuestring));
		}
		else
		{
			cJSON_AddItemToArray(final_at, cJSON_CreateString(at->valuestring));
		}
	}

	logger("UTIL", LOG_LEVEL_DEBUG, "register_at_list: %s", cJSON_PrintUnformatted(register_at_list));
	logger("UTIL", LOG_LEVEL_DEBUG, "delete_at_list: %s", cJSON_PrintUnformatted(delete_at_list));

	// deregister removed remote aea
	if (cJSON_GetArraySize(delete_at_list) > 0)
	{
		deregister_remote_annc(target_rtnode, delete_at_list);
	}

	cJSON_Delete(delete_at_list);
	char *at_str = NULL;

	if (cJSON_GetArraySize(register_at_list) > 0)
	{
		cJSON *pjson = NULL;
		cJSON_ArrayForEach(pjson, register_at_list)
		{
			at_str = create_remote_annc(target_rtnode->parent, target_rtnode->obj, pjson->valuestring, false);
			if (!at_str)
			{
				continue;
			}

			cJSON_AddItemToArray(final_at, cJSON_CreateString(at_str));
		}
	}
	return 0;
}

/**
 * @brief delete remote annc resource
 * @param target_rtnode target rtnode
 * @param delete_at_list cJSON Array of deleting announceTo
 * @return 0 on success, -1 on failure
 */
int deregister_remote_annc(RTNode *target_rtnode, cJSON *delete_at_list)
{
	logger("UTIL", LOG_LEVEL_DEBUG, "deregister_remote_annc");
	char buf[256] = {0};
	bool annc = false;

	// Check Parent Resource has attribute at
	cJSON *at = NULL;
	cJSON_ArrayForEach(at, delete_at_list)
	{
		logger("UTIL", LOG_LEVEL_DEBUG, "at: %s", at->valuestring);
		if (at->valuestring[0] == '/')
		{
			oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(sizeof(oneM2MPrimitive), 1);
			o2pt->fr = strdup("/" CSE_BASE_RI);
			o2pt->to = strdup(at->valuestring);
			o2pt->op = OP_DELETE;
			o2pt->ty = target_rtnode->ty + 10000;
			o2pt->rqi = strdup("delete-annc");
			o2pt->rvi = CSE_RVI;
			forwarding_onem2m_resource(o2pt, find_csr_rtnode_by_uri(at->valuestring));

			if (o2pt->rsc != RSC_DELETED)
			{
				logger("UTIL", LOG_LEVEL_ERROR, "Deletion failed");
				return -1;
			}
			free_o2pt(o2pt);
		}
		char *tokPtr;
		char *ptr = strtok_r(at->valuestring + 1, "/", &tokPtr);
		removeChildAnnc(target_rtnode, at->valuestring);
		*ptr = '/';
	}
	return 0;
}

void removeChildAnnc(RTNode *parent_rtnode, char *at)
{
	RTNode *rtnode = parent_rtnode->child;
	cJSON *at_obj = NULL;
	logger("UTIL", LOG_LEVEL_DEBUG, "removeChildAnnc %s", parent_rtnode->uri);
	while (rtnode)
	{
		if (rtnode->child)
		{
			removeChildAnnc(rtnode, at);
		}
		if (rtnode->ty == RT_CNT)
		{
			RTNode *cin_list = db_get_cin_rtnode_list(rtnode);
			RTNode *cin = cin_list;

			while (cin)
			{
				cJSON *pjson = NULL;
				at_obj = cJSON_GetObjectItem(cin->obj, "at");
				if (!at_obj)
				{
					cin = cin->sibling_right;
					continue;
				}
				cJSON *new_at = cJSON_CreateArray();
				cJSON_ArrayForEach(pjson, at_obj)
				{
					if (!strncmp(pjson->valuestring, at, strlen(at)))
					{
						continue;
					}
					cJSON_AddItemToArray(new_at, cJSON_CreateString(pjson->valuestring));
				}
				cJSON_DeleteItemFromObject(cin->obj, "at");
				cJSON_AddItemToObject(cin->obj, "at", new_at);
				db_update_resource(cin->obj, cJSON_GetObjectItem(cin->obj, "ri")->valuestring, cin->ty);
				cin = cin->sibling_right;
			}

			free_rtnode_list(cin_list);
		}
		cJSON *pjson = NULL;
		cJSON *new_at = cJSON_CreateArray();
		at_obj = cJSON_GetObjectItem(rtnode->obj, "at");
		if (!at_obj)
		{
			rtnode = rtnode->sibling_right;
			continue;
		}
		cJSON_ArrayForEach(pjson, at_obj)
		{
			if (checkResourceCseID(pjson->valuestring, at))
			{
				continue;
			}
			cJSON_AddItemToArray(new_at, cJSON_CreateString(pjson->valuestring));
		}
		cJSON_DeleteItemFromObject(rtnode->obj, "at");
		cJSON_AddItemToObject(rtnode->obj, "at", new_at);
		db_update_resource(rtnode->obj, cJSON_GetObjectItem(rtnode->obj, "ri")->valuestring, rtnode->ty);
		rtnode = rtnode->sibling_right;
	}
}

void announce_to_annc(RTNode *target_rtnode)
{
	logger("UTIL", LOG_LEVEL_DEBUG, "announce_to_annc");
	cJSON *at_list = cJSON_GetObjectItem(target_rtnode->obj, "at");
	cJSON *pjson;
	if (at_list)
	{
		cJSON *at = NULL;
		cJSON *aa_list = cJSON_GetObjectItem(target_rtnode->obj, "aa");
		cJSON *aa = NULL;
		cJSON *root = cJSON_CreateObject();
		cJSON *resource = cJSON_CreateObject();
		cJSON_AddItemToObject(root, get_resource_key(target_rtnode->ty + 10000), resource);
		cJSON_ArrayForEach(aa, aa_list)
		{
			if ((pjson = cJSON_GetObjectItem(target_rtnode->obj, aa->valuestring)))
			{
				cJSON_AddItemToObject(resource, aa->valuestring, cJSON_Duplicate(pjson, 1));
			}
		}
		if ((pjson = cJSON_GetObjectItem(target_rtnode->obj, "lbl")))
		{
			cJSON_AddItemToObject(resource, "lbl", cJSON_Duplicate(pjson, 1));
		}
		oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(1, sizeof(oneM2MPrimitive));
		o2pt->op = OP_UPDATE;
		o2pt->fr = strdup("/" CSE_BASE_RI);
		o2pt->ty = target_rtnode->ty + 10000;
		o2pt->rvi = CSE_RVI;
		o2pt->rqi = strdup("update_announce");
		o2pt->request_pc = root;
		o2pt->isForwarding = true;
		cJSON_ArrayForEach(at, at_list)
		{
			logger("UTIL", LOG_LEVEL_INFO, "at %s", at->valuestring);
			if (at->valuestring[0] == '/')
			{
				o2pt->to = strdup(at->valuestring);
				forwarding_onem2m_resource(o2pt, find_csr_rtnode_by_uri(at->valuestring));
				free(o2pt->to);
				o2pt->to = NULL;
			}
		}
		o2pt->request_pc = NULL;
		free_o2pt(o2pt);
		cJSON_Delete(root);
	}
}

/**
 * @brief Parse m2m:pointOfAccess
 * @param poa_str m2m:pointOfAccess string
 * @param prot Protocol
 * @param host Host - char pointer
 * @param port Port - Integer(Initalized with 0)
 * @param path Path - char pointer
 * @remark all parameters should be freed by caller
 * @return 0 on success, -1 on failure
 */
int parsePoa(char *poa_str, Protocol *prot, char **host, int *port, char **path)
{
	char *p = strdup(poa_str);
	char *ptr = NULL;
	if (!strncmp(poa_str, "http://", 7))
	{
		*prot = PROT_HTTP;
	}
	else if (!strncmp(poa_str, "mqtt://", 7))
	{
		*prot = PROT_MQTT;
	}
	else if (!strncmp(poa_str, "coap://", 7))
	{
		*prot = PROT_COAP;
	}
	else if (!strncmp(poa_str, "coaps://", 8))
	{
		*prot = PROT_COAPS;
	}
	else
	{
		free(p);
		return -1;
	}
	if (*prot == PROT_COAPS)
	{
		ptr = strchr(p + 8, ':');
		if (ptr)
		{
			*ptr = '\0';
			*port = atoi(ptr + 1);
		}
		*host = strdup(p + 8);
		if (ptr)
			*ptr = ':';
		ptr = strchr(p + 8, '/');
	}
	else
	{
		ptr = strchr(p + 7, ':');
		if (ptr)
		{
			*ptr = '\0';
			*port = atoi(ptr + 1);
		}
		*host = strdup(p + 7);
		if (ptr)
			*ptr = ':';
		ptr = strchr(p + 7, '/');
	}
	if (ptr)
	{
		*path = strdup(ptr);
		*ptr = '\0';
	}
	else
	{
		*path = strdup("/");
	}
	free(p);
	if (*prot == PROT_HTTP && port == 0)
	{
		*port = 80;
	}
	else if (*prot == PROT_MQTT && port == 0)
	{
		*port = 1883;
	}
	else if (*prot == PROT_COAP && port == 0)
	{
		*port = 5683;
	}
	else if (*prot == PROT_COAPS && port == 0)
	{
		*port = 5684;
	}
	return 0;
}

ResourceAddressingType checkResourceAddressingType(char *uri)
{
	if (uri == NULL)
	{
		return -1;
	}
	if (uri[0] == '/' && uri[1] == '/')
	{
		return ABSOLUTE;
	}
	else if (uri[0] == '/')
	{
		return SP_RELATIVE;
	}
	else if (strncmp(uri, "http://", 7) == 0 || strncmp(uri, "mqtt://", 7) == 0 || strcmp(uri, "coap://") == 0 || strcmp(uri, "coaps://") == 0)
	{
		return PROTOCOL_BINDING;
	}
	else
	{
		return CSE_RELATIVE;
	}

	return -1;
}

/**
 * @brief check csi for resource
 * @param resourceUri resource uri
 * @param cseID cseID
 * @return true if resource is under cseID, false if not
 */
bool checkResourceCseID(char *resourceUri, char *cseID)
{
	int cseIdLen = strlen(cseID);
	int resourceUriLen = strlen(resourceUri);
	if (resourceUriLen <= cseIdLen)
		return false;
	if (resourceUri[cseIdLen] == '/' && strncmp(cseID, resourceUri, cseIdLen) == 0)
	{
		return true;
	}
	return false;
}

bool isAptEnc(oneM2MPrimitive *o2pt, RTNode *target_rtnode, RTNode *sub_rtnode)
{
	cJSON *enc = cJSON_GetObjectItem(sub_rtnode->obj, "enc");
	cJSON *atr = cJSON_GetObjectItem(enc, "atr");
	cJSON *chty = cJSON_GetObjectItem(enc, "chty");

	if (atr)
	{
		cJSON *pjson = NULL;
		cJSON_ArrayForEach(pjson, atr)
		{
			if (!cJSON_GetObjectItem(cJSON_GetObjectItem(o2pt->request_pc, get_resource_key(target_rtnode->ty)), pjson->valuestring))
			{
				return false;
			}
		}
	}

	if (chty && !FC_isAptChty(chty, o2pt->ty))
	{
		logger("UTIL", LOG_LEVEL_DEBUG, "chty is not matched");
		return false;
	}

	return true;
}

bool isValidChildType(ResourceType parent, ResourceType child)
{
	switch (parent)
	{
	case RT_ACP:
		if (child == RT_SUB)
			return true;
		break;
	case RT_AE:
		if (child == RT_CNT || child == RT_SUB || child == RT_ACP || child == RT_GRP || child == RT_TS)
			return true;
		break;
	case RT_CNT:
		if (child == RT_CIN || child == RT_SUB || child == RT_CNT || child == RT_SMD || child == RT_FCNT || child == RT_TS)
			return true;
		break;
	case RT_CIN:
		if (child == RT_SMD)
			return true;
		break;
	case RT_CSE:
		if (child == RT_ACP || child == RT_AE || child == RT_CNT || child == RT_GRP || child == RT_SUB ||
			child == RT_CSR || child == RT_NOD || child == RT_MGMTOBJ || child == RT_CBA)
			return true;
		break;
	case RT_GRP:
		if (child == RT_SUB || child == RT_SMD)
			return true;
		break;
	case RT_CSR:
		if (child == RT_CNT || child == RT_CNTA || child == RT_FCNT || child == RT_GRP || child == RT_GRPA || child == RT_ACP ||
			child == RT_ACPA || child == RT_AE || child == RT_SUB || child == RT_AEA)
			return true;
		break;
	case RT_SUB:
		break;
	case RT_ACPA:
		if (child == RT_SUB)
			return true;
		break;
	case RT_AEA:
		if (child == RT_SUB || child == RT_CNT || child == RT_CNTA || child == RT_GRP || child == RT_GRPA || child == RT_ACP || child == RT_ACPA)
			return true;
		break;
	case RT_CNTA:
		if (child == RT_CNT || child == RT_CNTA || child == RT_CIN || child == RT_CINA || child == RT_SUB)
			return true;
		break;
	case RT_CBA:
		if (child == RT_CNT || child == RT_CNTA || child == RT_GRP || child == RT_GRPA || child == RT_ACP || child == RT_ACPA || child == RT_SUB || child == RT_AEA)
			return true;
		break;
	case RT_GRPA:
		if (child == RT_SUB)
			return true;
		break;
	}
	return false;
}