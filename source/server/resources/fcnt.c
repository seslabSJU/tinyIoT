#include <stdlib.h>
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"

extern ResourceTree *rt;
extern cJSON *ATTRIBUTES;

int create_fcnt(oneM2MPrimitive *o2pt, RTNode *parent_rtnode)
{
	cJSON *root = cJSON_Duplicate(o2pt->request_pc, 1);
	cJSON *pjson = NULL;

	cJSON *fcnt = cJSON_GetObjectItem(root, "m2m:fcnt");
	char *shortname = NULL;
	if (!fcnt)
	{
		cJSON *item = root->child;
		while (item)
		{
			if (item->type == cJSON_Object && item->string)
			{
				cJSON *cnd = cJSON_GetObjectItem(item, "cnd");
				if (cnd && cnd->type == cJSON_String && strchr(item->string, ':'))
				{
					fcnt = item;
					shortname = item->string; // Store the SDT shortname
					break;
				}
			}
			item = item->next;
		}
	}

	if (!fcnt)
	{
		cJSON_Delete(root);
		return handle_error(o2pt, RSC_BAD_REQUEST, "FlexContainer resource not found");
	}

	// Store the SDT shortname in the FCNT object for later use in responses
	if (shortname)
	{
		cJSON_AddStringToObject(fcnt, "_sn", shortname);
	}

	cJSON *customAttrs = extract_custom_attributes(fcnt);

	pjson = cJSON_GetObjectItem(fcnt, "cnd");
	if (!pjson || pjson->type != cJSON_String || strlen(pjson->valuestring) == 0)
	{
		if (customAttrs) cJSON_Delete(customAttrs);
		cJSON_Delete(root);
		return handle_error(o2pt, RSC_BAD_REQUEST, "containerDefinition (cnd) is mandatory");
	}

	int rsc = RSC_OK;
	char *error_msg = NULL;

	rsc = validate_shortname_cnd(shortname, pjson->valuestring, &error_msg);
	if (rsc != RSC_OK)
	{
		if (customAttrs) cJSON_Delete(customAttrs);
		cJSON_Delete(root);
		return handle_error(o2pt, rsc, error_msg);
	}

	if (customAttrs)
	{
		rsc = validate_custom_attributes(shortname, customAttrs, pjson->valuestring, &error_msg);
		if (rsc != RSC_OK)
		{
			cJSON_Delete(customAttrs);
			cJSON_Delete(root);
			return handle_error(o2pt, rsc, error_msg);
		}
	}

	add_general_attribute(fcnt, parent_rtnode, RT_FCNT);

	rsc = validate_fcnt(o2pt, fcnt, OP_CREATE);
	if (rsc != RSC_OK)
	{
		if (customAttrs) cJSON_Delete(customAttrs);
		cJSON_Delete(root);
		return rsc;
	}

	if ((pjson = cJSON_GetObjectItem(fcnt, "cr")))
	{
		if (pjson->type == cJSON_NULL)
		{
			cJSON_AddStringToObject(fcnt, "cr", o2pt->fr);
		}
		else
		{
			if (customAttrs) cJSON_Delete(customAttrs);
			handle_error(o2pt, RSC_BAD_REQUEST, "creator attribute with arbitrary value is not allowed");
			cJSON_Delete(root);
			return o2pt->rsc;
		}
	}

#if CSE_RVI >= RVI_3
	bool parent_was_announced = false;
	cJSON *final_at = cJSON_CreateArray();

	if (parent_rtnode->ty == RT_AE || parent_rtnode->ty == RT_CNT || parent_rtnode->ty == RT_FCNT)
	{
		cJSON *parent_at = cJSON_GetObjectItem(parent_rtnode->obj, "at");
		if(parent_at && cJSON_GetArraySize(parent_at) > 0)
		{
			parent_was_announced = true;
		}
	}

	if (parent_was_announced)
	{
		if (handle_annc_create(parent_rtnode, fcnt, cJSON_GetObjectItem(fcnt, "at"), final_at) == -1)
		{
			if (customAttrs) cJSON_Delete(customAttrs);
			cJSON_Delete(root);
			cJSON_Delete(final_at);
			return handle_error(o2pt, RSC_BAD_REQUEST, "invalid attribute in `aa`");
		}

		if (cJSON_GetArraySize(final_at) > 0)
		{
			cJSON_DeleteItemFromObject(fcnt, "at");
			cJSON_AddItemToObject(fcnt, "at", final_at);
		}
		else
		{
			cJSON_Delete(final_at);
		}
	}
	else
	{
		if (handle_annc_create(parent_rtnode->parent, fcnt, cJSON_GetObjectItem(fcnt, "at"), final_at) == -1)
		{
			if (customAttrs) cJSON_Delete(customAttrs);
			cJSON_Delete(root);
			cJSON_Delete(final_at);
			return handle_error(o2pt, RSC_BAD_REQUEST, "invalid attribute in `aa`");
		}

		if (cJSON_GetArraySize(final_at) > 0)
		{
			cJSON_DeleteItemFromObject(fcnt, "at");
			cJSON_AddItemToObject(fcnt, "at", final_at);
		}
		else
		{
			cJSON_Delete(final_at);
		}
	}
#endif

	cJSON_AddNumberToObject(fcnt, "st", 0);

	int contentSize = calculate_content_size(customAttrs);
	cJSON_AddNumberToObject(fcnt, "cs", contentSize);

	o2pt->rsc = RSC_CREATED;

	char *ptr = malloc(1024);
	cJSON *rn = cJSON_GetObjectItem(fcnt, "rn");
	sprintf(ptr, "%s/%s", get_uri_rtnode(parent_rtnode), rn->valuestring);

	int result = db_store_resource(fcnt, ptr);
	if (result != 1)
	{
		if (customAttrs) cJSON_Delete(customAttrs);
		handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "DB store fail");
		cJSON_Delete(root);
		free(ptr);
		ptr = NULL;
		return o2pt->rsc;
	}

	if (customAttrs)
	{
		cJSON *ri_obj = cJSON_GetObjectItem(fcnt, "ri");
		db_store_fcnt_custom_attributes(ri_obj->valuestring, customAttrs);
		cJSON_Delete(customAttrs);
	}

	free(ptr);
	ptr = NULL;

	RTNode *child_rtnode = create_rtnode(fcnt, RT_FCNT);
	add_child_resource_tree(parent_rtnode, child_rtnode);
	make_response_body(o2pt, child_rtnode);

	if (fcnt->string && strcmp(fcnt->string, "m2m:fcnt") == 0)
	{
		cJSON_DetachItemFromObject(root, "m2m:fcnt");
	}
	else if (fcnt->string)
	{
		cJSON_DetachItemFromObject(root, fcnt->string);
	}
	cJSON_Delete(root);

	return RSC_CREATED;
}

int update_fcnt(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{
	char invalid_key[][9] = {"ty", "pi", "ri", "rn", "ct", "cr", "cnd"};
	cJSON *m2m_fcnt = cJSON_GetObjectItem(o2pt->request_pc, "m2m:fcnt");
	if (!m2m_fcnt)
	{
		cJSON *item = o2pt->request_pc->child;
		while (item)
		{
			if (item->type == cJSON_Object && item->string)
			{
				// For update, accept any object with ':' in the name (SDT shortname)
				// We don't require 'cnd' since it might not be in the update request
				if (strchr(item->string, ':'))
				{
					m2m_fcnt = item;
					break;
				}
			}
			item = item->next;
		}
	}

	if (!m2m_fcnt)
	{
		return handle_error(o2pt, RSC_BAD_REQUEST, "FlexContainer resource not found");
	}

	int invalid_key_size = sizeof(invalid_key) / (9 * sizeof(char));

	int updateAttrCnt = cJSON_GetArraySize(m2m_fcnt);

	for (int i = 0; i < invalid_key_size; i++)
	{
		if (cJSON_GetObjectItem(m2m_fcnt, invalid_key[i]))
		{
			handle_error(o2pt, RSC_BAD_REQUEST, "unsupported attribute on update");
			return RSC_BAD_REQUEST;
		}
	}

	cJSON *fcnt = target_rtnode->obj;
	int result;
	cJSON *pjson = NULL;
	cJSON *acpi_obj = NULL;
	bool acpi_flag = false;

	cJSON *customAttrs = extract_custom_attributes(m2m_fcnt);
	if (customAttrs)
	{
		cJSON *cnd = cJSON_GetObjectItem(fcnt, "cnd");
		char *error_msg = NULL;

		char *shortname = NULL;
		if (fcnt->string && strcmp(fcnt->string, "m2m:fcnt") != 0) {
			shortname = fcnt->string;
		}

		result = validate_custom_attributes(shortname, customAttrs, cnd ? cnd->valuestring : NULL, &error_msg);
		if (result != RSC_OK)
		{
			cJSON_Delete(customAttrs);
			return handle_error(o2pt, result, error_msg);
		}

		cJSON *existing_custom = extract_custom_attributes(target_rtnode->obj);
		if (existing_custom)
		{
			cJSON *item = NULL;
			cJSON_ArrayForEach(item, customAttrs)
			{
				if (item->string)
				{
					cJSON *existing_item = cJSON_GetObjectItem(existing_custom, item->string);
					if (existing_item)
					{
						if (item->type != existing_item->type)
						{
							cJSON_Delete(existing_custom);
							cJSON_Delete(customAttrs);
							return handle_error(o2pt, RSC_BAD_REQUEST,
								"custom attribute type mismatch");
						}
					}
				}
			}
			cJSON_Delete(existing_custom);
		}
	}

	result = validate_fcnt(o2pt, m2m_fcnt, OP_UPDATE);

	if (cJSON_GetObjectItem(m2m_fcnt, "acpi"))
	{
		cJSON_ArrayForEach(acpi_obj, cJSON_GetObjectItem(fcnt, "acpi"))
		{
			acpi_flag = false;
			cJSON_ArrayForEach(pjson, cJSON_GetObjectItem(m2m_fcnt, "acpi"))
			{
				if (strcmp(acpi_obj->valuestring, pjson->valuestring) != 0)
				{
					acpi_flag = true;
					break;
				}
			}
			if (!acpi_flag)
			{
				logger("UTIL", LOG_LEVEL_INFO, "acpi %s", acpi_obj->valuestring);
				if (!has_acpi_update_privilege(o2pt, acpi_obj->valuestring))
				{
					if (customAttrs) cJSON_Delete(customAttrs);
					return handle_error(o2pt, RSC_ORIGINATOR_HAS_NO_PRIVILEGE, "no privilege to update acpi");
				}
			}
		}

		if (cJSON_GetArraySize(cJSON_GetObjectItem(m2m_fcnt, "acpi")) > 0)
		{
			if (validate_acpi(o2pt, cJSON_GetObjectItem(m2m_fcnt, "acpi"), ACOP_UPDATE) != RSC_OK)
			{
				if (customAttrs) cJSON_Delete(customAttrs);
				return handle_error(o2pt, RSC_BAD_REQUEST, "no privilege to update acpi");
			}
		}
	}

	if (result != RSC_OK)
	{
		if (customAttrs) cJSON_Delete(customAttrs);
		return result;
	}

	cJSON *at = NULL;
	if ((at = cJSON_GetObjectItem(m2m_fcnt, "at")))
	{
		cJSON *final_at = cJSON_CreateArray();
		handle_annc_update(target_rtnode, at, final_at);
		cJSON_DeleteItemFromObject(m2m_fcnt, "at");
		cJSON_AddItemToObject(m2m_fcnt, "at", final_at);
	}

	cJSON_AddItemToObject(m2m_fcnt, "lt", cJSON_CreateString(get_local_time(0)));

	if (customAttrs)
	{
		cJSON *existing_custom = extract_custom_attributes(target_rtnode->obj);
		if (existing_custom)
		{
			cJSON *item = NULL;
			cJSON_ArrayForEach(item, customAttrs)
			{
				if (item->string)
				{
					cJSON_DeleteItemFromObject(existing_custom, item->string);
					cJSON_AddItemToObject(existing_custom, item->string, cJSON_Duplicate(item, 1));
				}
			}
		}
		else
		{
			existing_custom = cJSON_Duplicate(customAttrs, 1);
		}

		int contentSize = calculate_content_size(existing_custom);
		cJSON_AddNumberToObject(m2m_fcnt, "cs", contentSize);

		// Increment stateTag when contentSize is modified (TS-0004 Section 7.4.37.2)
		cJSON *st_obj = cJSON_GetObjectItem(target_rtnode->obj, "st");
		if (st_obj && cJSON_IsNumber(st_obj))
		{
			int current_st = st_obj->valueint;
			cJSON_AddNumberToObject(m2m_fcnt, "st", current_st + 1);
		}

		cJSON_Delete(existing_custom);
	}

	update_resource(target_rtnode->obj, m2m_fcnt);

	result = db_update_resource(m2m_fcnt, cJSON_GetObjectItem(target_rtnode->obj, "ri")->valuestring, RT_FCNT);

	if (customAttrs)
	{
		cJSON *ri_obj = cJSON_GetObjectItem(target_rtnode->obj, "ri");
		db_update_fcnt_custom_attributes(ri_obj->valuestring, customAttrs);
		cJSON_Delete(customAttrs);
	}

	for (int i = 0; i < updateAttrCnt; i++)
	{
		cJSON_DeleteItemFromArray(m2m_fcnt, 0);
	}

	make_response_body(o2pt, target_rtnode);
	o2pt->rsc = RSC_UPDATED;
	return RSC_UPDATED;
}
