#include <malloc.h>
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
#include "mqttClient.h"

ResourceTree *rt;
void route(oneM2MPrimitive *o2pt);
void stop_server(int sig);
cJSON *ATTRIBUTES;
cJSON *ACP_SUPPORT_ACR;
cJSON *ACP_SUPPORT_ACCO;
char *PORT = SERVER_PORT;
int terminate = 0;
#ifdef ENABLE_MQTT
pthread_t mqtt;
int mqtt_thread_id;
#endif

int main(int argc, char **argv) {
	signal(SIGINT, stop_server);
	ATTRIBUTES = cJSON_Parse(
	"{ \
		\"general\": {\"rn\": \"\", \"ri\": \"\", \"pi\": \"\", \"ct\": \"\", \"et\": \"\", \"lt\": \"\" , \"uri\": \"\" , \"acpi\": [\"\"], \"lbl\": [\"\"], \"ty\":0}, \
		\"m2m:ae\": {\"ri\": \"\", \"api\": \"\", \"aei\" : \"\", \"rr\": true, \"poa\":[\"\"], \"apn\":\"\", \"srv\":[\"\"]}, \
		\"m2m:cnt\": {\"ri\":\"\", \"cr\": null, \"mni\":0, \"mbs\":0, \"mia\":0, \"st\":0, \"cni\":0, \"cbs\":0}, \
		\"m2m:cin\": {\"ri\":\"\", \"cs\":0, \"cr\":null, \"con\":\"\"}, \
		\"m2m:acp\": {\"ri\":\"\", \"pv\":{\"acr\":[{\"acor\":[\"\"],\"acop\":0, \"acco\":{\"acip\":{\"ipv4\":[\"\"], \"ipv6\":[\"\"]}}}]}, \"pvs\":{\"acr\":[{\"acor\":[\"\"],\"acop\":0, \"acco\":{\"acip\":{\"ipv4\":[\"\"], \"ipv6\":[\"\"]}}}]}}, \
		\"m2m:sub\": {\"ri\":\"\", \"enc\":{\"net\":[1]}, \"exc\":0, \"nu\":[\"\"], \"gpi\":0, \"nfu\":0, \"bn\":0, \"rl\":0, \"sur\":0, \"nct\":0, \"cr\":\"\", \"su\":\"\"},\
		\"m2m:grp\": {\"ri\":\"\", \"cr\":\"\", \"mt\":0, \"cnm\":0, \"mnm\":0, \"mid\":[\"\"], \"macp\":[\"\"], \"mtv\":true, \"csy\":0, \"gn\":0}, \
		\"m2m:csr\": {\"ri\":\"\", \"cst\":0, \"poa\":[\"\"], \"cb\":\"\", \"csi\":\"\", \"mei\":\"\", \"tri\":\"\", \"rr\":true, \"nl\":\"\", \"srv\":[\"\"]},\
		\"m2m:cb\": {\"ri\":\"\", \"cst\":0, \"csi\":\"\", \"srt\":[\"\"], \"poa\":[\"\"], \"srv\":[0], \"rr\":true} \
	 }"
    );

    if(ATTRIBUTES == NULL){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create attributes");
        logger("DB", LOG_LEVEL_DEBUG, "%s", cJSON_GetErrorPtr());
        return 0;
    }

	if(!init_dbp()){
		logger("MAIN", LOG_LEVEL_ERROR, "DB Error");
		return 0;
	}

	if(argc >= 3 && !strcmp(argv[1], "-p")){
		PORT = argv[2];
	}

	init_server();
	
	#ifdef ENABLE_MQTT
	mqtt_thread_id = pthread_create(&mqtt, NULL, mqtt_serve, "mqtt Client");
	if(mqtt_thread_id < 0){
		fprintf(stderr, "MQTT thread create error\n");
		return 0;
	}
	#endif

	serve_forever(PORT); // main oneM2M operation logic in void route()    

	#ifdef ENABLE_MQTT
	pthread_join(mqtt, NULL);
	if(terminate){
		return 0;
	}
	#endif

	return 0;
}

void route(oneM2MPrimitive *o2pt) {
	int rsc = 0;
    double start;

    start = (double)clock() / CLOCKS_PER_SEC; // runtime check - start
	RTNode* target_rtnode = parse_uri(o2pt, rt->cb);
	int e = result_parse_uri(o2pt, target_rtnode);
	if(e != -1) e = check_payload_size(o2pt);
	if(e == -1) {
		log_runtime(start);
		return;
	}

	if(o2pt->fc){
		if(rsc = validate_filter_criteria(o2pt) > 4000){
			return rsc;
		}
	}

	if(o2pt->isFopt)
		rsc = fopt_onem2m_resource(o2pt, target_rtnode);
	else{
		rsc = handle_onem2m_request(o2pt, target_rtnode);
	
		if(o2pt->op != OP_DELETE && target_rtnode->ty == RT_CIN){
			free_rtnode(target_rtnode);
			target_rtnode = NULL;
		}
	}

	if(o2pt->op != OP_DELETE && !o2pt->errFlag && target_rtnode) notify_onem2m_resource(o2pt, target_rtnode);
	log_runtime(start);
}

int handle_onem2m_request(oneM2MPrimitive *o2pt, RTNode *target_rtnode){
	logger("MAIN", LOG_LEVEL_INFO, "handle_onem2m_request");
	int rsc = 0;

	if(o2pt->op == OP_CREATE && o2pt->fc){
		return o2pt->rsc = rsc = RSC_BAD_REQUEST;
	}

	if(o2pt->isForwarding){
		rsc = forwarding_onem2m_resource(o2pt, target_rtnode);
		return rsc;
	}

	switch(o2pt->op) {
		
		case OP_CREATE:	
			rsc = create_onem2m_resource(o2pt, target_rtnode); 
			break;
		
		case OP_RETRIEVE:
			rsc = retrieve_onem2m_resource(o2pt, target_rtnode); 
			break;
			
		case OP_UPDATE: 
			rsc = update_onem2m_resource(o2pt, target_rtnode); 
			break;
			
		case OP_DELETE:
			rsc = delete_onem2m_resource(o2pt, target_rtnode); 
			break;

		case OP_VIEWER:
			rsc = tree_viewer_api(o2pt, target_rtnode); 
			break;
		
		case OP_OPTIONS:
			rsc = RSC_OK;
			set_o2pt_pc(o2pt, "{\"m2m:dbg\": \"response about options method\"}");
			break;
		case OP_DISCOVERY:
			rsc = discover_onem2m_resource(o2pt, target_rtnode); 
			break;
	
		default:
			handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "{\"m2m:dbg\": \"internal server error\"}");
			return RSC_INTERNAL_SERVER_ERROR;
		}
		
		return rsc;
}

void stop_server(int sig){
	logger("MAIN", LOG_LEVEL_INFO, "Shutting down server...");
	#ifdef ENABLE_MQTT
	pthread_kill(mqtt, SIGINT);
	#endif
	logger("MAIN", LOG_LEVEL_INFO, "Closing DB...");
	close_dbp();
	logger("MAIN", LOG_LEVEL_INFO, "Cleaning ResourceTree...");
	free_all_resource(rt->cb);
	free(rt);
	logger("MAIN", LOG_LEVEL_INFO, "Done");
	exit(0);
}