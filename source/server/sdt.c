#include "sdt.h"
#include "logger.h"
#include "onem2mTypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <regex.h>

SDTRegistry *g_sdt = NULL;

/**
 * @brief Remove comments from JSON string
 * Removes single-line (//, #) and multi-line comments, but not inside strings
 */
static char* remove_json_comments(const char *input) {
    if (!input) return NULL;

    size_t len = strlen(input);
    char *output = malloc(len + 1);
    if (!output) return NULL;

    const char *src = input;
    char *dst = output;
    bool in_string = false;

    while (*src) {
        // Handle strings
        if (*src == '"' && (src == input || *(src-1) != '\\')) {
            in_string = !in_string;
            *dst++ = *src++;
            continue;
        }

        if (!in_string) {
            // Remove // comments
            if (*src == '/' && *(src+1) == '/') {
                while (*src && *src != '\n') src++;
                continue;
            }
            // Remove # comments
            if (*src == '#') {
                while (*src && *src != '\n') src++;
                continue;
            }
            // Remove /* */ comments
            if (*src == '/' && *(src+1) == '*') {
                src += 2;
                while (*src && !(*src == '*' && *(src+1) == '/')) src++;
                if (*src) src += 2;
                continue;
            }
        }

        *dst++ = *src++;
    }

    *dst = '\0';
    return output;
}

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
        logger("SDT", LOG_LEVEL_ERROR, "Failed to open file: %s", path);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    if (size <= 0) {
        fclose(fp);
        logger("SDT", LOG_LEVEL_ERROR, "Invalid file size: %s", path);
        return -1;
    }
    fseek(fp, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(fp);
        logger("SDT", LOG_LEVEL_ERROR, "Failed to allocate memory for file: %s", path);
        return -1;
    }

    size_t read_size = fread(buf, 1, size, fp);
    buf[read_size] = '\0';
    fclose(fp);

    // Remove comments from JSON
    char *clean_buf = remove_json_comments(buf);
    free(buf);

    if (!clean_buf) {
        logger("SDT", LOG_LEVEL_ERROR, "Failed to remove comments from file: %s", path);
        return -1;
    }

    cJSON *root = cJSON_Parse(clean_buf);
    free(clean_buf);

    if (!root) {
        logger("SDT", LOG_LEVEL_ERROR, "Failed to parse JSON from file: %s", path);
        return -1;
    }

    if (!cJSON_IsArray(root)) {
        logger("SDT", LOG_LEVEL_ERROR, "JSON root is not an array: %s", path);
        cJSON_Delete(root);
        return -1;
    }

    int count = cJSON_GetArraySize(root);
    for (int i = 0; i < count; i++) {
        cJSON *def_json = cJSON_GetArrayItem(root, i);
        if (!def_json) continue;

        SDTDef *def = parse_sdt_def(def_json);

        if (def && def->type) {
            SDTDef **new_defs = realloc(g_sdt->defs, sizeof(SDTDef*) * (g_sdt->count + 1));
            if (!new_defs) {
                logger("SDT", LOG_LEVEL_ERROR, "Failed to allocate memory for SDT definitions");
                // Free the current def
                free(def->type);
                free(def->cnd);
                free(def->lname);
                free(def->sdttype);
                if (def->attributes) cJSON_Delete(def->attributes);
                if (def->children) cJSON_Delete(def->children);
                free(def);
                break;
            }
            g_sdt->defs = new_defs;
            g_sdt->defs[g_sdt->count++] = def;

            if (def->cnd && strlen(def->cnd) > 0) {
                cJSON_AddStringToObject(g_sdt->type_map, def->type, def->cnd);
                cJSON_AddStringToObject(g_sdt->cnd_map, def->cnd, def->type);
            }
        } else if (def) {
            // Clean up partially created def
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
    if (!dir_path) {
        logger("SDT", LOG_LEVEL_ERROR, "Invalid directory path");
        return -1;
    }

    g_sdt = calloc(1, sizeof(SDTRegistry));
    if (!g_sdt) {
        logger("SDT", LOG_LEVEL_ERROR, "Failed to allocate SDT registry");
        return -1;
    }

    g_sdt->type_map = cJSON_CreateObject();
    g_sdt->cnd_map = cJSON_CreateObject();

    if (!g_sdt->type_map || !g_sdt->cnd_map) {
        logger("SDT", LOG_LEVEL_ERROR, "Failed to create SDT maps");
        sdt_cleanup();
        return -1;
    }

    DIR *dir = opendir(dir_path);
    if (!dir) {
        logger("SDT", LOG_LEVEL_WARN, "Cannot open SDT directory: %s", dir_path);
        sdt_cleanup();
        return -1;
    }

    struct dirent *entry;
    int total = 0;
    int files = 0;
    size_t dir_len = strlen(dir_path);

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".fcp")) {
            // Calculate required path length
            size_t path_len = dir_len + strlen(entry->d_name) + 2; // +2 for '/' and '\0'
            char *path = malloc(path_len);
            if (!path) {
                logger("SDT", LOG_LEVEL_ERROR, "Failed to allocate memory for path");
                continue;
            }

            snprintf(path, path_len, "%s/%s", dir_path, entry->d_name);

            int n = load_fcp_file(path);
            free(path);

            if (n > 0) {
                total += n;
                files++;
            } else if (n < 0) {
                logger("SDT", LOG_LEVEL_WARN, "Failed to load file: %s", entry->d_name);
            }
        }
    }

    closedir(dir);

    if (total > 0) {
        logger("SDT", LOG_LEVEL_INFO, "Loaded %d SDT definitions from %d .fcp files", total, files);
    } else {
        logger("SDT", LOG_LEVEL_WARN, "No SDT definitions loaded from directory: %s", dir_path);
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

static int is_valid_timestamp(const char *s) {
    if (!s || strlen(s) < 15) return 0;
    // yyyyMMddTHHmmss (basic) or yyyy-MM-ddTHH:mm:ss (extended)
    for (int i = 0; i < 8 && s[i]; i++) {
        if (s[i] == '-') continue;
        if (s[i] < '0' || s[i] > '9') return 0;
    }
    return 1;
}

static int is_valid_date(const char *s) {
    if (!s || strlen(s) < 8) return 0;
    for (int i = 0; i < 8 && s[i]; i++) {
        if (s[i] == '-') continue;
        if (s[i] < '0' || s[i] > '9') return 0;
    }
    return 1;
}

static int is_valid_time(const char *s) {
    if (!s || strlen(s) < 6) return 0;
    for (int i = 0; i < 6 && s[i]; i++) {
        if (s[i] == ':') continue;
        if (s[i] < '0' || s[i] > '9') return 0;
    }
    return 1;
}

static int is_valid_base64(const char *s) {
    if (!s) return 0;
    for (size_t i = 0; s[i]; i++) {
        char c = s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=') continue;
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        return 0;
    }
    return 1;
}

static int is_valid_token(const char *s) {
    if (!s || strlen(s) == 0) return 0;
    for (const char *p = s; *p; p++) {
        if (*p == '\t' || *p == '\n' || *p == '\r') return 0;
        if (*p == ' ' && *(p+1) == ' ') return 0;
    }
    return 1;
}

static int is_valid_ncname(const char *s) {
    if (!s || strlen(s) == 0) return 0;
    char c = s[0];
    if ((c >= '0' && c <= '9') || c == '-' || c == '.') return 0;
    for (const char *p = s; *p; p++) {
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') return 0;
    }
    return 1;
}

static int is_valid_duration(const char *s) {
    if (!s || strlen(s) < 2) return 0;
    return s[0] == 'P' || s[0] == 'p';
}

int sdt_validate_attr_type(const char *sdt_type, cJSON *value, char **error) {
    if (!sdt_type || !value) return RSC_OK;

    // String types
    if (strcmp(sdt_type, "string") == 0 || strcmp(sdt_type, "anyURI") == 0 ||
        strcmp(sdt_type, "anyuri") == 0 || strcmp(sdt_type, "ID") == 0 ||
        strcmp(sdt_type, "imsi") == 0 || strcmp(sdt_type, "iccid") == 0 ||
        strcmp(sdt_type, "ipv4Address") == 0 || strcmp(sdt_type, "ipv6Address") == 0) {
        if (!cJSON_IsString(value)) {
            if (error) *error = "Expected string value";
            return RSC_BAD_REQUEST;
        }
    }
    else if (strcmp(sdt_type, "token") == 0) {
        if (!cJSON_IsString(value)) {
            if (error) *error = "Expected token string";
            return RSC_BAD_REQUEST;
        }
        if (!is_valid_token(value->valuestring)) {
            if (error) *error = "Invalid token format";
            return RSC_BAD_REQUEST;
        }
    }
    else if (strcmp(sdt_type, "ncname") == 0) {
        if (!cJSON_IsString(value)) {
            if (error) *error = "Expected NCName string";
            return RSC_BAD_REQUEST;
        }
        if (!is_valid_ncname(value->valuestring)) {
            if (error) *error = "Invalid NCName format";
            return RSC_BAD_REQUEST;
        }
    }
    else if (strcmp(sdt_type, "base64") == 0) {
        if (!cJSON_IsString(value)) {
            if (error) *error = "Expected base64 string";
            return RSC_BAD_REQUEST;
        }
        if (!is_valid_base64(value->valuestring)) {
            if (error) *error = "Invalid base64 encoding";
            return RSC_BAD_REQUEST;
        }
    }
    // Integer types
    else if (strcmp(sdt_type, "integer") == 0 || strcmp(sdt_type, "enum") == 0) {
        if (!cJSON_IsNumber(value)) {
            if (error) *error = "Expected integer value";
            return RSC_BAD_REQUEST;
        }
    }
    else if (strcmp(sdt_type, "positiveInteger") == 0) {
        if (!cJSON_IsNumber(value)) {
            if (error) *error = "Expected positive integer value";
            return RSC_BAD_REQUEST;
        }
        if (value->valuedouble <= 0) {
            if (error) *error = "Value must be positive";
            return RSC_BAD_REQUEST;
        }
    }
    else if (strcmp(sdt_type, "nonNegInteger") == 0 ||
             strcmp(sdt_type, "unsignedInt") == 0 ||
             strcmp(sdt_type, "unsignedLong") == 0) {
        if (!cJSON_IsNumber(value)) {
            if (error) *error = "Expected non-negative integer value";
            return RSC_BAD_REQUEST;
        }
        if (value->valuedouble < 0) {
            if (error) *error = "Value must be non-negative";
            return RSC_BAD_REQUEST;
        }
    }
    else if (strcmp(sdt_type, "float") == 0) {
        if (!cJSON_IsNumber(value)) {
            if (error) *error = "Expected float value";
            return RSC_BAD_REQUEST;
        }
    }
    else if (strcmp(sdt_type, "boolean") == 0) {
        if (!cJSON_IsBool(value)) {
            if (error) *error = "Expected boolean value";
            return RSC_BAD_REQUEST;
        }
    }
    // Timestamp types
    else if (strcmp(sdt_type, "timestamp") == 0 || strcmp(sdt_type, "dateTime") == 0) {
        if (!cJSON_IsString(value)) {
            if (error) *error = "Expected timestamp string";
            return RSC_BAD_REQUEST;
        }
        if (!is_valid_timestamp(value->valuestring)) {
            if (error) *error = "Invalid timestamp format";
            return RSC_BAD_REQUEST;
        }
    }
    else if (strcmp(sdt_type, "absRelTimestamp") == 0) {
        if (!cJSON_IsString(value) && !cJSON_IsNumber(value)) {
            if (error) *error = "Expected timestamp string or integer";
            return RSC_BAD_REQUEST;
        }
    }
    else if (strcmp(sdt_type, "duration") == 0) {
        if (!cJSON_IsString(value)) {
            if (error) *error = "Expected duration string";
            return RSC_BAD_REQUEST;
        }
        if (!is_valid_duration(value->valuestring)) {
            if (error) *error = "Invalid duration format";
            return RSC_BAD_REQUEST;
        }
    }
    else if (strcmp(sdt_type, "date") == 0) {
        if (!cJSON_IsString(value)) {
            if (error) *error = "Expected date string";
            return RSC_BAD_REQUEST;
        }
        if (!is_valid_date(value->valuestring)) {
            if (error) *error = "Invalid date format";
            return RSC_BAD_REQUEST;
        }
    }
    else if (strcmp(sdt_type, "time") == 0) {
        if (!cJSON_IsString(value)) {
            if (error) *error = "Expected time string";
            return RSC_BAD_REQUEST;
        }
        if (!is_valid_time(value->valuestring)) {
            if (error) *error = "Invalid time format";
            return RSC_BAD_REQUEST;
        }
    }
    // Collection types
    else if (strcmp(sdt_type, "list") == 0) {
        if (!cJSON_IsArray(value)) {
            if (error) *error = "Expected array value";
            return RSC_BAD_REQUEST;
        }
    }
    else if (strcmp(sdt_type, "listNE") == 0) {
        if (!cJSON_IsArray(value)) {
            if (error) *error = "Expected non-empty array value";
            return RSC_BAD_REQUEST;
        }
        if (cJSON_GetArraySize(value) == 0) {
            if (error) *error = "Array must not be empty";
            return RSC_BAD_REQUEST;
        }
    }
    // Object types
    else if (strcmp(sdt_type, "complex") == 0 || strcmp(sdt_type, "dict") == 0) {
        if (!cJSON_IsObject(value)) {
            if (error) *error = "Expected object value";
            return RSC_BAD_REQUEST;
        }
    }
    // Structured string types
    else if (strcmp(sdt_type, "schedule") == 0 || strcmp(sdt_type, "geoJsonCoordinate") == 0) {
        if (!cJSON_IsString(value)) {
            if (error) *error = "Expected string value";
            return RSC_BAD_REQUEST;
        }
    }
    // Permissive types
    else if (strcmp(sdt_type, "jsonLike") == 0 || strcmp(sdt_type, "any") == 0 ||
             strcmp(sdt_type, "void") == 0) {
        // Accept any value
    }
    // Unknown type -> reject (ACME compatible)
    else {
        if (error) *error = "Unknown attribute type";
        return RSC_BAD_REQUEST;
    }

    return RSC_OK;
}

int sdt_validate_fcnt(const char *shortname, const char *cnd, cJSON *custom_attrs, char **error, int op) {
    if (!cnd) {
        *error = "Missing containerDefinition";
        return RSC_BAD_REQUEST;
    }

    SDTDef *def = NULL;
    if (shortname) {
        def = sdt_find_by_type(shortname);
    }

    if (!def) {
        if (shortname) {
            *error = "unknown resource type";
            return RSC_BAD_REQUEST;
        }
        // m2m:fcnt without specialization shortname: skip SDT validation (ACME compatible)
        return RSC_OK;
    }

    if (def->cnd && strlen(def->cnd) > 0) {
        if (strcmp(def->cnd, cnd) != 0) {
            *error = "Mismatch between shortname and containerDefinition";
            return RSC_BAD_REQUEST;
        }
    }

    // No attributes defined: reject any custom attributes (ACME __none__ behavior)
    if (!def->attributes) {
        if (custom_attrs && cJSON_GetArraySize(custom_attrs) > 0) {
            *error = "Unknown attribute in FlexContainer";
            return RSC_BAD_REQUEST;
        }
        return RSC_OK;
    }

    int attr_count = cJSON_GetArraySize(def->attributes);

    // Validate each custom attribute against SDT definition
    if (custom_attrs) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, custom_attrs) {
            if (!item->string) continue;

            int found = 0;
            for (int i = 0; i < attr_count; i++) {
                cJSON *attr_def = cJSON_GetArrayItem(def->attributes, i);
                if (!attr_def) continue;

                cJSON *sname_item = cJSON_GetObjectItem(attr_def, "sname");
                if (!sname_item || strcmp(sname_item->valuestring, item->string) != 0)
                    continue;

                found = 1;

                // Type validation
                cJSON *type_item = cJSON_GetObjectItem(attr_def, "type");
                if (type_item && cJSON_IsString(type_item)) {
                    int rsc = sdt_validate_attr_type(type_item->valuestring, item, error);
                    if (rsc != RSC_OK) return rsc;
                }

                // UPDATE: check ou="NP" (attribute must not be provided)
                if (op == 3) { // OP_UPDATE
                    cJSON *ou_item = cJSON_GetObjectItem(attr_def, "ou");
                    if (ou_item && cJSON_IsString(ou_item) &&
                        strcmp(ou_item->valuestring, "NP") == 0) {
                        *error = "Attribute is not allowed in UPDATE";
                        return RSC_BAD_REQUEST;
                    }
                }

                // Cardinality: CAR1 means value cannot be null
                cJSON *car_item = cJSON_GetObjectItem(attr_def, "car");
                if (car_item && cJSON_IsString(car_item)) {
                    if (strcmp(car_item->valuestring, "1") == 0 ||
                        strcmp(car_item->valuestring, "1N") == 0) {
                        if (cJSON_IsNull(item)) {
                            *error = "Attribute with cardinality 1 cannot be null";
                            return RSC_BAD_REQUEST;
                        }
                    }
                }

                break;
            }
            if (!found) {
                *error = "Unknown attribute in FlexContainer";
                return RSC_BAD_REQUEST;
            }
        }
    }

    // CREATE: check mandatory attributes (oc="M")
    if (op == 1) { // OP_CREATE
        for (int i = 0; i < attr_count; i++) {
            cJSON *attr_def = cJSON_GetArrayItem(def->attributes, i);
            if (!attr_def) continue;

            cJSON *sname_item = cJSON_GetObjectItem(attr_def, "sname");
            cJSON *oc_item = cJSON_GetObjectItem(attr_def, "oc");
            if (!sname_item) continue;

            char *oc = oc_item ? oc_item->valuestring : "O";
            if (strcmp(oc, "M") == 0) {
                if (!custom_attrs || !cJSON_GetObjectItem(custom_attrs, sname_item->valuestring)) {
                    *error = "Missing mandatory attribute";
                    return RSC_BAD_REQUEST;
                }
            }
        }
    }

    return RSC_OK;
}
