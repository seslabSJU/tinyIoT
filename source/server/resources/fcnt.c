#include <stdlib.h>
#include "../onem2m.h"
#include "../logger.h"
#include "../util.h"
#include "../dbmanager.h"
#include "../config.h"
#include "../sdt.h"

extern ResourceTree *rt;

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
					shortname = item->string;
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

	if (shortname)
	{
		cJSON_AddStringToObject(fcnt, "_sn", shortname);
	}

	cJSON *or_item = cJSON_GetObjectItem(fcnt, "or");
	if (or_item)
	{
		cJSON *or_copy = cJSON_Duplicate(or_item, 1);
		cJSON_DeleteItemFromObject(fcnt, "or");
		cJSON_AddItemToObject(fcnt, "oref", or_copy);
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

	rsc = sdt_validate_fcnt(shortname, pjson->valuestring, customAttrs, &error_msg, 1);
	if (rsc != RSC_OK)
	{
		if (customAttrs) cJSON_Delete(customAttrs);
		cJSON_Delete(root);
		return handle_error(o2pt, rsc, error_msg);
	}

	{
		const char *np_attrs[] = {"ty", "ri", "pi", "ct", "lt", "st", "cs", "cni", "cbs", NULL};
		for (int i = 0; np_attrs[i] != NULL; i++)
		{
			if (cJSON_GetObjectItem(fcnt, np_attrs[i]))
			{
				if (customAttrs) cJSON_Delete(customAttrs);
				cJSON_Delete(root);
				return handle_error(o2pt, RSC_BAD_REQUEST, "not-permitted attribute on create");
			}
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
			cJSON_DeleteItemFromObject(fcnt, "cr");
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

	if (o2pt->rvi >= RVI_4)
	{
		cJSON *fcied = cJSON_GetObjectItem(fcnt, "fcied");
		if (fcied && !cJSON_IsNull(fcied))
		{
			// Set m* to local policy defaults if not provided
			if (!cJSON_GetObjectItem(fcnt, "mni"))
				cJSON_AddNumberToObject(fcnt, "mni", DEFAULT_MAX_NR_INSTANCES);
			if (!cJSON_GetObjectItem(fcnt, "mbs"))
				cJSON_AddNumberToObject(fcnt, "mbs", DEFAULT_MAX_BYTE_SIZE);
			if (!cJSON_GetObjectItem(fcnt, "mia"))
				cJSON_AddNumberToObject(fcnt, "mia", DEFAULT_MAX_INSTANCE_AGE);

			// Validate mbs >= cs
			cJSON *mbs_obj = cJSON_GetObjectItem(fcnt, "mbs");
			if (mbs_obj && cJSON_IsNumber(mbs_obj) && mbs_obj->valueint < contentSize)
			{
				if (customAttrs) cJSON_Delete(customAttrs);
				cJSON_Delete(root);
				return handle_error(o2pt, RSC_BAD_REQUEST, "mbs cannot be smaller than cs");
			}
		}
	}

	o2pt->rsc = RSC_CREATED;

	cJSON *rn = cJSON_GetObjectItem(fcnt, "rn");
	char *parent_uri = get_uri_rtnode(parent_rtnode);
	size_t uri_len = strlen(parent_uri) + strlen(rn->valuestring) + 2;
	char *ptr = malloc(uri_len);
	if (!ptr) {
		if (customAttrs) cJSON_Delete(customAttrs);
		cJSON_Delete(root);
		return handle_error(o2pt, RSC_INTERNAL_SERVER_ERROR, "Memory allocation failed");
	}
	snprintf(ptr, uri_len, "%s/%s", parent_uri, rn->valuestring);

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

	if (o2pt->rvi >= RVI_4)
	{
		cJSON *fcied = cJSON_GetObjectItem(fcnt, "fcied");
		if (fcied && !cJSON_IsNull(fcied))
		{
			prepare_fcnt_for_instances(child_rtnode, o2pt);

			if (add_flexcontainer_instance(child_rtnode, o2pt, true) == 0)
			{
				cJSON_AddNumberToObject(fcnt, "cni", 1);
				cJSON_AddNumberToObject(fcnt, "cbs", contentSize);
				db_update_resource(fcnt, get_ri_rtnode(child_rtnode), RT_FCNT);
			}
			else
			{
				logger("FCNT", LOG_LEVEL_ERROR, "Failed to create initial FCIN, cni/cbs not updated");
			}
		}
	}

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
	char invalid_key[][9] = {"ty", "pi", "ri", "rn", "ct", "lt", "cr", "cnd", "cs", "st", "cni", "cbs"};
	cJSON *m2m_fcnt = cJSON_GetObjectItem(o2pt->request_pc, "m2m:fcnt");
	if (!m2m_fcnt)
	{
		cJSON *item = o2pt->request_pc->child;
		while (item)
		{
			if (item->type == cJSON_Object && item->string)
			{
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

	cJSON *or_item = cJSON_GetObjectItem(m2m_fcnt, "or");
	if (or_item)
	{
		cJSON *or_copy = cJSON_Duplicate(or_item, 1);
		cJSON_DeleteItemFromObject(m2m_fcnt, "or");
		cJSON_AddItemToObject(m2m_fcnt, "oref", or_copy);
	}

	cJSON *fcnt = target_rtnode->obj;
	int result;
	cJSON *pjson = NULL;
	cJSON *acpi_obj = NULL;
	bool acpi_flag = false;
	bool needs_fci = false;

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

		result = sdt_validate_fcnt(shortname, cnd ? cnd->valuestring : NULL, customAttrs, &error_msg, OP_UPDATE);
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

	char *lt = get_local_time(0);
	cJSON_AddItemToObject(m2m_fcnt, "lt", cJSON_CreateString(lt));
	free(lt);

	cJSON *st_obj = cJSON_GetObjectItem(target_rtnode->obj, "st");
	if (st_obj && cJSON_IsNumber(st_obj))
	{
		int current_st = st_obj->valueint;
		cJSON_AddNumberToObject(m2m_fcnt, "st", current_st + 1);
	}

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

		cJSON_Delete(existing_custom);
	}
	else
	{
		cJSON *existing_custom = extract_custom_attributes(target_rtnode->obj);
		int contentSize = calculate_content_size(existing_custom);
		cJSON_AddNumberToObject(m2m_fcnt, "cs", contentSize);
		if (existing_custom) cJSON_Delete(existing_custom);
	}

	if (o2pt->rvi >= RVI_4)
	{
		cJSON *fcied_in_request = cJSON_GetObjectItem(m2m_fcnt, "fcied");
		cJSON *fcied_current = cJSON_GetObjectItem(fcnt, "fcied");

		cJSON *fcied_final = fcied_in_request ? fcied_in_request : fcied_current;

		if (fcied_in_request && cJSON_IsNull(fcied_in_request))
		{
			cJSON *mni_req = cJSON_GetObjectItem(m2m_fcnt, "mni");
			cJSON *mbs_req = cJSON_GetObjectItem(m2m_fcnt, "mbs");
			cJSON *mia_req = cJSON_GetObjectItem(m2m_fcnt, "mia");
			if ((mni_req && !cJSON_IsNull(mni_req)) ||
			    (mbs_req && !cJSON_IsNull(mbs_req)) ||
			    (mia_req && !cJSON_IsNull(mia_req)))
			{
				if (customAttrs) cJSON_Delete(customAttrs);
				return handle_error(o2pt, RSC_BAD_REQUEST, "mni, mbs, or mia must not be present when fcied is removed");
			}

			cJSON_DeleteItemFromObject(fcnt, "fcied");
			cJSON_DeleteItemFromObject(fcnt, "cni");
			cJSON_DeleteItemFromObject(fcnt, "cbs");
			cJSON_DeleteItemFromObject(fcnt, "mbs");
			cJSON_DeleteItemFromObject(fcnt, "mni");
			cJSON_DeleteItemFromObject(fcnt, "mia");

			// Ensure DB columns are cleared (null → DELETE in update_resource)
			if (!cJSON_GetObjectItem(m2m_fcnt, "cni")) cJSON_AddNullToObject(m2m_fcnt, "cni");
			if (!cJSON_GetObjectItem(m2m_fcnt, "cbs")) cJSON_AddNullToObject(m2m_fcnt, "cbs");
			if (!cJSON_GetObjectItem(m2m_fcnt, "mni")) cJSON_AddNullToObject(m2m_fcnt, "mni");
			if (!cJSON_GetObjectItem(m2m_fcnt, "mbs")) cJSON_AddNullToObject(m2m_fcnt, "mbs");
			if (!cJSON_GetObjectItem(m2m_fcnt, "mia")) cJSON_AddNullToObject(m2m_fcnt, "mia");

			cleanup_fcnt_instances(target_rtnode, false, false);
		}
		else if (!fcied_final || cJSON_IsNull(fcied_final))
		{
			cJSON *mni_req = cJSON_GetObjectItem(m2m_fcnt, "mni");
			cJSON *mbs_req = cJSON_GetObjectItem(m2m_fcnt, "mbs");
			cJSON *mia_req = cJSON_GetObjectItem(m2m_fcnt, "mia");
			if ((mni_req && !cJSON_IsNull(mni_req)) ||
			    (mbs_req && !cJSON_IsNull(mbs_req)) ||
			    (mia_req && !cJSON_IsNull(mia_req)))
			{
				if (customAttrs) cJSON_Delete(customAttrs);
				return handle_error(o2pt, RSC_BAD_REQUEST, "mni, mbs, or mia must not be present when fcied is not present");
			}
		}

		if (fcied_final && !cJSON_IsNull(fcied_final))
		{
			cJSON *mni_req = cJSON_GetObjectItem(m2m_fcnt, "mni");
			cJSON *mbs_req = cJSON_GetObjectItem(m2m_fcnt, "mbs");
			cJSON *mia_req = cJSON_GetObjectItem(m2m_fcnt, "mia");
			if ((mni_req && cJSON_IsNumber(mni_req) && mni_req->valueint == 0) ||
			    (mbs_req && cJSON_IsNumber(mbs_req) && mbs_req->valueint == 0) ||
			    (mia_req && cJSON_IsNumber(mia_req) && mia_req->valueint == 0))
			{
				if (customAttrs) cJSON_Delete(customAttrs);
				return handle_error(o2pt, RSC_BAD_REQUEST, "mni, mbs, or mia cannot be 0");
			}
		}

		if (cJSON_IsFalse(fcied_final))
		{
			bool was_true_or_none = !fcied_current || cJSON_IsTrue(fcied_current) || cJSON_IsNull(fcied_current);

			if (was_true_or_none && fcied_in_request && cJSON_IsFalse(fcied_in_request))
			{
				if (!fcied_current || cJSON_IsNull(fcied_current))
				{
					prepare_fcnt_for_instances(target_rtnode, o2pt);
					add_flexcontainer_instance(target_rtnode, o2pt, false);
				}

				cleanup_fcnt_instances(target_rtnode, true, true);

				cJSON *cs_obj = cJSON_GetObjectItem(m2m_fcnt, "cs");
				if (!cs_obj) cs_obj = cJSON_GetObjectItem(fcnt, "cs");
				int cs_value = cs_obj ? cs_obj->valueint : 0;
				cJSON_AddNumberToObject(m2m_fcnt, "cni", 1);
				cJSON_AddNumberToObject(m2m_fcnt, "cbs", cs_value);
			}
		}
		else if (cJSON_IsTrue(fcied_final))
		{
			bool was_false_or_none = !fcied_current || cJSON_IsFalse(fcied_current) || cJSON_IsNull(fcied_current);

			if (was_false_or_none && fcied_in_request && cJSON_IsTrue(fcied_in_request))
			{
				prepare_fcnt_for_instances(target_rtnode, o2pt);

				cJSON_AddNumberToObject(m2m_fcnt, "cni", 0);
				cJSON_AddNumberToObject(m2m_fcnt, "cbs", 0);
			}

			if (!cJSON_GetObjectItem(fcnt, "mni") && !cJSON_GetObjectItem(m2m_fcnt, "mni") &&
			    !cJSON_GetObjectItem(fcnt, "mbs") && !cJSON_GetObjectItem(m2m_fcnt, "mbs") &&
			    !cJSON_GetObjectItem(fcnt, "mia") && !cJSON_GetObjectItem(m2m_fcnt, "mia"))
			{
				cJSON_AddNumberToObject(m2m_fcnt, "mni", DEFAULT_MAX_NR_INSTANCES);
				cJSON_AddNumberToObject(m2m_fcnt, "mbs", DEFAULT_MAX_BYTE_SIZE);
				cJSON_AddNumberToObject(m2m_fcnt, "mia", DEFAULT_MAX_INSTANCE_AGE);
			}

			if (customAttrs ||
			    cJSON_GetObjectItem(m2m_fcnt, "lbl") ||
			    cJSON_GetObjectItem(m2m_fcnt, "loc"))
			{
				needs_fci = true;

				cJSON *cbs_obj = cJSON_GetObjectItem(fcnt, "cbs");
				cJSON *cni_obj = cJSON_GetObjectItem(fcnt, "cni");
				cJSON *cs_obj = cJSON_GetObjectItem(m2m_fcnt, "cs");
				if (!cs_obj) cs_obj = cJSON_GetObjectItem(fcnt, "cs");

				int cbs_value = cbs_obj ? cbs_obj->valueint : 0;
				int cni_value = cni_obj ? cni_obj->valueint : 0;
				int cs_value = cs_obj ? cs_obj->valueint : 0;

				cJSON_DeleteItemFromObject(m2m_fcnt, "cbs");
				cJSON_DeleteItemFromObject(m2m_fcnt, "cni");
				cJSON_AddNumberToObject(m2m_fcnt, "cbs", cbs_value + cs_value);
				cJSON_AddNumberToObject(m2m_fcnt, "cni", cni_value + 1);
			}

			if (was_false_or_none && fcied_in_request && cJSON_IsTrue(fcied_in_request))
			{
				if (!needs_fci)
				{
					needs_fci = true;
					cJSON *cs_obj = cJSON_GetObjectItem(m2m_fcnt, "cs");
					if (!cs_obj) cs_obj = cJSON_GetObjectItem(fcnt, "cs");
					int cs_val = cs_obj ? cs_obj->valueint : 0;
					cJSON_DeleteItemFromObject(m2m_fcnt, "cni");
					cJSON_DeleteItemFromObject(m2m_fcnt, "cbs");
					cJSON_AddNumberToObject(m2m_fcnt, "cni", 1);
					cJSON_AddNumberToObject(m2m_fcnt, "cbs", cs_val);
				}
			}

			cJSON *mni_final = cJSON_GetObjectItem(m2m_fcnt, "mni");
			if (!mni_final) mni_final = cJSON_GetObjectItem(fcnt, "mni");

			cJSON *mbs_final = cJSON_GetObjectItem(m2m_fcnt, "mbs");
			if (!mbs_final) mbs_final = cJSON_GetObjectItem(fcnt, "mbs");

			cJSON *cbs_final = cJSON_GetObjectItem(m2m_fcnt, "cbs");
			if (!cbs_final) cbs_final = cJSON_GetObjectItem(fcnt, "cbs");

			cJSON *cni_final = cJSON_GetObjectItem(m2m_fcnt, "cni");
			if (!cni_final) cni_final = cJSON_GetObjectItem(fcnt, "cni");

			cJSON *cs_final = cJSON_GetObjectItem(m2m_fcnt, "cs");
			if (!cs_final) cs_final = cJSON_GetObjectItem(fcnt, "cs");

			if (mbs_final && cJSON_IsNumber(mbs_final))
			{
				if (cs_final && cJSON_IsNumber(cs_final) && mbs_final->valueint < cs_final->valueint)
				{
					if (customAttrs) cJSON_Delete(customAttrs);
					return handle_error(o2pt, RSC_BAD_REQUEST,
						"mbs must be greater or equal to contentSize");
				}

				int deleted_count = 0;
				while (cbs_final && cJSON_IsNumber(cbs_final) &&
				       cbs_final->valueint > mbs_final->valueint &&
				       cbs_final->valueint > 0)
				{
					int deleted_cs = delete_oldest_fcin_rtnode(target_rtnode);
					if (deleted_cs < 0) {
						logger("FCNT", LOG_LEVEL_ERROR, "No more FCINs to delete for mbs enforcement");
						break;
					}

					cbs_final->valueint -= deleted_cs;
					deleted_count++;
					if (cni_final && cJSON_IsNumber(cni_final)) {
						cni_final->valueint--;
					}
				}

				if (deleted_count > 0) {
					int final_cbs_value = cbs_final->valueint;
					int final_cni_value = cni_final ? cni_final->valueint : 0;
					cJSON_DeleteItemFromObject(m2m_fcnt, "cbs");
					cJSON_AddNumberToObject(m2m_fcnt, "cbs", final_cbs_value);
					cJSON_DeleteItemFromObject(m2m_fcnt, "cni");
					cJSON_AddNumberToObject(m2m_fcnt, "cni", final_cni_value);
					logger("FCNT", LOG_LEVEL_INFO, "Deleted %d FCINs for mbs enforcement, updated cbs=%d, cni=%d", deleted_count, final_cbs_value, final_cni_value);
				}
			}

			if (mni_final && cJSON_IsNumber(mni_final))
			{
				int deleted_count = 0;
				while (cni_final && cJSON_IsNumber(cni_final) &&
				       cni_final->valueint > mni_final->valueint &&
				       cni_final->valueint > 0)
				{
					int deleted_cs = delete_oldest_fcin_rtnode(target_rtnode);
					if (deleted_cs < 0) {
						logger("FCNT", LOG_LEVEL_ERROR, "No more FCINs to delete for mni enforcement after %d deletions", deleted_count);
						break;
					}

					cni_final->valueint--;
					if (cbs_final && cJSON_IsNumber(cbs_final)) {
						cbs_final->valueint -= deleted_cs;
					}
					deleted_count++;
				}

				if (deleted_count > 0) {
					int final_cbs_value = cbs_final ? cbs_final->valueint : 0;
					int final_cni_value = cni_final->valueint;
					cJSON_DeleteItemFromObject(m2m_fcnt, "cni");
					cJSON_AddNumberToObject(m2m_fcnt, "cni", final_cni_value);
					cJSON_DeleteItemFromObject(m2m_fcnt, "cbs");
					cJSON_AddNumberToObject(m2m_fcnt, "cbs", final_cbs_value);
					logger("FCNT", LOG_LEVEL_INFO, "Deleted %d FCINs for mni enforcement, updated cni=%d, cbs=%d", deleted_count, final_cni_value, final_cbs_value);
				}
			}
		}
	}

	update_resource(target_rtnode->obj, m2m_fcnt);

	result = db_update_resource(m2m_fcnt, cJSON_GetObjectItem(target_rtnode->obj, "ri")->valuestring, RT_FCNT);

	if (customAttrs)
	{
		cJSON *ri_obj = cJSON_GetObjectItem(target_rtnode->obj, "ri");
		db_update_fcnt_custom_attributes(ri_obj->valuestring, customAttrs);

		// FCIN 생성 전: 인메모리 객체에도 커스텀 속성 반영
		// (add_flexcontainer_instance가 인메모리에서 읽으므로)
		cJSON *ca_item = NULL;
		cJSON_ArrayForEach(ca_item, customAttrs)
		{
			if (ca_item->string)
			{
				cJSON_DeleteItemFromObject(target_rtnode->obj, ca_item->string);
				cJSON_AddItemToObject(target_rtnode->obj, ca_item->string, cJSON_Duplicate(ca_item, 1));
			}
		}

		cJSON_Delete(customAttrs);
	}

	if (o2pt->rvi >= RVI_4 && needs_fci)
	{
		if (add_flexcontainer_instance(target_rtnode, o2pt, false) != 0)
		{
			// Rollback cni/cbs: undo the increment done before DB commit
			cJSON *cni_rb = cJSON_GetObjectItem(target_rtnode->obj, "cni");
			cJSON *cbs_rb = cJSON_GetObjectItem(target_rtnode->obj, "cbs");
			cJSON *cs_rb = cJSON_GetObjectItem(target_rtnode->obj, "cs");
			if (cni_rb && cJSON_IsNumber(cni_rb))
				cni_rb->valueint--;
			if (cbs_rb && cJSON_IsNumber(cbs_rb) && cs_rb && cJSON_IsNumber(cs_rb))
				cbs_rb->valueint -= cs_rb->valueint;

			cJSON *rb = cJSON_CreateObject();
			cJSON_AddNumberToObject(rb, "cni", cni_rb ? cni_rb->valueint : 0);
			cJSON_AddNumberToObject(rb, "cbs", cbs_rb ? cbs_rb->valueint : 0);
			db_update_resource(rb, cJSON_GetObjectItem(target_rtnode->obj, "ri")->valuestring, RT_FCNT);
			cJSON_Delete(rb);

			logger("FCNT", LOG_LEVEL_ERROR, "Failed to create FCIN, rolled back cni/cbs");
		}
	}

	for (int i = 0; i < updateAttrCnt; i++)
	{
		cJSON_DeleteItemFromArray(m2m_fcnt, 0);
	}

	make_response_body(o2pt, target_rtnode);
	o2pt->rsc = RSC_UPDATED;
	return RSC_UPDATED;
}
