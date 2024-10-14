#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <time.h>

#include "cJSON.h"
#include "onem2m.h"
#include "logger.h"
#include "dbmanager.h"
#include "util.h"
#include "config.h"
#include "jsonparser.h"

RTNode *find_export_target(oneM2MPrimitive *o2pt)
{
    // start = (double)clock() / CLOCKS_PER_SEC;
    char *target_resource_uri;
    cJSON *new_pc = cJSON_CreateObject();
	
    target_resource_uri = strchr(o2pt->to, '/');


    if (target_resource_uri != NULL) 
    {
        target_resource_uri++; 
        o2pt->to = target_resource_uri;
        logger("EXPT", LOG_LEVEL_DEBUG, "EXPORT TARGET: %s", target_resource_uri);
    }

    RTNode *target_rtnode = get_rtnode(o2pt);
    
    if (o2pt->rsc >= 4000)
	{
		// log_runtime(start);
		return;
	}
	if (o2pt->isForwarding)
	{
		// log_runtime(start);
		return;
	}
    cJSON_AddStringToObject(new_pc, "m2m:dbg", "export test");
    o2pt->response_pc = new_pc;

    
    return target_rtnode;
}

void resource_export(RTNode *export_target)
{
    printf("export target : %s\n", export_target->rn);

    return;
}