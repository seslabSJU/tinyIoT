#ifndef SDT_H
#define SDT_H

#include "cJSON.h"

typedef struct {
    char *type;
    char *cnd;
    char *lname;
    char *sdttype;
    cJSON *attributes;
    cJSON *children;
} SDTDef;

typedef struct {
    SDTDef **defs;
    int count;
    cJSON *type_map;
    cJSON *cnd_map;
} SDTRegistry;

extern SDTRegistry *g_sdt;

int sdt_init(const char *dir_path);
void sdt_cleanup(void);
SDTDef* sdt_find_by_type(const char *type);
SDTDef* sdt_find_by_cnd(const char *cnd);
int sdt_validate_attr_type(const char *sdt_type, cJSON *value, char **error);
int sdt_validate_fcnt(const char *shortname, const char *cnd, cJSON *custom_attrs, char **error, int op);

#endif
