#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "onem2m.h"
#include "dbmanager.h"
#include "logger.h"
#include "jsonparser.h"
#include "util.h"
#include "config.h"

#include <libpq-fe.h>

// Forward declaration for compatibility
typedef struct sqlite3_stmt sqlite3_stmt;

PGconn *pg_conn;

extern cJSON *ATTRIBUTES;
extern ResourceTree *rt;

/* Table definition structure */
typedef struct {
    const char *name;
    const char *sql;
} table_def_t;

#define ID_COLUMN_TYPE "SERIAL PRIMARY KEY"

/* Static table definitions for PostgreSQL */
static const table_def_t table_definitions[] = {
    {"general", 
     "CREATE TABLE IF NOT EXISTS general ( id " ID_COLUMN_TYPE ", "
     "rn VARCHAR(60), ri VARCHAR(40), pi VARCHAR(40), ct VARCHAR(30), et VARCHAR(30), lt VARCHAR(30), "
     "uri VARCHAR(255), acpi VARCHAR(255), lbl VARCHAR(255), ty INT, memberOf VARCHAR(255) );"
    },
    {"csr", 
     "CREATE TABLE IF NOT EXISTS csr ( id INTEGER, "
     "cst INT, poa VARCHAR(200), cb VARCHAR(200), csi VARCHAR(200), mei VARCHAR(45), "
     "tri VARCHAR(45), rr INT, nl VARCHAR(45), srv VARCHAR(45), dcse VARCHAR(200), csz VARCHAR(100), "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"ae", 
     "CREATE TABLE IF NOT EXISTS ae ( id INTEGER, "
     "api VARCHAR(45), aei VARCHAR(200), rr INT, poa VARCHAR(255), apn VARCHAR(100), srv VARCHAR(45), at VARCHAR(200), aa VARCHAR(100), ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"cnt", 
     "CREATE TABLE IF NOT EXISTS cnt ( id INTEGER, "
     "cr VARCHAR(40), mni INT, mbs INT, mia INT, st INT, cni INT, cbs INT, li VARCHAR(45), oref VARCHAR(45), disr VARCHAR(45), at VARCHAR(200), aa VARCHAR(100), ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"cin", 
     "CREATE TABLE IF NOT EXISTS cin ( id INTEGER, "
     "cs INT, cr VARCHAR(45), cnf VARCHAR(45), oref VARCHAR(45), con TEXT, st INT, at VARCHAR(200), aa VARCHAR(100), ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"acp", 
     "CREATE TABLE IF NOT EXISTS acp ( id INTEGER, pv VARCHAR(60), pvs VARCHAR(100), at VARCHAR(200), aa VARCHAR(100), ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"sub", 
     "CREATE TABLE IF NOT EXISTS sub ( id INTEGER, "
     "enc VARCHAR(45), exc INT, nu VARCHAR(200), gpi VARCHAR(45), nfu VARCHAR(45), bn VARCHAR(45), rl VARCHAR(45), "
     "sur VARCHAR(200), nct VARCHAR(45), net VARCHAR(45), cr VARCHAR(45), su VARCHAR(45), at VARCHAR(200), aa VARCHAR(100), ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"grp", 
     "CREATE TABLE IF NOT EXISTS grp ( id INTEGER, "
     "cr VARCHAR(45), mt INT, cnm INT, mnm INT, mid TEXT, macp TEXT, mtv INT, csy INT, gn VARCHAR(30), at VARCHAR(200), aa VARCHAR(100), ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"cb", 
     "CREATE TABLE IF NOT EXISTS cb ( id INTEGER, "
     "cst INT, csi VARCHAR(45), srt VARCHAR(100), poa VARCHAR(200), nl VARCHAR(45), ncp VARCHAR(45), srv VARCHAR(45), rr INT, at VARCHAR(200), aa VARCHAR(100), ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"cbA", 
     "CREATE TABLE IF NOT EXISTS cbA ( id INTEGER, "
     "cst INT, lnk VARCHAR(100), csi VARCHAR(45), srt VARCHAR(100), poa VARCHAR(200), nl VARCHAR(45), ncp VARCHAR(45), srv VARCHAR(45), rr INT, "
     "at VARCHAR(200), aa VARCHAR(100), ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"aeA", 
     "CREATE TABLE IF NOT EXISTS aeA ( id INTEGER, "
     "api VARCHAR(45), lnk VARCHAR(100), aei VARCHAR(200), rr INT, poa VARCHAR(255), apn VARCHAR(100), srv VARCHAR(45), at VARCHAR(200), aa VARCHAR(100), ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"cntA", 
     "CREATE TABLE IF NOT EXISTS cntA ( id INTEGER, "
     "lnk VARCHAR(100), cr VARCHAR(45), mni INT, mbs INT, st INT, cni INT, cbs INT, ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"cinA", 
     "CREATE TABLE IF NOT EXISTS cinA ( id INTEGER, "
     "lnk VARCHAR(100), cs INT, cr VARCHAR(45), cnf VARCHAR(45), st VARCHAR(45), con TEXT, ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    }
};

#define TABLE_COUNT (sizeof(table_definitions) / sizeof(table_def_t))

/* Helper function to execute SQL and handle errors for PostgreSQL */
static int execute_sql_with_error_handling(const char *sql, const char *context)
{
    PGresult *res = PQexec(pg_conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "%s error: %s", context, PQerrorMessage(pg_conn));
        PQclear(res);
        return 0;
    }
    PQclear(res);
    return 1;
}

/* Helper function to create a single table for PostgreSQL */
static int create_table(const table_def_t *table_def)
{
    if (!execute_sql_with_error_handling(table_def->sql, table_def->name)) {
        /* The detailed error is already logged by execute_sql_with_error_handling */
        return 0;
    }
    return 1;
}

/* DB init */
int init_dbp()
{
    char conninfo[256];
    sprintf(conninfo, "host=%s port=%d user=%s password=%s dbname=%s",
            PG_HOST, PG_PORT, PG_USER, PG_PASSWORD, PG_DBNAME);

    pg_conn = PQconnectdb(conninfo);
    if (PQstatus(pg_conn) != CONNECTION_OK) {
        logger("DB", LOG_LEVEL_ERROR, "PostgreSQL connection failed: %s", PQerrorMessage(pg_conn));
        PQfinish(pg_conn);
        return 0;
    }
    logger("DB", LOG_LEVEL_INFO, "PostgreSQL connected successfully.");

    // Begin transaction
    if (!execute_sql_with_error_handling("BEGIN", "Begin Transaction")) {
        PQfinish(pg_conn);
        return 0;
    }

    // Create all tables
    for (size_t i = 0; i < TABLE_COUNT; i++) {
        if (!create_table(&table_definitions[i])) {
            PQexec(pg_conn, "ROLLBACK");
            PQfinish(pg_conn);
            return 0;
        }
    }

    // Commit transaction
    if (!execute_sql_with_error_handling("COMMIT", "Commit Transaction")) {
        PQexec(pg_conn, "ROLLBACK");
        PQfinish(pg_conn);
        return 0;
    }

    logger("DB", LOG_LEVEL_INFO, "Database initialized successfully with %zu tables", TABLE_COUNT);
    return 1;
}

int close_dbp()
{
    if (pg_conn) {
        PQfinish(pg_conn);
        pg_conn = NULL;
    }
    return 1;
}

char *get_table_name(ResourceType ty)
{
    char *tableName = NULL;
    switch (ty)
    {
    case RT_CSE:
        tableName = "cb";
        break;
    case RT_AE:
        tableName = "ae";
        break;
    case RT_CNT:
        tableName = "cnt";
        break;
    case RT_CIN:
        tableName = "cin";
        break;
    case RT_ACP:
        tableName = "acp";
        break;
    case RT_GRP:
        tableName = "grp";
        break;
    case RT_SUB:
        tableName = "sub";
        break;
    case RT_CSR:
        tableName = "csr";
        break;
    case RT_AEA:
        tableName = "aeA";
        break;
    case RT_CBA:
        tableName = "cbA";
        break;
    case RT_CNTA:
        tableName = "cntA";
        break;
    case RT_CINA:
        tableName = "cinA";
        break;
    }
    return tableName;
}

/**
 * @brief Get resource from db by uri
 * @param uri Resource URI
 * @param ty Resource type
 * @return cJSON* Resource object or NULL
 */
cJSON *db_get_resource_by_uri(char *uri, ResourceType ty)
{
    char sql[1024];
    cJSON *resource = NULL;
    PGresult *res;

    sprintf(sql, "SELECT * FROM general, %s WHERE general.uri='%s' and %s.id=general.id;", 
            get_table_name(ty), uri, get_table_name(ty));

    res = PQexec(pg_conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Query failed: %s", PQerrorMessage(pg_conn));
        PQclear(res);
        return NULL;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        return NULL;
    }

    resource = cJSON_CreateObject();
    int cols = PQnfields(res);
    char *colname = NULL;
    cJSON *arr = NULL;

    for (int col = 0; col < cols; col++) {
        colname = PQfname(res, col);
        char *value = PQgetvalue(res, 0, col);

        if (strcmp(colname, "id") == 0)
            continue;

        if (value && strlen(value) > 0) {
            // Try to parse as JSON first
            arr = cJSON_Parse(value);
            if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)) {
                cJSON_AddItemToObject(resource, colname, arr);
            } else {
                // Check if it's a number
                char *endptr;
                long num = strtol(value, &endptr, 10);
                if (*endptr == '\0') {
                    cJSON_AddNumberToObject(resource, colname, num);
                } else {
                    cJSON_AddItemToObject(resource, colname, cJSON_CreateString(value));
                }
                cJSON_Delete(arr);
            }
        }
    }

    PQclear(res);
    return resource;
}

/**
 * @brief Get resource from db by resource ID
 * @param ri Resource ID
 * @param ty Resource type
 * @return cJSON* Resource object or NULL
 */
cJSON *db_get_resource(char *ri, ResourceType ty)
{
    char sql[1024];
    cJSON *resource = NULL;
    PGresult *res;

    sprintf(sql, "SELECT * FROM general, %s WHERE general.id=%s.id AND general.ri='%s';", 
            get_table_name(ty), get_table_name(ty), ri);

    res = PQexec(pg_conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Query failed: %s", PQerrorMessage(pg_conn));
        PQclear(res);
        return NULL;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        return NULL;
    }

    resource = cJSON_CreateObject();
    int cols = PQnfields(res);
    char *colname = NULL;
    cJSON *arr = NULL;

    for (int col = 0; col < cols; col++) {
        colname = PQfname(res, col);
        char *value = PQgetvalue(res, 0, col);

        if (strcmp(colname, "id") == 0)
            continue;

        if (value && strlen(value) > 0) {
            // Try to parse as JSON first
            arr = cJSON_Parse(value);
            if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)) {
                cJSON_AddItemToObject(resource, colname, arr);
            } else {
                // Check if it's a number
                char *endptr;
                long num = strtol(value, &endptr, 10);
                if (*endptr == '\0') {
                    cJSON_AddNumberToObject(resource, colname, num);
                } else {
                    cJSON_AddItemToObject(resource, colname, cJSON_CreateString(value));
                }
                cJSON_Delete(arr);
            }
        }
    }

    PQclear(res);
    return resource;
}

// PostgreSQL specific helper functions for parameter binding
static char *pg_escape_string_value(const char *value)
{
    if (!value) return NULL;
    
    size_t len = strlen(value);
    char *escaped = malloc(len * 2 + 1);
    PQescapeStringConn(pg_conn, escaped, value, len, NULL);
    return escaped;
}

static char *pg_format_json_value(cJSON *obj)
{
    char *json_str = cJSON_PrintUnformatted(obj);
    char *escaped = pg_escape_string_value(json_str);
    free(json_str);
    return escaped;
}

/**
 * @brief Store resource to db
 * @param obj Resource object
 * @param uri Resource URI
 * @return 0 if success, -1 if failed
 */
int db_store_resource(cJSON *obj, char *uri)
{
    char *sql = NULL;
    cJSON *pjson = NULL;
    cJSON *GENERAL_ATTR = cJSON_GetObjectItem(ATTRIBUTES, "general");
    int general_cnt = cJSON_GetArraySize(GENERAL_ATTR);
    PGresult *res;

    ResourceType ty = cJSON_GetObjectItem(obj, "ty")->valueint;

    // Begin transaction
    res = PQexec(pg_conn, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to begin transaction: %s", PQerrorMessage(pg_conn));
        PQclear(res);
        return -1;
    }
    PQclear(res);

    // Build INSERT statement for general table
    sql = malloc(2048);
    sprintf(sql, "INSERT INTO general (");
    for (int i = 0; i < general_cnt; i++) {
        strcat(sql, cJSON_GetArrayItem(GENERAL_ATTR, i)->string);
        if (i < general_cnt - 1) strcat(sql, ",");
    }
    strcat(sql, ") VALUES (");

    // Add values
    for (int i = 0; i < general_cnt; i++) {
        char *attr = cJSON_GetArrayItem(GENERAL_ATTR, i)->string;
        if (strcmp(attr, "uri") == 0) {
            char *escaped_uri = pg_escape_string_value(uri);
            sprintf(sql + strlen(sql), "'%s'", escaped_uri);
            free(escaped_uri);
        } else {
            pjson = cJSON_GetObjectItem(obj, attr);
            if (pjson) {
                if (cJSON_IsString(pjson)) {
                    char *escaped = pg_escape_string_value(pjson->valuestring);
                    sprintf(sql + strlen(sql), "'%s'", escaped);
                    free(escaped);
                } else if (cJSON_IsNumber(pjson)) {
                    sprintf(sql + strlen(sql), "%d", pjson->valueint);
                } else if (cJSON_IsArray(pjson) || cJSON_IsObject(pjson)) {
                    char *escaped = pg_format_json_value(pjson);
                    sprintf(sql + strlen(sql), "'%s'", escaped);
                    free(escaped);
                } else {
                    strcat(sql, "NULL");
                }
            } else {
                strcat(sql, "NULL");
            }
        }
        if (i < general_cnt - 1) strcat(sql, ",");
    }
    strcat(sql, ");");

    res = PQexec(pg_conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to insert into general table: %s", PQerrorMessage(pg_conn));
        PQclear(res);
        free(sql);
        PQexec(pg_conn, "ROLLBACK");
        return -1;
    }
    PQclear(res);

    // Insert into specific table
    cJSON *specific_attr = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(ty));
    sql[0] = '\0';
    sprintf(sql, "INSERT INTO %s (id, ", get_table_name(ty));
    
    for (int i = 0; i < cJSON_GetArraySize(specific_attr); i++) {
        strcat(sql, cJSON_GetArrayItem(specific_attr, i)->string);
        if (i < cJSON_GetArraySize(specific_attr) - 1) strcat(sql, ",");
    }
    strcat(sql, ") VALUES ((SELECT id FROM general WHERE ri='");
    
    char *ri_str = cJSON_GetObjectItem(obj, "ri")->valuestring;
    char *escaped_ri = pg_escape_string_value(ri_str);
    strcat(sql, escaped_ri);
    free(escaped_ri);
    strcat(sql, "'),");

    for (int i = 0; i < cJSON_GetArraySize(specific_attr); i++) {
        pjson = cJSON_GetObjectItem(obj, cJSON_GetArrayItem(specific_attr, i)->string);
        if (pjson) {
            if (cJSON_IsString(pjson)) {
                char *escaped = pg_escape_string_value(pjson->valuestring);
                sprintf(sql + strlen(sql), "'%s'", escaped);
                free(escaped);
            } else if (cJSON_IsNumber(pjson)) {
                sprintf(sql + strlen(sql), "%d", pjson->valueint);
            } else if (cJSON_IsArray(pjson) || cJSON_IsObject(pjson)) {
                char *escaped = pg_format_json_value(pjson);
                sprintf(sql + strlen(sql), "'%s'", escaped);
                free(escaped);
            } else {
                strcat(sql, "NULL");
            }
        } else {
            strcat(sql, "NULL");
        }
        if (i < cJSON_GetArraySize(specific_attr) - 1) strcat(sql, ",");
    }
    strcat(sql, ");");

    res = PQexec(pg_conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to insert into %s table: %s", get_table_name(ty), PQerrorMessage(pg_conn));
        PQclear(res);
        free(sql);
        PQexec(pg_conn, "ROLLBACK");
        return -1;
    }
    PQclear(res);

    // Commit transaction
    res = PQexec(pg_conn, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to commit transaction: %s", PQerrorMessage(pg_conn));
        PQclear(res);
        free(sql);
        return -1;
    }
    PQclear(res);

    free(sql);
    return 1;
}

// PostgreSQL에서는 prepared statement를 사용하지 않으므로 이 함수들은 빈 구현으로 남겨둡니다
void db_test_and_bind_value(sqlite3_stmt *stmt, int index, cJSON *obj)
{
    // PostgreSQL implementation doesn't use prepared statements in this way
    // This function is kept for interface compatibility
}

void db_test_and_set_bind_text(sqlite3_stmt *stmt, int index, char *context)
{
    // PostgreSQL implementation doesn't use prepared statements in this way
    // This function is kept for interface compatibility
}

void db_test_and_set_bind_int(sqlite3_stmt *stmt, int index, int value)
{
    // PostgreSQL implementation doesn't use prepared statements in this way
    // This function is kept for interface compatibility
}

// 나머지 함수들은 PostgreSQL 구현으로 계속 작성해야 합니다.
// 여기서는 주요 함수들만 구현하고, 나머지는 필요에 따라 추가 구현합니다.

int db_update_resource(cJSON *obj, char *ri, ResourceType ty)
{
    logger("DB", LOG_LEVEL_DEBUG, "Call db_update_resource [RI]: %s", ri);
    char *sql = NULL;
    cJSON *pjson = NULL;
    cJSON *GENERAL_ATTR = cJSON_GetObjectItem(ATTRIBUTES, "general");
    int general_cnt = cJSON_GetArraySize(GENERAL_ATTR);
    PGresult *res;

    // Begin transaction
    res = PQexec(pg_conn, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to begin transaction: %s", PQerrorMessage(pg_conn));
        PQclear(res);
        return 0;
    }
    PQclear(res);

    sql = malloc(2048);
    int has_general_updates = 0;
    
    // Build UPDATE statement for general table
    sprintf(sql, "UPDATE general SET ");
    for (int i = 0; i < general_cnt; i++) {
        char *attr = cJSON_GetArrayItem(GENERAL_ATTR, i)->string;
        if (cJSON_GetObjectItem(obj, attr)) {
            if (has_general_updates > 0) strcat(sql, ", ");
            strcat(sql, attr);
            strcat(sql, "=");
            
            pjson = cJSON_GetObjectItem(obj, attr);
            if (cJSON_IsString(pjson)) {
                char *escaped = pg_escape_string_value(pjson->valuestring);
                sprintf(sql + strlen(sql), "'%s'", escaped);
                free(escaped);
            } else if (cJSON_IsNumber(pjson)) {
                sprintf(sql + strlen(sql), "%d", pjson->valueint);
            } else if (cJSON_IsArray(pjson) || cJSON_IsObject(pjson)) {
                char *escaped = pg_format_json_value(pjson);
                sprintf(sql + strlen(sql), "'%s'", escaped);
                free(escaped);
            } else {
                strcat(sql, "NULL");
            }
            has_general_updates++;
        }
    }
    
    if (has_general_updates > 0) {
        char *escaped_ri = pg_escape_string_value(ri);
        sprintf(sql + strlen(sql), " WHERE ri='%s';", escaped_ri);
        free(escaped_ri);

        res = PQexec(pg_conn, sql);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            logger("DB", LOG_LEVEL_ERROR, "Failed to update general table: %s", PQerrorMessage(pg_conn));
            PQclear(res);
            free(sql);
            PQexec(pg_conn, "ROLLBACK");
            return 0;
        }
        PQclear(res);
    }

    // Update specific table
    cJSON *specific_attr = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(ty));
    int has_specific_updates = 0;
    
    sql[0] = '\0';
    sprintf(sql, "UPDATE %s SET ", get_table_name(ty));
    for (int i = 0; i < cJSON_GetArraySize(specific_attr); i++) {
        char *attr = cJSON_GetArrayItem(specific_attr, i)->string;
        if (cJSON_GetObjectItem(obj, attr)) {
            if (has_specific_updates > 0) strcat(sql, ", ");
            strcat(sql, attr);
            strcat(sql, "=");
            
            pjson = cJSON_GetObjectItem(obj, attr);
            if (cJSON_IsString(pjson)) {
                char *escaped = pg_escape_string_value(pjson->valuestring);
                sprintf(sql + strlen(sql), "'%s'", escaped);
                free(escaped);
            } else if (cJSON_IsNumber(pjson)) {
                sprintf(sql + strlen(sql), "%d", pjson->valueint);
            } else if (cJSON_IsArray(pjson) || cJSON_IsObject(pjson)) {
                char *escaped = pg_format_json_value(pjson);
                sprintf(sql + strlen(sql), "'%s'", escaped);
                free(escaped);
            } else {
                strcat(sql, "NULL");
            }
            has_specific_updates++;
        }
    }
    
    if (has_specific_updates > 0) {
        char *escaped_ri = pg_escape_string_value(ri);
        sprintf(sql + strlen(sql), " WHERE id=(SELECT id FROM general WHERE ri='%s');", escaped_ri);
        free(escaped_ri);

        res = PQexec(pg_conn, sql);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            logger("DB", LOG_LEVEL_ERROR, "Failed to update %s table: %s", get_table_name(ty), PQerrorMessage(pg_conn));
            PQclear(res);
            free(sql);
            PQexec(pg_conn, "ROLLBACK");
            return 0;
        }
        PQclear(res);
    }

    // Commit transaction
    res = PQexec(pg_conn, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to commit transaction: %s", PQerrorMessage(pg_conn));
        PQclear(res);
        free(sql);
        return 0;
    }
    PQclear(res);

    free(sql);
    return 1;
}

int db_delete_onem2m_resource(RTNode *rtnode)
{
    char *ri = get_ri_rtnode(rtnode);
    logger("DB", LOG_LEVEL_DEBUG, "Delete [RI] %s", ri);
    char sql[1024] = {0};
    PGresult *res;
    
    if (!ri) {
        return 0;
    }

    char *escaped_uri = pg_escape_string_value(get_uri_rtnode(rtnode));
    char *escaped_ri = pg_escape_string_value(ri);
    
    sprintf(sql, "DELETE FROM general WHERE uri LIKE '%s/%%' OR ri='%s';", escaped_uri, escaped_ri);
    logger("DB", LOG_LEVEL_DEBUG, "SQL : %s", sql);
    
    res = PQexec(pg_conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource: %s", PQerrorMessage(pg_conn));
        PQclear(res);
        free(escaped_uri);
        free(escaped_ri);
        return 0;
    }
    PQclear(res);
    
    free(escaped_uri);
    free(escaped_ri);
    return 1;
}

int db_delete_one_cin_mni(RTNode *cnt)
{
    // TODO: PostgreSQL implementation
    logger("DB", LOG_LEVEL_ERROR, "db_delete_one_cin_mni not implemented for PostgreSQL yet");
    return 0;
}

RTNode *db_get_all_resource_as_rtnode()
{
    // TODO: PostgreSQL implementation
    logger("DB", LOG_LEVEL_ERROR, "db_get_all_resource_as_rtnode not implemented for PostgreSQL yet");
    return NULL;
}

RTNode *db_get_cin_rtnode_list(RTNode *parent_rtnode)
{
    // TODO: PostgreSQL implementation
    logger("DB", LOG_LEVEL_ERROR, "db_get_cin_rtnode_list not implemented for PostgreSQL yet");
    return NULL;
}

RTNode *db_get_latest_cins()
{
    // TODO: PostgreSQL implementation
    logger("DB", LOG_LEVEL_ERROR, "db_get_latest_cins not implemented for PostgreSQL yet");
    return NULL;
}

cJSON *db_get_cin_laol(RTNode *parent_rtnode, int laol)
{
    // TODO: PostgreSQL implementation
    logger("DB", LOG_LEVEL_ERROR, "db_get_cin_laol not implemented for PostgreSQL yet");
    return NULL;
}

cJSON *db_get_filter_criteria(oneM2MPrimitive *o2pt)
{
    // TODO: PostgreSQL implementation
    logger("DB", LOG_LEVEL_ERROR, "db_get_filter_criteria not implemented for PostgreSQL yet");
    return NULL;
}

bool db_check_cin_rn_dup(char *rn, char *pi)
{
    if (!rn || !pi) return false;
    
    char sql[1024] = {0};
    PGresult *res;
    
    char *escaped_rn = pg_escape_string_value(rn);
    char *escaped_pi = pg_escape_string_value(pi);
    
    sprintf(sql, "SELECT * FROM general WHERE rn='%s' AND pi='%s';", escaped_rn, escaped_pi);
    
    res = PQexec(pg_conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Query failed: %s", PQerrorMessage(pg_conn));
        PQclear(res);
        free(escaped_rn);
        free(escaped_pi);
        return false;
    }
    
    bool result = (PQntuples(res) > 0);
    PQclear(res);
    free(escaped_rn);
    free(escaped_pi);
    
    return result;
}

cJSON *getForbiddenUri(cJSON *acp_list)
{
    // TODO: PostgreSQL implementation
    logger("DB", LOG_LEVEL_ERROR, "getForbiddenUri not implemented for PostgreSQL yet");
    return cJSON_CreateArray();
}
