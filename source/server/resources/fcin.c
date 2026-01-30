#include <stdlib.h>
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;

int create_fcin(oneM2MPrimitive *o2pt, RTNode *parent_rtnode)
{
	return handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "creating FCIN is forbidden");
}

int update_fcin(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{
	return handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "updating FCIN is forbidden");
}

int validate_fcin(oneM2MPrimitive *o2pt, cJSON *parent_fcnt, cJSON *fcin, Operation op)
{
	cJSON *pjson = NULL;

	if ((pjson = cJSON_GetObjectItem(fcin, "rn")))
	{
		if (!strcmp(pjson->valuestring, "la") || !strcmp(pjson->valuestring, "latest"))
		{
			handle_error(o2pt, RSC_NOT_ACCEPTABLE, "attribute `rn` is invalid");
			return RSC_BAD_REQUEST;
		}
		if (!strcmp(pjson->valuestring, "ol") || !strcmp(pjson->valuestring, "oldest"))
		{
			handle_error(o2pt, RSC_NOT_ACCEPTABLE, "attribute `rn` is invalid");
			return RSC_BAD_REQUEST;
		}
	}

	if ((pjson = cJSON_GetObjectItem(fcin, "acpi")))
	{
		return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `acpi` for `fcin` is not supported");
	}

	cJSON *aa = cJSON_GetObjectItem(fcin, "aa");
	if (aa && CSE_RVI < RVI_3)
	{
		return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `aa` is not supported");
	}

	cJSON *attr = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(RT_FCIN));
	cJSON_ArrayForEach(pjson, aa)
	{
		if (strcmp(pjson->valuestring, "lbl") == 0)
			continue;
		if (strcmp(pjson->valuestring, "ast") == 0)
			continue;
		if (strcmp(pjson->valuestring, "lnk") == 0)
			continue;
		if (!cJSON_GetObjectItem(attr, pjson->valuestring))
		{
			return handle_error(o2pt, RSC_BAD_REQUEST, "invalid attribute in `aa`");
		}
	}

	return RSC_OK;
}
