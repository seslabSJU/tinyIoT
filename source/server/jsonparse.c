#include "onem2m.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

AE* JSON_to_AE(char *json_payload) {
	AE *ae = (AE *)malloc(sizeof(AE));

	cJSON *root = NULL;
	cJSON *api = NULL;
	cJSON *rr = NULL;
	cJSON *rn = NULL;

	cJSON* json = cJSON_Parse(json_payload);
	if (json == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			//fprintf(stderr, "Error before: %s\n", error_ptr);
		}
		return NULL;
	}

	root = cJSON_GetObjectItem(json, "m2m:ae");

	// api
	api = cJSON_GetObjectItem(root, "api");
	if (!cJSON_IsString(api) && (api->valuestring == NULL))
	{
		goto end;
	}
	ae->api = cJSON_Print(api);
	ae->api = strtok(ae->api, "\"");

	// rr
	rr = cJSON_GetObjectItemCaseSensitive(root, "rr");
	if (!cJSON_IsTrue(rr) && !cJSON_IsFalse(rr))
	{
		goto end;
	}
	else if (cJSON_IsTrue(rr))
	{
		ae->rr = true;
	}
	else if (cJSON_IsFalse(rr))
	{
		ae->rr = false;
	}
	
	// rn
	rn = cJSON_GetObjectItem(root, "rn");
	if (!cJSON_IsString(rn) && (rn->valuestring == NULL))
	{
		goto end;
	}
	ae->rn = cJSON_Print(rn);
	ae->rn = strtok(ae->rn, "\"");

end:
	cJSON_Delete(json);

	return ae;
}

CNT* JSON_to_CNT(char *json_payload) {
	CNT *cnt = (CNT *)malloc(sizeof(CNT));

	cJSON *root = NULL;
	cJSON *rn = NULL;
	
	cJSON* json = cJSON_Parse(json_payload);
	if (json == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			//fprintf(stderr, "Error before: %s\n", error_ptr);
		}
		return NULL;
	}

	root = cJSON_GetObjectItem(json, "m2m:cnt");
	// rn
	rn = cJSON_GetObjectItem(root, "rn");
	if (!cJSON_IsString(rn) && (rn->valuestring == NULL))
	{
		goto end;
	}
	cnt->rn = cJSON_Print(rn);
	cnt->rn = strtok(cnt->rn, "\"");

end:
	cJSON_Delete(json);

	return cnt;
}

CIN* JSON_to_CIN(char *json_payload) {
	CIN *cin = (CIN *)malloc(sizeof(CIN));

	cJSON *root = NULL;
	cJSON *con = NULL;

	cJSON* json = cJSON_Parse(json_payload);
	if (json == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			//fprintf(stderr, "Error before: %s\n", error_ptr);
		}
		return NULL;
	}

	root = cJSON_GetObjectItem(json, "m2m:cin");

	// con
	con = cJSON_GetObjectItem(root, "con");
	if (!cJSON_IsString(con) && (con->valuestring == NULL))
	{
		goto end;
	}
	cin->con = cJSON_Print(con);
	cin->con = strtok(cin->con, "\"");

	// cs
	cin->cs = strlen(cin->con);

end:
	cJSON_Delete(json);

	return cin;
}

Sub* JSON_to_Sub(char *json_payload) {
	Sub *sub = (Sub *)malloc(sizeof(Sub));

	cJSON *root = NULL;
	cJSON *rn = NULL;
	cJSON *enc = NULL;
	cJSON *net = NULL;
	cJSON *nu = NULL;

	cJSON* json = cJSON_Parse(json_payload);
	if (json == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			//fprintf(stderr, "Error before: %s\n", error_ptr);
		}
		return NULL;
	}

	root = cJSON_GetObjectItem(json, "m2m:sub");

	// rn
	rn = cJSON_GetObjectItem(root, "rn");
	if (!cJSON_IsString(rn) && rn->valuestring == NULL)
	{
		goto end;
	}
	sub->rn = cJSON_Print(rn);
	sub->rn = strtok(sub->rn, "\"");

	// enc
	enc = cJSON_GetObjectItem(root, "enc");

	// net
	net = cJSON_GetObjectItem(enc, "net");
	int net_size = cJSON_GetArraySize(net);
	char net_str[10] = { '\0' };
	char tmp[10] = { '\0' };
	for (int i = 0; i < net_size; i++) {
		sprintf(tmp, "%d", cJSON_GetArrayItem(net, i)->valueint);
		strcat(net_str, tmp);
		if (i < net_size - 1) {
			strcat(net_str, ",");
		}
	}
	sub->net = (char *)malloc(sizeof(char) * strlen(net_str) + 1);
	strcpy(sub->net, net_str);

	// nu
	nu = cJSON_GetObjectItem(root, "nu");
	int nu_size = cJSON_GetArraySize(nu);
	char nu_str[100] = { '\0' };
	for (int i = 0; i < nu_size; i++) {
		strcat(nu_str, cJSON_GetArrayItem(nu, i)->valuestring);
		if (i < nu_size - 1) {
			strcat(nu_str, ",");
		}
	}
	sub->nu = (char *)malloc(sizeof(char) * strlen(nu_str) + 1);
	strcpy(sub->nu, nu_str);


end:
	cJSON_Delete(json);

	return sub;
}

char* Node_to_json(Node *node) {
	char *json = NULL;

	cJSON *obj = NULL;
	cJSON *child = NULL;

	/* Our "obj" item: */
	obj = cJSON_CreateObject();
	cJSON_AddStringToObject(obj, "ri", node->ri);
	cJSON_AddStringToObject(obj, "rn", node->rn);
	cJSON_AddStringToObject(obj, "pi", node->pi);
	cJSON_AddNumberToObject(obj, "ty", node->ty);

	child = cJSON_CreateArray();
	cJSON_AddItemToObject(obj, "children", child);

	json = cJSON_Print(obj);

	cJSON_Delete(obj);

	return json;
}

char* CSE_to_json(CSE* cse_object) {
	char *json = NULL;

	cJSON *root = NULL;
	cJSON *cse = NULL;

	/* Our "cse" item: */
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:cb", cse = cJSON_CreateObject());
	cJSON_AddStringToObject(cse, "pi", cse_object->pi);
	cJSON_AddStringToObject(cse, "ri", cse_object->ri);
	cJSON_AddNumberToObject(cse, "ty", cse_object->ty);
	cJSON_AddStringToObject(cse, "ct", cse_object->ct);
	cJSON_AddStringToObject(cse, "rn", cse_object->rn);
	cJSON_AddStringToObject(cse, "lt", cse_object->lt);
	cJSON_AddStringToObject(cse, "csi", cse_object->csi);

	json = cJSON_Print(root);

	cJSON_Delete(root);

	return json;
}

char* AE_to_json(AE *ae_object) {
	char *json = NULL;

	cJSON *root = NULL;
	cJSON *ae = NULL;

	/* Our "ae" item: */
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:ae", ae = cJSON_CreateObject());
	cJSON_AddStringToObject(ae, "rn", ae_object->rn);
	cJSON_AddNumberToObject(ae, "ty", ae_object->ty);
	cJSON_AddStringToObject(ae, "pi", ae_object->pi);
	cJSON_AddStringToObject(ae, "ri", ae_object->ri);
	cJSON_AddStringToObject(ae, "ct", ae_object->ct);
	cJSON_AddStringToObject(ae, "lt", ae_object->lt);
	cJSON_AddStringToObject(ae, "et", ae_object->et);
	cJSON_AddStringToObject(ae, "api", ae_object->api);
	cJSON_AddBoolToObject(ae, "rr", ae_object->rr);
	cJSON_AddStringToObject(ae, "aei", ae_object->aei);

	json = cJSON_Print(root);

	cJSON_Delete(root);

	return json;
}

char* CNT_to_json(CNT* cnt_object) {
	char *json = NULL;

	cJSON *root = NULL;
	cJSON *cnt = NULL;

	/* Our "cnt" item: */
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:cnt", cnt = cJSON_CreateObject());
	cJSON_AddStringToObject(cnt, "rn", cnt_object->rn);
	cJSON_AddNumberToObject(cnt, "ty", cnt_object->ty);
	cJSON_AddStringToObject(cnt, "pi", cnt_object->pi);
	cJSON_AddStringToObject(cnt, "ri", cnt_object->ri);
	cJSON_AddStringToObject(cnt, "ct", cnt_object->ct);
	cJSON_AddStringToObject(cnt, "lt", cnt_object->lt);
	cJSON_AddNumberToObject(cnt, "st", cnt_object->st);
	cJSON_AddStringToObject(cnt, "et", cnt_object->et);
	cJSON_AddNumberToObject(cnt, "cni", cnt_object->cni);
	cJSON_AddNumberToObject(cnt, "cbs", cnt_object->cbs);

	json = cJSON_Print(root);

	cJSON_Delete(root);

	return json;
}

char* CIN_to_json(CIN* cin_object) {
	char *json = NULL;

	cJSON *root = NULL;
	cJSON *cin = NULL;

	/* Our "cin" item: */
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:cin", cin = cJSON_CreateObject());
	cJSON_AddStringToObject(cin, "rn", cin_object->rn);
	cJSON_AddNumberToObject(cin, "ty", cin_object->ty);
	cJSON_AddStringToObject(cin, "pi", cin_object->pi);
	cJSON_AddStringToObject(cin, "ri", cin_object->ri);
	cJSON_AddStringToObject(cin, "ct", cin_object->ct);
	cJSON_AddStringToObject(cin, "lt", cin_object->lt);
	cJSON_AddNumberToObject(cin, "st", cin_object->st);
	cJSON_AddStringToObject(cin, "et", cin_object->et);
	cJSON_AddNumberToObject(cin, "cs", cin_object->cs);
	cJSON_AddStringToObject(cin, "con", cin_object->con);

	json = cJSON_Print(root);

	cJSON_Delete(root);

	return json;
}

char* Sub_to_json(Sub *sub_object) {
	char *json = NULL;

	cJSON *root = NULL;
	cJSON *sub = NULL;
	cJSON *nu = NULL;
	cJSON *enc = NULL;
	cJSON *net = NULL;

	/* Our "sub" item: */
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:sub", sub = cJSON_CreateObject());
	cJSON_AddStringToObject(sub, "rn", sub_object->rn);
	cJSON_AddNumberToObject(sub, "ty", sub_object->ty);
	cJSON_AddStringToObject(sub, "pi", sub_object->pi);
	cJSON_AddStringToObject(sub, "ri", sub_object->ri);
	cJSON_AddStringToObject(sub, "ct", sub_object->ct);
	cJSON_AddStringToObject(sub, "lt", sub_object->lt);
	cJSON_AddStringToObject(sub, "et", sub_object->et);

	// nu
	nu = cJSON_CreateArray();
	char *nu_str = strtok(sub_object->nu, ",");
	do {
		cJSON_AddItemToArray(nu, cJSON_CreateString(nu_str));
		nu_str = strtok(NULL, ",");
	} while (nu_str != NULL);
	cJSON_AddItemToObject(sub, "nu", nu);

	// net
	cJSON_AddItemToObject(sub, "enc", enc = cJSON_CreateObject());

	net = cJSON_CreateArray();
	char *net_str = strtok(sub_object->net, ",");
	do {
		cJSON_AddItemToArray(net, cJSON_CreateNumber(atof(net_str)));
		net_str = strtok(NULL, ",");
	} while (net_str != NULL);
	cJSON_AddItemToObject(enc, "net", net);

	// nct
	cJSON_AddNumberToObject(sub, "nct", sub_object->nct);

	json = cJSON_Print(root);

	cJSON_Delete(root);

	return json;
}

char* Noti_to_json(char *sur, int net, char *rep) {
	char *json = NULL;

	cJSON *root = NULL;
	cJSON *sgn = NULL;
	cJSON *nev = NULL;

	/* Our "noti" item: */
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "m2m:sgn", sgn = cJSON_CreateObject());
	cJSON_AddStringToObject(sgn, "sur", sur);
	cJSON_AddItemToObject(sgn, "nev", nev = cJSON_CreateObject());
	cJSON_AddNumberToObject(nev, "net", net);
	cJSON_AddStringToObject(nev, "rep", rep);

	json = cJSON_Print(root);

	cJSON_Delete(root);

	return json;
}

char* JSON_label_value(char *json_payload) {
	char *resource = NULL;
	char *label_value = NULL;

	cJSON *root = NULL;
	cJSON *lbl = NULL;

	cJSON* json = cJSON_Parse(json_payload);
	if (json == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			//fprintf(stderr, "Error before: %s\n", error_ptr);
		}
		return NULL;
	}

	//Extracting resources from json_payload
	resource = strstr(json_payload, "m2m:");
	resource = strtok(resource, "\"");


	root = cJSON_GetObjectItem(json, resource);

	//lbl
	lbl = cJSON_GetObjectItem(root, "lbl");
	if (!cJSON_IsString(lbl) && lbl->valuestring == NULL)
	{
		goto end;
	}
	label_value = cJSON_Print(lbl);
	label_value = strtok(label_value, "\"");

end:
	cJSON_Delete(json);

	return label_value;
}

char* Get_JSON_Value_char(char *key, char *json) {
	char tmp[16] = "\"";
	strcat(tmp,key);
	strcat(tmp,"\":");
	if(!strstr(json,tmp)) return NULL;

	char json_copy[1024];
	char *resource = NULL;
	char *value = NULL;

	cJSON *root = NULL;
	cJSON *ckey = NULL;

	cJSON *cjson = cJSON_Parse(json);
	if (cjson == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			//fprintf(stderr, "Error before: %s\n", error_ptr);
		}
		return NULL;
	}

	//Extracting resources from json
	strcpy(json_copy, json);
	resource = strstr(json_copy, "m2m:");
	resource = strtok(resource, "\"");

	root = cJSON_GetObjectItem(cjson, resource);

	ckey = cJSON_GetObjectItem(root, key);
	if (!cJSON_IsString(ckey) && ckey->valuestring == NULL) {
		goto end;
	}
	value = cJSON_Print(ckey);
	value = strtok(value, "\"");

end:
	cJSON_Delete(cjson);

	return value;
}

int Get_JSON_Value_int(char *key, char *json) {
	char tmp[16] = "\"";
	strcat(tmp,key);
	strcat(tmp,"\":");
	if(!strstr(json,tmp)) return 0;

	char json_copy[1024];
	char *resource = NULL;
	int value = 0;

	cJSON *root = NULL;
	cJSON *ckey = NULL;

	cJSON *cjson = cJSON_Parse(json);
	if (cjson == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			fprintf(stderr, "Error before: %s\n", error_ptr);
		}
		goto end;
	}

	// Extracting resources from json
	strcpy(json_copy, json);
	resource = strstr(json_copy, "m2m:");
	resource = strtok(resource, "\"");

	root = cJSON_GetObjectItem(cjson, resource);

	ckey = cJSON_GetObjectItem(root, key);
	if (!cJSON_IsNumber(ckey)) {
		goto end;
	}
	value = ckey->valueint;

end:
	cJSON_Delete(cjson);

	return value;
}

bool Get_JSON_Value_bool(char *key, char *json) {
	char tmp[16] = "\"";
	strcat(tmp,key);
	strcat(tmp,"\":");
	if(!strstr(json,tmp)) return false;

	char json_copy[1024];
	char *resource = NULL;

	cJSON *root = NULL;
	cJSON *ckey = NULL;

	cJSON *cjson = cJSON_Parse(json);
	if (cjson == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			fprintf(stderr, "Error before: %s\n", error_ptr);
		}
		goto end;
	}

	//Extracting resources from json
	strcpy(json_copy, json);
	resource = strstr(json_copy, "m2m:");
	resource = strtok(resource, "\"");

	root = cJSON_GetObjectItem(cjson, resource);

	ckey = cJSON_GetObjectItem(root, key);
	if (!cJSON_IsTrue(ckey) && !cJSON_IsFalse(ckey))
	{
		goto end;
	}
	else if (cJSON_IsTrue(ckey))
	{
		return true;
	}
	else if (cJSON_IsFalse(ckey))
	{
		return false;
	}

end:
	cJSON_Delete(cjson);
}