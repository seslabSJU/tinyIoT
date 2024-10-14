#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#define MAX_RESOURCES 100

typedef struct {
    char *data;
} Resource;

void save_resource(const char *data);
const char* get_resource(int index);
int get_resource_count();

#endif // RESOURCE_MANAGER_H
