#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "logger.h"
#include "onem2mTypes.h"
#include "sdt.h"

/* sdt.c only needs logging while loading definitions.  Keeping a local stub
 * makes this a focused unit test without pulling in the complete server. */
int logger(const char *tag, LOGLEVEL level, const char *msg, ...)
{
    (void)tag;
    (void)level;
    (void)msg;
    return 0;
}

static int failures;

static void expect_rsc(const char *name, int actual, int expected)
{
    if (actual == expected) {
        printf("PASS: %s\n", name);
        return;
    }

    fprintf(stderr, "FAIL: %s (expected %d, got %d)\n",
            name, expected, actual);
    failures++;
}

static cJSON *parse_object(const char *json)
{
    cJSON *obj = cJSON_Parse(json);
    if (!obj || !cJSON_IsObject(obj)) {
        fprintf(stderr, "FAIL: could not parse test JSON: %s\n", json);
        cJSON_Delete(obj);
        failures++;
        return NULL;
    }
    return obj;
}

int main(void)
{
    const char *gis_cnd = "org.onem2m.genericInterworkingService";
    const char *gio_cnd = "org.onem2m.genericInterworkingOperationInstance";
    const char *missing_cnd = "urn:m2m:nonExistingSchemaDefinition.xsd";
    char *error = NULL;

    int loaded = sdt_init("sdt_definitions");
    if (loaded <= 0) {
        fprintf(stderr, "FAIL: no SDT definitions loaded\n");
        return 1;
    }

    SDTDef *gis = sdt_find_by_type("m2m:gis");
    if (!gis || !gis->cnd || strcmp(gis->cnd, gis_cnd) != 0) {
        fprintf(stderr, "FAIL: m2m:gis does not declare its standard CND\n");
        failures++;
    } else {
        printf("PASS: m2m:gis standard CND is registered\n");
    }

    cJSON *gis_attrs = parse_object("{\"gisn\":\"test-service\"}");
    if (gis_attrs) {
        expect_rsc("unknown GIS schema returns 4125",
                   sdt_validate_fcnt("m2m:gis", missing_cnd, gis_attrs,
                                     &error, OP_CREATE),
                   RSC_SPECIALIZATION_SCHEMA_NOT_FOUND);
        expect_rsc("standard GIS schema is accepted",
                   sdt_validate_fcnt("m2m:gis", gis_cnd, gis_attrs,
                                     &error, OP_CREATE),
                   RSC_OK);
        expect_rsc("bare flexContainer resolves the GIS schema by CND",
                   sdt_validate_fcnt(NULL, gis_cnd, gis_attrs,
                                     &error, OP_CREATE),
                   RSC_OK);
        expect_rsc("known schema with the wrong specialization returns 4000",
                   sdt_validate_fcnt("m2m:gio", gis_cnd, gis_attrs,
                                     &error, OP_CREATE),
                   RSC_BAD_REQUEST);
        cJSON_Delete(gis_attrs);
    }

    expect_rsc("unknown bare flexContainer schema returns 4125",
               sdt_validate_fcnt(NULL, missing_cnd, NULL,
                                 &error, OP_CREATE),
               RSC_SPECIALIZATION_SCHEMA_NOT_FOUND);

    expect_rsc("GIS mandatory serviceName is enforced",
               sdt_validate_fcnt("m2m:gis", gis_cnd, NULL,
                                 &error, OP_CREATE),
               RSC_BAD_REQUEST);

    cJSON *gio_attrs = parse_object(
        "{\"gion\":\"test-operation\",\"gios\":\"operation_ended\"}");
    if (gio_attrs) {
        expect_rsc("standard GIO schema is accepted",
                   sdt_validate_fcnt("m2m:gio", gio_cnd, gio_attrs,
                                     &error, OP_CREATE),
                   RSC_OK);
        cJSON_Delete(gio_attrs);
    }

    sdt_cleanup();

    if (failures) {
        fprintf(stderr, "%d schema validation test(s) failed\n", failures);
        return 1;
    }

    printf("All schema validation tests passed.\n");
    return 0;
}
