#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <limits.h>
#include <signal.h>
#include "onem2m.h"
#include "jsonparser.h"
#include "dbmanager.h"
#include "httpd.h"
#include "cJSON.h"
#include "util.h"
#include "config.h"
#include "onem2mTypes.h"

#ifdef ENABLE_MQTT
#include "mqttClient.h"
#endif

#ifdef UPPERTESTER
#include "upperTester.h"
#endif

#include "coap.h"
#include "sdt.h"

ResourceTree *rt;
RTNode *registrar_csr = NULL;

#if MONO_THREAD == 0
pthread_mutex_t main_lock;
pthread_mutex_t csr_lock;
pthread_mutexattr_t Attr;

#endif

void route(oneM2MPrimitive *o2pt);
void stop_server(int sig);
void log_runtime(double start);
cJSON *ATTRIBUTES;

static const char *AE_M[]  = { "api", "rr", NULL };
static const char *AE_O[]  = { "rn", "lbl", "acpi", "et", "daci", "at", "aa", "ast",
                               "poa", "apn", "or", "nl", "csz", "esi", "mei", "srv", NULL };
static const char *ACP_M[] = { "pv", "pvs", NULL };
static const char *ACP_O[] = { "rn", "lbl", "et", "at", "aa", "ast", NULL };
static const char *SUB_M[] = { "nu", NULL };
static const char *SUB_O[] = { "rn", "lbl", "acpi", "et", "cr", "exc", "gpi", "nfu",
                               "bn", "rl", "psn", "pn", "nsp", "nct", "nec", "su", NULL };
static const char *GRP_M[] = { "mid", NULL };
static const char *GRP_O[] = { "rn", "lbl", "acpi", "et", "daci", "at", "aa", "ast",
                               "mnm", "spty", "macp", "csy", "gn", NULL };
static const char *CSR_M[] = { "cb", "rr", "srv", NULL };
static const char *CSR_O[] = { "rn", "lbl", "acpi", "et", "at", "aa", "ast", "cst",
                               "poa", "csi", "mei", "nl", "csz", "dcse", NULL };
static const char *CNT_O[] = { "rn", "lbl", "acpi", "et", "daci", "at", "aa", "ast",
                               "cr", "mni", "mbs", "mia", "or", "disr", "li", NULL };
static const char *CIN_M[] = { "con", NULL };
static const char *CIN_O[] = { "rn", "lbl", "et", "at", "aa", "cr", "cnf", "or", NULL };
static const char *TS_O[]  = { "rn", "lbl", "acpi", "et", "daci", "at", "aa", "ast",
                               "cr", "mni", "mbs", "mia", "pei", "peid", "mdd", "mdn",
                               "mdt", "or", "cnf", NULL };
static const char *TSI_M[] = { "dgt", "con", NULL };
static const char *TSI_O[] = { "rn", "lbl", "et", "snr", NULL };
static const char *FCNT_M[]= { "cnd", NULL };
static const char *FCNT_O[]= { "rn", "lbl", "acpi", "et", "daci", "at", "aa", "ast",
                               "cr", "or", "nl", "mni", "mia", "mbs", NULL };

const CreateAttrDef M_TABLE[] = {
	{ RT_AE,   AE_M,   AE_O   },
	{ RT_ACP,  ACP_M,  ACP_O  },
	{ RT_SUB,  SUB_M,  SUB_O  },
	{ RT_GRP,  GRP_M,  GRP_O  },
	{ RT_CSR,  CSR_M,  CSR_O  },
	{ RT_CNT,  NULL,   CNT_O  },
	{ RT_CIN,  CIN_M,  CIN_O  },
	{ RT_TS,   NULL,   TS_O   },
	{ RT_TSI,  TSI_M,  TSI_O  },
	{ RT_FCNT, FCNT_M, FCNT_O },
	{ RT_MIXED, NULL,  NULL   },
};

static const char *AE_ANNC_MA[]  = { "et", "acpi", "lbl", "ast", "srv", NULL };
static const char *AE_ANNC_OA[]  = { "daci", "loc", "apn", "api", "aei", "poa", "or",
                                     "nl", "rr", "csz", "regs", "trps", "scp", NULL };
static const char *CNT_ANNC_MA[] = { "et", "acpi", "lbl", "ast", NULL };
static const char *CNT_ANNC_OA[] = { "daci", "loc", "mni", "mbs", "mia", "li", "or", "disr", NULL };
static const char *CIN_ANNC_MA[] = { "lbl", "ast", NULL };
static const char *CIN_ANNC_OA[] = { "loc", "cnf", "conr", "or", "con", NULL };
static const char *GRP_ANNC_MA[] = { "et", "acpi", "lbl", "ast", NULL };
static const char *GRP_ANNC_OA[] = { "daci", "spty", "cnm", "mnm", "mid", "macp", "mtv",
                                     "csy", "gn", "ssi", "nar", "scen", "scal", "mt", NULL };
static const char *ACP_ANNC_MA[] = { "et", "lbl", "ast", "pv", "pvs", "adri", "apri", "airi", NULL };
static const char *ACP_ANNC_OA[] = { NULL };
static const char *TS_ANNC_MA[]  = { "et", "acpi", "lbl", "ast", NULL };
static const char *TS_ANNC_OA[]  = { "daci", "loc", "mni", "mbs", "mia", "pei", "peid",
                                     "mdn", "mdt", "or", "cnf", NULL };
static const char *FCNT_ANNC_MA[]= { "et", "acpi", "lbl", "ast", "cnd", NULL };
static const char *FCNT_ANNC_OA[]= { "daci", "loc", "or", "nl", "mni", "mia", "mbs",
                                     "cni", "cbs", NULL };
static const char *CSE_ANNC_MA[] = { "acpi", "lbl", "srv", NULL };   // et synthesised in create_remote_cba
static const char *CSE_ANNC_OA[] = { NULL };

const AnncAttrDef ANNC_ATTR_TABLE[] = {
	{ RT_AE,   AE_ANNC_MA,   AE_ANNC_OA   },
	{ RT_CNT,  CNT_ANNC_MA,  CNT_ANNC_OA  },
	{ RT_CIN,  CIN_ANNC_MA,  CIN_ANNC_OA  },
	{ RT_GRP,  GRP_ANNC_MA,  GRP_ANNC_OA  },
	{ RT_ACP,  ACP_ANNC_MA,  ACP_ANNC_OA  },
	{ RT_TS,   TS_ANNC_MA,   TS_ANNC_OA   },
	{ RT_FCNT, FCNT_ANNC_MA, FCNT_ANNC_OA },
	{ RT_CSE,  CSE_ANNC_MA,  CSE_ANNC_OA  },
	{ RT_MIXED, NULL, NULL },
};

char *PORT = SERVER_PORT;
int terminate = 0;
int call_stop = 0;

#ifdef ENABLE_MQTT
pthread_t mqtt;
int mqtt_thread_id;
#endif

#ifdef ENABLE_COAP
pthread_t coap;
int coap_thread_id;
#ifdef ENABLE_COAP_DTLS
extern char *cert_file;
extern char *key_file;
extern char *ca_file;
extern char *root_ca_file;
extern uint8_t *key;
extern ssize_t key_length;
extern int key_defined;

static ssize_t cmdline_read_key(char *arg, unsigned char **buf, size_t maxlen)
{
	size_t len = strnlen(arg, maxlen);
	if (len)
	{
		*buf = (unsigned char *)arg;
		return len;
	}

	/* Need at least one byte for the pre-shared key */
	logger("COAP", LOG_LEVEL_ERROR, "Invalid Pre-Shared Key specified\n");

	return -1;
}
#endif
#endif

int main(int argc, char **argv)
{
	signal(SIGINT, stop_server);
	logger_init();

#if MONO_THREAD == 0
	pthread_mutexattr_init(&Attr);
	pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&main_lock, &Attr);
	pthread_mutex_init(&csr_lock, NULL);
#endif
	// Attributes for resources
	// all attributes are verified in validate_sub_attr in util.c
	// if you want to add new attributes, you should add it to here
	// Request with null value will be regarded as valid because update request can have null value
	// So you should check it each resource validation
	// Attributes not added here will be regarded as invalid
	// Attributes with null value will be ignored

	ATTRIBUTES = cJSON_Parse(
		"{ \
		\"general\": {\"rn\": \"\", \"ri\": \"\", \"pi\": \"\", \"ct\": \"\", \"et\": \"\", \"lt\": \"\" , \"uri\": \"\" , \"acpi\": [\"\"], \"lbl\": [\"\"], \"ty\":0, \"memberOf\": [\"\"]}, \
		\"m2m:ae\": {\"api\": \"\", \"aei\" : \"\", \"rr\": true, \"poa\":[\"\"], \"apn\":\"\", \"srv\":[\"\"], \"at\":[\"\"], \"aa\":[\"\"], \"ast\":0}, \
		\"m2m:cnt\": {\"cr\": null, \"mni\":0, \"mbs\":0, \"mia\":0, \"st\":0, \"cni\":0, \"cbs\":0,\"at\":[\"\"], \"aa\":[\"\"], \"ast\":0}, \
		\"m2m:cin\": {\"cs\":0, \"cr\":null, \"con\":\"\", \"cnf\":\"\", \"st\":\"\",\"at\":[\"\"], \"aa\":[\"\"], \"ast\":0}, \
		\"m2m:acp\": {\"pv\":{\"acr\":[{\"acor\":[\"\"],\"acop\":0, \"acco\":[{\"acip\":{\"ipv4\":[\"\"], \"ipv6\":[\"\"]}}]}]}, \"pvs\":{\"acr\":[{\"acor\":[\"\"],\"acop\":0, \"acco\":[{\"acip\":{\"ipv4\":[\"\"], \"ipv6\":[\"\"]}}]}]}, \"at\":[\"\"], \"aa\":[\"\"], \"ast\":0}, \
		\"m2m:sub\": {\"enc\":{\"net\":[1], \"atr\":[\"\"], \"chty\":[0], \"md\":{\"num\":0, \"dur\":\"\"} }, \"exc\":0, \"nu\":[\"\"], \"gpi\":0, \"nfu\":0, \"bn\":0, \"rl\":0, \"sur\":0, \"nct\":0, \"cr\":\"\", \"su\":\"\"},\
		\"m2m:grp\": {\"gn\": \"\", \"cr\":\"\", \"mt\":0, \"cnm\":0, \"mnm\":0, \"mid\":[\"\"], \"macp\":[\"\"], \"mtv\":true, \"csy\":0, \"at\":[\"\"], \"aa\":[\"\"], \"ast\":0},\
		\"m2m:csr\": {\"cst\":0, \"poa\":[\"\"], \"cb\":\"\", \"dcse\":[\"\"], \"csi\":\"\", \"mei\":\"\", \"tri\":\"\", \"csz\":[\"\"], \"rr\":true, \"nl\":\"\", \"srv\":[\"\"], \"spi\":\"\"},\
		\"m2m:cb\": {\"cst\":0, \"csi\":\"\", \"srt\":[\"\"], \"rr\":true, \"poa\":[\"\"], \"srv\":[0], \"at\":[], \"aa\":[],\"ast\":0, \"spi\":\"//" CSE_BASE_SP_ID "\"}, \
		\"m2m:acpA\": {\"lnk\":\"\", \"pv\":{\"acr\":[{\"acor\":[\"\"],\"acop\":0, \"acco\":[{\"acip\":{\"ipv4\":[\"\"], \"ipv6\":[\"\"]}}]}]}, \"pvs\":{\"acr\":[{\"acor\":[\"\"],\"acop\":0, \"acco\":[{\"acip\":{\"ipv4\":[\"\"], \"ipv6\":[\"\"]}}]}]}, \"ast\":0}, \
		\"m2m:cbA\": {\"lnk\":\"\", \"cst\":0, \"csi\":\"\", \"srt\":[\"\"], \"poa\":[\"\"], \"srv\":[\"\"], \"rr\":true, \"ast\":0}, \
		\"m2m:aeA\": {\"lnk\":\"\", \"api\":\"\", \"aei\":\"\", \"rr\":true, \"poa\":[\"\"], \"apn\":\"\", \"srv\":[\"\"], \"ast\":0}, \
		\"m2m:cntA\": {\"lnk\":\"\", \"cr\":\"\", \"mni\":0, \"mbs\":0, \"st\":0, \"cni\":0, \"cbs\":0, \"ast\":0}, \
		\"m2m:grpA\": {\"lnk\":\"\", \"cr\":\"\", \"mt\":0, \"cnm\":0, \"mnm\":0, \"mid\":[\"\"], \"macp\":[\"\"], \"mtv\":true, \"csy\":0, \"gn\":\"\", \"at\":[\"\"], \"aa\":[\"\"], \"ast\":0}, \
		\"m2m:cinA\": {\"lnk\":\"\", \"cs\":0, \"cr\":\"\", \"con\":\"\", \"cnf\":\"\", \"st\":\"\", \"ast\":0}, \
		\"m2m:fcnt\": {\"cnd\":\"\", \"oref\":\"\", \"nl\":\"\", \"cr\":null, \"at\":[\"\"], \"aa\":[\"\"], \"ast\":0, \"st\":0, \"cs\":0, \"fcied\":0, \"mni\":0, \"mbs\":0, \"mia\":0, \"cni\":0, \"cbs\":0, \"loc\":\"\", \"daci\":[\"\"]}, \
		\"m2m:fcin\": {\"cs\":0, \"st\":0, \"org\":\"\", \"loc\":\"\", \"at\":[\"\"], \"aa\":[\"\"], \"ast\":0}, \
		\"m2m:ts\": {\"cr\": null, \"mni\":0, \"mbs\":0, \"mia\":0, \"cni\":0, \"cbs\":0,\"cnf\":\"\", \"pei\":0, \"peid\":0,\"mdd\":false, \"mdn\":0, \"mdt\":0, \"mdc\":0, \"mdlt\":[\"\"], \"at\":[\"\"], \"aa\":[\"\"], \"ast\":0}, \
		\"m2m:tsA\": {\"lnk\":\"\", \"cr\":\"\", \"mni\":0, \"mbs\":0, \"mia\":0, \"cni\":0, \"cbs\":0, \"cnf\":\"\", \"pei\":0, \"peid\":0, \"mdd\":false, \"mdn\":0, \"mdt\":0, \"ast\":0}, \
		\"m2m:tsi\": {\"dgt\":\"\", \"con\":\"\", \"snr\":0, \"cs\":0} \
	 }");

	if (ATTRIBUTES == NULL)
	{
		logger("DB", LOG_LEVEL_ERROR, "Cannot create attributes");
		logger("DB", LOG_LEVEL_DEBUG, "%s", cJSON_GetErrorPtr());
		return 0;
	}

	if (!init_dbp())
	{
		logger("MAIN", LOG_LEVEL_ERROR, "DB Error");
		return 0;
	}

	if (argc >= 3 && !strcmp(argv[1], "-p"))
	{
		PORT = argv[2];
	}

#ifdef ENABLE_COAP_DTLS
	int opt;
	while ((opt = getopt(argc, argv, "c:C:h:j:k:r")) != -1)
	{
		switch (opt)
		{
		case 'c':
			cert_file = optarg;
			break;
		case 'C':
			ca_file = optarg;
			break;
		case 'j':
			key_file = optarg;
			break;
		case 'k':
			key_length = cmdline_read_key(optarg, &key, MAX_KEY);
			if (key_length < 0)
				break;
			key_defined = 1;
			break;
		case 'r':
			root_ca_file = optarg;
			break;
		}
	}
#endif

	int sdt_count = sdt_init("./sdt_definitions");
	if (sdt_count > 0) {
		logger("MAIN", LOG_LEVEL_INFO, "Loaded %d SDT definitions", sdt_count);
	} else {
		logger("MAIN", LOG_LEVEL_WARN, "No SDT definitions loaded");
	}

	if (!bootstrap_cse())
	{
		logger("MAIN", LOG_LEVEL_ERROR, "CSE bootstrap failed");
		return 0;
	}

#ifdef ENABLE_MQTT
	mqtt_thread_id = pthread_create(&mqtt, NULL, (void *)mqtt_serve, "mqtt Client");
	if (mqtt_thread_id < 0)
	{
		fprintf(stderr, "MQTT thread create error\n");
		return 0;
	}
#endif

#ifdef ENABLE_COAP
	coap_thread_id = pthread_create(&coap, NULL, coap_serve, "CoAP Server");
	if (coap_thread_id < 0)
	{
		fprintf(stderr, "CoAP thread create error\n");
		return 0;
	}
#endif

	serve_forever(PORT); // main oneM2M operation logic in void route()

#ifdef ENABLE_MQTT
	pthread_join(mqtt, NULL);
	if (terminate)
	{
		return 0;
	}
#endif

#ifdef ENABLE_COAP
	pthread_join(coap, NULL);
#endif

	return 0;
}

void route(oneM2MPrimitive *o2pt)
{
	int rsc = 0;
	double start;

	start = (double)clock() / CLOCKS_PER_SEC; // runtime check - start

#ifdef UPPERTESTER
	if (o2pt->op == OP_UPPERTESTER)
	{
		handle_uppertester_procedure(o2pt);
		log_runtime(start);
		return;
	}
#endif

	RTNode *target_rtnode = get_rtnode(o2pt);

	if (o2pt->rsc >= 4000)
	{
		log_runtime(start);
		return;
	}
	if (o2pt->isForwarding)
	{
		log_runtime(start);
		return;
	}

	int e = result_parse_uri(o2pt, target_rtnode);
	if (e != -1)
		e = check_mandatory_attributes(o2pt);
	if (e == -1)
	{
		log_runtime(start);
		return;
	}

	if (o2pt->fc)
	{
		if ((rsc = validate_filter_criteria(o2pt)) >= 4000)
		{
			return;
		}
	}

	if (o2pt->isFopt)
		rsc = fopt_onem2m_resource(o2pt, target_rtnode);
	else
	{
		rsc = handle_onem2m_request(o2pt, target_rtnode);

		if (o2pt->op != OP_DELETE && target_rtnode->ty == RT_CIN)
		{
			if (strcmp(target_rtnode->rn, "la"))
			{
				logger("MAIN", LOG_LEVEL_DEBUG, "delete cin rtnode");
				free_rtnode(target_rtnode);
				target_rtnode = NULL;
			}
		}
		if (o2pt->op != OP_DELETE && target_rtnode && target_rtnode->ty == RT_FCIN)
		{
			if (target_rtnode->parent &&
				target_rtnode->parent->child != target_rtnode &&
				!target_rtnode->sibling_left)
			{
				free_rtnode(target_rtnode);
				target_rtnode = NULL;
			}
		}
	}
	if (o2pt->op != OP_DELETE && !o2pt->errFlag && target_rtnode)
        notify_via_sub(o2pt, target_rtnode);
}

int handle_onem2m_request(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{
	logger("MAIN", LOG_LEVEL_DEBUG, "handle_onem2m_request");
	int rsc = 0;
	if (!o2pt || !target_rtnode)
	{
		logger("MAIN", LOG_LEVEL_ERROR, "INTERNAL SERVER ERROR");
		return o2pt->rsc = RSC_INTERNAL_SERVER_ERROR;
	}

	switch (o2pt->op)
	{

	case OP_CREATE:
		if (o2pt->rcn == RCN_ATTRIBUTES_AND_CHILD_RESOURCES ||
			o2pt->rcn == RCN_CHILD_RESOURCES ||
			o2pt->rcn == RCN_ATTRIBUTES_AND_CHILD_RESOURCE_REFERENCES ||
			o2pt->rcn == RCN_CHILD_RESOURCE_REFERENCES ||
			o2pt->rcn == RCN_ORIGINAL_RESOURCE ||
			o2pt->rcn == RCN_SEMANTIC_CONTENT ||
			o2pt->rcn == RCN_PERMISSIONS)
		{
			handle_error(o2pt, RSC_BAD_REQUEST, "requested rcn is not supported for create operation");
			break;
		}
		rsc = create_onem2m_resource(o2pt, target_rtnode);
		break;

	case OP_RETRIEVE:
		if (
			o2pt->rcn == RCN_MODIFIED_ATTRIBUTES ||
			o2pt->rcn == RCN_HIERARCHICAL_ADDRESS ||
			o2pt->rcn == RCN_HIERARCHICAL_ADDRESS_ATTRIBUTES ||
			o2pt->rcn == RCN_NOTHING ||
			o2pt->rcn == RCN_DISCOVERY_RESULT_REFERENCES)
		{
			handle_error(o2pt, RSC_BAD_REQUEST, "requested rcn is not supported for retrieve operation");
			break;
		}
		rsc = retrieve_onem2m_resource(o2pt, target_rtnode);
		break;

	case OP_UPDATE:
		if (o2pt->rcn == RCN_HIERARCHICAL_ADDRESS ||
			o2pt->rcn == RCN_HIERARCHICAL_ADDRESS_ATTRIBUTES ||
			o2pt->rcn == RCN_CHILD_RESOURCES ||
			o2pt->rcn == RCN_ATTRIBUTES_AND_CHILD_RESOURCE_REFERENCES ||
			o2pt->rcn == RCN_CHILD_RESOURCE_REFERENCES ||
			o2pt->rcn == RCN_ORIGINAL_RESOURCE ||
			o2pt->rcn == RCN_SEMANTIC_CONTENT ||
			o2pt->rcn == RCN_PERMISSIONS)
		{
			handle_error(o2pt, RSC_BAD_REQUEST, "requested rcn is not supported for update operation");
			break;
		}
		rsc = update_onem2m_resource(o2pt, target_rtnode);
		break;

	case OP_DELETE:
		if (
			o2pt->rcn == RCN_MODIFIED_ATTRIBUTES ||
			o2pt->rcn == RCN_HIERARCHICAL_ADDRESS ||
			o2pt->rcn == RCN_HIERARCHICAL_ADDRESS_ATTRIBUTES ||
			o2pt->rcn == RCN_ORIGINAL_RESOURCE ||
			o2pt->rcn == RCN_SEMANTIC_CONTENT ||
			o2pt->rcn == RCN_PERMISSIONS)
		{

			handle_error(o2pt, RSC_BAD_REQUEST, "requested rcn is not supported for delete operation");
			break;
		}
		rsc = delete_onem2m_resource(o2pt, target_rtnode);
		break;

	case OP_OPTIONS:
		rsc = RSC_OK;
		break;
	case OP_DISCOVERY:
		// Note : discovery operation rcn validation is reversed with other operations
		if (
			o2pt->rcn != RCN_CHILD_RESOURCE_REFERENCES &&
			o2pt->rcn != RCN_DISCOVERY_RESULT_REFERENCES)
		{
			handle_error(o2pt, RSC_BAD_REQUEST, "requested rcn is not supported for discovery operation");
			break;
		}
		rsc = discover_onem2m_resource(o2pt, target_rtnode);
		break;

	case OP_NOTIFY:
		notify_onem2m_resource(o2pt, target_rtnode);
		break;

	default:
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "internal server error");
		return RSC_INTERNAL_SERVER_ERROR;
	}

	return rsc;
}

void stop_server(int sig)
{
	if (call_stop)
	{
		// logger("MAIN", LOG_LEVEL_WARN, "Server is already shutting down...");
		return;
	}
	call_stop = 1;

	logger("MAIN", LOG_LEVEL_INFO, "Shutting down server...");

	logger("MAIN", LOG_LEVEL_INFO, "De-registering cse...");
#ifdef DEREGISTER_AT_SHUTDOWN
	if (SERVER_TYPE == MN_CSE || SERVER_TYPE == ASN_CSE)
	{
		if (deRegister_csr() != 0)
		{
			logger("MAIN", LOG_LEVEL_ERROR, "Remote CSE de-registration failed");
		}
	}
#endif
#ifdef ENABLE_MQTT
	if (mqtt)
	{
		pthread_kill(mqtt, SIGINT);
		pthread_detach(mqtt);
	}
#endif
#ifdef ENABLE_COAP
	logger("MAIN", LOG_LEVEL_INFO, "Closing CoAP...");
	if (coap)
	{
		pthread_kill(coap, SIGINT);
		pthread_detach(coap);
	}
#endif
	logger("MAIN", LOG_LEVEL_INFO, "Closing DB...");
	close_dbp();
	logger("MAIN", LOG_LEVEL_INFO, "Cleaning ResourceTree...");
	free_all_nodelist(rt->csr_list);
	free_rtnode(rt->cb);
	free(rt);
	cJSON_Delete(ATTRIBUTES);

	logger("MAIN", LOG_LEVEL_INFO, "Cleaning SDT...");
	sdt_cleanup();

	logger("MAIN", LOG_LEVEL_INFO, "Done");
	logger_free();
	exit(0);
}

void log_runtime(double start)
{
    double end = (double)clock() / CLOCKS_PER_SEC;
    double elapsed = end - start;

    logger("MAIN", LOG_LEVEL_DEBUG, "runtime: %.6f sec", elapsed);
}
