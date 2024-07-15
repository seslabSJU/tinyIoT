#include "resource_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Resource resources[MAX_RESOURCES];
static int resource_count = 0;

void save_resource(const char *data) {
    if (resource_count < MAX_RESOURCES) {
        resources[resource_count].data = strdup(data);
        resource_count++;
        printf("Resource saved: %s\n", data);
    } else {
        printf("Resource storage full!\n");
    }
}

const char* get_resource(int index) {
    if (index >= 0 && index < resource_count) {
        return resources[index].data;
    }
    return NULL;
}

int get_resource_count() {
    return resource_count;
}
