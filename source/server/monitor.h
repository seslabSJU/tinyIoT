#pragma once
#include "onem2m.h"

typedef enum {
    MDC_SRC_MONITOR_TIMEOUT = 1,
    MDC_SRC_TSI_GAP        = 2
} mdc_source_t;

void notify_missing_data(RTNode *ts_node, int current_mdc, mdc_source_t src);
void *monitor_serve(void *arg);