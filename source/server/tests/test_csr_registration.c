#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "dbmanager.h"
#include "logger.h"
#include "onem2m.h"
#include "util.h"

ResourceTree *rt;
cJSON *ATTRIBUTES;
pthread_mutex_t main_lock = PTHREAD_MUTEX_INITIALIZER;

static RTNode created_node;
static char stored_uri[256];
static int store_calls;
static int failures;

int logger(const char *tag, LOGLEVEL level, const char *msg, ...)
{
    (void)tag;
    (void)level;
    (void)msg;
    return 0;
}

int check_rn_invalid(oneM2MPrimitive *o2pt, ResourceType ty)
{
    (void)o2pt;
    (void)ty;
    return 0;
}

int handle_error(oneM2MPrimitive *o2pt, int rsc, char *err)
{
    (void)err;
    o2pt->rsc = rsc;
    return rsc;
}

void add_general_attribute(cJSON *resource, RTNode *parent, ResourceType ty)
{
    cJSON_AddNumberToObject(resource, "ty", ty);
    cJSON_AddStringToObject(resource, "ri", "16-generated");
    if (!cJSON_GetObjectItem(resource, "rn"))
        cJSON_AddStringToObject(resource, "rn", "16-generated");
    cJSON_AddStringToObject(resource, "pi",
                            cJSON_GetObjectItem(parent->obj, "ri")->valuestring);
}

int validate_mandatory_attrs(ResourceType ty, cJSON *obj, Operation op,
                             char **error_msg)
{
    (void)ty;
    (void)obj;
    (void)op;
    (void)error_msg;
    return RSC_OK;
}

char *get_uri_rtnode(RTNode *rtnode)
{
    return rtnode->uri;
}

int db_store_resource(cJSON *obj, char *uri)
{
    (void)obj;
    store_calls++;
    snprintf(stored_uri, sizeof(stored_uri), "%s", uri);
    return 1;
}

RTNode *create_rtnode(cJSON *resource, ResourceType ty)
{
    memset(&created_node, 0, sizeof(created_node));
    created_node.obj = resource;
    created_node.ty = ty;
    return &created_node;
}

int add_child_resource_tree(RTNode *parent, RTNode *child)
{
    parent->child = child;
    child->parent = parent;
    return 1;
}

int make_response_body(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{
    (void)o2pt;
    (void)target_rtnode;
    return 0;
}

int update_remote_csr_dcse(void)
{
    return 0;
}

static void expect_int(const char *name, int actual, int expected)
{
    if (actual == expected) {
        printf("PASS: %s\n", name);
        return;
    }

    fprintf(stderr, "FAIL: %s (expected %d, got %d)\n",
            name, expected, actual);
    failures++;
}

static void expect_string(const char *name, const char *actual,
                          const char *expected)
{
    if (actual && strcmp(actual, expected) == 0) {
        printf("PASS: %s\n", name);
        return;
    }

    fprintf(stderr, "FAIL: %s (expected '%s', got '%s')\n",
            name, expected, actual ? actual : "(null)");
    failures++;
}

static const char *json_string(cJSON *obj, const char *name)
{
    cJSON *item = cJSON_GetObjectItem(obj, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static void init_parent(ResourceTree *tree, RTNode *parent)
{
    memset(tree, 0, sizeof(*tree));
    memset(parent, 0, sizeof(*parent));
    memset(&created_node, 0, sizeof(created_node));
    memset(stored_uri, 0, sizeof(stored_uri));
    store_calls = 0;

    parent->ty = RT_CSE;
    parent->uri = "TinyIoT";
    parent->obj = cJSON_CreateObject();
    cJSON_AddStringToObject(parent->obj, "ri", "tinyiot");
    cJSON_AddItemToObject(parent->obj, "dcse", cJSON_CreateArray());
    tree->cb = parent;
    rt = tree;
}

static cJSON *new_csr_request(const char *rn, const char *csi)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *csr = cJSON_AddObjectToObject(root, "m2m:csr");
    if (rn)
        cJSON_AddStringToObject(csr, "rn", rn);
    if (csi)
        cJSON_AddStringToObject(csr, "csi", csi);
    cJSON_AddStringToObject(csr, "cb", "/id-mn/MnCSE");
    cJSON_AddItemToObject(csr, "srv", cJSON_CreateStringArray(
        (const char *const[]){"3"}, 1));
    cJSON_AddBoolToObject(csr, "rr", 1);
    return root;
}

static void free_success_fixture(cJSON *request, RTNode *parent)
{
    cJSON_Delete(request);
    cJSON_Delete(created_node.obj);
    created_node.obj = NULL;
    cJSON_Delete(parent->obj);
    parent->obj = NULL;
}

static void test_supplied_name_and_generated_id_are_preserved(void)
{
    ResourceTree tree;
    RTNode parent;
    init_parent(&tree, &parent);

    cJSON *request = new_csr_request("remote-by-name", "/spoofed");
    oneM2MPrimitive o2pt = {0};
    o2pt.fr = "/id-mn";
    o2pt.request_pc = request;

    expect_int("remoteCSE create succeeds",
               create_csr(&o2pt, &parent), RSC_CREATED);
    expect_string("supplied rn is preserved",
                  json_string(created_node.obj, "rn"), "remote-by-name");
    expect_string("Hosting CSE generated ri is preserved",
                  json_string(created_node.obj, "ri"), "16-generated");
    expect_string("csi is assigned from From",
                  json_string(created_node.obj, "csi"), "/id-mn");
    expect_string("structured URI uses supplied rn", stored_uri,
                  "TinyIoT/remote-by-name");
    expect_int("resource is stored once", store_calls, 1);

    free_success_fixture(request, &parent);
}

static void test_missing_name_keeps_generated_name(void)
{
    ResourceTree tree;
    RTNode parent;
    init_parent(&tree, &parent);

    cJSON *request = new_csr_request(NULL, NULL);
    oneM2MPrimitive o2pt = {0};
    o2pt.fr = "/id-mn";
    o2pt.request_pc = request;

    expect_int("remoteCSE create without rn succeeds",
               create_csr(&o2pt, &parent), RSC_CREATED);
    expect_string("missing rn receives Hosting CSE generated name",
                  json_string(created_node.obj, "rn"), "16-generated");

    free_success_fixture(request, &parent);
}

static void test_duplicate_csi_is_rejected(void)
{
    ResourceTree tree;
    RTNode parent;
    RTNode existing = {0};
    init_parent(&tree, &parent);

    existing.ty = RT_CSR;
    existing.obj = cJSON_CreateObject();
    cJSON_AddStringToObject(existing.obj, "ri", "16-existing");
    cJSON_AddStringToObject(existing.obj, "csi", "/id-mn");
    parent.child = &existing;

    cJSON *request = new_csr_request("another-name", NULL);
    oneM2MPrimitive o2pt = {0};
    o2pt.fr = "/id-mn";
    o2pt.request_pc = request;

    expect_int("duplicate CSE-ID returns 4117",
               create_csr(&o2pt, &parent),
               RSC_ORIGINATOR_HAS_ALREADY_REGISTERD);
    expect_int("duplicate CSE-ID is not stored", store_calls, 0);

    cJSON_Delete(request);
    cJSON_Delete(existing.obj);
    cJSON_Delete(parent.obj);
}

static void test_payload_csi_cannot_spoof_duplicate_check(void)
{
    ResourceTree tree;
    RTNode parent;
    RTNode existing = {0};
    init_parent(&tree, &parent);

    existing.ty = RT_CSR;
    existing.obj = cJSON_CreateObject();
    cJSON_AddStringToObject(existing.obj, "csi", "/id-mn");
    parent.child = &existing;

    cJSON *request = new_csr_request("other-cse", "/id-mn");
    oneM2MPrimitive o2pt = {0};
    o2pt.fr = "/id-other";
    o2pt.request_pc = request;

    expect_int("payload csi is replaced before duplicate validation",
               create_csr(&o2pt, &parent), RSC_CREATED);
    expect_string("stored csi follows From instead of payload",
                  json_string(created_node.obj, "csi"), "/id-other");

    cJSON_Delete(existing.obj);
    free_success_fixture(request, &parent);
}

static void test_missing_originator_is_rejected(void)
{
    ResourceTree tree;
    RTNode parent;
    init_parent(&tree, &parent);

    cJSON *request = new_csr_request("remote", NULL);
    oneM2MPrimitive o2pt = {0};
    o2pt.request_pc = request;

    expect_int("missing From is rejected without dereference",
               create_csr(&o2pt, &parent), RSC_BAD_REQUEST);
    expect_int("missing From is not stored", store_calls, 0);

    cJSON_Delete(request);
    cJSON_Delete(parent.obj);
}

int main(void)
{
    test_supplied_name_and_generated_id_are_preserved();
    test_missing_name_keeps_generated_name();
    test_duplicate_csi_is_rejected();
    test_payload_csi_cannot_spoof_duplicate_check();
    test_missing_originator_is_rejected();

    if (failures) {
        fprintf(stderr, "%d remoteCSE registration test(s) failed\n", failures);
        return 1;
    }

    printf("All remoteCSE registration tests passed.\n");
    return 0;
}
