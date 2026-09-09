#pragma once
#include "onem2m.h"

typedef enum {
    MDC_SRC_MONITOR_TIMEOUT = 1,
    MDC_SRC_TSI_GAP        = 2
} mdc_source_t;

// `new_count` is how many missing data points this event added; they are the newest
// entries of the <timeSeries> mdlt attribute.
void notify_missing_data(RTNode *ts_node, int current_mdc, int new_count, mdc_source_t src);
void *monitor_serve(void *arg);