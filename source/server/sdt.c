#include "sdt.h"
#include "logger.h"
#include "onem2mTypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

SDTRegistry *g_sdt = NULL;

static SDTDef* parse_sdt_def(cJSON *json) {
    SDTDef *def = calloc(1, sizeof(SDTDef));
    if (!def) return NULL;

    cJSON *item = cJSON_GetObjectItem(json, "type");
    def->type = item ? strdup(item->valuestring) : NULL;

    item = cJSON_GetObjectItem(json, "cnd");
    def->cnd = item ? strdup(item->valuestring) : NULL;

    item = cJSON_GetObjectItem(json, "lname");
    def->lname = item ? strdup(item->valuestring) : NULL;

    item = cJSON_GetObjectItem(json, "sdttype");
    def->sdttype = item ? strdup(item->valuestring) : NULL;

    item = cJSON_GetObjectItem(json, "attributes");
    def->attributes = item ? cJSON_Duplicate(item, 1) : NULL;

    item = cJSON_GetObjectItem(json, "children");
    def->children = item ? cJSON_Duplicate(item, 1) : NULL;

    return def;
}

static int load_fcp_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(fp);
        return -1;
    }

    fread(buf, 1, size, fp);
    buf[size] = 0;
    fclose(fp);

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        return -1;
    }

    if (!cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return -1;
    }

    int count = cJSON_GetArraySize(root);
    for (int i = 0; i < count; i++) {
        cJSON *def_json = cJSON_GetArrayItem(root, i);
        SDTDef *def = parse_sdt_def(def_json);

        if (def && def->type) {
            g_sdt->defs = realloc(g_sdt->defs, sizeof(SDTDef*) * (g_sdt->count + 1));
            g_sdt->defs[g_sdt->count++] = def;

            if (def->cnd && strlen(def->cnd) > 0) {
                cJSON_AddStringToObject(g_sdt->type_map, def->type, def->cnd);
                cJSON_AddStringToObject(g_sdt->cnd_map, def->cnd, def->type);
            }
        } else if (def) {
            free(def->type);
            free(def->cnd);
            free(def->lname);
            free(def->sdttype);
            if (def->attributes) cJSON_Delete(def->attributes);
            if (def->children) cJSON_Delete(def->children);
            free(def);
        }
    }

    cJSON_Delete(root);
    return count;
}

int sdt_init(const char *dir_path) {
    g_sdt = calloc(1, sizeof(SDTRegistry));
    if (!g_sdt) {
        return -1;
    }

    g_sdt->type_map = cJSON_CreateObject();
    g_sdt->cnd_map = cJSON_CreateObject();

    DIR *dir = opendir(dir_path);
    if (!dir) {
        logger("SDT", LOG_LEVEL_WARN, "Cannot open SDT directory: %s", dir_path);
        return -1;
    }

    struct dirent *entry;
    int total = 0;
    int files = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".fcp")) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

            int n = load_fcp_file(path);
            if (n > 0) {
                total += n;
                files++;
            }
        }
    }

    closedir(dir);

    if (total > 0) {
        logger("SDT", LOG_LEVEL_INFO, "Loaded %d SDT definitions from %d .fcp files", total, files);
    }

    return total;
}

void sdt_cleanup(void) {
    if (!g_sdt) return;

    for (int i = 0; i < g_sdt->count; i++) {
        SDTDef *def = g_sdt->defs[i];
        if (def) {
            free(def->type);
            free(def->cnd);
            free(def->lname);
            free(def->sdttype);
            if (def->attributes) cJSON_Delete(def->attributes);
            if (def->children) cJSON_Delete(def->children);
            free(def);
        }
    }

    free(g_sdt->defs);
    if (g_sdt->type_map) cJSON_Delete(g_sdt->type_map);
    if (g_sdt->cnd_map) cJSON_Delete(g_sdt->cnd_map);
    free(g_sdt);
    g_sdt = NULL;
}

SDTDef* sdt_find_by_type(const char *type) {
    if (!g_sdt || !type) return NULL;

    for (int i = 0; i < g_sdt->count; i++) {
        if (g_sdt->defs[i]->type && strcmp(g_sdt->defs[i]->type, type) == 0) {
            return g_sdt->defs[i];
        }
    }
    return NULL;
}

SDTDef* sdt_find_by_cnd(const char *cnd) {
    if (!g_sdt || !cnd) return NULL;

    for (int i = 0; i < g_sdt->count; i++) {
        if (g_sdt->defs[i]->cnd && strcmp(g_sdt->defs[i]->cnd, cnd) == 0) {
            return g_sdt->defs[i];
        }
    }
    return NULL;
}

int sdt_validate_fcnt(const char *shortname, const char *cnd, cJSON *custom_attrs, char **error, int check_mandatory) {
    if (!shortname || !cnd) {
        *error = "Missing shortname or containerDefinition";
        return RSC_BAD_REQUEST;
    }

    SDTDef *def = sdt_find_by_type(shortname);
    if (!def) {
        return RSC_OK;
    }

    if (def->cnd && strlen(def->cnd) > 0) {
        if (strcmp(def->cnd, cnd) != 0) {
            *error = "Mismatch between shortname and containerDefinition";
            return RSC_BAD_REQUEST;
        }
    }

    if (!def->attributes) {
        return RSC_OK;
    }

    if (check_mandatory) {
        int attr_count = cJSON_GetArraySize(def->attributes);
        for (int i = 0; i < attr_count; i++) {
            cJSON *attr_def = cJSON_GetArrayItem(def->attributes, i);
            if (!attr_def) continue;

            cJSON *sname_item = cJSON_GetObjectItem(attr_def, "sname");
            cJSON *oc_item = cJSON_GetObjectItem(attr_def, "oc");

            if (!sname_item) continue;

            char *sname = sname_item->valuestring;
            char *oc = oc_item ? oc_item->valuestring : "O";

            if (strcmp(oc, "M") == 0) {
                if (!custom_attrs || !cJSON_GetObjectItem(custom_attrs, sname)) {
                    *error = "Missing mandatory attribute";
                    return RSC_BAD_REQUEST;
                }
            }
        }
    }

    return RSC_OK;
}
