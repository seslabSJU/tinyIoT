#define _XOPEN_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "onem2m.h"
#include "logger.h"
#include "util.h"
#include "dbmanager.h"
#include "config.h"
#include "jsonparser.h"
#include "monitor.h"
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>

extern pthread_mutex_t main_lock;
extern ResourceTree *rt;
extern volatile int terminate;

typedef struct _ts_miss_throttle_entry {
    char ri[256];
    long long total_missing;
    struct _ts_miss_throttle_entry *next;
} ts_miss_throttle_entry_t;

static ts_miss_throttle_entry_t *g_ts_miss_throttle = NULL;
static void ts_forget_missing_total(const char *ri);
// notify_missing_data() now runs on both the monitoring thread and the request
// threads that create a <timeSeriesInstance>, so this list needs its own lock;
// two concurrent head-inserts otherwise lose an entry, and a lost running total
// makes `pending` go negative and silences that subscription for good.
static pthread_mutex_t g_ts_miss_throttle_lock = PTHREAD_MUTEX_INITIALIZER;

// Per-<timeSeries> missing-data cursor: armed by every arriving
// <timeSeriesInstance>, consumed by the monitoring thread.
//
// The two timestamps deliberately live on different clocks. `deadline_us` is the
// CSE's own wall clock - the instant at which a data point stops being merely
// late and counts as missing, i.e. pei + mdt after the sender last spoke.
// `expected_dgt_us` is on the sending AE's clock - the dataGenerationTime the
// missing instance would have carried, which is what TS-0018
// TP/oneM2M/CSE/TS/001 expects to find in missingDataList. Deriving both from one
// value would make an AE whose clock is offset from the CSE's either detected at
// the wrong moment or recorded with timestamps that are not its own.
typedef struct _ts_md_cursor {
    char ri[256];
    long long deadline_us;
    long long expected_dgt_us;
    struct _ts_md_cursor *next;
} ts_md_cursor_t;

static ts_md_cursor_t *g_ts_md_cursor = NULL;
static pthread_mutex_t g_ts_md_cursor_lock = PTHREAD_MUTEX_INITIALIZER;

void ts_md_arm(const char *ri, long long deadline_us, long long expected_dgt_us) {
    if (!ri) return;
    pthread_mutex_lock(&g_ts_md_cursor_lock);
    ts_md_cursor_t *e = g_ts_md_cursor;
    while (e && strcmp(e->ri, ri) != 0) e = e->next;
    if (!e) {
        e = (ts_md_cursor_t *)calloc(1, sizeof(ts_md_cursor_t));
        if (!e) { pthread_mutex_unlock(&g_ts_md_cursor_lock); return; }
        strncpy(e->ri, ri, sizeof(e->ri) - 1);
        e->next = g_ts_md_cursor;
        g_ts_md_cursor = e;
    }
    e->deadline_us = deadline_us;
    e->expected_dgt_us = expected_dgt_us;
    pthread_mutex_unlock(&g_ts_md_cursor_lock);
}

// Forget a <timeSeries>'s detection state. Used when missingDataDetect changes,
// so a stale deadline cannot fire the moment detection is switched back on.
void ts_md_disarm(const char *ri) {
    if (!ri) return;
    pthread_mutex_lock(&g_ts_md_cursor_lock);
    ts_md_cursor_t *prev = NULL, *e = g_ts_md_cursor;
    while (e && strcmp(e->ri, ri) != 0) { prev = e; e = e->next; }
    if (e) {
        if (prev) prev->next = e->next;
        else g_ts_md_cursor = e->next;
        free(e);
    }
    pthread_mutex_unlock(&g_ts_md_cursor_lock);
    ts_forget_missing_total(ri);
}

// Whether any instance has arrived for this <timeSeries> yet, i.e. whether there
// is already a grid to measure the next one against.
int ts_md_is_armed(const char *ri) {
    if (!ri) return 0;
    int found = 0;
    pthread_mutex_lock(&g_ts_md_cursor_lock);
    for (ts_md_cursor_t *e = g_ts_md_cursor; e; e = e->next) {
        if (strcmp(e->ri, ri) == 0) { found = 1; break; }
    }
    pthread_mutex_unlock(&g_ts_md_cursor_lock);
    return found;
}

// Drop all missing-data state. The upper tester's reset frees and rebuilds the
// whole resource tree, which would otherwise leave armed deadlines and running
// totals behind for resources that no longer exist.
void ts_md_clear_all(void) {
    pthread_mutex_lock(&g_ts_md_cursor_lock);
    while (g_ts_md_cursor) {
        ts_md_cursor_t *e = g_ts_md_cursor;
        g_ts_md_cursor = e->next;
        free(e);
    }
    pthread_mutex_unlock(&g_ts_md_cursor_lock);

    pthread_mutex_lock(&g_ts_miss_throttle_lock);
    while (g_ts_miss_throttle) {
        ts_miss_throttle_entry_t *e = g_ts_miss_throttle;
        g_ts_miss_throttle = e->next;
        free(e);
    }
    pthread_mutex_unlock(&g_ts_miss_throttle_lock);
}

// Whether any <timeSeries> is being watched at all. Nothing is armed until an
// instance arrives, so on a CSE with no active time series the monitoring tick
// can return immediately.
static int ts_md_any_armed(void) {
    pthread_mutex_lock(&g_ts_md_cursor_lock);
    int any = (g_ts_md_cursor != NULL);
    pthread_mutex_unlock(&g_ts_md_cursor_lock);
    return any;
}

// If a data point is overdue for `ri`, report the dataGenerationTime it should
// have carried and move the cursor on by one period, so the next point falls due
// a period later and a single call reports at most one missing point.
static int ts_md_take_due(const char *ri, long long now_us, long long pei_us,
                          long long *expected_dgt_us) {
    if (!ri || pei_us <= 0) return 0;
    int due = 0;
    pthread_mutex_lock(&g_ts_md_cursor_lock);
    ts_md_cursor_t *e = g_ts_md_cursor;
    while (e && strcmp(e->ri, ri) != 0) e = e->next;
    if (e && e->deadline_us > 0 && now_us >= e->deadline_us) {
        *expected_dgt_us = e->expected_dgt_us;
        e->deadline_us += pei_us;
        e->expected_dgt_us += pei_us;
        due = 1;
    }
    pthread_mutex_unlock(&g_ts_md_cursor_lock);
    return due;
}

// Running total of missing data points ever detected for a TS. mdlt only keeps the
// most recent `mdn` of them, so it cannot serve as the progress marker that decides
// which points a given subscription has already been told about.
static long long ts_add_missing_total(const char *ri, int n) {
    if (!ri || n <= 0) return 0;
    long long total = 0;
    pthread_mutex_lock(&g_ts_miss_throttle_lock);
    ts_miss_throttle_entry_t *e = g_ts_miss_throttle;
    while (e && strcmp(e->ri, ri) != 0) e = e->next;
    if (!e) {
        e = (ts_miss_throttle_entry_t *)calloc(1, sizeof(ts_miss_throttle_entry_t));
        if (e) {
            strncpy(e->ri, ri, sizeof(e->ri) - 1);
            e->next = g_ts_miss_throttle;
            g_ts_miss_throttle = e;
        }
    }
    if (e) { e->total_missing += n; total = e->total_missing; }
    pthread_mutex_unlock(&g_ts_miss_throttle_lock);
    return total;
}

// The running total as it currently stands, without changing it.
static long long ts_get_missing_total(const char *ri) {
    if (!ri) return 0;
    long long v = 0;
    pthread_mutex_lock(&g_ts_miss_throttle_lock);
    for (ts_miss_throttle_entry_t *e = g_ts_miss_throttle; e; e = e->next) {
        if (strcmp(e->ri, ri) == 0) { v = e->total_missing; break; }
    }
    pthread_mutex_unlock(&g_ts_miss_throttle_lock);
    return v;
}

// Forget a <timeSeries>'s running total, so a deleted resource does not leave
// its counter on the list for the lifetime of the process.
static void ts_forget_missing_total(const char *ri) {
    if (!ri) return;
    pthread_mutex_lock(&g_ts_miss_throttle_lock);
    ts_miss_throttle_entry_t *prev = NULL, *e = g_ts_miss_throttle;
    while (e && strcmp(e->ri, ri) != 0) { prev = e; e = e->next; }
    if (e) {
        if (prev) prev->next = e->next; else g_ts_miss_throttle = e->next;
        free(e);
    }
    pthread_mutex_unlock(&g_ts_miss_throttle_lock);
}

// Per-subscription missing-data notification state.
//
// TS-0001 clause 10.2.39 makes the missingData condition a window, not a
// cooldown: notify once `number` data points have gone missing within
// `duration`, and if the window runs out short of that number, send nothing and
// start counting again (TS-0018 TP/oneM2M/CSE/TS/003 and TP/004). This used to
// be implemented as a minimum delay between notifications, applied only to
// missing points found by the monitoring thread and skipped entirely for those
// found when an instance arrived - which made the two detection paths behave
// differently for the same subscription.
//
// `reported` is how many of the <timeSeries>'s missing points this subscription
// has already accounted for, whether notified or discarded with an expired
// window. It is counted against a TS-wide running total rather than against
// mdc/mdlt, which are resource attributes capped at mdn and would lose history.
typedef struct _sub_md_notify_throttle_entry {
    char ri[256];
    long long reported;
    long long window_start_us;      // 0 when no window is open
    struct _sub_md_notify_throttle_entry *next;
} sub_md_notify_throttle_entry_t;

static sub_md_notify_throttle_entry_t *g_sub_md_notify_throttle = NULL;
static pthread_mutex_t g_sub_md_notify_lock = PTHREAD_MUTEX_INITIALIZER;

// Caller must hold g_sub_md_notify_lock.
static sub_md_notify_throttle_entry_t *md_entry(const char *sub_ri, int create) {
    sub_md_notify_throttle_entry_t *e = g_sub_md_notify_throttle;
    while (e && strcmp(e->ri, sub_ri) != 0) e = e->next;
    if (!e && create) {
        e = (sub_md_notify_throttle_entry_t *)calloc(1, sizeof(sub_md_notify_throttle_entry_t));
        if (e) {
            strncpy(e->ri, sub_ri, sizeof(e->ri) - 1);
            e->next = g_sub_md_notify_throttle;
            g_sub_md_notify_throttle = e;
        }
    }
    return e;
}

// Account for `new_count` freshly detected missing data points against one
// subscription and decide whether that subscription should be notified now.
//
// Returns how many points to report (0 for none) and, when that is non-zero,
// writes the running total this subscription stood at beforehand to
// *out_reported_before, which is what locates the batch inside mdlt.
//
// The whole decision happens under one lock: both the monitoring thread and the
// request thread that creates a <timeSeriesInstance> can arrive here for the
// same subscription, and a read-then-update would let both send the same batch.
static int md_window_take(const char *sub_ri, long long ts_total, int new_count,
                          int num, long long dur_us, long long now_us,
                          long long *out_reported_before) {
    if (!sub_ri || num <= 0) return 0;
    int batch = 0;

    pthread_mutex_lock(&g_sub_md_notify_lock);
    sub_md_notify_throttle_entry_t *e = md_entry(sub_ri, 1);
    if (e) {
        if (e->window_start_us > 0 && dur_us > 0 &&
            (now_us - e->window_start_us) > dur_us) {
            // The window closed without reaching `num`. Those points are not
            // reported and are not carried into the next window.
            long long discarded = (ts_total - new_count) - e->reported;
            if (discarded > 0) {
                logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                       "Missing-data window expired: subRi=%s discarded=%lld (num=%d dur_us=%lld)",
                       sub_ri, discarded, num, dur_us);
                e->reported = ts_total - new_count;
            }
            e->window_start_us = 0;
        }

        if (e->window_start_us == 0) e->window_start_us = now_us;

        long long pending = ts_total - e->reported;
        if (pending >= num) {
            if (out_reported_before) *out_reported_before = e->reported;
            e->reported += num;
            e->window_start_us = 0;     // a fresh window opens on the next point
            batch = num;
        }
    }
    pthread_mutex_unlock(&g_sub_md_notify_lock);
    return batch;
}

// Points counted for this subscription but not yet notified.
static long long md_pending(const char *sub_ri, long long ts_total) {
    if (!sub_ri) return 0;
    long long pending = 0;
    pthread_mutex_lock(&g_sub_md_notify_lock);
    sub_md_notify_throttle_entry_t *e = md_entry(sub_ri, 0);
    pending = ts_total - (e ? e->reported : 0);
    pthread_mutex_unlock(&g_sub_md_notify_lock);
    return pending < 0 ? 0 : pending;
}

// Forget a subscription's state; called when the <subscription> goes away.
static void md_forget(const char *sub_ri) {
    if (!sub_ri) return;
    pthread_mutex_lock(&g_sub_md_notify_lock);
    sub_md_notify_throttle_entry_t *prev = NULL, *e = g_sub_md_notify_throttle;
    while (e && strcmp(e->ri, sub_ri) != 0) { prev = e; e = e->next; }
    if (e) {
        if (prev) prev->next = e->next; else g_sub_md_notify_throttle = e->next;
        free(e);
    }
    pthread_mutex_unlock(&g_sub_md_notify_lock);
}

static long long parse_md_dur_to_us(const char *dur) {
    if (!dur) return 0;
    // Expected: PT<value>S
    if (strncmp(dur, "PT", 2) != 0) return 0;
    const char *p = dur + 2;
    char *endp = NULL;
    double sec = strtod(p, &endp);
    if (endp == p) return 0;
    // Accept trailing 'S' (case sensitive as in examples)
    if (*endp != 'S') return 0;
    if (sec <= 0.0) return 0;
    return (long long)(sec * 1000000.0);
}



// Forward declarations (C99: prevent implicit function declaration errors)
long long parse_time_monitor(char *s);
long long parse_time_monitor_us(char *s);
void us_to_iso8601_monitor(long long us, char *buf);

// Use wall-clock time directly (avoid parsing formatted timestamps).
static long long wallclock_now_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000LL + (long long)tv.tv_usec;
}

long long parse_time_monitor(char *s) {
    if (!s || strlen(s) < 15) return 0;
    struct tm t = {0};
    int us = 0;
    sscanf(s, "%4d%2d%2dT%2d%2d%2d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec);
    t.tm_year -= 1900;
    t.tm_mon -= 1;
    char *comma = strchr(s, ',');
    if (comma) { sscanf(comma + 1, "%d", &us); while (us > 999) us /= 10; }
    return ((long long)timegm(&t) * 1000) + us;
}

long long parse_time_monitor_us(char *s) {
    if (!s || strlen(s) < 15) return 0;
    struct tm t = {0};
    if (sscanf(s, "%4d%2d%2dT%2d%2d%2d", &t.tm_year, &t.tm_mon, &t.tm_mday,
               &t.tm_hour, &t.tm_min, &t.tm_sec) < 6) return 0;
    t.tm_year -= 1900;
    t.tm_mon -= 1;
    t.tm_isdst = -1;

    time_t epoch_sec = timegm(&t);
    if (epoch_sec == -1) return 0;

    long long t_us = (long long)epoch_sec * 1000000LL;
    char *comma = strchr(s, ',');
    if (comma) {
        char us_str[7] = "000000";
        for (int i = 0; i < 6 && comma[1 + i] >= '0' && comma[1 + i] <= '9'; i++) {
            us_str[i] = comma[1 + i];
        }
        t_us += atoll(us_str);
    }
    return t_us;
}

void us_to_iso8601_monitor(long long us, char *buf) {
    time_t sec = us / 1000000;
    int micro = us % 1000000;
    struct tm *t = gmtime(&sec); // UTC 기준 변환
    sprintf(buf, "%04d%02d%02dT%02d%02d%02d,%06d",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, micro);
}

// Build and send one missingData notification (m2m:tsn) for `sub_child`,
// carrying entries [batch_start, batch_start + batch_len) of the parent
// <timeSeries>'s mdlt. A notification reports only the points that went missing
// since the previous one for this subscription, not the whole mdlt attribute.
static void md_send_notification(RTNode *sub_child, cJSON *ts_mdlt,
                                 int batch_start, int batch_len) {
    if (!sub_child || !ts_mdlt || batch_len <= 0) return;

    cJSON *noti_cjson = cJSON_CreateObject();
    cJSON *sgn = cJSON_CreateObject();
    cJSON_AddItemToObject(noti_cjson, "m2m:sgn", sgn);

    char *sur = make_subscription_reference(get_ri_rtnode(sub_child));
    if (sur) {
        cJSON_AddStringToObject(sgn, "sur", sur);
        free(sur);
    }

    cJSON *nev = cJSON_CreateObject();
    cJSON_AddItemToObject(sgn, "nev", nev);
    cJSON_AddNumberToObject(nev, "net", NET_REPORT_ON_MISSING_DATA_POINTS);
    cJSON *rep = cJSON_CreateObject();
    cJSON_AddItemToObject(nev, "rep", rep);
    cJSON *tsn = cJSON_CreateObject();
    cJSON_AddItemToObject(rep, "m2m:tsn", tsn);

    cJSON *mdltBatch = cJSON_CreateArray();
    for (int bi = 0; bi < batch_len; bi++) {
        cJSON *item = cJSON_GetArrayItem(ts_mdlt, batch_start + bi);
        if (item && cJSON_IsString(item) && item->valuestring) {
            cJSON_AddItemToArray(mdltBatch, cJSON_CreateString(item->valuestring));
        }
    }
    cJSON_AddNumberToObject(tsn, "mdc", cJSON_GetArraySize(mdltBatch));
    cJSON_AddItemToObject(tsn, "mdlt", mdltBatch);

    int rsc = notify_to_nu(sub_child, noti_cjson, NET_REPORT_ON_MISSING_DATA_POINTS);
    logger("TSI_TRACE", LOG_LEVEL_DEBUG,
           "Missing-data notification result: subRi=%s batch=%d rsc=%d",
           get_ri_rtnode(sub_child), batch_len, rsc);

    cJSON_Delete(noti_cjson);
}

void notify_missing_data(RTNode *ts_node, int current_mdc, int new_count, mdc_source_t src) {
    if (!ts_node || !ts_node->child) return;

#if 1
    if (src != MDC_SRC_MONITOR_TIMEOUT && src != MDC_SRC_TSI_GAP) {
        logger("TSI_TRACE", LOG_LEVEL_DEBUG,
               "Missing-data notification skipped (src=%d) for TS [%s] mdc=%d",
               (int)src, get_ri_rtnode(ts_node), current_mdc);
        return;
    }

    // Advance the TS-wide running total once per event, before visiting the subscriptions.
    const char *ts_ri_for_total = get_ri_rtnode(ts_node);
    long long ts_total = ts_add_missing_total(ts_ri_for_total, new_count > 0 ? new_count : 1);

    RTNode *child = ts_node->child;
    while (child) {
        if (child->ty == RT_SUB) {
            cJSON *obj = child->obj;

            cJSON *wrapper = cJSON_GetObjectItem(obj, "m2m:sub");
            cJSON *subObj = wrapper ? wrapper : obj;

            cJSON *nu = cJSON_GetObjectItem(subObj, "nu");

            int net_has_8 = 0;
            cJSON *enc = cJSON_GetObjectItem(subObj, "enc");
            if (enc) {
                cJSON *net = cJSON_GetObjectItem(enc, "net");
                if (net && cJSON_IsArray(net)) {
                    int n = cJSON_GetArraySize(net);
                    for (int i = 0; i < n; i++) {
                        cJSON *v = cJSON_GetArrayItem(net, i);
                        if (v && cJSON_IsNumber(v) && (int)v->valuedouble == 8) {
                            net_has_8 = 1;
                            break;
                        }
                    }
                }
            }

            int md_num = 0;
            if (enc) {
                cJSON *md = cJSON_GetObjectItem(enc, "md");
                if (md) {
                    cJSON *num = cJSON_GetObjectItem(md, "num");
                    if (num && cJSON_IsNumber(num)) {
                        md_num = (int)num->valuedouble;
                    }
                }
            }

            if (!net_has_8) {
                child = child->sibling_right;
                continue;
            }

            // Determine notification threshold.
            // Per oneM2M missing-data notification control, use SUB.enc.md.num.
            // If not configured, do not emit missing-data notifications.
            int threshold = 0;
            if (md_num > 0) {
                threshold = md_num;
            }
            if (threshold <= 0) {
                // Not configured -> no missing-data notifications.
                child = child->sibling_right;
                continue;
            }

            // Read TS.mdlt length (missing timestamps)
            int mdlt_len = 0;
            cJSON *ts_mdlt = NULL;
            if (ts_node && ts_node->obj) {
                ts_mdlt = cJSON_GetObjectItem(ts_node->obj, "mdlt");
                if (ts_mdlt && cJSON_IsArray(ts_mdlt)) {
                    mdlt_len = cJSON_GetArraySize(ts_mdlt);
                }
            }


            // md.dur is the window the md.num points have to fall inside.
            long long dur_us = 0;
            if (enc) {
                cJSON *md = cJSON_GetObjectItem(enc, "md");
                cJSON *dur = md ? cJSON_GetObjectItem(md, "dur") : NULL;
                if (dur && cJSON_IsString(dur) && dur->valuestring) {
                    dur_us = parse_md_dur_to_us(dur->valuestring);
                }
            }

            const char *sub_ri_for_progress = get_ri_rtnode(child);
            const char *sub_ri_s = sub_ri_for_progress;
            long long reported_before = 0;
            int batch_len = md_window_take(sub_ri_for_progress, ts_total, new_count > 0 ? new_count : 1,
                                           threshold, dur_us, wallclock_now_us(), &reported_before);
            if (batch_len <= 0) {
                child = child->sibling_right;
                continue;
            }

            // The points to report are the oldest still-pending entries of mdlt.
            int batch_start = mdlt_len - (int)(ts_total - reported_before);
            if (batch_start < 0) batch_start = 0;          // older entries already dropped by mdn
            if (batch_start + batch_len > mdlt_len) batch_len = mdlt_len - batch_start;
            if (batch_len <= 0) {
                child = child->sibling_right;
                continue;
            }

            if (nu && cJSON_IsArray(nu)) {
                md_send_notification(child, ts_mdlt, batch_start, batch_len);
                logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                       "Missing-data notify sent: subRi=%s batch=%d of ts_total=%lld",
                       sub_ri_for_progress, batch_len, ts_total);
            }
        }
        child = child->sibling_right;
    }
#endif
}


// Traverse the in-memory resource tree and apply missing-data logic for TS resources.
// TS-0018 TP/oneM2M/CSE/TS/005: deleting a missing-data <subscription> flushes
// the points it has collected but not yet reported, so a subscriber that stops
// listening still learns about the gap it was counting towards. The notification
// carries however many are outstanding, which is by definition fewer than
// enc/md/num - had it reached num, it would already have been sent.
//
// This is separate from the subscription-deletion notification sent to
// subscriberURI: that one reports that the <subscription> went away, this one
// reports missing data.
void ts_md_flush_sub(RTNode *sub_rtnode) {
    if (!sub_rtnode || sub_rtnode->ty != RT_SUB || !sub_rtnode->obj) return;

    const char *sub_ri = get_ri_rtnode(sub_rtnode);
    RTNode *ts_node = sub_rtnode->parent;
    if (!sub_ri) return;

    if (!ts_node || ts_node->ty != RT_TS || !ts_node->obj) {
        md_forget(sub_ri);
        return;
    }

    // Only subscriptions that asked for missing-data notifications.
    cJSON *enc = cJSON_GetObjectItem(sub_rtnode->obj, "enc");
    cJSON *net = enc ? cJSON_GetObjectItem(enc, "net") : NULL;
    int net_has_8 = 0;
    if (net && cJSON_IsArray(net)) {
        cJSON *v = NULL;
        cJSON_ArrayForEach(v, net) {
            if (cJSON_IsNumber(v) && (int)cJSON_GetNumberValue(v) == NET_REPORT_ON_MISSING_DATA_POINTS) {
                net_has_8 = 1;
                break;
            }
        }
    }
    cJSON *nu = cJSON_GetObjectItem(sub_rtnode->obj, "nu");
    if (!net_has_8 || !nu || !cJSON_IsArray(nu)) {
        md_forget(sub_ri);
        return;
    }

    long long ts_total = ts_get_missing_total(get_ri_rtnode(ts_node));
    long long pending = md_pending(sub_ri, ts_total);

    cJSON *ts_mdlt = cJSON_GetObjectItem(ts_node->obj, "mdlt");
    int mdlt_len = (ts_mdlt && cJSON_IsArray(ts_mdlt)) ? cJSON_GetArraySize(ts_mdlt) : 0;

    if (pending > 0 && mdlt_len > 0) {
        int batch_len = (pending > mdlt_len) ? mdlt_len : (int)pending;
        int batch_start = mdlt_len - batch_len;
        logger("TSI_TRACE", LOG_LEVEL_DEBUG,
               "Missing-data final notify on <sub> delete: subRi=%s pending=%lld batch=%d",
               sub_ri, pending, batch_len);
        md_send_notification(sub_rtnode, ts_mdlt, batch_start, batch_len);
    }

    md_forget(sub_ri);
}

static void traverse_and_check_ts_missing(RTNode *node, long long now_us) {
    if (!node) return;

    // Preorder traversal: check current node
    if (node->ty == RT_TS && node->obj) {
        cJSON *mddObj = cJSON_GetObjectItem(node->obj, "mdd");
        int mdd = 0;
        if (mddObj && (cJSON_IsBool(mddObj) || cJSON_IsNumber(mddObj))) {
            mdd = cJSON_IsBool(mddObj) ? cJSON_IsTrue(mddObj) : ((int)mddObj->valuedouble != 0);
        }

        if (mdd) {
            cJSON *peiObj = cJSON_GetObjectItem(node->obj, "pei");
            cJSON *mdcObj = cJSON_GetObjectItem(node->obj, "mdc");

            long long pei_us = 0;
            if (peiObj && cJSON_IsNumber(peiObj)) {
                // pei is treated as milliseconds in this codebase; convert to microseconds.
                pei_us = (long long)peiObj->valuedouble * 1000LL;
            }

            const char *ri = get_ri_rtnode(node);
            long long expected_dgt_us = 0;

            // The cursor is armed by the last instance that arrived, so an
            // untouched <timeSeries> - one that has never received an instance -
            // reports nothing rather than counting missing points from creation.
            if (ri && pei_us > 0 && ts_md_take_due(ri, now_us, pei_us, &expected_dgt_us)) {
                pthread_mutex_lock(&main_lock);

                // TP/oneM2M/CSE/TS/001 asks for the dataGenerationTime of the
                // missing data point, so record the timestamp the instance would
                // have carried, not the moment its absence was noticed. lt is
                // left alone: it is the resource's lastModifiedTime and nothing
                // about the resource has been modified by the data not arriving.
                char md_time[64];
                us_to_iso8601_monitor(expected_dgt_us, md_time);

                // mdlt is absent until the first missing point, and
                // cJSON_ReplaceItemInObject() is a no-op on a key that does not
                // exist yet - so the array has to be added, not replaced, or
                // every entry is silently dropped and mdc stays at zero.
                cJSON *mdlt = cJSON_GetObjectItem(node->obj, "mdlt");
                if (!mdlt) {
                    mdlt = cJSON_AddArrayToObject(node->obj, "mdlt");
                } else if (!cJSON_IsArray(mdlt)) {
                    cJSON_ReplaceItemInObject(node->obj, "mdlt", cJSON_CreateArray());
                    mdlt = cJSON_GetObjectItem(node->obj, "mdlt");
                }
                cJSON_AddItemToArray(mdlt, cJSON_CreateString(md_time));

                // mdlt keeps at most mdn entries, oldest first out, and mdc
                // reports its current size - same rule as the TSI-gap path.
                cJSON *mdn_obj = cJSON_GetObjectItem(node->obj, "mdn");
                int mdn = (mdn_obj && cJSON_IsNumber(mdn_obj)) ? (int)cJSON_GetNumberValue(mdn_obj) : 0;
                if (mdn > 0) {
                    while (cJSON_GetArraySize(mdlt) > mdn) {
                        cJSON_DeleteItemFromArray(mdlt, 0);
                    }
                }

                int after_mdc = cJSON_GetArraySize(mdlt);
                if (mdcObj && cJSON_IsNumber(mdcObj)) {
                    cJSON_SetNumberValue(mdcObj, after_mdc);
                } else if (mdcObj) {
                    cJSON_ReplaceItemInObject(node->obj, "mdc", cJSON_CreateNumber(after_mdc));
                } else {
                    // Replace is a no-op on an absent key, which would leave a
                    // grown mdlt with no mdc to describe it.
                    cJSON_AddNumberToObject(node->obj, "mdc", after_mdc);
                }

                logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                       "Missing data point for TS [%s] detected on timeout: expected dgt=%s, mdc=%d",
                       ri, md_time, after_mdc);

                // Persist via DB manager (backend-agnostic)
                db_update_resource(node->obj, (char *)ri, RT_TS);

                pthread_mutex_unlock(&main_lock);
                notify_missing_data(node, after_mdc, 1, MDC_SRC_MONITOR_TIMEOUT);
            }
        }
    }

    // Traverse children first, then siblings
    if (node->child) traverse_and_check_ts_missing(node->child, now_us);
    if (node->sibling_right) traverse_and_check_ts_missing(node->sibling_right, now_us);
}

void *monitor_serve(void *arg) {
    (void)arg;

    logger("MONITOR", LOG_LEVEL_INFO, "TS Monitoring Thread Started (in-memory traversal)");

    while (!terminate) {
        // Nothing is armed until a <timeSeriesInstance> arrives, so a CSE with
        // no active time series does no work here and, importantly, writes no
        // log lines: this runs twice a second, and looking the root up through
        // find_rtnode() on every tick used to bury the log in two DEBUG lines
        // per tick whether or not there was anything to check.
        if (!ts_md_any_armed()) {
            usleep(500000);
            continue;
        }

        long long now_us = wallclock_now_us();

        // The whole walk runs under main_lock. Request threads add and free
        // RTNodes and their cJSON objects under that same lock, so walking the
        // tree unlocked would hand this thread pointers a concurrent DELETE can
        // free underneath it. main_lock is recursive, so the nested acquisition
        // further down is fine. rt->cb is read directly rather than looked up:
        // under the lock it is the same node, without the per-tick logging.
        pthread_mutex_lock(&main_lock);
        if (rt && rt->cb) {
            traverse_and_check_ts_missing(rt->cb, now_us);
        }
        pthread_mutex_unlock(&main_lock);

        usleep(500000);
    }

    return NULL;
}