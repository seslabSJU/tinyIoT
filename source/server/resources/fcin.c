#include <stdlib.h>
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"

extern cJSON *ATTRIBUTES;

int create_fcin(oneM2MPrimitive *o2pt, RTNode *parent_rtnode)
{
	return handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "FlexContainerInstance cannot be created via API");
}

int update_fcin(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{
	return handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "FlexContainerInstance cannot be updated via API");
}

int validate_fcin(oneM2MPrimitive *o2pt, cJSON *parent_fcnt, cJSON *fcin, Operation op)
{
	cJSON *pjson = NULL;

	if (!fcin || !o2pt)
	{
		return RSC_BAD_REQUEST;
	}

	// Validate resource name - cannot be 'la', 'latest', 'ol', or 'oldest'
	if ((pjson = cJSON_GetObjectItem(fcin, "rn")))
	{
		if (pjson->type != cJSON_String)
		{
			return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `rn` must be a string");
		}

		if (!strcmp(pjson->valuestring, "la") || !strcmp(pjson->valuestring, "latest"))
		{
			return handle_error(o2pt, RSC_NOT_ACCEPTABLE, "resource name 'la' or 'latest' is reserved");
		}
		if (!strcmp(pjson->valuestring, "ol") || !strcmp(pjson->valuestring, "oldest"))
		{
			return handle_error(o2pt, RSC_NOT_ACCEPTABLE, "resource name 'ol' or 'oldest' is reserved");
		}
	}

	// FCIN does not support acpi attribute (inherits from parent)
	if ((pjson = cJSON_GetObjectItem(fcin, "acpi")))
	{
		return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `acpi` is not supported for FlexContainerInstance");
	}

	// Validate announceable attributes (aa)
	cJSON *aa = cJSON_GetObjectItem(fcin, "aa");
	if (aa)
	{
		if (CSE_RVI < RVI_3)
		{
			return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `aa` is not supported in this release");
		}

		if (aa->type != cJSON_Array)
		{
			return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `aa` must be an array");
		}

		cJSON *attr = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(RT_FCIN));
		if (!attr)
		{
			return handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "FCIN attributes not found");
		}

		cJSON_ArrayForEach(pjson, aa)
		{
			if (!pjson || pjson->type != cJSON_String)
			{
				return handle_error(o2pt, RSC_BAD_REQUEST, "invalid value in `aa` array");
			}

			// Allow specific attributes that can be announced
			if (strcmp(pjson->valuestring, "lbl") == 0 ||
			    strcmp(pjson->valuestring, "ast") == 0 ||
			    strcmp(pjson->valuestring, "lnk") == 0 ||
			    strcmp(pjson->valuestring, "loc") == 0)
			{
				continue;
			}

			// Check if attribute is valid for FCIN
			if (!cJSON_GetObjectItem(attr, pjson->valuestring))
			{
				return handle_error(o2pt, RSC_BAD_REQUEST, "invalid attribute in `aa`");
			}
		}
	}

	// Validate contentSize attribute
	if ((pjson = cJSON_GetObjectItem(fcin, "cs")))
	{
		if (pjson->type != cJSON_Number || pjson->valueint < 0)
		{
			return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `cs` must be a non-negative number");
		}
	}

	// Validate originator attribute
	if ((pjson = cJSON_GetObjectItem(fcin, "org")))
	{
		if (pjson->type != cJSON_String)
		{
			return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `org` must be a string");
		}
	}

	// Validate stateTag attribute
	if ((pjson = cJSON_GetObjectItem(fcin, "st")))
	{
		if (pjson->type != cJSON_Number || pjson->valueint < 0)
		{
			return handle_error(o2pt, RSC_BAD_REQUEST, "attribute `st` must be a non-negative number");
		}
	}

	return RSC_OK;
}
