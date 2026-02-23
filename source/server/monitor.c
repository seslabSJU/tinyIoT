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
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

extern pthread_mutex_t main_lock;
extern int terminate;

// Throttle missing-data generation per TS to avoid generating multiple missing periods
// in a short time window (e.g., when lt is far behind). This aligns behavior with
// unit tests that expect at most one missing period to be generated per pei interval.
typedef struct _ts_miss_throttle_entry {
    char ri[256];
    long long last_gen_us;
    struct _ts_miss_throttle_entry *next;
} ts_miss_throttle_entry_t;

static ts_miss_throttle_entry_t *g_ts_miss_throttle = NULL;

static long long get_last_gen_us_for_ri(const char *ri) {
    ts_miss_throttle_entry_t *e = g_ts_miss_throttle;
    while (e) {
        if (strcmp(e->ri, ri) == 0) return e->last_gen_us;
        e = e->next;
    }
    return 0;
}

static void set_last_gen_us_for_ri(const char *ri, long long v) {
    ts_miss_throttle_entry_t *e = g_ts_miss_throttle;
    while (e) {
        if (strcmp(e->ri, ri) == 0) {
            e->last_gen_us = v;
            return;
        }
        e = e->next;
    }
    // not found -> create
    e = (ts_miss_throttle_entry_t *)calloc(1, sizeof(ts_miss_throttle_entry_t));
    if (!e) return;
    strncpy(e->ri, ri, sizeof(e->ri) - 1);
    e->last_gen_us = v;
    e->next = g_ts_miss_throttle;
    g_ts_miss_throttle = e;
}

static int should_throttle_missing(const char *ri, long long now_us, long long pei_us) {
    if (!ri || pei_us <= 0) return 0;
    long long last = get_last_gen_us_for_ri(ri);
    // If we generated missing data recently (< pei), skip this cycle.
    if (last > 0 && (now_us - last) < pei_us) return 1;
    return 0;
}

// Throttle missing-data NOTIFICATION per SUB to respect SUB.enc.md.dur as a cooldown between notifications.
// Tracks pending in-flight notification to prevent duplicate sends.
typedef struct _sub_md_notify_throttle_entry {
    char ri[256];
    long long last_notify_ts_us;     // TS-time when we last successfully sent a missing-data notification for this SUB
    long long pending_reserve_us;    // TS-time reserved for an in-flight notification (prevents duplicate sends)
    int pending;                     // 1 while a notification is in-flight for this SUB
    struct _sub_md_notify_throttle_entry *next;
} sub_md_notify_throttle_entry_t;


static sub_md_notify_throttle_entry_t *g_sub_md_notify_throttle = NULL;
// Protect g_sub_md_notify_throttle against concurrent access from monitor thread and request threads.
static pthread_mutex_t g_sub_md_notify_lock = PTHREAD_MUTEX_INITIALIZER;

// Begin missing-data notification cooldown enforcement for a SUB.
// Returns 1 if the notification should be SUPPRESSED.
// Returns 0 if allowed, and marks the SUB as "pending" to prevent duplicate sends from concurrent callers.
static int md_notify_try_begin(const char *sub_ri, long long event_us, long long dur_us) {
    if (!sub_ri || dur_us <= 0) return 0;
    logger("TSI_TRACE", LOG_LEVEL_DEBUG, "md_notify_try_begin: subRi=%s event_us=%lld dur_us=%lld", sub_ri, event_us, dur_us);

    pthread_mutex_lock(&g_sub_md_notify_lock);
    sub_md_notify_throttle_entry_t *e = g_sub_md_notify_throttle;
    while (e) {
        if (strcmp(e->ri, sub_ri) == 0) {
            logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                   "md_notify_try_begin: found entry subRi=%s last_notify_ts_us=%lld pending=%d pending_reserve_us=%lld",
                   sub_ri, e->last_notify_ts_us, e->pending, e->pending_reserve_us);
            // If another thread is already sending a missing-data notification for this SUB,
            // then wait briefly for it to finish. This prevents a race where the caller that
            // hits the threshold gets suppressed before the in-flight notification is actually
            // delivered (unit tests then observe no notification).
            if (e->pending) {
                logger("TSI_TRACE", LOG_LEVEL_DEBUG, "md_notify_try_begin: subRi=%s is pending -> waiting", sub_ri);
                pthread_mutex_unlock(&g_sub_md_notify_lock);

                // Wait up to ~1s in small steps for the in-flight send to complete.
                // After it completes, re-run the checks (including md.dur) in a fresh attempt.
                for (int k = 0; k < 20; ++k) {
                    usleep(50000); // 50ms
                    pthread_mutex_lock(&g_sub_md_notify_lock);
                    sub_md_notify_throttle_entry_t *e2 = g_sub_md_notify_throttle;
                    while (e2) {
                        if (strcmp(e2->ri, sub_ri) == 0) break;
                        e2 = e2->next;
                    }
                    if (!e2) {
                        // Entry disappeared; fail open.
                        pthread_mutex_unlock(&g_sub_md_notify_lock);
                        return 0;
                    }
                    if (!e2->pending) {
                        // In-flight finished; fall through by restarting the whole function.
                        pthread_mutex_unlock(&g_sub_md_notify_lock);
                        logger("TSI_TRACE", LOG_LEVEL_DEBUG, "md_notify_try_begin: subRi=%s pending cleared -> retry", sub_ri);
                        return md_notify_try_begin(sub_ri, event_us, dur_us);
                    }
                    pthread_mutex_unlock(&g_sub_md_notify_lock);
                }

                // Still pending after waiting: suppress to avoid duplicates.
                logger("TSI_TRACE", LOG_LEVEL_DEBUG, "md_notify_try_begin: subRi=%s still pending after wait -> suppress", sub_ri);
                return 1;
            }
            // Implement md.dur as a COOLDOWN between notifications.
            // Allow the first notification immediately when threshold is reached.
            if (e->last_notify_ts_us > 0) {
                long long elapsed = event_us - e->last_notify_ts_us;
                if (elapsed < 0) elapsed = 0;
                logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                       "md_notify_try_begin: subRi=%s cooldown elapsed_us=%lld (event_us=%lld - last=%lld)",
                       sub_ri, elapsed, event_us, e->last_notify_ts_us);
                if (elapsed < dur_us) {
                    logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                           "md_notify_try_begin: subRi=%s suppressed by md.dur cooldown (elapsed_us=%lld < dur_us=%lld)",
                           sub_ri, elapsed, dur_us);
                    pthread_mutex_unlock(&g_sub_md_notify_lock);
                    return 1;
                }
            } else {
                logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                       "md_notify_try_begin: subRi=%s no last_notify_ts_us -> allow first notify", sub_ri);
            }
            // Allow and mark pending.
            e->pending = 1;
            e->pending_reserve_us = event_us;
            logger("TSI_TRACE", LOG_LEVEL_DEBUG, "md_notify_try_begin: subRi=%s ALLOW -> set pending=1 reserve=%lld", sub_ri, event_us);
            pthread_mutex_unlock(&g_sub_md_notify_lock);
            return 0;
        }
        e = e->next;
    }

    // Not found -> create and mark pending.
    e = (sub_md_notify_throttle_entry_t *)calloc(1, sizeof(sub_md_notify_throttle_entry_t));
    if (!e) {
        pthread_mutex_unlock(&g_sub_md_notify_lock);
        return 0; // fail open
    }
    strncpy(e->ri, sub_ri, sizeof(e->ri) - 1);
    e->last_notify_ts_us = 0;
    e->pending = 1;
    e->pending_reserve_us = event_us;
    e->next = g_sub_md_notify_throttle;
    g_sub_md_notify_throttle = e;
    logger("TSI_TRACE", LOG_LEVEL_DEBUG, "md_notify_try_begin: new entry subRi=%s last_notify_ts_us=0 ALLOW -> set pending=1 reserve=%lld", sub_ri, event_us);
    pthread_mutex_unlock(&g_sub_md_notify_lock);
    return 0;
}

// Finish a missing-data notification attempt.
// Always clears the pending flag. If sent_ok is non-zero, updates last_notify_ts_us.
static void md_notify_finish(const char *sub_ri, int sent_ok) {
    if (!sub_ri) return;
    logger("TSI_TRACE", LOG_LEVEL_DEBUG, "md_notify_finish: subRi=%s sent_ok=%d", sub_ri, sent_ok);

    pthread_mutex_lock(&g_sub_md_notify_lock);
    sub_md_notify_throttle_entry_t *e = g_sub_md_notify_throttle;
    while (e) {
        if (strcmp(e->ri, sub_ri) == 0) {
            logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                   "md_notify_finish: before update subRi=%s last_notify_ts_us=%lld pending=%d pending_reserve_us=%lld",
                   sub_ri, e->last_notify_ts_us, e->pending, e->pending_reserve_us);
            if (sent_ok) {
                // Commit the reserved TS-time as the last successful notify timestamp.
                e->last_notify_ts_us = e->pending_reserve_us;
                logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                       "md_notify_finish: sent_ok=1 -> commit last_notify_ts_us=%lld for subRi=%s",
                       e->last_notify_ts_us, sub_ri);
            } else {
                logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                       "md_notify_finish: sent_ok=0 -> keep last_notify_ts_us=%lld for subRi=%s",
                       e->last_notify_ts_us, sub_ri);
            }

            e->pending_reserve_us = 0;
            e->pending = 0;

            logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                   "md_notify_finish: after update subRi=%s last_notify_ts_us=%lld pending=%d pending_reserve_us=%lld",
                   sub_ri, e->last_notify_ts_us, e->pending, e->pending_reserve_us);
            pthread_mutex_unlock(&g_sub_md_notify_lock);
            return;
        }
        e = e->next;
    }
    pthread_mutex_unlock(&g_sub_md_notify_lock);
}

// Parse oneM2M duration string of the form "PT<seconds>S" into microseconds.
// Supports fractional seconds (e.g., "PT10.0S"). Returns 0 on parse failure.
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

// Determine a TS-based timestamp (microseconds) for missing-data notification cooldown.
// Uses the last entry in TS.mdlt (format: YYYYMMDDTHHMMSS or YYYYMMDDTHHMMSS,ffffff).
// Falls back to wall-clock time if mdlt is missing/empty or cannot be parsed.
static long long current_md_event_us(cJSON *ts_mdlt) {
    if (ts_mdlt && cJSON_IsArray(ts_mdlt)) {
        int n = cJSON_GetArraySize(ts_mdlt);
        if (n > 0) {
            cJSON *last = cJSON_GetArrayItem(ts_mdlt, n - 1);
            if (last && cJSON_IsString(last) && last->valuestring) {
                logger("TSI_TRACE", LOG_LEVEL_DEBUG, "current_md_event_us: using mdlt last='%s'", last->valuestring);
                long long t = parse_time_monitor_us((char *)last->valuestring);
                if (t > 0) return t;
            }
        }
    }
    logger("TSI_TRACE", LOG_LEVEL_DEBUG, "current_md_event_us: mdlt missing/empty -> fallback to wallclock");
    return wallclock_now_us();
}

// --- Simple HTTP notifier (non-VRQ) -------------------------------------------------
// Sends a plain HTTP/1.1 POST with JSON body to the given URL (http://host:port[/path]).
// This is used for SUB notifications. It intentionally does NOT mark the payload as VRQ.
// Returns 2000 on 2xx response, else 4000.
static int http_post_json_simple(const char *url, const char *json_body) {
    if (!url || !json_body) return 4000;
    logger("HTTPNOTI", LOG_LEVEL_DEBUG, "http_post_json_simple: url=%s body_len=%zu", url, strlen(json_body));

    // Only support http:// URLs for unit tests.
    const char *p = NULL;
    if (strncmp(url, "http://", 7) == 0) {
        p = url + 7;
    } else {
        logger("HTTPNOTI", LOG_LEVEL_DEBUG, "http_post_json_simple: unsupported url scheme: %s", url);
        return 4000;
    }

    // Extract host[:port][/path]
    char host[256] = {0};
    char port[16] = {0};
    char path[512] = {0};

    const char *slash = strchr(p, '/');
    size_t hostport_len = slash ? (size_t)(slash - p) : strlen(p);
    if (hostport_len == 0 || hostport_len >= sizeof(host)) {
        logger("HTTPNOTI", LOG_LEVEL_DEBUG, "http_post_json_simple: invalid hostport in url: %s", url);
        return 4000;
    }

    char hostport[256] = {0};
    memcpy(hostport, p, hostport_len);
    hostport[hostport_len] = '\0';

    if (slash) {
        snprintf(path, sizeof(path), "%s", slash);
    } else {
        strcpy(path, "/");
    }

    // Split host and port
    const char *colon = strrchr(hostport, ':');
    if (colon) {
        size_t hlen = (size_t)(colon - hostport);
        if (hlen == 0 || hlen >= sizeof(host)) {
            logger("HTTPNOTI", LOG_LEVEL_DEBUG, "http_post_json_simple: invalid host in url: %s", url);
            return 4000;
        }
        memcpy(host, hostport, hlen);
        host[hlen] = '\0';
        snprintf(port, sizeof(port), "%s", colon + 1);
        if (strlen(port) == 0) {
            logger("HTTPNOTI", LOG_LEVEL_DEBUG, "http_post_json_simple: missing port in url: %s", url);
            return 4000;
        }
    } else {
        snprintf(host, sizeof(host), "%s", hostport);
        snprintf(port, sizeof(port), "%d", 80);
    }

    // Resolve & connect
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    if (getaddrinfo(host, port, &hints, &res) != 0 || !res) {
        logger("HTTPNOTI", LOG_LEVEL_DEBUG, "http_post_json_simple: getaddrinfo failed host=%s port=%s", host, port);
        return 4000;
    }

    int sock = -1;
    struct addrinfo *rp;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sock = (int)socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) continue;
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sock);
        sock = -1;
    }
    freeaddrinfo(res);
    if (sock < 0) {
        logger("HTTPNOTI", LOG_LEVEL_DEBUG, "http_post_json_simple: connect failed host=%s port=%s path=%s", host, port, path);
        return 4000;
    }

    // Prevent blocking forever on recv
    struct timeval rcv_to;
    rcv_to.tv_sec = 2;
    rcv_to.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&rcv_to, sizeof(rcv_to));

    // Build request
    const size_t body_len = strlen(json_body);
    char header[1024];
    long long ri = wallclock_now_us();
    int header_len = snprintf(header, sizeof(header),
                              "POST %s HTTP/1.1\r\n"
                              "Host: %s:%s\r\n"
                              "User-Agent: tinyIoT-monitor\r\n"
                              "Accept: application/json\r\n"
                              "X-M2M-RI: %lld\r\n"
                              "X-M2M-Origin: CAdmin\r\n"
                              "X-M2M-RVI: 2a\r\n"
                              "Connection: close\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: %zu\r\n"
                              "\r\n",
                              path, host, port, ri, body_len);
    if (header_len <= 0 || (size_t)header_len >= sizeof(header)) {
        logger("HTTPNOTI", LOG_LEVEL_DEBUG, "http_post_json_simple: header build failed");
        close(sock);
        return 4000;
    }

    // Send header + body
    ssize_t sent = send(sock, header, (size_t)header_len, 0);
    if (sent != (ssize_t)header_len) {
        logger("HTTPNOTI", LOG_LEVEL_DEBUG, "http_post_json_simple: send header failed");
        close(sock);
        return 4000;
    }
    sent = send(sock, json_body, body_len, 0);
    if (sent != (ssize_t)body_len) {
        logger("HTTPNOTI", LOG_LEVEL_DEBUG, "http_post_json_simple: send body failed");
        close(sock);
        return 4000;
    }

    // Read response header (at least the status line). Some servers may split the response.
    char resp[1024];
    size_t used = 0;
    int got_line = 0;
    while (used < sizeof(resp) - 1) {
        ssize_t n = recv(sock, resp + used, (int)(sizeof(resp) - 1 - used), 0);
        if (n <= 0) break;
        used += (size_t)n;
        resp[used] = '\0';
        if (strstr(resp, "\r\n") || strchr(resp, '\n')) {
            got_line = 1;
            break;
        }
    }
    close(sock);
    if (!got_line || used == 0) {
        logger("HTTPNOTI", LOG_LEVEL_DEBUG, "http_post_json_simple: no response (timeout or empty)");
        return 4000;
    }

    // Extract the first line
    char *line_end = strstr(resp, "\r\n");
    if (!line_end) line_end = strchr(resp, '\n');
    if (line_end) *line_end = '\0';
    logger("HTTPNOTI", LOG_LEVEL_DEBUG, "http_post_json_simple: status_line=%s", resp);

    // Expect: HTTP/1.1 200 ... (be tolerant about formatting)
    int status = 0;
    if (sscanf(resp, "HTTP/%*[^ ] %d", &status) == 1) {
        if (status >= 200 && status < 300) {
            logger("HTTPNOTI", LOG_LEVEL_DEBUG, "http_post_json_simple: success status=%d", status);
            return 2000;
        }
    }
    logger("HTTPNOTI", LOG_LEVEL_DEBUG, "http_post_json_simple: non-2xx or parse failed");
    return 4000;
}
// ------------------------------------------------------------------------------------


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

// Update TS.mdc and append a timestamp to TS.mdlt via the DB manager API.
// This avoids embedding PostgreSQL-specific SQL in resource/monitor logic.
void ts_mdc_update_db_with_mdlt(const char *ts_ri, int val, const char *time_str) {
    if (!ts_ri || !time_str) return;

    // Fetch TS object from DB (independent of the backend)
    cJSON *ts = db_get_resource((char *)ts_ri, RT_TS);
    if (!ts) return;

    // Update mdc
    cJSON *mdcObj = cJSON_GetObjectItem(ts, "mdc");
    if (mdcObj && cJSON_IsNumber(mdcObj)) {
        cJSON_SetNumberValue(mdcObj, val);
    } else {
        cJSON_ReplaceItemInObject(ts, "mdc", cJSON_CreateNumber(val));
    }

    // Ensure mdlt is an array and append time_str
    cJSON *mdltObj = cJSON_GetObjectItem(ts, "mdlt");
    if (!mdltObj || !cJSON_IsArray(mdltObj)) {
        cJSON_ReplaceItemInObject(ts, "mdlt", cJSON_CreateArray());
        mdltObj = cJSON_GetObjectItem(ts, "mdlt");
    }
    if (mdltObj && cJSON_IsArray(mdltObj)) {
        cJSON_AddItemToArray(mdltObj, cJSON_CreateString(time_str));
    }

    // Persist changes through dbmanager
    db_update_resource(ts, (char *)ts_ri, RT_TS);

    cJSON_Delete(ts);
}

void notify_missing_data(RTNode *ts_node, int current_mdc, mdc_source_t src) {
    if (!ts_node || !ts_node->child) return;

#if 1
    if (src != MDC_SRC_MONITOR_TIMEOUT && src != MDC_SRC_TSI_GAP) {
        logger("TSI_TRACE", LOG_LEVEL_DEBUG,
               "Missing-data notification skipped (src=%d) for TS [%s] mdc=%d",
               (int)src, get_ri_rtnode(ts_node), current_mdc);
        return;
    }


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


            // Suppress notification until BOTH missing-data count and missing timestamps reach the threshold
            if (threshold > 0 && (current_mdc < threshold || mdlt_len < threshold)) {
                logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                       "Missing-data notify suppressed: subRi=%s mdc=%d mdlt_len=%d < threshold=%d",
                       get_ri_rtnode(child), current_mdc, mdlt_len, threshold);
                logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                       "Missing-data notify not ready: will wait until mdc>=thr AND mdlt_len>=thr for subRi=%s",
                       get_ri_rtnode(child));
                child = child->sibling_right;
                continue;
            }

            // NOTE on md.dur semantics for this codebase/tests:
            // The ACME unit tests for missing-data notifications triggered by TSI gaps
            // (MDC_SRC_TSI_GAP) expect notifications to be sent as soon as the threshold
            // (md.num) is reached, and then again after the next threshold batch.
            // They do NOT apply md.dur as a cooldown in that case.
            //
            // Therefore, apply md.dur throttling ONLY for monitor-timeout based missing
            // data generation (MDC_SRC_MONITOR_TIMEOUT). For TSI-gap based missing data,
            // ignore md.dur.

            long long dur_us = 0;
            const char *sub_ri_s = NULL;
            int md_begin_called = 0;

            // Resolve subscription ri once (used for throttling and logs)
            cJSON *sub_ri_obj2 = cJSON_GetObjectItem(subObj, "ri");
            if (sub_ri_obj2 && cJSON_IsString(sub_ri_obj2) && sub_ri_obj2->valuestring) {
                sub_ri_s = sub_ri_obj2->valuestring;
            }

            if (src == MDC_SRC_MONITOR_TIMEOUT && enc && sub_ri_s) {
                // Read md.dur (if present) and apply as cooldown between notifications.
                cJSON *md = cJSON_GetObjectItem(enc, "md");
                if (md) {
                    cJSON *dur = cJSON_GetObjectItem(md, "dur");
                    if (dur && cJSON_IsString(dur) && dur->valuestring) {
                        dur_us = parse_md_dur_to_us(dur->valuestring);
                    }
                }

                if (dur_us > 0) {
                    // Use TS-time for cooldown so tests that advance TS time without real waiting behave correctly.
                    long long event_us_for_md = current_md_event_us(ts_mdlt);
                    logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                           "Missing-data notify timing: subRi=%s event_us=%lld dur_us=%lld (src=%d)",
                           sub_ri_s, event_us_for_md, dur_us, (int)src);

                    md_begin_called = 1;
                    if (md_notify_try_begin(sub_ri_s, event_us_for_md, dur_us)) {
                        logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                               "Missing-data notify suppressed by md.dur cooldown: subRi=%s (mdc=%d mdlt_len=%d thr=%d)",
                               sub_ri_s, current_mdc, mdlt_len, threshold);
                        child = child->sibling_right;
                        continue;
                    }
                }
            }

            if (nu && cJSON_IsArray(nu)) {
                cJSON *noti_cjson = cJSON_CreateObject();
                cJSON *sgn = cJSON_GetObjectItem(noti_cjson, "m2m:sgn");
                if (!sgn) {
                    sgn = cJSON_CreateObject();
                    cJSON_AddItemToObject(noti_cjson, "m2m:sgn", sgn);
                }
                // oneM2M notifications should include the subscription reference (sur)
                cJSON *sub_ri = NULL;
                cJSON *sub_ri_obj = cJSON_GetObjectItem(subObj, "ri");
                if (sub_ri_obj && cJSON_IsString(sub_ri_obj) && sub_ri_obj->valuestring) {
                    sub_ri = sub_ri_obj;
                }
                if (!cJSON_GetObjectItem(sgn, "sur") && sub_ri) {
                    cJSON_AddStringToObject(sgn, "sur", sub_ri->valuestring);
                }
                cJSON *nev = cJSON_CreateObject();
                cJSON_AddItemToObject(sgn, "nev", nev);
                cJSON_AddNumberToObject(nev, "net", 8);
                cJSON *rep = cJSON_CreateObject();
                cJSON_AddItemToObject(nev, "rep", rep);

                cJSON *tsn = cJSON_CreateObject();
                cJSON_AddItemToObject(rep, "m2m:tsn", tsn);

                // Report mdc as the notification threshold.
                cJSON_AddNumberToObject(tsn, "mdc", threshold);

                // Include mdlt array in the notification (deep copy)
                if (ts_mdlt && cJSON_IsArray(ts_mdlt)) {
                    cJSON *mdltCopy = cJSON_Duplicate(ts_mdlt, 1);
                    if (mdltCopy) {
                        cJSON_AddItemToObject(tsn, "mdlt", mdltCopy);
                    }
                }

                int sent_ok_any = 0;
                int nu_size = cJSON_GetArraySize(nu);
                for (int j = 0; j < nu_size; j++) {
                    cJSON *nuItem = cJSON_GetArrayItem(nu, j);
                    if (nuItem && cJSON_IsString(nuItem) && nuItem->valuestring) {
                        // Deliver a normal (non-VRQ) notification to the NU endpoint.
                        char *payload = cJSON_PrintUnformatted(noti_cjson);
                        if (payload) {
                            logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                                   "Sending missing-data notification: subRi=%s nu=%s payload_len=%zu",
                                   sub_ri_s ? sub_ri_s : get_ri_rtnode(child), nuItem->valuestring, strlen(payload));
                            int rsc = http_post_json_simple(nuItem->valuestring, payload);
                            logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                                   "Missing-data notification result: subRi=%s nu=%s rsc=%d",
                                   sub_ri_s ? sub_ri_s : get_ri_rtnode(child), nuItem->valuestring, rsc);
                            if (rsc == 2000) sent_ok_any = 1;
                            free(payload);
                        }
                    }
                }

                logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                       "Missing-data notification send summary: subRi=%s sent_ok_any=%d",
                       sub_ri_s ? sub_ri_s : get_ri_rtnode(child), sent_ok_any);

                // Finish throttle bookkeeping only if we actually began throttling.
                if (md_begin_called && sub_ri_s) {
                    md_notify_finish(sub_ri_s, sent_ok_any);
                }

                // Reset missing-data tracking after notification so that subsequent notifications
                // are emitted only after the next full threshold batch.
                logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                       "Resetting TS missing-data tracking after notification: tsRi=%s",
                       get_ri_rtnode(ts_node));
                if (threshold > 0 && ts_node && ts_node->obj) {
                    // In-memory reset
                    cJSON *mdcObj = cJSON_GetObjectItem(ts_node->obj, "mdc");
                    if (mdcObj && cJSON_IsNumber(mdcObj)) {
                        cJSON_SetNumberValue(mdcObj, 0);
                    } else {
                        cJSON_ReplaceItemInObject(ts_node->obj, "mdc", cJSON_CreateNumber(0));
                    }

                    cJSON *mdltObj = cJSON_GetObjectItem(ts_node->obj, "mdlt");
                    if (mdltObj && cJSON_IsArray(mdltObj)) {
                        // Clear array items
                        while (cJSON_GetArraySize(mdltObj) > 0) {
                            cJSON_DeleteItemFromArray(mdltObj, 0);
                        }
                    } else {
                        cJSON_ReplaceItemInObject(ts_node->obj, "mdlt", cJSON_CreateArray());
                    }

                    // Persist the reset via the DB manager (backend-agnostic)
                    const char *ts_ri = get_ri_rtnode(ts_node);
                    if (ts_ri) {
                        db_update_resource(ts_node->obj, (char *)ts_ri, RT_TS);
                        logger("TSI_TRACE", LOG_LEVEL_DEBUG,
                               "Persisted reset to DB: tsRi=%s", ts_ri);
                    }
                }

                cJSON_Delete(noti_cjson);
            }
            // Finish throttle bookkeeping only if we actually began throttling.
            if (md_begin_called && sub_ri_s && !(nu && cJSON_IsArray(nu))) {
                md_notify_finish(sub_ri_s, 0);
            }
        }
        child = child->sibling_right;
    }
#endif
}


// Traverse the in-memory resource tree and apply missing-data logic for TS resources.
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
            cJSON *ltObj  = cJSON_GetObjectItem(node->obj, "lt");
            cJSON *mdcObj = cJSON_GetObjectItem(node->obj, "mdc");

            long long pei_us = 0;
            if (peiObj && cJSON_IsNumber(peiObj)) {
                // pei is treated as milliseconds in this codebase; convert to microseconds.
                pei_us = (long long)peiObj->valuedouble * 1000LL;
            }

            const char *lt_str = (ltObj && cJSON_IsString(ltObj)) ? ltObj->valuestring : NULL;
            long long lt_us = lt_str ? parse_time_monitor_us((char *)lt_str) : 0;

            int cur_mdc = 0;
            if (mdcObj && cJSON_IsNumber(mdcObj)) cur_mdc = (int)mdcObj->valuedouble;

            if (pei_us > 0 && lt_us > 0) {
                long long diff = now_us - lt_us;
                long long tolerance_us = 2000000LL; // 2 seconds

                if (diff > (pei_us + tolerance_us)) {
                    const char *ri = get_ri_rtnode(node);
                    if (!ri) goto traverse_children;

                    // Throttle to avoid generating multiple missing periods in a short window.
                    if (should_throttle_missing(ri, now_us, pei_us)) goto traverse_children;

                    pthread_mutex_lock(&main_lock);

                    // Increment by exactly one missing period per cycle.
                    int missed_count = 1;

                    // Next expected timestamp: lt + 1 period
                    char new_lt_str[64];
                    us_to_iso8601_monitor(lt_us + pei_us, new_lt_str);

                    int after_mdc = cur_mdc + missed_count;

                    // Update in-memory TS object
                    if (mdcObj && cJSON_IsNumber(mdcObj)) {
                        cJSON_SetNumberValue(mdcObj, after_mdc);
                    } else {
                        cJSON_ReplaceItemInObject(node->obj, "mdc", cJSON_CreateNumber(after_mdc));
                    }

                    // Update lt
                    if (ltObj && cJSON_IsString(ltObj)) {
                        cJSON_SetValuestring(ltObj, new_lt_str);
                    } else {
                        cJSON_ReplaceItemInObject(node->obj, "lt", cJSON_CreateString(new_lt_str));
                    }

                    // Append to mdlt
                    cJSON *mdlt = cJSON_GetObjectItem(node->obj, "mdlt");
                    if (!mdlt || !cJSON_IsArray(mdlt)) {
                        cJSON_ReplaceItemInObject(node->obj, "mdlt", cJSON_CreateArray());
                        mdlt = cJSON_GetObjectItem(node->obj, "mdlt");
                    }
                    cJSON_AddItemToArray(mdlt, cJSON_CreateString(new_lt_str));

                    // Persist via DB manager (backend-agnostic)
                    db_update_resource(node->obj, (char *)ri, RT_TS);

                    // Notify subscriptions (same behavior as PostgreSQL path)
                    int should_notify = 1;

                    set_last_gen_us_for_ri(ri, now_us);

                    pthread_mutex_unlock(&main_lock);
                    if (should_notify) notify_missing_data(node, after_mdc, MDC_SRC_MONITOR_TIMEOUT);
                }
            }
        }
    }

traverse_children:
    // Traverse children first, then siblings
    if (node->child) traverse_and_check_ts_missing(node->child, now_us);
    if (node->sibling_right) traverse_and_check_ts_missing(node->sibling_right, now_us);
}

void *monitor_serve(void *arg) {
    (void)arg;

    logger("MONITOR", LOG_LEVEL_INFO, "TS Monitoring Thread Started (in-memory traversal)");

    while (!terminate) {
        long long now_us = wallclock_now_us();
        char *now_str = get_local_time(0);

        // Start traversal from the CSE base node (CSE_BASE_RI) if available.
        // This avoids any dependency on a ResourceTree wrapper header.
        RTNode *root = find_rtnode(CSE_BASE_RI);
        if (root) {
            traverse_and_check_ts_missing(root, now_us);
        }

        free(now_str);
        usleep(500000);
    }

    return NULL;
}