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

#if UPPERTESTER
#include "upperTester.h"
#endif

#include "coap.h"

ResourceTree *rt;
RTNode *registrar_csr = NULL;

#if MONO_THREAD == 0
pthread_mutex_t main_lock;
pthread_mutex_t csr_lock;
pthread_mutexattr_t Attr;

#endif

void route(oneM2MPrimitive *o2pt);
void stop_server(int sig);
cJSON *ATTRIBUTES;
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
	bool initialBoot = false;
	signal(SIGINT, stop_server);
	logger_init();

#if MONO_THREAD == 0
	pthread_mutexattr_init(&Attr);
	pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&main_lock, &Attr);
	pthread_mutex_init(&csr_lock, NULL);
#endif

	ATTRIBUTES = cJSON_Parse(
		"{ \
		\"general\": {\"rn\": \"\", \"ri\": \"\", \"pi\": \"\", \"ct\": \"\", \"et\": \"\", \"lt\": \"\" , \"uri\": \"\" , \"acpi\": [\"\"], \"lbl\": [\"\"], \"ty\":0, \"memberOf\": [\"\"]}, \
		\"m2m:ae\": {\"api\": \"\", \"aei\" : \"\", \"rr\": true, \"poa\":[\"\"], \"apn\":\"\", \"srv\":[\"\"], \"at\":[\"\"], \"aa\":[\"\"], \"ast\":0}, \
		\"m2m:cnt\": {\"cr\": null, \"mni\":0, \"mbs\":0, \"st\":0, \"cni\":0, \"cbs\":0,\"at\":[\"\"], \"aa\":[\"\"], \"ast\":0}, \
		\"m2m:cin\": {\"cs\":0, \"cr\":null, \"con\":\"\", \"cnf\":\"\", \"st\":\"\",\"at\":[\"\"], \"aa\":[\"\"], \"ast\":0}, \
		\"m2m:acp\": {\"pv\":{\"acr\":[{\"acor\":[\"\"],\"acop\":0, \"acco\":[{\"acip\":{\"ipv4\":[\"\"], \"ipv6\":[\"\"]}}]}]}, \"pvs\":{\"acr\":[{\"acor\":[\"\"],\"acop\":0, \"acco\":[{\"acip\":{\"ipv4\":[\"\"], \"ipv6\":[\"\"]}}]}]}, \"at\":[\"\"], \"aa\":[\"\"], \"ast\":0}, \
		\"m2m:sub\": {\"enc\":{\"net\":[1], \"atr\":[\"\"], \"chty\":[0] }, \"exc\":0, \"nu\":[\"\"], \"gpi\":0, \"nfu\":0, \"bn\":0, \"rl\":0, \"sur\":0, \"nct\":0, \"cr\":\"\", \"su\":\"\"},\
		\"m2m:grp\": {\"cr\":\"\", \"mt\":0, \"cnm\":0, \"mnm\":0, \"mid\":[\"\"], \"macp\":[\"\"], \"mtv\":true, \"csy\":0}, \
		\"m2m:csr\": {\"cst\":0, \"poa\":[\"\"], \"cb\":\"\", \"dcse\":[\"\"], \"csi\":\"\", \"mei\":\"\", \"tri\":\"\", \"csz\":[\"\"], \"rr\":true, \"nl\":\"\", \"srv\":[\"\"]},\
		\"m2m:cb\": {\"cst\":0, \"csi\":\"\", \"srt\":[\"\"], \"poa\":[\"\"], \"srv\":[0], \"rr\":true, \"at\":[], \"aa\":[],\"ast\":0}, \
		\"m2m:cbA\": {\"lnk\":\"\", \"cst\":0, \"csi\":\"\", \"srt\":[\"\"], \"poa\":[\"\"], \"srv\":[\"\"], \"rr\":true, \"ast\":0}, \
		\"m2m:aeA\": {\"lnk\":\"\", \"api\":\"\", \"aei\":\"\", \"rr\":true, \"poa\":[\"\"], \"apn\":\"\", \"srv\":[\"\"], \"ast\":0} \
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

	initialBoot = init_server();

	init_resource_tree();

	if (initialBoot)
	{
		cJSON *acp = cJSON_CreateObject();
		init_acp(acp);
		db_store_resource(acp, CSE_BASE_NAME "/defaultACP");
		RTNode *acp_rtnode = create_rtnode(acp, RT_ACP);
		add_child_resource_tree(rt->cb, acp_rtnode);
	}

	if (SERVER_TYPE == MN_CSE || SERVER_TYPE == ASN_CSE)
	{
		if (register_remote_cse() != 0)
		{
			logger("MAIN", LOG_LEVEL_ERROR, "Remote CSE registration failed");
			return 0;
		}

		if (create_local_csr())
		{
			logger("MAIN", LOG_LEVEL_ERROR, "Local CSR creation failed");
			return 0;
		}
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

	// if (o2pt->op == OP_UPPERTESTER)
	// {
	// 	handle_uppertester_procedure(o2pt);
	// 	log_runtime(start);
	// 	return;
	// }

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
	}
	if (o2pt->op != OP_DELETE && !o2pt->errFlag && target_rtnode)
		notify_via_sub(o2pt, target_rtnode);
	log_runtime(start);
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
			o2pt->rcn == RCN_NOTHING)
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
	if (SERVER_TYPE == MN_CSE || SERVER_TYPE == ASN_CSE)
	{
		if (deRegister_csr() != 0)
		{
			logger("MAIN", LOG_LEVEL_ERROR, "Remote CSE de-registration failed");
		}
	}
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

	logger("MAIN", LOG_LEVEL_INFO, "Done");
	logger_free();
	exit(0);
}
