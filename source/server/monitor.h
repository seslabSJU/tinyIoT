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

// Missing-data detection state for one <timeSeries>, kept by the monitoring
// thread. `deadline_us` is on the CSE's wall clock (when a data point stops
// being late and counts as missing); `expected_dgt_us` is on the sending AE's
// clock (the dataGenerationTime that point would have carried). Every arriving
// <timeSeriesInstance> re-arms both; changing missingDataDetect disarms them.
void ts_md_arm(const char *ri, long long deadline_us, long long expected_dgt_us);
void ts_md_disarm(const char *ri);
void ts_md_clear_all(void);
int ts_md_is_armed(const char *ri);
