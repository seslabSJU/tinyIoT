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

// Thread-local PostgreSQL connection and connection info
static __thread PGconn *pg_conn = NULL;
static char pg_conninfo[256] = {0};

// Get or create a thread-local PG connection
static PGconn *get_pg_conn(void)
{
    if (pg_conn && PQstatus(pg_conn) == CONNECTION_OK)
    {
        return pg_conn;
    }

    if (pg_conn)
    {
        PQfinish(pg_conn);
        pg_conn = NULL;
    }

    pg_conn = PQconnectdb(pg_conninfo);
    if (PQstatus(pg_conn) != CONNECTION_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "PostgreSQL connection failed: %s", PQerrorMessage(pg_conn));
        PQfinish(pg_conn);
        pg_conn = NULL;
        return NULL;
    }

    return pg_conn;
}

extern cJSON *ATTRIBUTES;
extern ResourceTree *rt;

/* Debug helper functions for cJSON */
void debug_print_cjson(const char *label, cJSON *json) {
    if (!json) {
        logger("DEBUG", LOG_LEVEL_DEBUG, "%s: NULL", label);
        return;
    }
    
    char *json_string = cJSON_Print(json);
    if (json_string) {
        logger("DEBUG", LOG_LEVEL_DEBUG, "%s: %s", label, json_string);
        free(json_string);
    } else {
        logger("DEBUG", LOG_LEVEL_DEBUG, "%s: Failed to print JSON", label);
    }
}

void debug_print_cjson_type_and_value(const char *label, cJSON *json) {
    if (!json) {
        logger("DEBUG", LOG_LEVEL_DEBUG, "%s: NULL", label);
        return;
    }
    
    switch (json->type) {
        case cJSON_String:
            logger("DEBUG", LOG_LEVEL_DEBUG, "%s: STRING = '%s'", label, json->valuestring ? json->valuestring : "NULL");
            break;
        case cJSON_Number:
            logger("DEBUG", LOG_LEVEL_DEBUG, "%s: NUMBER = %d (double: %.2f)", label, json->valueint, json->valuedouble);
            break;
        case cJSON_True:
            logger("DEBUG", LOG_LEVEL_DEBUG, "%s: TRUE", label);
            break;
        case cJSON_False:
            logger("DEBUG", LOG_LEVEL_DEBUG, "%s: FALSE", label);
            break;
        case cJSON_NULL:
            logger("DEBUG", LOG_LEVEL_DEBUG, "%s: NULL_VALUE", label);
            break;
        case cJSON_Array:
            logger("DEBUG", LOG_LEVEL_DEBUG, "%s: ARRAY (size: %d)", label, cJSON_GetArraySize(json));
            break;
        case cJSON_Object:
            logger("DEBUG", LOG_LEVEL_DEBUG, "%s: OBJECT", label);
            break;
        default:
            logger("DEBUG", LOG_LEVEL_DEBUG, "%s: UNKNOWN_TYPE (%d)", label, json->type);
            break;
    }
}

/* Table definition structure */
typedef struct {
    const char *name;
    const char *sql;
} table_def_t;

#define ID_COLUMN_TYPE "SERIAL PRIMARY KEY"

/* Static table definitions for PostgreSQL */
#if PG_SCHEMA_TYPE == PG_SCHEMA_VARCHAR
// VARCHAR-based schema
static const table_def_t table_definitions[] = {
    {"general", 
     "CREATE TABLE IF NOT EXISTS general ( id " ID_COLUMN_TYPE ", "
     "rn VARCHAR(255), ri VARCHAR(40), pi VARCHAR(40), ct VARCHAR(30), et VARCHAR(30), lt VARCHAR(30), "
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
     "CREATE TABLE IF NOT EXISTS acp ( id INTEGER, pv VARCHAR(255), pvs VARCHAR(100), at VARCHAR(200), aa VARCHAR(100), ast INT, "
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

#elif PG_SCHEMA_TYPE == PG_SCHEMA_TEXT
// TEXT-based schema
static const table_def_t table_definitions[] = {
    {"general", 
     "CREATE TABLE IF NOT EXISTS general ( id " ID_COLUMN_TYPE ", "
     "rn TEXT, ri TEXT, pi TEXT, ct TEXT, et TEXT, lt TEXT, "
     "uri TEXT, acpi TEXT, lbl TEXT, ty INT, memberOf TEXT );"
    },
    {"csr", 
     "CREATE TABLE IF NOT EXISTS csr ( id INTEGER, "
     "cst INT, poa TEXT, cb TEXT, csi TEXT, mei TEXT, "
     "tri TEXT, rr INT, nl TEXT, srv TEXT, dcse TEXT, csz TEXT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"ae", 
     "CREATE TABLE IF NOT EXISTS ae ( id INTEGER, "
     "api TEXT, aei TEXT, rr INT, poa TEXT, apn TEXT, srv TEXT, at TEXT, aa TEXT, ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"cnt", 
     "CREATE TABLE IF NOT EXISTS cnt ( id INTEGER, "
     "cr TEXT, mni INT, mbs INT, mia INT, st INT, cni INT, cbs INT, li TEXT, oref TEXT, disr TEXT, at TEXT, aa TEXT, ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"cin", 
     "CREATE TABLE IF NOT EXISTS cin ( id INTEGER, "
     "cs INT, cr TEXT, cnf TEXT, oref TEXT, con TEXT, st INT, at TEXT, aa TEXT, ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"acp", 
     "CREATE TABLE IF NOT EXISTS acp ( id INTEGER, pv TEXT, pvs TEXT, at TEXT, aa TEXT, ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"sub", 
     "CREATE TABLE IF NOT EXISTS sub ( id INTEGER, "
     "enc TEXT, exc INT, nu TEXT, gpi TEXT, nfu TEXT, bn TEXT, rl TEXT, "
     "sur TEXT, nct TEXT, net TEXT, cr TEXT, su TEXT, at TEXT, aa TEXT, ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"grp", 
     "CREATE TABLE IF NOT EXISTS grp ( id INTEGER, "
     "cr TEXT, mt INT, cnm INT, mnm INT, mid TEXT, macp TEXT, mtv INT, csy INT, gn TEXT, at TEXT, aa TEXT, ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"cb", 
     "CREATE TABLE IF NOT EXISTS cb ( id INTEGER, "
     "cst INT, csi TEXT, srt TEXT, poa TEXT, nl TEXT, ncp TEXT, srv TEXT, rr INT, at TEXT, aa TEXT, ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"cbA", 
     "CREATE TABLE IF NOT EXISTS cbA ( id INTEGER, "
     "cst INT, lnk TEXT, csi TEXT, srt TEXT, poa TEXT, nl TEXT, ncp TEXT, srv TEXT, rr INT, "
     "at TEXT, aa TEXT, ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"aeA", 
     "CREATE TABLE IF NOT EXISTS aeA ( id INTEGER, "
     "api TEXT, lnk TEXT, aei TEXT, rr INT, poa TEXT, apn TEXT, srv TEXT, at TEXT, aa TEXT, ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"cntA", 
     "CREATE TABLE IF NOT EXISTS cntA ( id INTEGER, "
     "lnk TEXT, cr TEXT, mni INT, mbs INT, st INT, cni INT, cbs INT, ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"cinA", 
     "CREATE TABLE IF NOT EXISTS cinA ( id INTEGER, "
     "lnk TEXT, cs INT, cr TEXT, cnf TEXT, st TEXT, con TEXT, ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    }
};

#else
#error "PG_SCHEMA_TYPE must be either PG_SCHEMA_VARCHAR or PG_SCHEMA_TEXT"
#endif

#define TABLE_COUNT (sizeof(table_definitions) / sizeof(table_def_t))

/* Helper function to execute SQL and handle errors for PostgreSQL */
static int execute_sql_with_error_handling(const char *sql, const char *context)
{
    PGconn *conn = get_pg_conn();
    if (!conn)
        return 0;

    PGresult *res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "%s error: %s", context, PQerrorMessage(conn));
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
        return 0;
    }
    return 1;
}

/* DB init */
int init_dbp()
{
    snprintf(pg_conninfo, sizeof(pg_conninfo), "host=%s port=%d user=%s password=%s dbname=%s",
             PG_HOST, PG_PORT, PG_USER, PG_PASSWORD, PG_DBNAME);

    PGconn *conn = get_pg_conn();
    if (!conn) {
        return 0;
    }
    logger("DB", LOG_LEVEL_INFO, "PostgreSQL connected successfully.");

    // Begin transaction
    if (!execute_sql_with_error_handling("BEGIN", "Begin Transaction")) {
        return 0;
    }

    // Create all tables
    for (size_t i = 0; i < TABLE_COUNT; i++) {
        if (!create_table(&table_definitions[i])) {
            PQexec(conn, "ROLLBACK");
            PQfinish(conn);
            pg_conn = NULL;
            return 0;
        }
    }

    // Commit transaction
    if (!execute_sql_with_error_handling("COMMIT", "Commit Transaction")) {
        PQexec(conn, "ROLLBACK");
        PQfinish(conn);
        pg_conn = NULL;
        return 0;
    }

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
    PGconn *conn = get_pg_conn();
    if (!conn) {
        return NULL;
    }

    sprintf(sql, "SELECT * FROM general, %s WHERE general.uri='%s' and %s.id=general.id;", 
            get_table_name(ty), uri, get_table_name(ty));

    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Query failed: %s", PQerrorMessage(conn));
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
    PGconn *conn = get_pg_conn();
    if (!conn) {
        return NULL;
    }

    sprintf(sql, "SELECT * FROM general, %s WHERE general.id=%s.id AND general.ri='%s';", 
            get_table_name(ty), get_table_name(ty), ri);

    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Query failed: %s", PQerrorMessage(conn));
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
    
    PGconn *conn = get_pg_conn();
    if (!conn) return NULL;

    size_t len = strlen(value);
    char *escaped = malloc(len * 2 + 1);
    PQescapeStringConn(conn, escaped, value, len, NULL);
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
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_resource with URI: %s", uri ? uri : "NULL");
    
    // Debug: Print the entire resource object
    debug_print_cjson("Resource Object", obj);
    
    char *sql = NULL;
    cJSON *pjson = NULL;
    cJSON *GENERAL_ATTR = cJSON_GetObjectItem(ATTRIBUTES, "general");
    int general_cnt = cJSON_GetArraySize(GENERAL_ATTR);
    PGresult *res;
    PGconn *conn = get_pg_conn();
    if (!conn) {
        return -1;
    }

    if (!obj) {
        logger("DB", LOG_LEVEL_ERROR, "Resource object is NULL");
        return -1;
    }

    cJSON *ty_obj = cJSON_GetObjectItem(obj, "ty");
    if (!ty_obj) {
        logger("DB", LOG_LEVEL_ERROR, "Missing 'ty' field in resource object");
        return -1;
    }
    ResourceType ty = ty_obj->valueint;
    logger("DB", LOG_LEVEL_DEBUG, "Resource type: %d", ty);
    
    // Debug: Print key fields
    debug_print_cjson_type_and_value("ty field", ty_obj);
    debug_print_cjson_type_and_value("ri field", cJSON_GetObjectItem(obj, "ri"));
    debug_print_cjson_type_and_value("rn field", cJSON_GetObjectItem(obj, "rn"));
    debug_print_cjson_type_and_value("pi field", cJSON_GetObjectItem(obj, "pi"));

    // Begin transaction
    logger("DB", LOG_LEVEL_DEBUG, "Executing: BEGIN");
    res = PQexec(conn, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to begin transaction: %s", PQerrorMessage(conn));
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
                } else if (cJSON_IsTrue(pjson)) {
                    strcat(sql, "1");
                } else if (cJSON_IsFalse(pjson)) {
                    strcat(sql, "0");
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

    logger("DB", LOG_LEVEL_DEBUG, "Executing General Table INSERT: %s", sql);
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to insert into general table: %s", PQerrorMessage(conn));
        logger("DB", LOG_LEVEL_ERROR, "Failed SQL was: %s", sql);
        PQclear(res);
        free(sql);
        PQexec(conn, "ROLLBACK");
        return -1;
    }
    PQclear(res);
    logger("DB", LOG_LEVEL_DEBUG, "General table INSERT successful");

    // Insert into specific table
    cJSON *specific_attr = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(ty));
    if (!specific_attr) {
        logger("DB", LOG_LEVEL_ERROR, "No specific attributes found for resource type %d", ty);
        free(sql);
        PQexec(conn, "ROLLBACK");
        return -1;
    }
    
    sql[0] = '\0';
    sprintf(sql, "INSERT INTO %s (id, ", get_table_name(ty));
    
    for (int i = 0; i < cJSON_GetArraySize(specific_attr); i++) {
        strcat(sql, cJSON_GetArrayItem(specific_attr, i)->string);
        if (i < cJSON_GetArraySize(specific_attr) - 1) strcat(sql, ",");
    }
    strcat(sql, ") VALUES ((SELECT id FROM general WHERE ri='");
    
    cJSON *ri_obj = cJSON_GetObjectItem(obj, "ri");
    if (!ri_obj || !cJSON_IsString(ri_obj)) {
        logger("DB", LOG_LEVEL_ERROR, "Missing or invalid 'ri' field in resource object");
        free(sql);
        PQexec(conn, "ROLLBACK");
        return -1;
    }
    
    char *ri_str = ri_obj->valuestring;
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
            } else if (cJSON_IsTrue(pjson)) {
                strcat(sql, "1");
            } else if (cJSON_IsFalse(pjson)) {
                strcat(sql, "0");
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

    logger("DB", LOG_LEVEL_DEBUG, "Executing Specific Table INSERT: %s", sql);
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to insert into %s table: %s", get_table_name(ty), PQerrorMessage(conn));
        logger("DB", LOG_LEVEL_ERROR, "Failed SQL was: %s", sql);
        PQclear(res);
        free(sql);
        PQexec(conn, "ROLLBACK");
        return -1;
    }
    PQclear(res);
    logger("DB", LOG_LEVEL_DEBUG, "Specific table INSERT successful");

    // Commit transaction
    logger("DB", LOG_LEVEL_DEBUG, "Executing: COMMIT");
    res = PQexec(conn, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to commit transaction: %s", PQerrorMessage(conn));
        PQclear(res);
        free(sql);
        return -1;
    }
    PQclear(res);
    logger("DB", LOG_LEVEL_DEBUG, "Transaction committed successfully");

    free(sql);
    logger("DB", LOG_LEVEL_DEBUG, "db_store_resource completed successfully");
    return 1;
}

int db_update_resource(cJSON *obj, char *ri, ResourceType ty)
{
    logger("DB", LOG_LEVEL_DEBUG, "Call db_update_resource [RI]: %s", ri);
    char *sql = NULL;
    cJSON *pjson = NULL;
    cJSON *GENERAL_ATTR = cJSON_GetObjectItem(ATTRIBUTES, "general");
    int general_cnt = cJSON_GetArraySize(GENERAL_ATTR);
    PGresult *res;
    PGconn *conn = get_pg_conn();
    if (!conn) {
        return 0;
    }

    // Begin transaction
    res = PQexec(conn, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to begin transaction: %s", PQerrorMessage(conn));
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
            } else if (cJSON_IsTrue(pjson)) {
                strcat(sql, "1");
            } else if (cJSON_IsFalse(pjson)) {
                strcat(sql, "0");
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

        res = PQexec(conn, sql);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            logger("DB", LOG_LEVEL_ERROR, "Failed to update general table: %s", PQerrorMessage(conn));
            PQclear(res);
            free(sql);
            PQexec(conn, "ROLLBACK");
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
            } else if (cJSON_IsTrue(pjson)) {
                strcat(sql, "1");
            } else if (cJSON_IsFalse(pjson)) {
                strcat(sql, "0");
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

        res = PQexec(conn, sql);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            logger("DB", LOG_LEVEL_ERROR, "Failed to update %s table: %s", get_table_name(ty), PQerrorMessage(conn));
            PQclear(res);
            free(sql);
            PQexec(conn, "ROLLBACK");
            return 0;
        }
        PQclear(res);
    }

    // Commit transaction
    res = PQexec(conn, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to commit transaction: %s", PQerrorMessage(conn));
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
    PGconn *conn = get_pg_conn();
    if (!conn) {
        return 0;
    }
    
    if (!ri) {
        return 0;
    }

    char *escaped_uri = pg_escape_string_value(get_uri_rtnode(rtnode));
    char *escaped_ri = pg_escape_string_value(ri);
    
    sprintf(sql, "DELETE FROM general WHERE uri LIKE '%s/%%' OR ri='%s';", escaped_uri, escaped_ri);
    logger("DB", LOG_LEVEL_DEBUG, "SQL : %s", sql);
    
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource: %s", PQerrorMessage(conn));
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
    char sql[1024] = {0};
    PGresult *res;
    PGconn *conn = get_pg_conn();
    if (!conn) {
        return -1;
    }
    char *latest_ri = NULL;
    int latest_cs = 0;

    if (!cnt) {
        logger("DB", LOG_LEVEL_ERROR, "CNT node is NULL");
        return -1;
    }

    char *cnt_ri = get_ri_rtnode(cnt);
    if (!cnt_ri) {
        logger("DB", LOG_LEVEL_ERROR, "Cannot get RI from CNT node");
        return -1;
    }

    char *escaped_cnt_ri = pg_escape_string_value(cnt_ri);
    
    // Find the oldest CIN (lowest lt value) under this CNT
    sprintf(sql, "SELECT general.ri, cin.cs FROM general, cin WHERE general.pi='%s' AND general.id = cin.id ORDER BY general.lt ASC LIMIT 1;", escaped_cnt_ri);
    
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to select oldest CIN: %s", PQerrorMessage(conn));
        PQclear(res);
        free(escaped_cnt_ri);
        return -1;
    }

    if (PQntuples(res) == 0) {
        logger("DB", LOG_LEVEL_DEBUG, "No CIN found to delete under CNT: %s", cnt_ri);
        PQclear(res);
        free(escaped_cnt_ri);
        return 0;
    }

    // Get the RI and CS of the oldest CIN
    latest_ri = PQgetvalue(res, 0, 0);
    latest_cs = atoi(PQgetvalue(res, 0, 1));

    logger("DB", LOG_LEVEL_DEBUG, "latest_ri: %s, latest_cs: %d", latest_ri, latest_cs);

    // Create a copy of latest_ri since PQclear will free the result
    char *ri_copy = strdup(latest_ri);
    PQclear(res);

    // Delete the oldest CIN resource from general table (cascade will handle cin table)
    char *escaped_ri = pg_escape_string_value(ri_copy);
    sprintf(sql, "DELETE FROM general WHERE ri='%s';", escaped_ri);
    
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource from general: %s", PQerrorMessage(conn));
        PQclear(res);
        free(escaped_cnt_ri);
        free(escaped_ri);
        free(ri_copy);
        return -1;
    }
    PQclear(res);

    free(escaped_cnt_ri);
    free(escaped_ri);
    free(ri_copy);
    
    return latest_cs;
}

RTNode *db_get_all_resource_as_rtnode()
{
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_resource_as_rtnode");

    char sql[1024] = {0};
    PGresult *res, *res2;
    PGconn *conn = get_pg_conn();
    if (!conn) {
        return NULL;
    }
    RTNode *head = NULL, *rtnode = NULL;
    cJSON *json, *arr;

    // Select all resources except CIN (ty=4) and Sub (ty=23) 
    sprintf(sql, "SELECT * FROM general WHERE ty != 4 AND ty != 23;");
    
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed select from general: %s", PQerrorMessage(conn));
        PQclear(res);
        return NULL;
    }

    int rows = PQntuples(res);
    int cols = PQnfields(res);

    for (int row = 0; row < rows; row++) {
        json = cJSON_CreateObject();
        
        // Process general table columns
        for (int col = 0; col < cols; col++) {
            char *colname = PQfname(res, col);
            char *value = PQgetvalue(res, row, col);

            if (value && strlen(value) > 0) {
                // Try to parse as JSON first
                arr = cJSON_Parse(value);
                if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)) {
                    cJSON_AddItemToObject(json, colname, arr);
                } else {
                    // Check if it's a number
                    char *endptr;
                    long num = strtol(value, &endptr, 10);
                    if (*endptr == '\0') {
                        cJSON_AddNumberToObject(json, colname, num);
                    } else {
                        cJSON_AddItemToObject(json, colname, cJSON_CreateString(value));
                    }
                    cJSON_Delete(arr);
                }
            }
        }

        // Get resource type and fetch specific table data
        int ty = cJSON_GetObjectItem(json, "ty")->valueint;
        int id = cJSON_GetObjectItem(json, "id")->valueint;
        
        char sql2[1024] = {0};
        sprintf(sql2, "SELECT * FROM %s WHERE id=%d;", get_table_name(ty), id);
        
        res2 = PQexec(conn, sql2);
        if (PQresultStatus(res2) != PGRES_TUPLES_OK) {
            logger("DB", LOG_LEVEL_ERROR, "Failed select from %s: %s", get_table_name(ty), PQerrorMessage(conn));
            PQclear(res2);
            cJSON_Delete(json);
            continue;
        }

        if (PQntuples(res2) > 0) {
            int cols2 = PQnfields(res2);
            for (int col = 0; col < cols2; col++) {
                char *colname = PQfname(res2, col);
                char *value = PQgetvalue(res2, 0, col);

                if (strcmp(colname, "id") == 0) continue;
                
                if (value && strlen(value) > 0) {
                    // Try to parse as JSON first
                    arr = cJSON_Parse(value);
                    if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)) {
                        cJSON_AddItemToObject(json, colname, arr);
                    } else {
                        // Check if it's a number
                        char *endptr;
                        long num = strtol(value, &endptr, 10);
                        if (*endptr == '\0') {
                            cJSON_AddNumberToObject(json, colname, num);
                        } else {
                            cJSON_AddItemToObject(json, colname, cJSON_CreateString(value));
                        }
                        cJSON_Delete(arr);
                    }
                }
            }
        }
        PQclear(res2);

        // Remove id from JSON object
        cJSON_DeleteItemFromObject(json, "id");

        // Create RTNode and add to list
        if (!head) {
            head = create_rtnode(json, ty);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(json, ty);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }
    }

    PQclear(res);
    return head;
}

RTNode *db_get_cin_rtnode_list(RTNode *parent_rtnode)
{
    logger("DB", LOG_LEVEL_DEBUG, "call db_get_cin_rtnode_list");

    if (!parent_rtnode) {
        logger("DB", LOG_LEVEL_ERROR, "Parent rtnode is NULL");
        return NULL;
    }

    char *pi = get_ri_rtnode(parent_rtnode);
    if (!pi) {
        logger("DB", LOG_LEVEL_ERROR, "Cannot get RI from parent rtnode");
        return NULL;
    }

    char sql[1024] = {0};
    PGresult *res;
    PGconn *conn = get_pg_conn();
    if (!conn) {
        return NULL;
    }
    RTNode *head = NULL, *rtnode = NULL;
    cJSON *json, *arr;

    char *escaped_pi = pg_escape_string_value(pi);
    sprintf(sql, "SELECT * FROM general, cin WHERE general.pi='%s' AND general.id=cin.id ORDER BY general.id ASC;", escaped_pi);

    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed select: %s", PQerrorMessage(conn));
        PQclear(res);
        free(escaped_pi);
        return NULL;
    }

    int rows = PQntuples(res);
    int cols = PQnfields(res);

    for (int row = 0; row < rows; row++) {
        json = cJSON_CreateObject();
        
        for (int col = 0; col < cols; col++) {
            char *colname = PQfname(res, col);
            char *value = PQgetvalue(res, row, col);

            if (strcmp(colname, "id") == 0) continue;
            
            if (value && strlen(value) > 0) {
                // Try to parse as JSON first
                arr = cJSON_Parse(value);
                if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)) {
                    cJSON_AddItemToObject(json, colname, arr);
                } else {
                    // Check if it's a number
                    char *endptr;
                    long num = strtol(value, &endptr, 10);
                    if (*endptr == '\0') {
                        cJSON_AddNumberToObject(json, colname, num);
                    } else {
                        cJSON_AddItemToObject(json, colname, cJSON_CreateString(value));
                    }
                    cJSON_Delete(arr);
                }
            }
        }

        // Create RTNode and add to list
        if (!head) {
            head = create_rtnode(json, RT_CIN);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(json, RT_CIN);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }
    }

    PQclear(res);
    free(escaped_pi);
    return head;
}

RTNode *db_get_latest_cins()
{
    logger("DB", LOG_LEVEL_DEBUG, "call db_get_latest_cins");

    char sql[1024] = {0};
    PGresult *res;
    PGconn *conn = get_pg_conn();
    if (!conn) {
        return NULL;
    }
    RTNode *head = NULL, *rtnode = NULL;
    cJSON *json, *arr;

    // Get the latest CIN (highest id) for each CNT (grouped by pi)
    sprintf(sql, "SELECT * FROM general, cin WHERE general.id IN (SELECT max(id) FROM general WHERE ty=4 GROUP BY pi) AND general.id=cin.id;");
    
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed select: %s", PQerrorMessage(conn));
        PQclear(res);
        return NULL;
    }

    int rows = PQntuples(res);
    int cols = PQnfields(res);

    for (int row = 0; row < rows; row++) {
        json = cJSON_CreateObject();
        
        for (int col = 0; col < cols; col++) {
            char *colname = PQfname(res, col);
            char *value = PQgetvalue(res, row, col);

            if (strcmp(colname, "id") == 0) continue;
            
            if (value && strlen(value) > 0) {
                // Try to parse as JSON first
                arr = cJSON_Parse(value);
                if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)) {
                    cJSON_AddItemToObject(json, colname, arr);
                } else {
                    // Check if it's a number
                    char *endptr;
                    long num = strtol(value, &endptr, 10);
                    if (*endptr == '\0') {
                        cJSON_AddNumberToObject(json, colname, num);
                    } else {
                        cJSON_AddItemToObject(json, colname, cJSON_CreateString(value));
                    }
                    cJSON_Delete(arr);
                }
            }
        }

        // Create RTNode and add to list
        if (!head) {
            head = create_rtnode(json, RT_CIN);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(json, RT_CIN);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }
    }

    PQclear(res);
    return head;
}

cJSON *db_get_cin_laol(RTNode *parent_rtnode, int laol)
{
    if (!parent_rtnode) {
        logger("DB", LOG_LEVEL_ERROR, "Parent rtnode is NULL");
        return NULL;
    }

    char sql[1024] = {0};
    char *ord = NULL;
    PGresult *res;
    PGconn *conn = get_pg_conn();
    if (!conn) {
        return NULL;
    }
    cJSON *json, *arr;

    // Determine order (Latest: DESC, Oldest: ASC)
    switch (laol) {
    case 0:
        ord = "DESC";
        break;
    case 1:
        ord = "ASC";
        break;
    default:
        logger("DB", LOG_LEVEL_DEBUG, "Invalid la ol flag");
        return NULL;
    }

    char *parent_ri = get_ri_rtnode(parent_rtnode);
    if (!parent_ri) {
        logger("DB", LOG_LEVEL_ERROR, "Cannot get RI from parent rtnode");
        return NULL;
    }

    char *escaped_pi = pg_escape_string_value(parent_ri);
    sprintf(sql, "SELECT * FROM general, cin WHERE general.pi='%s' AND general.id = cin.id ORDER BY general.lt %s, general.id %s LIMIT 1;", escaped_pi, ord, ord);
    
    logger("DB", LOG_LEVEL_DEBUG, "SQL: %s", sql);
    
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed select: %s", PQerrorMessage(conn));
        PQclear(res);
        free(escaped_pi);
        return NULL;
    }

    if (PQntuples(res) == 0) {
        logger("DB", LOG_LEVEL_DEBUG, "No CIN found for parent: %s", parent_ri);
        PQclear(res);
        free(escaped_pi);
        return NULL;
    }

    json = cJSON_CreateObject();
    int cols = PQnfields(res);
    
    for (int col = 0; col < cols; col++) {
        char *colname = PQfname(res, col);
        char *value = PQgetvalue(res, 0, col);

        if (strcmp(colname, "id") == 0) continue;
        
        if (value && strlen(value) > 0) {
            // Try to parse as JSON first
            arr = cJSON_Parse(value);
            if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)) {
                cJSON_AddItemToObject(json, colname, arr);
            } else {
                // Check if it's a number
                char *endptr;
                long num = strtol(value, &endptr, 10);
                if (*endptr == '\0') {
                    cJSON_AddNumberToObject(json, colname, num);
                } else {
                    cJSON_AddItemToObject(json, colname, cJSON_CreateString(value));
                }
                cJSON_Delete(arr);
            }
        }
    }

    PQclear(res);
    free(escaped_pi);
    return json;
}

cJSON *db_get_filter_criteria(oneM2MPrimitive *o2pt)
{
    logger("DB", LOG_LEVEL_DEBUG, "call db_get_filter_criteria");
    
    if (!o2pt || !o2pt->fc) {
        logger("DB", LOG_LEVEL_ERROR, "Invalid parameters for filter criteria");
        return cJSON_CreateArray();
    }

    char sql[2048] = {0};
    char buf[256] = {0};
    PGresult *res;
    cJSON *fc = o2pt->fc;
    cJSON *pjson = NULL, *ptr = NULL;
    cJSON *json = cJSON_CreateArray();
    PGconn *conn = get_pg_conn();
    if (!conn) {
        return json;
    }
    int fo = cJSON_GetNumberValue(cJSON_GetObjectItem(fc, "fo"));

    char *uri = o2pt->to;
    if (strncmp(o2pt->to, CSE_BASE_NAME, strlen(CSE_BASE_NAME)) != 0) {
        uri = ri_to_uri(o2pt->to);
    }

    char *escaped_uri = pg_escape_string_value(uri);
    
    // Build base query
    if (o2pt->drt == DRT_STRUCTURED) {
        sprintf(sql, "SELECT rn, ty, uri FROM general WHERE uri LIKE '%s/%%'", escaped_uri);
    } else {
        sprintf(sql, "SELECT rn, ty, ri FROM general WHERE uri LIKE '%s/%%'", escaped_uri);
    }

    // Add level filter
    if ((pjson = cJSON_GetObjectItem(fc, "lvl"))) {
        sprintf(buf, " AND uri NOT LIKE '%s", escaped_uri);
        strcat(sql, buf);
        for (int i = 0; i < pjson->valueint; i++) {
            strcat(sql, "/%%");
        }
        strcat(sql, "'");
    }

    // Add creation time filters
    if ((pjson = cJSON_GetObjectItem(fc, "cra"))) {
        char *escaped_cra = pg_escape_string_value(pjson->valuestring);
        sprintf(buf, " AND ct > '%s'", escaped_cra);
        strcat(sql, buf);
        free(escaped_cra);
    }
    if ((pjson = cJSON_GetObjectItem(fc, "crb"))) {
        char *escaped_crb = pg_escape_string_value(pjson->valuestring);
        sprintf(buf, " AND ct <= '%s'", escaped_crb);
        strcat(sql, buf);
        free(escaped_crb);
    }

    // Add expiration time filters
    if ((pjson = cJSON_GetObjectItem(fc, "exa"))) {
        char *escaped_exa = pg_escape_string_value(pjson->valuestring);
        sprintf(buf, " AND et > '%s'", escaped_exa);
        strcat(sql, buf);
        free(escaped_exa);
    }
    if ((pjson = cJSON_GetObjectItem(fc, "exb"))) {
        char *escaped_exb = pg_escape_string_value(pjson->valuestring);
        sprintf(buf, " AND et <= '%s'", escaped_exb);
        strcat(sql, buf);
        free(escaped_exb);
    }

    // Add modification time filters
    if ((pjson = cJSON_GetObjectItem(fc, "ms"))) {
        char *escaped_ms = pg_escape_string_value(pjson->valuestring);
        sprintf(buf, " AND lt > '%s'", escaped_ms);
        strcat(sql, buf);
        free(escaped_ms);
    }
    if ((pjson = cJSON_GetObjectItem(fc, "us"))) {
        char *escaped_us = pg_escape_string_value(pjson->valuestring);
        sprintf(buf, " AND lt <= '%s'", escaped_us);
        strcat(sql, buf);
        free(escaped_us);
    }

    // Add type filter
    if ((pjson = cJSON_GetObjectItem(fc, "ty"))) {
        strcat(sql, " AND (");
        if (cJSON_IsArray(pjson)) {
            int first = 1;
            cJSON_ArrayForEach(ptr, pjson) {
                if (!first) strcat(sql, " OR ");
                sprintf(buf, "ty = %d", ptr->valueint);
                strcat(sql, buf);
                first = 0;
            }
        } else if (cJSON_IsNumber(pjson)) {
            sprintf(buf, "ty = %d", pjson->valueint);
            strcat(sql, buf);
        }
        strcat(sql, ")");
    }

    // Add label filter
    if ((pjson = cJSON_GetObjectItem(fc, "lbl"))) {
        strcat(sql, " AND (");
        if (cJSON_IsArray(pjson)) {
            int first = 1;
            cJSON_ArrayForEach(ptr, pjson) {
                if (!first) strcat(sql, " OR ");
                char *escaped_lbl = pg_escape_string_value(cJSON_GetStringValue(ptr));
                sprintf(buf, "lbl LIKE '%%\"%s\"%%'", escaped_lbl);
                strcat(sql, buf);
                free(escaped_lbl);
                first = 0;
            }
        } else if (cJSON_IsString(pjson)) {
            char *escaped_lbl = pg_escape_string_value(cJSON_GetStringValue(pjson));
            sprintf(buf, "lbl LIKE '%%\"%s\"%%'", escaped_lbl);
            strcat(sql, buf);
            free(escaped_lbl);
        }
        strcat(sql, ")");
    }

    // Add ordering and limit
    if (DEFAULT_DISCOVERY_SORT == SORT_DESC) {
        strcat(sql, " ORDER BY id DESC");
    } else {
        strcat(sql, " ORDER BY id ASC");
    }

    if ((pjson = cJSON_GetObjectItem(fc, "lim"))) {
        int limit = pjson->valueint > DEFAULT_DISCOVERY_LIMIT ? DEFAULT_DISCOVERY_LIMIT + 1 : pjson->valueint + 1;
        sprintf(buf, " LIMIT %d", limit);
        strcat(sql, buf);
    } else {
        sprintf(buf, " LIMIT %d", DEFAULT_DISCOVERY_LIMIT + 1);
        strcat(sql, buf);
    }

    if ((pjson = cJSON_GetObjectItem(fc, "ofst"))) {
        sprintf(buf, " OFFSET %d", pjson->valueint);
        strcat(sql, buf);
    }

    strcat(sql, ";");
    logger("DB", LOG_LEVEL_DEBUG, "Filter SQL: %s", sql);
    
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed filter query: %s", PQerrorMessage(conn));
        PQclear(res);
        free(escaped_uri);
        return json;
    }

    int rows = PQntuples(res);
    for (int row = 0; row < rows; row++) {
        cJSON *pjson = cJSON_CreateObject();
        char *rn = PQgetvalue(res, row, 0);
        char *ty_str = PQgetvalue(res, row, 1);
        char *val = PQgetvalue(res, row, 2);

        if (rn && strlen(rn) > 0) {
            cJSON_AddItemToObject(pjson, "name", cJSON_CreateString(rn));
            cJSON_AddItemToObject(pjson, "type", cJSON_CreateNumber(atoi(ty_str)));
            cJSON_AddItemToObject(pjson, "val", cJSON_CreateString(val));
            cJSON_AddItemToArray(json, pjson);
        } else {
            cJSON_Delete(pjson);
            break;
        }
    }

    PQclear(res);
    free(escaped_uri);
    return json;
}

bool db_check_cin_rn_dup(char *rn, char *pi)
{
    if (!rn || !pi) return false;
    
    char sql[1024] = {0};
    PGresult *res;
    PGconn *conn = get_pg_conn();
    if (!conn) {
        return false;
    }
    
    char *escaped_rn = pg_escape_string_value(rn);
    char *escaped_pi = pg_escape_string_value(pi);
    
    sprintf(sql, "SELECT * FROM general WHERE rn='%s' AND pi='%s';", escaped_rn, escaped_pi);
    
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Query failed: %s", PQerrorMessage(conn));
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

bool do_uri_exist(cJSON *list, char *uri)
{
    char *ptr = NULL;
    int lsize = cJSON_GetArraySize(list);
    for (int i = 0; i < lsize; i++)
    {
        ptr = cJSON_GetArrayItem(list, i)->valuestring;
        if (!strcmp(ptr, uri))
        {
            return true;
        }
    }
    return false;
}

cJSON *getForbiddenUri(cJSON *acp_list)
{
    cJSON *result = cJSON_CreateArray();
    
    if (!acp_list || cJSON_GetArraySize(acp_list) == 0) {
        return result;
    }

    char sql[2048] = {0};
    PGresult *res;
    PGconn *conn = get_pg_conn();
    if (!conn) {
        return result;
    }
    cJSON *ptr = NULL;

    strcat(sql, "SELECT uri FROM general WHERE ");
    
    int first = 1;
    cJSON_ArrayForEach(ptr, acp_list) {
        if (cJSON_IsString(ptr)) {
            if (!first) {
                strcat(sql, " OR ");
            }
            
            char *escaped_acp = pg_escape_string_value(cJSON_GetStringValue(ptr));
            char condition[512];
            sprintf(condition, "acpi LIKE '%%\"%s\"%%'", escaped_acp);
            strcat(sql, condition);
            free(escaped_acp);
            first = 0;
        }
    }
    strcat(sql, ";");
    
    logger("DB", LOG_LEVEL_DEBUG, "getForbiddenUri SQL: %s", sql);
    
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed select in getForbiddenUri: %s", PQerrorMessage(conn));
        PQclear(res);
        return result;
    }

    int rows = PQntuples(res);
    for (int row = 0; row < rows; row++) {
        char *uri = PQgetvalue(res, row, 0);
        if (uri && strlen(uri) > 0) {
            cJSON_AddItemToArray(result, cJSON_CreateString(uri));
        }
    }

    PQclear(res);
    return result;
}
