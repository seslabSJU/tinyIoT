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

#include "sqlite/sqlite3.h"
sqlite3 *db;

static pthread_mutex_t sqlite_tx_lock;
static pthread_mutexattr_t sqlite_tx_lock_attr;
static int sqlite_lock_initialized = 0;
static int sqlite_tx_depth = 0;
static pthread_t sqlite_tx_owner;
static int sqlite_tx_owner_valid = 0;

static void sqlite_lock(void)
{
    if (sqlite_lock_initialized)
        pthread_mutex_lock(&sqlite_tx_lock);
}

static void sqlite_unlock(void)
{
    if (sqlite_lock_initialized)
        pthread_mutex_unlock(&sqlite_tx_lock);
}

int db_begin_tx()
{
    if (sqlite_tx_owner_valid && pthread_equal(sqlite_tx_owner, pthread_self())) {
        sqlite_tx_depth++;
        return 1;
    }

    sqlite_lock();
    if (sqlite_tx_depth != 0) {
        logger("DB", LOG_LEVEL_ERROR, "Transaction owner mismatch");
        sqlite_unlock();
        return 0;
    }

    int rc = sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to begin transaction: %s", sqlite3_errmsg(db));
        sqlite_unlock();
        return 0;
    }
    sqlite_tx_owner = pthread_self();
    sqlite_tx_owner_valid = 1;
    sqlite_tx_depth = 1;
    return 1;
}

int db_commit_tx()
{
    if (!sqlite_tx_owner_valid || !pthread_equal(sqlite_tx_owner, pthread_self())) {
        logger("DB", LOG_LEVEL_ERROR, "Commit without transaction ownership");
        return 0;
    }
    if (sqlite_tx_depth == 0) {
        return 0;
    }
    sqlite_tx_depth--;
    if (sqlite_tx_depth == 0) {
        int rc = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            logger("DB", LOG_LEVEL_ERROR, "Failed to commit transaction: %s", sqlite3_errmsg(db));
            return 0;
        }
        sqlite_tx_owner_valid = 0;
        sqlite_unlock();
    }
    return 1;
}

int db_rollback_tx()
{
    if (!sqlite_tx_owner_valid || !pthread_equal(sqlite_tx_owner, pthread_self())) {
        logger("DB", LOG_LEVEL_ERROR, "Rollback without transaction ownership");
        return 0;
    }
    if (sqlite_tx_depth == 0) {
        sqlite_tx_owner_valid = 0;
        sqlite_unlock();
        return 0;
    }
    sqlite_tx_depth = 0;
    int rc = sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to rollback transaction: %s", sqlite3_errmsg(db));
        return 0;
    }
    sqlite_tx_owner_valid = 0;
    sqlite_unlock();
    return 1;
}

extern cJSON *ATTRIBUTES;
extern ResourceTree *rt;

/* Table definition structure */
typedef struct {
    const char *name;
    const char *sql;
} table_def_t;

#define ID_COLUMN_TYPE "INTEGER PRIMARY KEY AUTOINCREMENT"

/* Static table definitions */
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
     "spi VARCHAR(45), "
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
     "spi VARCHAR(45), "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"acpA",
     "CREATE TABLE IF NOT EXISTS acpA ( id INTEGER, "
     "lnk VARCHAR(100), pv VARCHAR(255), pvs VARCHAR(100), ast INT, "
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
    {"grpA",
     "CREATE TABLE IF NOT EXISTS grpA ( id INTEGER, "
     "lnk VARCHAR(100), cr VARCHAR(45), mt INT, cnm INT, mnm INT, mid TEXT, macp TEXT, mtv INT, csy INT, gn VARCHAR(30), at VARCHAR(200), aa VARCHAR(100), ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"cinA",
     "CREATE TABLE IF NOT EXISTS cinA ( id INTEGER, "
     "lnk VARCHAR(100), cs INT, cr VARCHAR(45), cnf VARCHAR(45), st VARCHAR(45), con TEXT, ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"tsA",
     "CREATE TABLE IF NOT EXISTS tsA ( id INTEGER, "
     "lnk VARCHAR(100), cr VARCHAR(45), mni INT, mbs INT, mia INT, cni INT, cbs INT, "
     "pei INT, peid INT, mdd INT, mdn INT, mdt INT, cnf VARCHAR(45), ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"fcin",
     "CREATE TABLE IF NOT EXISTS fcin ( id INTEGER, "
     "cs INT, st INT, org VARCHAR(200), loc TEXT, at VARCHAR(200), aa VARCHAR(100), ast INT, custom_attrs TEXT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"fcnt",
     "CREATE TABLE IF NOT EXISTS fcnt ( id INTEGER, "
     "cnd VARCHAR(200), oref VARCHAR(100), nl VARCHAR(45), cr VARCHAR(40), at VARCHAR(200), aa VARCHAR(100), ast INT, "
     "st INT, cs INT, fcied INT, mni INT, mbs INT, mia INT, cni INT, cbs INT, "
     "custom_attrs TEXT, loc TEXT, daci VARCHAR(200), "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"ts",
     "CREATE TABLE IF NOT EXISTS ts ( id INTEGER, "
     "cr VARCHAR(45), mni INT, mbs INT, mia INT, cni INT, cbs INT, "
     "pei INT, peid INT, "
     "mdd INT, mdn INT, mdt INT, mdc INT, mdlt TEXT, cnf VARCHAR(45),"
     "at VARCHAR(200), aa VARCHAR(100), ast INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
    {"tsi",
     "CREATE TABLE IF NOT EXISTS tsi ( id INTEGER, "
     "dgt TEXT, con TEXT, snr INT, cs INT, vrq INT, "
     "CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );"
    },
};

#define TABLE_COUNT (sizeof(table_definitions) / sizeof(table_def_t))

/* Helper function to execute SQL and handle errors */
static int execute_sql_with_error_handling(const char *sql, const char *context, char **err_msg)
{
    int rc = sqlite3_exec(db, sql, NULL, NULL, err_msg);
    if (rc != SQLITE_OK) {
        logger("DB", LOG_LEVEL_ERROR, "%s error: %s", context, *err_msg ? *err_msg : "Unknown error");
        return 0;
    }
    return 1;
}

/* Helper function to create a single table */
static int create_table(const table_def_t *table_def, char **err_msg)
{
    if (!execute_sql_with_error_handling(table_def->sql, table_def->name, err_msg)) {
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[%s]: %s", table_def->name, *err_msg ? *err_msg : "Unknown error");
        return 0;
    }
    return 1;
}

/* DB init */
int init_dbp()
{
    char *err_msg = NULL;
    int rc = 0;

    if (!sqlite_lock_initialized) {
        pthread_mutexattr_init(&sqlite_tx_lock_attr);
        pthread_mutexattr_settype(&sqlite_tx_lock_attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&sqlite_tx_lock, &sqlite_tx_lock_attr);
        sqlite_lock_initialized = 1;
    }

    // Open (or create) DB File
    rc = sqlite3_open("data.db", &db);
    if (rc != SQLITE_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Cannot open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    // Begin transaction for atomic table creation
    if (!execute_sql_with_error_handling("BEGIN TRANSACTION;", "Begin Transaction", &err_msg)) {
        sqlite3_close(db);
        return 0;
    }

    // Enable foreign keys
    if (!execute_sql_with_error_handling("PRAGMA foreign_keys = ON;", "foreign_keys", &err_msg)) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        sqlite3_close(db);
        return 0;
    }

    // Create all tables
    for (size_t i = 0; i < TABLE_COUNT; i++) {
        if (!create_table(&table_definitions[i], &err_msg)) {
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            sqlite3_close(db);
            return 0;
        }
    }

    // Commit transaction
    if (!execute_sql_with_error_handling("COMMIT;", "Commit Transaction", &err_msg)) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        sqlite3_close(db);
        return 0;
    }

    logger("DB", LOG_LEVEL_INFO, "Database initialized successfully with %zu tables", TABLE_COUNT);
    return 1;
}

int close_dbp()
{
    if (db) {
        sqlite3_close_v2(db);
        db = NULL;
    }
    return 1;
}

#ifdef UPPERTESTER
int db_reset_all()
{
    char *err_msg = NULL;
    int rc;

    sqlite_lock();
    // FK cascade removes rows from every child table.
    rc = sqlite3_exec(db,
                      "PRAGMA foreign_keys = ON; DELETE FROM general;",
                      NULL, NULL, &err_msg);
    sqlite_unlock();

    if (rc != SQLITE_OK) {
        logger("DB", LOG_LEVEL_ERROR, "db_reset_all failed: %s",
               err_msg ? err_msg : "unknown error");
        sqlite3_free(err_msg);
        return 0;
    }
    logger("DB", LOG_LEVEL_INFO, "db_reset_all: all resources deleted");
    return 1;
}
#endif

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
    case RT_TS:
        tableName = "ts";
        break;
    case RT_TSI:
        tableName = "tsi";
        break;
    case RT_CSR:
        tableName = "csr";
        break;
    case RT_ACPA:
        tableName = "acpA";
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
    case RT_GRPA:
        tableName = "grpA";
        break;
    case RT_TSA:
        tableName = "tsA";
        break;
    case RT_CINA:
        tableName = "cinA";
        break;
    case RT_FCIN:
        tableName = "fcin";
        break;
    case RT_FCNT:
        tableName = "fcnt";
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
    char *sql = NULL;
    cJSON *resource = NULL;

    sql = malloc(1024);

    sprintf(sql, "SELECT * FROM general, %s WHERE general.uri='%s' and %s.id=general.id;", get_table_name(ty), uri, get_table_name(ty));

    sqlite3_stmt *stmt;
    int rc = 0;
    char *err_msg = NULL;

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        free(sql);
        logger("DB", LOG_LEVEL_ERROR, "prepare error");
        return NULL;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        free(sql);
        sqlite3_finalize(stmt);
        return NULL;
    }

    resource = cJSON_CreateObject();
    int cols = sqlite3_column_count(stmt);
    const char *colname = NULL;
    int bytes = 0;
    int coltype = 0;
    char buf[256] = {0};
    cJSON *arr = NULL;

    for (int col = 0; col < cols; col++)
    {

        colname = sqlite3_column_name(stmt, col);
        bytes = sqlite3_column_bytes(stmt, col);
        coltype = sqlite3_column_type(stmt, col);

        if (strcmp(colname, "id") == 0)
            continue;
        switch (coltype)
        {
        case SQLITE_TEXT:
            memset(buf, 0, 256);
            strncpy(buf, sqlite3_column_text(stmt, col), bytes);
            arr = cJSON_Parse(buf);
            if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object))
            {
                cJSON_AddItemToObject(resource, colname, arr);
            }
            else
            {
                cJSON_AddItemToObject(resource, colname, cJSON_CreateString(buf));
                cJSON_Delete(arr);
            }
            break;
        case SQLITE_INTEGER:
            if (strcmp(colname, "fcied") == 0)
                cJSON_AddBoolToObject(resource, colname, sqlite3_column_int(stmt, col));
            else
                cJSON_AddNumberToObject(resource, colname, sqlite3_column_int(stmt, col));
            break;
        }
    }

    sqlite3_finalize(stmt);
    free(sql);

    // For FlexContainer/FlexContainerInstance, merge custom_attrs into the main resource object
    if (ty == RT_FCNT || ty == RT_FCIN) {
        cJSON *custom_attrs = cJSON_GetObjectItem(resource, "custom_attrs");
        if (custom_attrs && cJSON_IsObject(custom_attrs)) {
            cJSON *item = NULL;
            cJSON_ArrayForEach(item, custom_attrs) {
                if (item->string) {
                    cJSON_AddItemToObject(resource, item->string, cJSON_Duplicate(item, 1));
                }
            }
            cJSON_DeleteItemFromObject(resource, "custom_attrs");
        }
    }

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
    char *sql = NULL;
    cJSON *resource = NULL;

    sql = malloc(1024);

    sprintf(sql, "SELECT * FROM general, %s WHERE general.id=%s.id AND general.ri='%s';", get_table_name(ty), get_table_name(ty), ri);

    sqlite3_stmt *stmt;
    int rc = 0;
    char *err_msg = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        free(sql);
        logger("DB", LOG_LEVEL_ERROR, "prepare error get_resource");
        return NULL;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        free(sql);
        sqlite3_finalize(stmt);
        return NULL;
    }

    resource = cJSON_CreateObject();
    int cols = sqlite3_column_count(stmt);
    const char *colname = NULL;
    int bytes = 0;
    int coltype = 0;
    char buf[256] = {0};
    cJSON *arr = NULL;

    for (int col = 0; col < cols; col++)
    {

        colname = sqlite3_column_name(stmt, col);
        bytes = sqlite3_column_bytes(stmt, col);
        coltype = sqlite3_column_type(stmt, col);

        if (strcmp(colname, "id") == 0)
            continue;
        switch (coltype)
        {
        case SQLITE_TEXT:
            memset(buf, 0, 256);
            strncpy(buf, sqlite3_column_text(stmt, col), bytes);
            arr = cJSON_Parse(buf);
            if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object))
            {
                cJSON_AddItemToObject(resource, colname, arr);
            }
            else
            {
                cJSON_AddItemToObject(resource, colname, cJSON_CreateString(buf));
                cJSON_Delete(arr);
            }
            break;
        case SQLITE_INTEGER:
            if (strcmp(colname, "fcied") == 0)
                cJSON_AddBoolToObject(resource, colname, sqlite3_column_int(stmt, col));
            else
                cJSON_AddNumberToObject(resource, colname, sqlite3_column_int(stmt, col));
            break;
        }
    }

    sqlite3_finalize(stmt);
    free(sql);

    // For FlexContainer/FlexContainerInstance, merge custom_attrs into the main resource object
    if (ty == RT_FCNT || ty == RT_FCIN) {
        cJSON *custom_attrs = cJSON_GetObjectItem(resource, "custom_attrs");
        if (custom_attrs && cJSON_IsObject(custom_attrs)) {
            cJSON *item = NULL;
            cJSON_ArrayForEach(item, custom_attrs) {
                if (item->string) {
                    cJSON_AddItemToObject(resource, item->string, cJSON_Duplicate(item, 1));
                }
            }
            cJSON_DeleteItemFromObject(resource, "custom_attrs");
        }
    }

    return resource;
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
    
    sqlite_lock();
    char *sql = NULL, *err_msg = NULL;
    cJSON *pjson = NULL;
    cJSON *GENERAL_ATTR = cJSON_GetObjectItem(ATTRIBUTES, "general");
    int general_cnt = cJSON_GetArraySize(GENERAL_ATTR);

    if (!obj) {
        logger("DB", LOG_LEVEL_ERROR, "Resource object is NULL");
        sqlite_unlock();
        return -1;
    }

    cJSON *ty_obj = cJSON_GetObjectItem(obj, "ty");
    if (!ty_obj) {
        logger("DB", LOG_LEVEL_ERROR, "Missing 'ty' field in resource object");
        sqlite_unlock();
        return -1;
    }
    ResourceType ty = ty_obj->valueint;
    logger("DB", LOG_LEVEL_DEBUG, "Resource type: %d", ty);

    sql = malloc(1024);
    sprintf(sql, "INSERT INTO general (");
    for (int i = 0; i < general_cnt; i++)
    {
        strcat(sql, cJSON_GetArrayItem(GENERAL_ATTR, i)->string);
        strcat(sql, ",");
    }

    sql[strlen(sql) - 1] = ')';
    strcat(sql, " VALUES(");

    for (int i = 0; i < general_cnt; i++)
    {
        strcat(sql, "?,");
    }

    sql[strlen(sql) - 1] = ')';
    strcat(sql, ";");

    logger("DB", LOG_LEVEL_DEBUG, "Executing General Table INSERT: %s", sql);
    
    sqlite3_stmt *stmt;
    int rc = 0;
    int need_tx = (sqlite_tx_depth == 0);
    if (need_tx) {
        logger("DB", LOG_LEVEL_DEBUG, "Executing: BEGIN TRANSACTION");
        sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &err_msg);
    }

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_DEBUG, "Failed SQL was: %s", sql);
        free(sql);
        logger("DB", LOG_LEVEL_ERROR, "prepare error store_resource");
        if (need_tx) sqlite3_exec(db, "ROLLBACK;", NULL, NULL, &err_msg);
        sqlite3_finalize(stmt);
        sqlite_unlock();
        return -1;
    }

    for (int i = 0; i < general_cnt; i++)
    {
        if (strcmp(cJSON_GetArrayItem(GENERAL_ATTR, i)->string, "uri") == 0)
        {
            sqlite3_bind_text(stmt, i + 1, uri, strlen(uri), SQLITE_STATIC);
            continue;
        }
        pjson = cJSON_GetObjectItem(obj, cJSON_GetArrayItem(GENERAL_ATTR, i)->string);
        if (pjson)
        {
            db_test_and_bind_value(stmt, i + 1, pjson);
        }
        else
        {
            sqlite3_bind_null(stmt, i + 1);
        }
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert SQL: %s, msg : %s", sql, err_msg ? err_msg : "NULL");
        free(sql);
        sqlite3_finalize(stmt);
        if (need_tx) sqlite3_exec(db, "ROLLBACK;", NULL, NULL, &err_msg);
        sqlite_unlock();
        return -1;
    }

    sqlite3_finalize(stmt);
    logger("DB", LOG_LEVEL_DEBUG, "General table INSERT successful");

    cJSON *specific_attr = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(ty));
    if (!specific_attr) {
        logger("DB", LOG_LEVEL_ERROR, "No specific attributes found for resource type %d", ty);
        free(sql);
        if (need_tx) sqlite3_exec(db, "ROLLBACK;", NULL, NULL, &err_msg);
        sqlite_unlock();
        return -1;
    }

    sql[0] = '\0';
    sprintf(sql, "INSERT INTO %s (id, ", get_table_name(ty));
    for (int i = 0; i < cJSON_GetArraySize(specific_attr); i++)
    {
        strcat(sql, cJSON_GetArrayItem(specific_attr, i)->string);
        strcat(sql, ",");
    }

    sql[strlen(sql) - 1] = ')';
    strcat(sql, " VALUES( (SELECT id FROM general WHERE ri=?),");

    for (int i = 0; i < cJSON_GetArraySize(specific_attr); i++)
    {
        strcat(sql, "?,");
    }

    sql[strlen(sql) - 1] = ')';
    strcat(sql, ";");

    logger("DB", LOG_LEVEL_DEBUG, "Executing Specific Table INSERT: %s", sql);

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "prepare error %d", rc);
        logger("DB", LOG_LEVEL_ERROR, "Failed SQL was: %s", sql);
        free(sql);
        if (need_tx) sqlite3_exec(db, "ROLLBACK;", NULL, NULL, &err_msg);
        sqlite_unlock();
        return -1;
    }

    cJSON *ri_obj = cJSON_GetObjectItem(obj, "ri");
    if (!ri_obj || !cJSON_IsString(ri_obj)) {
        logger("DB", LOG_LEVEL_ERROR, "Missing or invalid 'ri' field in resource object");
        free(sql);
        if (need_tx) sqlite3_exec(db, "ROLLBACK;", NULL, NULL, &err_msg);
        sqlite_unlock();
        return -1;
    }

    db_test_and_bind_value(stmt, 1, ri_obj);

    for (int i = 0; i < cJSON_GetArraySize(specific_attr); i++)
    {
        pjson = cJSON_GetObjectItem(obj, cJSON_GetArrayItem(specific_attr, i)->string);
        if (pjson)
        {
            db_test_and_bind_value(stmt, i + 2, pjson);
        }
        else
        {
            sqlite3_bind_null(stmt, i + 2);
        }
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert SQL: %s, msg : %s", sql, err_msg ? err_msg : "NULL");
        free(sql);
        if (need_tx) sqlite3_exec(db, "ROLLBACK;", NULL, NULL, &err_msg);
        sqlite3_finalize(stmt);
        sqlite_unlock();
        return -1;
    }
    logger("DB", LOG_LEVEL_DEBUG, "Specific table INSERT successful");

    if (need_tx) {
        logger("DB", LOG_LEVEL_DEBUG, "Executing: END TRANSACTION");
        sqlite3_exec(db, "END TRANSACTION", NULL, NULL, &err_msg);
        if (err_msg)
            logger("DB", LOG_LEVEL_ERROR, "Error: %s", err_msg);
        else
            logger("DB", LOG_LEVEL_DEBUG, "Transaction committed successfully");
    }

    sqlite3_finalize(stmt);
    free(sql);
    sqlite_unlock();
    logger("DB", LOG_LEVEL_DEBUG, "db_store_resource completed successfully");
    return 1;
}

int db_update_resource(cJSON *obj, char *ri, ResourceType ty)
{
    sqlite_lock();
    logger("DB", LOG_LEVEL_DEBUG, "Call db_update_resource [RI]: %s", ri);
    char *sql = NULL;
    cJSON *pjson = NULL;
    cJSON *GENERAL_ATTR = cJSON_GetObjectItem(ATTRIBUTES, "general");
    int general_cnt = cJSON_GetArraySize(GENERAL_ATTR);
    char *err_msg = NULL;

    int need_tx = (sqlite_tx_depth == 0);
    if (need_tx) {
        int rc2 = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &err_msg);
        if (rc2 != SQLITE_OK)
        {
            logger("DB", LOG_LEVEL_ERROR, "Failed Begin Transaction: %s", err_msg);
            sqlite_unlock();
            return 0;
        }
    }
    int rc = 0;
    int idx = 0;
    sql = malloc(1024);
    sprintf(sql, "UPDATE general SET ");
    for (int i = 0; i < general_cnt; i++)
    {
        char *attr = cJSON_GetArrayItem(GENERAL_ATTR, i)->string;
        if (cJSON_GetObjectItem(obj, attr))
        {
            strcat(sql, attr);
            strcat(sql, "=?,");
            idx++;
        }
    }

    sql[strlen(sql) - 1] = '\0';
    strcat(sql, " WHERE ri=?;");

    sqlite3_stmt *stmt;
    rc = 0;

    if (idx > 0)
    {
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            free(sql);
            logger("DB", LOG_LEVEL_ERROR, "prepare error");
            if (need_tx) sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
            sqlite_unlock();
            return 0;
        }
        int idx = 1;
        for (int i = 0; i < general_cnt; i++)
        {
            pjson = cJSON_GetObjectItem(obj, cJSON_GetArrayItem(GENERAL_ATTR, i)->string);
            if (pjson)
            {
                db_test_and_bind_value(stmt, idx, pjson);
                idx++;
            }
        }
        sqlite3_bind_text(stmt, idx, ri, -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            free(sql);
            logger("DB", LOG_LEVEL_ERROR, "Failed Update SQL: %s, msg : %s", sql, err_msg);
            if (need_tx) sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
            sqlite_unlock();
            return 0;
        }

        sqlite3_finalize(stmt);
    }

    cJSON *specific_attr = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(ty));

    sql[0] = '\0';
    idx = 0;
    sprintf(sql, "UPDATE %s SET ", get_table_name(ty));
    for (int i = 0; i < cJSON_GetArraySize(specific_attr); i++)
    {
        char *attr = cJSON_GetArrayItem(specific_attr, i)->string;
        if (cJSON_GetObjectItem(obj, attr))
        {
            strcat(sql, attr);
            strcat(sql, "=?,");
            idx++;
        }
    }

    sql[strlen(sql) - 1] = '\0';
    strcat(sql, " WHERE id=(SELECT id FROM general WHERE ri=?);");
    if (idx > 0)
    {
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            logger("DB", LOG_LEVEL_ERROR, "prepare error");
            free(sql);
            sqlite3_finalize(stmt);
            if (need_tx) sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
            sqlite_unlock();
            return 0;
        }

        idx = 1;
        for (int i = 0; i < cJSON_GetArraySize(specific_attr); i++)
        {
            pjson = cJSON_GetObjectItem(obj, cJSON_GetArrayItem(specific_attr, i)->string);
            if (pjson)
            {
                db_test_and_bind_value(stmt, idx, pjson);
                idx++;
            }
        }
        sqlite3_bind_text(stmt, idx, ri, -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            logger("DB", LOG_LEVEL_ERROR, "Failed Insert SQL: %s, msg : %s", sql, err_msg);
            free(sql);
            if (need_tx) sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
            sqlite_unlock();
            return 0;
        }

        sqlite3_finalize(stmt);
    }

    if (need_tx) sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
    free(sql);
    sqlite_unlock();
    // Update uri of all child resources
    return 1;
}

void db_test_and_bind_value(sqlite3_stmt *stmt, int index, cJSON *obj)
{
    if (!obj)
        return;
    char *temp = NULL;
    switch (obj->type)
    {
    case cJSON_String:
        db_test_and_set_bind_text(stmt, index, obj->valuestring);
        break;
    case cJSON_Number:
        db_test_and_set_bind_int(stmt, index, obj->valueint);
        break;
    case cJSON_Array:
    case cJSON_Object:
        temp = cJSON_PrintUnformatted(obj);
        db_test_and_set_bind_text(stmt, index, temp);
        free(temp);
        break;
    case cJSON_True:
        db_test_and_set_bind_int(stmt, index, 1);
        break;
    case cJSON_False:
        db_test_and_set_bind_int(stmt, index, 0);
        break;
    case cJSON_NULL:
        sqlite3_bind_null(stmt, index);
        break;
    }
}

void db_test_and_set_bind_text(sqlite3_stmt *stmt, int index, char *context)
{
    if (context)
        sqlite3_bind_text(stmt, index, context, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, index);
}

void db_test_and_set_bind_int(sqlite3_stmt *stmt, int index, int value)
{
    if (value >= 0)
        sqlite3_bind_int(stmt, index, value);
    else
        sqlite3_bind_null(stmt, index);
}

int db_delete_onem2m_resource(RTNode *rtnode)
{
    char *ri = get_ri_rtnode(rtnode);
    logger("DB", LOG_LEVEL_DEBUG, "Delete [RI] %s", ri);
    char sql[1024] = {0};
    char *err_msg;
    char *tableName = NULL;
    int rc;
    if (!ri)
    {
        return 0;
    }
    sqlite_lock();

    // sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &err_msg);
    sprintf(sql, "DELETE FROM general WHERE uri LIKE '%s/%%' OR ri='%s';", get_uri_rtnode(rtnode), ri);
    logger("DB", LOG_LEVEL_DEBUG, "SQL : %s", sql);
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource from %s/ msg : %s", tableName, err_msg);
        sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
        sqlite_unlock();
        return 0;
    }
    sqlite_unlock();

    return 1;
}

int db_delete_one_cin_mni(RTNode *cnt)
{
    char sql[1024] = {0};
    char *err_msg;
    sqlite3_stmt *res;
    int rc;

    const char *latest_ri = NULL;
    int latest_cs = 0;

    sprintf(sql, "SELECT general.ri, cin.cs from general, cin WHERE pi='%s' AND general.id = cin.id ORDER BY general.lt ASC LIMIT 1;", get_ri_rtnode(cnt));
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);

    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Failed select");
        return -1;
    }

    if (sqlite3_step(res) != SQLITE_ROW)
    {
        return 0;
    }

    latest_ri = sqlite3_column_text(res, 0);
    latest_cs = sqlite3_column_int(res, 1);

    logger("DB", LOG_LEVEL_DEBUG, "latest_ri : %s, latest_cs : %d", latest_ri, latest_cs);

    sprintf(sql, "DELETE FROM general WHERE ri='%s';", latest_ri);
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource from general/ msg : %s", err_msg);
        return -1;
    }

    // sprintf(sql, "DELETE FROM cin WHERE ='%s';", latest_ri);
    // rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    // if(rc != SQLITE_OK){
    //     logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource from cin/ msg : %s", err_msg);
    //     return -1;
    // }
    sqlite3_finalize(res);
    return latest_cs;
}

RTNode *db_get_all_resource_as_rtnode()
{
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_resource_as_rtnode");

    int rc = 0;
    int cols = 0;
    int coltype = 0, bytes = 0;
    int ty = 0;
    cJSON *json, *root;
    sqlite3_stmt *res;
    const char *colname = NULL;
    char sql[1024] = {0};
    char buf[256] = {0};

    sprintf(sql, "SELECT * FROM general WHERE ty != 4 AND ty != 5 AND ty != 58;");
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Failed select from general");
        return 0;
    }
    RTNode *head = NULL, *rtnode = NULL;
    rc = sqlite3_step(res);
    cols = sqlite3_column_count(res);
    cJSON *arr = NULL;
    while (rc == SQLITE_ROW)
    {
        json = cJSON_CreateObject();
        for (int col = 0; col < cols; col++)
        {

            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);

            switch (coltype)
            {
            case SQLITE_TEXT:
                memset(buf, 0, 256);
                strncpy(buf, sqlite3_column_text(res, col), bytes);
                arr = cJSON_Parse(buf);
                if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object))
                {
                    cJSON_AddItemToObject(json, colname, arr);
                }
                else
                {
                    cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                    cJSON_Delete(arr);
                }
                break;
            case SQLITE_INTEGER:
                cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                break;
            }
        }
        char sql2[1024] = {0};
        ty = cJSON_GetObjectItem(json, "ty")->valueint;
        sprintf(sql2, "SELECT * FROM %s WHERE id='%d';", get_table_name(ty), cJSON_GetObjectItem(json, "id")->valueint);
        // logger("DB", LOG_LEVEL_DEBUG, "SQL : %s", sql2);

        sqlite3_stmt *res2;
        rc = sqlite3_prepare_v2(db, sql2, -1, &res2, NULL);
        if (rc != SQLITE_OK)
        {
            logger("DB", LOG_LEVEL_ERROR, "Failed select from %s", get_table_name(ty));
            return 0;
        }
        rc = sqlite3_step(res2);
        int cols2 = sqlite3_column_count(res2);
        while (rc == SQLITE_ROW)
        {
            for (int col = 0; col < cols2; col++)
            {
                colname = sqlite3_column_name(res2, col);
                bytes = sqlite3_column_bytes(res2, col);
                coltype = sqlite3_column_type(res2, col);

                if (bytes == 0)
                    continue;
                if (strcmp(colname, "id") == 0)
                    continue;
                switch (coltype)
                {
                case SQLITE_TEXT:
                    memset(buf, 0, 256);
                    strncpy(buf, sqlite3_column_text(res2, col), bytes);
                    arr = cJSON_Parse(buf);
                    if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object))
                    {
                        cJSON_AddItemToObject(json, colname, arr);
                    }
                    else
                    {
                        cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                        cJSON_Delete(arr);
                    }
                    break;
                case SQLITE_INTEGER:
                    if (strcmp(colname, "fcied") == 0)
                        cJSON_AddItemToObject(json, colname, sqlite3_column_int(res2, col) ? cJSON_CreateTrue() : cJSON_CreateFalse());
                    else
                        cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res2, col)));
                    break;
                }
            }
            rc = sqlite3_step(res2);
        }
        sqlite3_finalize(res2);

        cJSON_DeleteItemFromObject(json, "id");

        // FCNT: custom_attrs를 top-level로 병합 후 제거 (db_get_resource와 동일)
        if (ty == RT_FCNT) {
            cJSON *custom_attrs = cJSON_GetObjectItem(json, "custom_attrs");
            if (custom_attrs && cJSON_IsObject(custom_attrs)) {
                cJSON *item = NULL;
                cJSON_ArrayForEach(item, custom_attrs) {
                    if (item->string) {
                        cJSON_AddItemToObject(json, item->string, cJSON_Duplicate(item, 1));
                    }
                }
                cJSON_DeleteItemFromObject(json, "custom_attrs");
            }
        }

        if (!head)
        {
            head = create_rtnode(json, ty);
            rtnode = head;
        }
        else
        {
            rtnode->sibling_right = create_rtnode(json, ty);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }
        rc = sqlite3_step(res);
    }
    sqlite3_finalize(res);
    return head;
}

RTNode *db_get_cin_rtnode_list(RTNode *parent_rtnode)
{
    logger("DB", LOG_LEVEL_DEBUG, "call db_get_cin_rtnode_list");
    char *pi = get_ri_rtnode(parent_rtnode);
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json, *root;
    sqlite3_stmt *res = NULL;
    const char *colname = NULL;
    char *sql;

    sql = "SELECT * FROM 'general', 'cin' WHERE general.pi=? AND general.id=cin.id ORDER BY id ASC;";
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Failed select, %d", rc);
        return 0;
    }
    sqlite3_bind_text(res, 1, pi, -1, NULL);

    RTNode *head = NULL, *rtnode = NULL;
    rc = sqlite3_step(res);
    cols = sqlite3_column_count(res);
    cJSON *arr = NULL;
    while (rc == SQLITE_ROW)
    {
        json = cJSON_CreateObject();

        for (int col = 0; col < cols; col++)
        {

            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);

            if (bytes == 0)
                continue;
            if (strcmp(colname, "id") == 0)
                continue;
            switch (coltype)
            {
                case SQLITE_TEXT:
                    const char *text = (const char*)sqlite3_column_text(res, col);
                    int len = sqlite3_column_bytes(res, col);
                    char *safe_str = malloc(len + 1);
                    if (safe_str) {
                        memcpy(safe_str, text, len);
                        safe_str[len] = '\0'; 
                        arr = cJSON_Parse(safe_str);
                        if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)) {
                            cJSON_AddItemToObject(json, colname, arr);
                        } else {
                            cJSON_AddItemToObject(json, colname, cJSON_CreateString(safe_str));
                            cJSON_Delete(arr);
                        }
                        free(safe_str);
                    }
                    break;
                case SQLITE_INTEGER:
                    cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                    break;
                }
        }
        if (!head)
        {
            head = create_rtnode(json, RT_CIN);
            rtnode = head;
        }
        else
        {
            rtnode->sibling_right = create_rtnode(json, RT_CIN);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }
        json = NULL;
        rc = sqlite3_step(res);
    }

    sqlite3_finalize(res);
    return head;
}

RTNode *db_get_latest_cins()
{
    char sql[1024] = {0};
    char buf[256] = {0};
    const char *colname;
    int rc = 0;
    int cols, bytes, coltype;
    cJSON *json, *root;
    sqlite3_stmt *res = NULL;
    RTNode *head = NULL, *rtnode = NULL;

    sprintf(sql, "SELECT  * FROM general, cin WHERE general.id IN (SELECT max(id) FROM general WHERE ty=4 GROUP BY pi)  AND general.id=cin.id");
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Failed select");
        return 0;
    }
    rc = sqlite3_step(res);
    while (rc == SQLITE_ROW)
    {
        cols = sqlite3_column_count(res);
        json = cJSON_CreateObject();
        cJSON *arr = NULL;

        for (int col = 0; col < cols; col++)
        {

            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);

            if (bytes == 0)
                continue;
            if (strcmp(colname, "id") == 0)
                continue;
            switch (coltype)
            {
            case SQLITE_TEXT:
                memset(buf, 0, 256);
                strncpy(buf, sqlite3_column_text(res, col), bytes);
                arr = cJSON_Parse(buf);
                if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object))
                {
                    cJSON_AddItemToObject(json, colname, arr);
                }
                else
                {
                    cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                    cJSON_Delete(arr);
                }
                break;
            case SQLITE_INTEGER:
                cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                break;
            }
        }

        if (!head)
        {
            head = create_rtnode(json, RT_CIN);
            rtnode = head;
        }
        else
        {
            rtnode->sibling_right = create_rtnode(json, RT_CIN);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }
        rc = sqlite3_step(res);
    }

    sqlite3_finalize(res);
    return head;
}

/**
 * get Latest/Oldest Content Instance
 * Latest Flag = 0, Oldest flag = 1
 */
cJSON *db_get_cin_laol(RTNode *parent_rtnode, int laol)
{
    char sql[1024] = {0}, buf[256] = {0};
    char *ord = NULL;
    const char *colname;
    int rc = 0;
    int cols, bytes, coltype;
    cJSON *json, *root;
    sqlite3_stmt *res = NULL;

    switch (laol)
    {
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

    sprintf(sql, "SELECT * from general, cin WHERE pi='%s' AND general.id = cin.id ORDER BY general.lt %s, general.id %s LIMIT 1;", get_ri_rtnode(parent_rtnode), ord, ord);
    logger("DB", LOG_LEVEL_DEBUG, "SQL : %s", sql);
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Failed select");
        return 0;
    }

    if (sqlite3_step(res) != SQLITE_ROW)
    {
        sqlite3_finalize(res);
        return 0;
    }
    cols = sqlite3_column_count(res);
    json = cJSON_CreateObject();
    cJSON *arr = NULL;

    for (int col = 0; col < cols; col++)
    {

        colname = sqlite3_column_name(res, col);
        bytes = sqlite3_column_bytes(res, col);
        coltype = sqlite3_column_type(res, col);

        if (bytes == 0)
            continue;
        if (strcmp(colname, "id") == 0)
            continue;
        switch (coltype)
        {
        case SQLITE_TEXT:
            memset(buf, 0, 256);
            strncpy(buf, sqlite3_column_text(res, col), bytes);
            arr = cJSON_Parse(buf);
            if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object))
            {
                cJSON_AddItemToObject(json, colname, arr);
            }
            else
            {
                cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                cJSON_Delete(arr);
            }
            break;
        case SQLITE_INTEGER:
            cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
            break;
        }
    }

    sqlite3_finalize(res);
    return json;
}

cJSON *db_get_filter_criteria(oneM2MPrimitive *o2pt)
{
    logger("DB", LOG_LEVEL_DEBUG, "call db_get_filter_criteria");
    extern pthread_mutex_t main_lock;
    char buf[256] = {0};
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *fc = o2pt->fc;
    cJSON *pjson = NULL, *ptr = NULL;
    cJSON *result = NULL, *json = NULL, *prtjson = NULL, *chjson = NULL, *tmp = NULL;
    sqlite3_stmt *res = NULL;
    const char *colname = NULL;
    char sql[1024] = {0};
    int jsize = 0, psize = 0, chsize = 0;
    int fo = cJSON_GetNumberValue(cJSON_GetObjectItem(fc, "fo"));

    char *uri = o2pt->to;
    if (strncmp(o2pt->to, CSE_BASE_NAME, strlen(CSE_BASE_NAME)) != 0)
    {
        uri = ri_to_uri(o2pt->to);
    }

    if (o2pt->drt == DRT_STRUCTURED)
    {
        sprintf(buf, "SELECT rn, ty, uri FROM general WHERE uri LIKE '%s/%%' AND ", uri);
        strcat(sql, buf);
    }
    else
    {
        sprintf(buf, "SELECT rn, ty, ri FROM general WHERE uri LIKE '%s/%%' AND ", uri);
        strcat(sql, buf);
    }

    if ((pjson = cJSON_GetObjectItem(fc, "lvl")))
    {
        sprintf(buf, "uri NOT LIKE '%s/%%", uri);
        strcat(sql, buf);
        for (int i = 0; i < pjson->valueint; i++)
        {
            strcat(sql, "/%%");
        }
        strcat(sql, "' AND ");
    }

    if ((pjson = cJSON_GetObjectItem(fc, "cra")))
    {
        sprintf(buf, "ct>'%s' ", pjson->valuestring);
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }
    if ((pjson = cJSON_GetObjectItem(fc, "crb")))
    {
        sprintf(buf, "ct<='%s' ", pjson->valuestring);
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }
    if ((pjson = cJSON_GetObjectItem(fc, "exa")))
    {
        sprintf(buf, "et>'%s' ", pjson->valuestring);
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }
    if ((pjson = cJSON_GetObjectItem(fc, "exb")))
    {
        sprintf(buf, "et<='%s' ", pjson->valuestring);
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }

    if ((pjson = cJSON_GetObjectItem(fc, "ms")))
    {
        sprintf(buf, "lt>'%s' ", pjson->valuestring);
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }
    if ((pjson = cJSON_GetObjectItem(fc, "us")))
    {
        sprintf(buf, "lt<='%s' ", pjson->valuestring);
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }

    if ((pjson = cJSON_GetObjectItem(fc, "lbl")))
    {
        if (cJSON_IsArray(pjson))
        {
            cJSON_ArrayForEach(ptr, pjson)
            {
                sprintf(buf, "lbl LIKE '%%\"%s\"%%' OR ", cJSON_GetStringValue(ptr));
                strcat(sql, buf);
            }
            sql[strlen(sql) - 3] = '\0';
        }
        else if (cJSON_IsString(pjson))
        {
            sprintf(buf, "lbl LIKE '%%\"%s\"%%'", cJSON_GetStringValue(pjson));
            strcat(sql, buf);
        }
        filterOptionStr(fo, sql);
    }

    if ((pjson = cJSON_GetObjectItem(fc, "ty")))
    {

        strcat(sql, "(");
        if (cJSON_IsArray(pjson))
        {
            cJSON_ArrayForEach(ptr, pjson)
            {
                sprintf(buf, " ty = %d OR", ptr->valueint);
                strcat(sql, buf);
            }
            sql[strlen(sql) - 2] = '\0';
        }
        else if (cJSON_IsNumber(pjson))
        {
            sprintf(buf, " ty = %d", pjson->valueint);
            strcat(sql, buf);
        }

        strcat(sql, ") ");
        filterOptionStr(fo, sql);
    }
// get only discoverable resource
#if MONO_THREAD == 0
    pthread_mutex_lock(&main_lock);
#endif

    cJSON *acpiList = getNonDiscoverableAcp(o2pt, rt->cb);
    cJSON *forbiddenURI = getForbiddenUri(acpiList);

#if MONO_THREAD == 0
    pthread_mutex_unlock(&main_lock);
#endif
    logger("DB", LOG_LEVEL_DEBUG, "Forbidden URI : %s", cJSON_Print(forbiddenURI));
    if (cJSON_GetArraySize(forbiddenURI) > 0)
    {
        strcat(sql, "(");
        cJSON_ArrayForEach(ptr, forbiddenURI)
        {
            if (cJSON_IsString(ptr))
            {
                sprintf(buf, "uri NOT LIKE '%s%%' OR ", cJSON_GetStringValue(ptr));
                strcat(sql, buf);
            }
        }
        sql[strlen(sql) - 3] = '\0';
        strcat(sql, ")");
        filterOptionStr(fo, sql);
    }

    cJSON_Delete(acpiList);
    cJSON_Delete(forbiddenURI);

    if ((pjson = cJSON_GetObjectItem(fc, "ops")))
    {
#if MONO_THREAD == 0
        pthread_mutex_lock(&main_lock);
#endif
        acpiList = getNoPermAcopDiscovery(o2pt, rt->cb, pjson->valueint);
        forbiddenURI = getForbiddenUri(acpiList);

#if MONO_THREAD == 0
        pthread_mutex_unlock(&main_lock);
#endif
        if (cJSON_GetArraySize(forbiddenURI) > 0)
        {
            strcat(sql, " (");
            cJSON_ArrayForEach(ptr, forbiddenURI)
            {
                if (cJSON_IsString(ptr))
                {
                    sprintf(buf, "uri NOT LIKE '%s%%' OR ", cJSON_GetStringValue(ptr));
                    strcat(sql, buf);
                }
            }
            sql[strlen(sql) - 3] = '\0';
            strcat(sql, ")");
            filterOptionStr(fo, sql);
        }
        cJSON_Delete(forbiddenURI);
        cJSON_Delete(acpiList);
    }

    if ((pjson = cJSON_GetObjectItem(fc, "sza")))
    {
        sprintf(buf, " id IN (SELECT id FROM cin WHERE cs >= %d) ", pjson->valueint);
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }

    if ((pjson = cJSON_GetObjectItem(fc, "szb")))
    {
        sprintf(buf, " id IN (SELECT id FROM cin WHERE cs < %d) ", pjson->valueint);
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }

    if ((pjson = cJSON_GetObjectItem(fc, "sts")))
    {
        sprintf(buf, " id IN (SELECT id FROM cnt WHERE st < %d UNION SELECT id FROM fcnt WHERE st < %d) ", pjson->valueint, pjson->valueint);
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }

    if ((pjson = cJSON_GetObjectItem(fc, "stb")))
    {
        sprintf(buf, " id IN (SELECT id FROM cnt WHERE st >= %d UNION SELECT id FROM fcnt WHERE st >= %d) ", pjson->valueint, pjson->valueint);
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }

    if (cJSON_GetObjectItem(fc, "pty") || cJSON_GetObjectItem(fc, "palb"))
    {
        strcat(sql, "pi IN (SELECT ri from general WHERE ");
        if ((pjson = cJSON_GetObjectItem(fc, "pty")))
        {
            strcat(sql, "(");

            if (cJSON_IsArray(pjson))
            {
                cJSON_ArrayForEach(ptr, pjson)
                {
                    sprintf(buf, " ty = %d OR", ptr->valueint);
                    strcat(sql, buf);
                }
                sql[strlen(sql) - 2] = '\0';
            }
            else if (cJSON_IsNumber(pjson))
            {
                sprintf(buf, " ty = %d", pjson->valueint);
                strcat(sql, buf);
            }

            strcat(sql, ") ");
            filterOptionStr(fo, sql);
        }

        if ((pjson = cJSON_GetObjectItem(fc, "palb")))
        {
            if (cJSON_IsArray(pjson))
            {
                cJSON_ArrayForEach(ptr, pjson)
                {
                    sprintf(buf, "lbl LIKE '%%\"%s\"%%' OR ", cJSON_GetStringValue(ptr));
                    strcat(sql, buf);
                }
                sql[strlen(sql) - 3] = '\0';
            }
            else if (cJSON_IsString(pjson))
            {
                sprintf(buf, "lbl LIKE '%%\"%s\"%%'", cJSON_GetStringValue(pjson));
                strcat(sql, buf);
            }
            filterOptionStr(fo, sql);
        }

        if (fo == FO_AND)
        {
            sql[strlen(sql) - 4] = '\0';
        }
        else if (fo == FO_OR)
        {
            sql[strlen(sql) - 3] = '\0';
        }

        strcat(sql, ")");

        filterOptionStr(fo, sql);
    }

    if (cJSON_GetObjectItem(fc, "chty") || cJSON_GetObjectItem(fc, "clbl"))
    {
        strcat(sql, "ri IN (SELECT pi from general WHERE ");
        if ((pjson = cJSON_GetObjectItem(fc, "chty")))
        {
            strcat(sql, "(");

            if (cJSON_IsArray(pjson))
            {
                cJSON_ArrayForEach(ptr, pjson)
                {
                    sprintf(buf, " ty = %d OR", ptr->valueint);
                    strcat(sql, buf);
                }
                sql[strlen(sql) - 2] = '\0';
            }
            else if (cJSON_IsNumber(pjson))
            {
                sprintf(buf, " ty = %d", pjson->valueint);
                strcat(sql, buf);
            }

            strcat(sql, ") ");
            filterOptionStr(fo, sql);
        }

        if ((pjson = cJSON_GetObjectItem(fc, "clbl")))
        {
            if (cJSON_IsArray(pjson))
            {
                cJSON_ArrayForEach(ptr, pjson)
                {
                    sprintf(buf, "lbl LIKE '%%\"%s\"%%' OR ", cJSON_GetStringValue(ptr));
                    strcat(sql, buf);
                }
                sql[strlen(sql) - 3] = '\0';
            }
            else if (cJSON_IsString(pjson))
            {
                sprintf(buf, "lbl LIKE '%%\"%s\"%%'", cJSON_GetStringValue(pjson));
                strcat(sql, buf);
            }
            filterOptionStr(fo, sql);
        }

        if (fo == FO_AND)
        {
            sql[strlen(sql) - 4] = '\0';
        }
        else if (fo == FO_OR)
        {
            sql[strlen(sql) - 3] = '\0';
        }

        strcat(sql, ")");

        filterOptionStr(fo, sql);
    }

    if (fo == FO_AND)
    {
        sql[strlen(sql) - 4] = '\0';
    }
    else if (fo == FO_OR)
    {
        sql[strlen(sql) - 3] = '\0';
    }

    if (DEFAULT_DISCOVERY_SORT == SORT_DESC)
    {
        strcat(sql, " ORDER BY general.id DESC ");
    }
    else
    {
        strcat(sql, " ORDER BY general.id ASC ");
    }

    if ((pjson = cJSON_GetObjectItem(fc, "lim")))
    {
        sprintf(buf, " LIMIT %d", pjson->valueint > DEFAULT_DISCOVERY_LIMIT ? DEFAULT_DISCOVERY_LIMIT + 1 : pjson->valueint + 1);
        strcat(sql, buf);
    }
    else
    {
        sprintf(buf, " LIMIT %d", DEFAULT_DISCOVERY_LIMIT + 1);
        strcat(sql, buf);
    }
    if ((pjson = cJSON_GetObjectItem(fc, "ofst")))
    {
        sprintf(buf, " OFFSET %d", pjson->valueint);
        strcat(sql, buf);
    }
    strcat(sql, ";");
    logger("DB", LOG_LEVEL_DEBUG, "SQL : %s", sql);
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Failed select, %s (%d)", sql, rc);
        return 0;
    }

    json = cJSON_CreateArray();
    rc = sqlite3_step(res);
    pjson = NULL;
    while (rc == SQLITE_ROW)
    {
        bytes = sqlite3_column_bytes(res, 0);
        if (bytes == 0)
        {
            logger("DB", LOG_LEVEL_ERROR, "empty URI");
            break;
        }
        pjson = cJSON_CreateObject();
        cJSON_AddItemToObject(pjson, "name", cJSON_CreateString(sqlite3_column_text(res, 0)));
        cJSON_AddItemToObject(pjson, "type", cJSON_CreateNumber(sqlite3_column_int(res, 1)));
        cJSON_AddItemToObject(pjson, "val", cJSON_CreateString(sqlite3_column_text(res, 2)));
        cJSON_AddItemToArray(json, pjson);
        rc = sqlite3_step(res);
    }
    sqlite3_finalize(res);
    return json;
}

bool db_check_cin_rn_dup(char *rn, char *pi)
{
    if (!rn || !pi)
        return false;
    char sql[1024] = {0};
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json, *root;
    sqlite3_stmt *res = NULL;
    const char *colname = NULL;
    char buf[256] = {0};

    sprintf(sql, "SELECT * FROM general WHERE rn='%s' AND pi='%s';", rn, pi);
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Failed select from general");
        return 0;
    }
    rc = sqlite3_step(res);
    if (rc == SQLITE_ROW)
    {
        sqlite3_finalize(res);
        return true;
    }
    sqlite3_finalize(res);
    return false;
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
    char buf[256] = {0};
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json, *root;
    sqlite3_stmt *res = NULL;
    const char *colname = NULL;
    char sql[2048] = {0};
    cJSON *ptr = NULL;
    cJSON *result = cJSON_CreateArray();

    if (cJSON_GetArraySize(acp_list) == 0)
    {
        return result;
    }

    strcat(sql, "SELECT uri FROM 'general' WHERE ");
    cJSON_ArrayForEach(ptr, acp_list)
    {
        if (cJSON_IsString(ptr))
        {
            sprintf(buf, "acpi LIKE '%%\"%s\"%%' OR ", cJSON_GetStringValue(ptr));
            strcat(sql, buf);
        }
    }
    sql[strlen(sql) - 3] = '\0';
    strcat(sql, ";");
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Failed select, %d", rc);
        return 0;
    }

    rc = sqlite3_step(res);
    while (rc == SQLITE_ROW)
    {
        cJSON_AddItemToArray(result, cJSON_CreateString(sqlite3_column_text(res, 0)));
        rc = sqlite3_step(res);
    }

    sqlite3_finalize(res);
    return result;
}

// FCIN related functions
int db_delete_one_fcin_mni(RTNode *fcnt)
{
    logger("DB", LOG_LEVEL_DEBUG, "db_delete_one_fcin_mni called");
    if (!fcnt) return -1;

    char *pi = get_ri_rtnode(fcnt);
    sqlite3_stmt *res = NULL;
    int deleted_cs = 0;

    // Find the oldest FCIN (smallest ct) and get its cs value
    char *sql_select = "SELECT general.id, fcin.cs FROM general "
                       "LEFT JOIN fcin ON general.id = fcin.id "
                       "WHERE general.pi=? AND general.ty=58 "
                       "ORDER BY fcin.st ASC LIMIT 1";

    int rc = sqlite3_prepare_v2(db, sql_select, -1, &res, NULL);
    if (rc != SQLITE_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to prepare select FCIN statement");
        return -1;
    }

    sqlite3_bind_text(res, 1, pi, -1, NULL);
    rc = sqlite3_step(res);

    if (rc == SQLITE_ROW) {
        int fcin_id = sqlite3_column_int(res, 0);
        deleted_cs = sqlite3_column_int(res, 1);
        sqlite3_finalize(res);

        char *sql_delete = "DELETE FROM general WHERE id=?";
        rc = sqlite3_prepare_v2(db, sql_delete, -1, &res, NULL);
        if (rc != SQLITE_OK) {
            logger("DB", LOG_LEVEL_ERROR, "Failed to prepare delete FCIN statement");
            return -1;
        }

        sqlite3_bind_int(res, 1, fcin_id);
        rc = sqlite3_step(res);
        sqlite3_finalize(res);

        if (rc != SQLITE_DONE) {
            logger("DB", LOG_LEVEL_ERROR, "Failed to delete oldest FCIN");
            return -1;
        }

        logger("DB", LOG_LEVEL_DEBUG, "Successfully deleted oldest FCIN with cs=%d", deleted_cs);
        return deleted_cs;
    } else {
        sqlite3_finalize(res);
        logger("DB", LOG_LEVEL_ERROR, "No FCIN found to delete");
        return -1;
    }
}

int db_delete_one_fcin_mbs(RTNode *fcnt)
{
    logger("DB", LOG_LEVEL_DEBUG, "db_delete_one_fcin_mbs called");
    if (!fcnt) return -1;

    char *pi = get_ri_rtnode(fcnt);
    sqlite3_stmt *res = NULL;
    int deleted_cs = 0;

    char *sql_select = "SELECT general.id, fcin.cs FROM general "
                       "LEFT JOIN fcin ON general.id = fcin.id "
                       "WHERE general.pi=? AND general.ty=58 "
                       "ORDER BY fcin.st ASC LIMIT 1";

    int rc = sqlite3_prepare_v2(db, sql_select, -1, &res, NULL);
    if (rc != SQLITE_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to prepare select FCIN statement");
        return -1;
    }

    sqlite3_bind_text(res, 1, pi, -1, NULL);
    rc = sqlite3_step(res);

    if (rc == SQLITE_ROW) {
        int fcin_id = sqlite3_column_int(res, 0);
        deleted_cs = sqlite3_column_int(res, 1);
        sqlite3_finalize(res);

        char *sql_delete = "DELETE FROM general WHERE id=?";
        rc = sqlite3_prepare_v2(db, sql_delete, -1, &res, NULL);
        if (rc != SQLITE_OK) {
            logger("DB", LOG_LEVEL_ERROR, "Failed to prepare delete FCIN statement");
            return -1;
        }

        sqlite3_bind_int(res, 1, fcin_id);
        rc = sqlite3_step(res);
        sqlite3_finalize(res);

        if (rc != SQLITE_DONE) {
            logger("DB", LOG_LEVEL_ERROR, "Failed to delete oldest FCIN");
            return -1;
        }

        logger("DB", LOG_LEVEL_DEBUG, "Successfully deleted oldest FCIN with cs=%d", deleted_cs);
        return deleted_cs;
    } else {
        sqlite3_finalize(res);
        logger("DB", LOG_LEVEL_ERROR, "No FCIN found to delete");
        return -1;
    }
}

int db_get_fcin_cbs_cni(RTNode *fcnt, int *out_cbs, int *out_cni)
{
    logger("DB", LOG_LEVEL_DEBUG, "db_get_fcin_cbs_cni called");
    if (!fcnt || !out_cbs || !out_cni) return -1;

    char *pi = get_ri_rtnode(fcnt);
    sqlite3_stmt *res = NULL;

    char *sql = "SELECT COUNT(*), COALESCE(SUM(fcin.cs), 0) FROM general "
                "LEFT JOIN fcin ON general.id = fcin.id "
                "WHERE general.pi=? AND general.ty=58";

    int rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if (rc != SQLITE_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to prepare FCIN count statement");
        return -1;
    }

    sqlite3_bind_text(res, 1, pi, -1, NULL);
    rc = sqlite3_step(res);

    if (rc == SQLITE_ROW) {
        *out_cni = sqlite3_column_int(res, 0);
        *out_cbs = sqlite3_column_int(res, 1);
        sqlite3_finalize(res);
        logger("DB", LOG_LEVEL_DEBUG, "db_get_fcin_cbs_cni: cni=%d, cbs=%d", *out_cni, *out_cbs);
        return 0;
    } else {
        sqlite3_finalize(res);
        logger("DB", LOG_LEVEL_ERROR, "Failed to get FCIN count/sum");
        return -1;
    }
}

RTNode *db_get_fcin_rtnode_list(RTNode *rtnode)
{
    logger("DB", LOG_LEVEL_DEBUG, "call db_get_fcin_rtnode_list");
    char *pi = get_ri_rtnode(rtnode);
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json;
    sqlite3_stmt *res = NULL;
    const char *colname = NULL;
    char *sql;

    sql = "SELECT * FROM 'general', 'fcin' WHERE general.pi=? AND general.ty=58 AND general.id=fcin.id ORDER BY fcin.st ASC;";
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Failed select, %d", rc);
        return NULL;
    }
    sqlite3_bind_text(res, 1, pi, -1, NULL);

    RTNode *head = NULL, *rtnode_ptr = NULL;
    rc = sqlite3_step(res);
    cols = sqlite3_column_count(res);
    cJSON *arr = NULL;

    while (rc == SQLITE_ROW)
    {
        json = cJSON_CreateObject();

        for (int col = 0; col < cols; col++)
        {
            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);

            if (bytes == 0)
                continue;
            if (strcmp(colname, "id") == 0)
                continue;

            switch (coltype)
            {
                case SQLITE_TEXT:
                    const char *text = (const char*)sqlite3_column_text(res, col);
                    int len = sqlite3_column_bytes(res, col);
                    char *safe_str = malloc(len + 1);
                    if (safe_str) {
                        memcpy(safe_str, text, len);
                        safe_str[len] = '\0';
                        arr = cJSON_Parse(safe_str);
                        if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)) {
                            cJSON_AddItemToObject(json, colname, arr);
                        } else {
                            cJSON_AddItemToObject(json, colname, cJSON_CreateString(safe_str));
                            if (arr) cJSON_Delete(arr);
                        }
                        free(safe_str);
                    }
                    break;
                case SQLITE_INTEGER:
                    cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                    break;
            }
        }

        if (!head)
        {
            head = create_rtnode(json, RT_FCIN);
            rtnode_ptr = head;
        }
        else
        {
            rtnode_ptr->sibling_right = create_rtnode(json, RT_FCIN);
            rtnode_ptr->sibling_right->sibling_left = rtnode_ptr;
            rtnode_ptr = rtnode_ptr->sibling_right;
        }
        json = NULL;
        rc = sqlite3_step(res);
    }

    sqlite3_finalize(res);
    return head;
}

cJSON *db_get_fcin_laol(RTNode *parent_rtnode, int laol)
{
    logger("DB", LOG_LEVEL_DEBUG, "call db_get_fcin_laol, laol=%d", laol);
    char *pi = get_ri_rtnode(parent_rtnode);
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json = NULL;
    sqlite3_stmt *res = NULL;
    const char *colname = NULL;

    const char *sql = (laol == 0)
        ? "SELECT * FROM general, fcin WHERE general.pi=? AND general.ty=58 AND general.id=fcin.id ORDER BY fcin.st DESC LIMIT 1"
        : "SELECT * FROM general, fcin WHERE general.pi=? AND general.ty=58 AND general.id=fcin.id ORDER BY fcin.st ASC LIMIT 1";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Failed select, %d", rc);
        return NULL;
    }
    sqlite3_bind_text(res, 1, pi, -1, SQLITE_STATIC);

    rc = sqlite3_step(res);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(res);
        return NULL;
    }

    cols = sqlite3_column_count(res);
    json = cJSON_CreateObject();
    cJSON *arr = NULL;

    for (int col = 0; col < cols; col++)
    {
        colname = sqlite3_column_name(res, col);
        bytes = sqlite3_column_bytes(res, col);
        coltype = sqlite3_column_type(res, col);

        if (bytes == 0)
            continue;
        if (strcmp(colname, "id") == 0)
            continue;
        if (strcmp(colname, "custom_attrs") == 0)
            continue;

        switch (coltype)
        {
            case SQLITE_TEXT:
                const char *text = (const char*)sqlite3_column_text(res, col);
                int len = sqlite3_column_bytes(res, col);
                char *safe_str = malloc(len + 1);
                if (safe_str) {
                    memcpy(safe_str, text, len);
                    safe_str[len] = '\0';
                    arr = cJSON_Parse(safe_str);
                    if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)) {
                        cJSON_AddItemToObject(json, colname, arr);
                    } else {
                        cJSON_AddItemToObject(json, colname, cJSON_CreateString(safe_str));
                        if (arr) cJSON_Delete(arr);
                    }
                    free(safe_str);
                }
                break;
            case SQLITE_INTEGER:
                cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                break;
        }
    }

    sqlite3_finalize(res);

    cJSON *ri_obj = cJSON_GetObjectItem(json, "ri");
    if (ri_obj && cJSON_IsString(ri_obj))
    {
        cJSON *customAttrs = db_get_fcnt_custom_attributes(ri_obj->valuestring);
        if (customAttrs)
        {
            cJSON *item = NULL;
            cJSON_ArrayForEach(item, customAttrs)
            {
                if (item->string)
                {
                    cJSON_AddItemToObject(json, item->string, cJSON_Duplicate(item, 1));
                }
            }
            cJSON_Delete(customAttrs);
        }
    }

    return json;
}


// FCNT custom attributes functions
int db_store_fcnt_custom_attributes(const char *ri, cJSON *customAttrs)
{
    if (!ri || !customAttrs) {
        logger("DB", LOG_LEVEL_DEBUG, "db_store_fcnt_custom_attributes: invalid parameters");
        return 0;
    }

    char *json_str = cJSON_PrintUnformatted(customAttrs);
    if (!json_str) {
        logger("DB", LOG_LEVEL_ERROR, "db_store_fcnt_custom_attributes: failed to serialize JSON");
        return 0;
    }

    // Update FCNT custom_attrs
    const char *sql_fcnt = "UPDATE fcnt SET custom_attrs=? WHERE id=(SELECT id FROM general WHERE ri=?)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql_fcnt, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to prepare store FCNT custom attributes: %d", rc);
        free(json_str);
        return 0;
    }
    sqlite3_bind_text(stmt, 1, json_str, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, ri, -1, SQLITE_STATIC);

    logger("DB", LOG_LEVEL_DEBUG, "db_store_fcnt_custom_attributes (FCNT): ri=%s", ri);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to store FCNT custom attributes: %d", rc);
        free(json_str);
        return 0;
    }

    // Update FCIN custom_attrs
    const char *sql_fcin = "UPDATE fcin SET custom_attrs=? WHERE id=(SELECT id FROM general WHERE ri=?)";
    rc = sqlite3_prepare_v2(db, sql_fcin, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to prepare store FCIN custom attributes: %d", rc);
        free(json_str);
        return 0;
    }
    sqlite3_bind_text(stmt, 1, json_str, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, ri, -1, SQLITE_STATIC);

    logger("DB", LOG_LEVEL_DEBUG, "db_store_fcnt_custom_attributes (FCIN): ri=%s", ri);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    free(json_str);

    if (rc != SQLITE_DONE) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to store FCIN custom attributes: %d", rc);
        return 0;
    }

    return 1;
}

int db_update_fcnt_custom_attributes(const char *ri, cJSON *customAttrs)
{
    return db_store_fcnt_custom_attributes(ri, customAttrs);
}

cJSON *db_get_fcnt_custom_attributes(const char *ri)
{
    if (!ri) {
        return NULL;
    }

    const char *sql = "SELECT COALESCE(fcnt.custom_attrs, fcin.custom_attrs) AS custom_attrs "
        "FROM general LEFT JOIN fcnt ON general.id = fcnt.id "
        "LEFT JOIN fcin ON general.id = fcin.id WHERE general.ri = ?";

    logger("DB", LOG_LEVEL_DEBUG, "db_get_fcnt_custom_attributes: ri=%s", ri);

    sqlite3_stmt *res = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if (rc != SQLITE_OK) {
        logger("DB", LOG_LEVEL_ERROR, "Failed to prepare get FCNT/FCIN custom attributes: %d", rc);
        return NULL;
    }
    sqlite3_bind_text(res, 1, ri, -1, SQLITE_STATIC);

    rc = sqlite3_step(res);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(res);
        return NULL;
    }

    const char *json_str = (const char*)sqlite3_column_text(res, 0);
    if (!json_str) {
        sqlite3_finalize(res);
        return NULL;
    }

    cJSON *customAttrs = cJSON_Parse(json_str);
    sqlite3_finalize(res);

    return customAttrs;
}

// Forward declarations (avoid implicit function declarations in C99+)
int db_ts_update_mdc_with_mdlt(const char *ts_ri, int val, const char *time_str);
int db_general_update_lt(const char *ri, const char *lt);

// -----------------------------------------------------------------------------
// Compatibility wrappers (must match dbmanager.h symbols)
// Keep TS/TSI resource logic DB-agnostic by implementing these helpers per backend.
// -----------------------------------------------------------------------------

int db_general_set_lt(const char *ri, const char *lt)
{
    return db_general_update_lt(ri, lt);
}

int db_ts_set_mdc_and_append_mdlt(const char *ts_ri, int val, const char *time_str)
{
    return db_ts_update_mdc_with_mdlt(ts_ri, val, time_str);
}

// Check duplicate TSI.dgt under a TS (by TS ri == general.pi)
int db_tsi_check_dgt_dup(const char *ts_ri, const char *dgt)
{
    if (!ts_ri || !dgt) return 0;

    sqlite3_mutex_enter(sqlite3_db_mutex(db));

    const char *sql =
        "SELECT 1 "
        "FROM tsi JOIN general ON tsi.id = general.id "
        "WHERE general.pi = ? AND tsi.dgt = ? "
        "LIMIT 1;";

    sqlite3_stmt *stmt = NULL;
    int exists = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ts_ri, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, dgt, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = 1;
        }
    }

    if (stmt) sqlite3_finalize(stmt);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return exists;
}

//------------------------------------------------------------------------------
// TS / TSI helper APIs (SQLite backend)
// These functions are used to decouple TS/TSI logic from raw SQL in resource code.
// All functions are best-effort and return safe defaults on error.
//------------------------------------------------------------------------------

cJSON *db_get_tsi_laol(RTNode *parent_rtnode, int laol) {
    const char *ord = (laol == 0) ? "DESC" : "ASC";
    char sql[1024], buf[256];
    sqlite3_stmt *res = NULL;
    snprintf(sql, sizeof(sql),
             "SELECT * FROM general, tsi WHERE pi='%s' AND general.id = tsi.id "
             "ORDER BY general.lt %s, general.id %s LIMIT 1;",
             get_ri_rtnode(parent_rtnode), ord, ord);

    if (sqlite3_prepare_v2(db, sql, -1, &res, NULL) != SQLITE_OK) return NULL;
    if (sqlite3_step(res) != SQLITE_ROW) { sqlite3_finalize(res); return NULL; }

    int cols = sqlite3_column_count(res);
    cJSON *json = cJSON_CreateObject();
    for (int col=0; col<cols; col++) {
        const char *colname = sqlite3_column_name(res, col);
        if (!strcmp(colname, "id")) continue;
        int coltype = sqlite3_column_type(res, col);
        int bytes = sqlite3_column_bytes(res, col);
        if (bytes == 0) continue;

        if (coltype == SQLITE_TEXT) {
            memset(buf, 0, sizeof(buf));
            strncpy(buf, (const char*)sqlite3_column_text(res, col), bytes);
            cJSON *arr = cJSON_Parse(buf);
            if (arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)) cJSON_AddItemToObject(json, colname, arr);
            else { cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf)); cJSON_Delete(arr); }
        } else if (coltype == SQLITE_INTEGER) {
            cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
        }
    }
    sqlite3_finalize(res);
    return json;
}

int db_tsi_get_count(const char *ts_ri)
{
    if (!ts_ri) return 0;
    sqlite3_mutex_enter(sqlite3_db_mutex(db));

    const char *sql =
        "SELECT COUNT(*) "
        "FROM tsi JOIN general ON tsi.id = general.id "
        "WHERE general.pi = ?;";

    sqlite3_stmt *stmt = NULL;
    int count = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ts_ri, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
    }

    if (stmt) sqlite3_finalize(stmt);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return count;
}

int db_tsi_get_max_snr(const char *ts_ri)
{
    if (!ts_ri) return 0;
    sqlite3_mutex_enter(sqlite3_db_mutex(db));

    const char *sql =
        "SELECT COALESCE(MAX(tsi.snr), 0) "
        "FROM tsi JOIN general ON tsi.id = general.id "
        "WHERE general.pi = ?;";

    sqlite3_stmt *stmt = NULL;
    int max_snr = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ts_ri, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            max_snr = sqlite3_column_int(stmt, 0);
        }
    }

    if (stmt) sqlite3_finalize(stmt);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return max_snr;
}

int db_tsi_get_min_snr(const char *ts_ri)
{
    if (!ts_ri) return 0;
    sqlite3_mutex_enter(sqlite3_db_mutex(db));

    const char *sql =
        "SELECT COALESCE(MIN(tsi.snr), 0) "
        "FROM tsi JOIN general ON tsi.id = general.id "
        "WHERE general.pi = ?;";

    sqlite3_stmt *stmt = NULL;
    int min_snr = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ts_ri, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            min_snr = sqlite3_column_int(stmt, 0);
        }
    }

    if (stmt) sqlite3_finalize(stmt);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return min_snr;
}

int db_tsi_check_snr_dup(const char *ts_ri, int snr)
{
    if (!ts_ri) return 0;
    sqlite3_mutex_enter(sqlite3_db_mutex(db));

    const char *sql =
        "SELECT 1 "
        "FROM tsi JOIN general ON tsi.id = general.id "
        "WHERE general.pi = ? AND tsi.snr = ? "
        "LIMIT 1;";

    sqlite3_stmt *stmt = NULL;
    int exists = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ts_ri, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, snr);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = 1;
        }
    }

    if (stmt) sqlite3_finalize(stmt);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return exists;
}

// Arrival time of the first data point: the general.ct of the instance with the
// lowest sequenceNr.
char *db_tsi_get_first_ct(const char *ts_ri)
{
    if (!ts_ri) return NULL;
    sqlite3_mutex_enter(sqlite3_db_mutex(db));

    const char *sql =
        "SELECT general.ct "
        "FROM tsi JOIN general ON tsi.id = general.id "
        "WHERE general.pi = ? "
        "ORDER BY tsi.snr ASC LIMIT 1;";

    sqlite3_stmt *stmt = NULL;
    char *dgt = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ts_ri, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *v = sqlite3_column_text(stmt, 0);
            if (v && strlen((const char *)v) > 0) dgt = strdup((const char *)v);
        }
    }

    if (stmt) sqlite3_finalize(stmt);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return dgt;
}

int db_ts_get_mdc(const char *ts_ri)
{
    if (!ts_ri) return 0;
    sqlite3_mutex_enter(sqlite3_db_mutex(db));

    const char *sql =
        "SELECT COALESCE(ts.mdc, 0) "
        "FROM ts JOIN general ON ts.id = general.id "
        "WHERE general.ri = ? "
        "LIMIT 1;";

    sqlite3_stmt *stmt = NULL;
    int mdc = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ts_ri, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            mdc = sqlite3_column_int(stmt, 0);
        }
    }

    if (stmt) sqlite3_finalize(stmt);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return mdc;
}

int db_ts_update_mdc(const char *ts_ri, int val)
{
    if (!ts_ri) return 0;
    sqlite3_mutex_enter(sqlite3_db_mutex(db));

    const char *sql =
        "UPDATE ts SET mdc = ? "
        "WHERE id = (SELECT id FROM general WHERE ri = ?);";

    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, val);
        sqlite3_bind_text(stmt, 2, ts_ri, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = 1;
        }
    }

    if (stmt) sqlite3_finalize(stmt);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return ok;
}

// Append a missing-data timestamp to TS.mdlt and update TS.mdc.
// SQLite doesn't have native jsonb; store mdlt as a JSON string (TEXT) like "[\"...\", ...]".
int db_ts_update_mdc_with_mdlt(const char *ts_ri, int val, const char *time_str)
{
    if (!ts_ri || !time_str) return 0;

    sqlite3_mutex_enter(sqlite3_db_mutex(db));

    // 1) Read existing mdlt text (if any)
    const char *sel =
        "SELECT ts.mdlt "
        "FROM ts JOIN general ON ts.id = general.id "
        "WHERE general.ri = ? "
        "LIMIT 1;";

    sqlite3_stmt *stmt = NULL;
    char *new_mdlt = NULL;
    const unsigned char *mdlt_txt = NULL;

    if (sqlite3_prepare_v2(db, sel, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ts_ri, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            mdlt_txt = sqlite3_column_text(stmt, 0);
        }
    }
    if (stmt) sqlite3_finalize(stmt);

    // Build updated JSON array string
    if (!mdlt_txt || strlen((const char *)mdlt_txt) == 0) {
        // Create new array
        size_t need = strlen(time_str) + 8;
        new_mdlt = (char *)calloc(1, need);
        if (new_mdlt) {
            snprintf(new_mdlt, need, "[\"%s\"]", time_str);
        }
    } else {
        // Append to existing array: replace trailing ']' with ,"time"]
        const char *old = (const char *)mdlt_txt;
        size_t old_len = strlen(old);
        // If old isn't a JSON array, reset it.
        if (old_len < 2 || old[0] != '[' || old[old_len - 1] != ']') {
            size_t need = strlen(time_str) + 8;
            new_mdlt = (char *)calloc(1, need);
            if (new_mdlt) {
                snprintf(new_mdlt, need, "[\"%s\"]", time_str);
            }
        } else {
            int is_empty = (old_len == 2); // "[]"
            size_t need = old_len + strlen(time_str) + 6; // comma + quotes + bracket + null
            new_mdlt = (char *)calloc(1, need);
            if (new_mdlt) {
                memcpy(new_mdlt, old, old_len - 1); // up to before ']'
                if (!is_empty) {
                    strcat(new_mdlt, ",");
                }
                strcat(new_mdlt, "\"");
                strcat(new_mdlt, time_str);
                strcat(new_mdlt, "\"");
                strcat(new_mdlt, "]");
            }
        }
    }

    if (!new_mdlt) {
        sqlite3_mutex_leave(sqlite3_db_mutex(db));
        return 0;
    }

    // 2) Update ts.mdc and ts.mdlt
    const char *upd =
        "UPDATE ts SET mdc = ?, mdlt = ? "
        "WHERE id = (SELECT id FROM general WHERE ri = ?);";

    int ok = 0;
    if (sqlite3_prepare_v2(db, upd, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, val);
        sqlite3_bind_text(stmt, 2, new_mdlt, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, ts_ri, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = 1;
        }
    }

    if (stmt) sqlite3_finalize(stmt);
    free(new_mdlt);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return ok;
}

int db_clear_attribute(const char *ri, ResourceType ty, const char *attr)
{
    if (!ri || !attr) return 0;
    char *table_name = get_table_name(ty);
    if (!table_name) return 0;

    sqlite3_mutex_enter(sqlite3_db_mutex(db));

    char sql[512];
    snprintf(sql, sizeof(sql),
             "UPDATE %s SET %s = NULL WHERE id = (SELECT id FROM general WHERE ri = ?);",
             table_name, attr);

    sqlite3_stmt *stmt = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ri, -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return ok;
}

int db_ts_clear_mdlt(const char *ts_ri)
{
    if (!ts_ri) return 0;
    sqlite3_mutex_enter(sqlite3_db_mutex(db));

    const char *sql =
        "UPDATE ts SET mdlt = NULL "
        "WHERE id = (SELECT id FROM general WHERE ri = ?);";

    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ts_ri, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) ok = 1;
    }

    if (stmt) sqlite3_finalize(stmt);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return ok;
}

int db_general_update_lt(const char *ri, const char *lt)
{
    if (!ri || !lt) return 0;
    sqlite3_mutex_enter(sqlite3_db_mutex(db));

    const char *sql = "UPDATE general SET lt = ? WHERE ri = ?;";
    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, lt, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, ri, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = 1;
        }
    }

    if (stmt) sqlite3_finalize(stmt);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return ok;
}

// Return mdlt JSON string for a TS, caller must free(). Returns NULL if missing.
char *db_ts_get_mdlt_json(const char *ts_ri)
{
    if (!ts_ri) return NULL;
    sqlite3_mutex_enter(sqlite3_db_mutex(db));

    const char *sql =
        "SELECT ts.mdlt "
        "FROM ts JOIN general ON ts.id = general.id "
        "WHERE general.ri = ? "
        "LIMIT 1;";

    sqlite3_stmt *stmt = NULL;
    char *out = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ts_ri, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *txt = sqlite3_column_text(stmt, 0);
            if (txt && strlen((const char *)txt) > 0) {
                out = strdup((const char *)txt);
            }
        }
    }

    if (stmt) sqlite3_finalize(stmt);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return out;
}

/* ===========================================================================
 * db_get_descendants – SQLite port of the PostgreSQL implementation
 * ===========================================================================*/

typedef enum {
    DESC_OP_EQ,
    DESC_OP_LT,
    DESC_OP_GE,
    DESC_OP_CONTAINS,
    DESC_OP_IN_ARRAY,
    DESC_OP_EXISTS
} DescFcOp;

typedef struct {
    const char *short_name;
    int ty;
    const char *col;
    DescFcOp op;
} DescFcMap;

/* Resource-type → comparison operators and column mappings */
static const DescFcMap DESC_FC_MAP[] = {
    /* cin */
    {"cty", RT_CIN, "cnf", DESC_OP_EQ},
    {"sza", RT_CIN, "cs",  DESC_OP_GE},
    {"szb", RT_CIN, "cs",  DESC_OP_LT},
    {"con", RT_CIN, "con", DESC_OP_CONTAINS},
    {"cnf", RT_CIN, "cnf", DESC_OP_EQ},
    {"cs",  RT_CIN, "cs",  DESC_OP_EQ},
    {"cr",  RT_CIN, "cr",  DESC_OP_CONTAINS},
    {"sts", RT_CIN, "st",  DESC_OP_LT},
    {"stb", RT_CIN, "st",  DESC_OP_GE},
    {"st",  RT_CIN, "st",  DESC_OP_EQ},

    /* cnt */
    {"sts", RT_CNT, "st",  DESC_OP_LT},
    {"stb", RT_CNT, "st",  DESC_OP_GE},
    {"st",  RT_CNT, "st",  DESC_OP_EQ},
    {"mni", RT_CNT, "mni", DESC_OP_EQ},
    {"cni", RT_CNT, "cni", DESC_OP_EQ},
    {"cr",  RT_CNT, "cr",  DESC_OP_CONTAINS},

    /* ae */
    {"api", RT_AE, "api", DESC_OP_EQ},
    {"aei", RT_AE, "aei", DESC_OP_EQ},
    {"apn", RT_AE, "apn", DESC_OP_CONTAINS},
    {"poa", RT_AE, "poa", DESC_OP_IN_ARRAY},

    /* grp */
    {"mid",  RT_GRP, "mid",  DESC_OP_IN_ARRAY},
    {"macp", RT_GRP, "macp", DESC_OP_IN_ARRAY},
    {"gn",   RT_GRP, "gn",   DESC_OP_CONTAINS},

    /* sub */
    {"nu", RT_SUB, "nu", DESC_OP_IN_ARRAY},

    /* acp */
    {"pv",  RT_ACP, "pv",  DESC_OP_EXISTS},
    {"pvs", RT_ACP, "pvs", DESC_OP_EXISTS},

    /* annc */
    {"lnk", RT_ACPA,  "lnk", DESC_OP_CONTAINS},
    {"lnk", RT_AEA,   "lnk", DESC_OP_CONTAINS},
    {"lnk", RT_CNTA,  "lnk", DESC_OP_CONTAINS},
    {"lnk", RT_CINA,  "lnk", DESC_OP_CONTAINS},
    {"lnk", RT_CBA,   "lnk", DESC_OP_CONTAINS},
    {"lnk", RT_GRPA,  "lnk", DESC_OP_CONTAINS},
    {"lnk", RT_FCNTA, "lnk", DESC_OP_CONTAINS},
    {"lnk", RT_TSA,   "lnk", DESC_OP_CONTAINS},
};
#define DESC_FC_MAP_LEN (int)(sizeof(DESC_FC_MAP) / sizeof(DescFcMap))

/* Escape single quotes for use in an SQL string literal. Caller frees. */
static char *desc_escape(const char *s)
{
    if (!s) return strdup("");
    size_t len = strlen(s), q = 0;
    for (size_t i = 0; i < len; i++)
        if (s[i] == '\'') q++;
    char *out = malloc(len + q + 1);
    char *p = out;
    for (size_t i = 0; i < len; i++) {
        *p++ = s[i];
        if (s[i] == '\'') *p++ = '\'';
    }
    *p = '\0';
    return out;
}

static void desc_strcat(char **sql, size_t *cap, const char *s)
{
    size_t need = strlen(*sql) + strlen(s) + 1;
    if (need > *cap) {
        while (*cap < need) *cap *= 2;
        *sql = realloc(*sql, *cap);
    }
    strcat(*sql, s);
}

static void desc_append_forbidden(oneM2MPrimitive *o2pt, char **sql, size_t *cap, cJSON *fc)
{
    char buf[512];
    cJSON *ptr, *pjson;
    cJSON *acpiList = getNonDiscoverableAcp(o2pt, rt->cb);
    cJSON *forbiddenURI = getForbiddenUri(acpiList);
    cJSON_ArrayForEach(ptr, forbiddenURI) {
        if (!cJSON_IsString(ptr)) continue;
        char *e = desc_escape(ptr->valuestring);
        snprintf(buf, sizeof(buf), " AND uri NOT LIKE '%s'", e);
        desc_strcat(sql, cap, buf);
        free(e);
    }
    cJSON_Delete(forbiddenURI);
    cJSON_Delete(acpiList);

    if ((pjson = cJSON_GetObjectItem(fc, "ops"))) {
        acpiList = getNoPermAcopDiscovery(o2pt, rt->cb, pjson->valueint);
        forbiddenURI = getForbiddenUri(acpiList);
        cJSON_ArrayForEach(ptr, forbiddenURI) {
            if (!cJSON_IsString(ptr)) continue;
            char *e = desc_escape(ptr->valuestring);
            snprintf(buf, sizeof(buf), " AND uri NOT LIKE '%s%%'", e);
            desc_strcat(sql, cap, buf);
            free(e);
        }
        cJSON_Delete(forbiddenURI);
        cJSON_Delete(acpiList);
    }
}

/* Appends one filter-criteria condition, joining with AND for the first and fo for the rest. */
static void desc_open_cond(char **sql, size_t *cap, int *first, const char *fo_str)
{
    if (*first) {
        desc_strcat(sql, cap, " AND (");
        *first = 0;
    } else {
        desc_strcat(sql, cap, " ");
        desc_strcat(sql, cap, fo_str);
        desc_strcat(sql, cap, " ");
    }
}

/* Builds the WHERE clause on the general table. Returns a malloc'd string. */
static char *desc_build_general_where(oneM2MPrimitive *o2pt, cJSON *fc, const char *esc_uri, bool is_discover, const char *fo_str)
{
    size_t cap = 1024;
    char *sql = calloc(1, cap);
    char buf[512];
    cJSON *p, *ptr;
    int first = 1;

    snprintf(buf, sizeof(buf), "WHERE uri LIKE '%s/%%'", esc_uri);
    desc_strcat(&sql, &cap, buf);

    if (is_discover && !cJSON_GetObjectItem(fc, "arp"))
        desc_append_forbidden(o2pt, &sql, &cap, fc);
    if (cJSON_GetObjectItem(fc, "arp"))
        cJSON_DeleteItemFromObject(fc, "arp");

    if ((p = cJSON_GetObjectItem(fc, "lvl")) && cJSON_IsNumber(p)) {
        snprintf(buf, sizeof(buf), " AND uri NOT LIKE '%s/%%", esc_uri);
        desc_strcat(&sql, &cap, buf);
        for (int i = 0; i < p->valueint; i++)
            desc_strcat(&sql, &cap, "/%");
        desc_strcat(&sql, &cap, "'");
    }

    static const struct { const char *attr; const char *col; const char *op; } time_fc[] = {
        {"cra", "ct", ">"}, {"crb", "ct", "<="},
        {"exa", "et", ">"}, {"exb", "et", "<="},
        {"ms",  "lt", ">"}, {"us",  "lt", "<="},
    };
    for (int i = 0; i < (int)(sizeof(time_fc) / sizeof(time_fc[0])); i++) {
        if ((p = cJSON_GetObjectItem(fc, time_fc[i].attr)) && cJSON_IsString(p)) {
            char *e = desc_escape(p->valuestring);
            desc_open_cond(&sql, &cap, &first, fo_str);
            snprintf(buf, sizeof(buf), "%s %s '%s'", time_fc[i].col, time_fc[i].op, e);
            desc_strcat(&sql, &cap, buf);
            free(e);
        }
    }

    if ((p = cJSON_GetObjectItem(fc, "ty"))) {
        desc_open_cond(&sql, &cap, &first, fo_str);
        desc_strcat(&sql, &cap, "(");
        if (cJSON_IsArray(p)) {
            int f = 1;
            cJSON_ArrayForEach(ptr, p) {
                snprintf(buf, sizeof(buf), "%sty = %d", f ? "" : " OR ", (int)ptr->valuedouble);
                desc_strcat(&sql, &cap, buf);
                f = 0;
            }
            if (f) desc_strcat(&sql, &cap, "1=1");
        } else {
            snprintf(buf, sizeof(buf), "ty = %d", (int)p->valuedouble);
            desc_strcat(&sql, &cap, buf);
        }
        desc_strcat(&sql, &cap, ")");
    }

    if ((p = cJSON_GetObjectItem(fc, "lbl"))) {
        desc_open_cond(&sql, &cap, &first, fo_str);
        desc_strcat(&sql, &cap, "(");
        int f = 1;
        cJSON *single = cJSON_IsArray(p) ? NULL : p;
        cJSON *iter = cJSON_IsArray(p) ? p->child : single;
        for (; iter; iter = single ? NULL : iter->next) {
            if (!cJSON_IsString(iter)) continue;
            char *e = desc_escape(iter->valuestring);
            snprintf(buf, sizeof(buf), "%slbl LIKE '%%\"%s\"%%'", f ? "" : " OR ", e);
            desc_strcat(&sql, &cap, buf);
            free(e);
            f = 0;
        }
        if (f) desc_strcat(&sql, &cap, "1=1");
        desc_strcat(&sql, &cap, ")");
    }

    if (!first) desc_strcat(&sql, &cap, ")");
    return sql;
}

/* Builds the resource-specific conditions for one resource type. Returns a malloc'd string ("" if none). */
static char *desc_build_res_where(cJSON *fc, int ty, const char *fo_str)
{
    size_t cap = 512;
    char *sql = calloc(1, cap);
    char buf[1024];
    int first = 1;

    for (int i = 0; i < DESC_FC_MAP_LEN; i++) {
        const DescFcMap *m = &DESC_FC_MAP[i];
        if (m->ty != ty) continue;
        cJSON *val = cJSON_GetObjectItem(fc, m->short_name);
        if (!val) continue;

        buf[0] = '\0';
        if (m->op == DESC_OP_EXISTS) {
            snprintf(buf, sizeof(buf), "%s IS NOT NULL", m->col);
        } else if (cJSON_IsString(val) && val->valuestring[0] != '\0') {
            char *e = desc_escape(val->valuestring);
            if (!strcmp(m->short_name, "con") || !strcmp(m->short_name, "lnk")) {
                /* con/lnk support the '*' wildcard */
                for (char *c = e; *c; c++)
                    if (*c == '*') *c = '%';
            }
            switch (m->op) {
            case DESC_OP_CONTAINS: snprintf(buf, sizeof(buf), "%s LIKE '%%%s%%'", m->col, e); break;
            case DESC_OP_IN_ARRAY: snprintf(buf, sizeof(buf), "%s LIKE '%%\"%s\"%%'", m->col, e); break;
            case DESC_OP_LT:       snprintf(buf, sizeof(buf), "%s < '%s'", m->col, e); break;
            case DESC_OP_GE:       snprintf(buf, sizeof(buf), "%s >= '%s'", m->col, e); break;
            default:               snprintf(buf, sizeof(buf), "%s = '%s'", m->col, e); break;
            }
            free(e);
        } else if (cJSON_IsNumber(val)) {
            int num = (int)val->valuedouble;
            switch (m->op) {
            case DESC_OP_LT: snprintf(buf, sizeof(buf), "%s < %d", m->col, num); break;
            case DESC_OP_GE: snprintf(buf, sizeof(buf), "%s >= %d", m->col, num); break;
            default:         snprintf(buf, sizeof(buf), "%s = %d", m->col, num); break;
            }
        }
        if (buf[0] == '\0') continue;

        desc_open_cond(&sql, &cap, &first, fo_str);
        desc_strcat(&sql, &cap, buf);
    }
    if (!first) desc_strcat(&sql, &cap, ")");
    return sql;
}

static bool desc_has_res_fc(cJSON *fc)
{
    for (int i = 0; i < DESC_FC_MAP_LEN; i++)
        if (cJSON_GetObjectItem(fc, DESC_FC_MAP[i].short_name)) return true;
    return false;
}

static bool desc_fc_needs_ty(cJSON *fc, int ty)
{
    for (int i = 0; i < DESC_FC_MAP_LEN; i++)
        if (DESC_FC_MAP[i].ty == ty && cJSON_GetObjectItem(fc, DESC_FC_MAP[i].short_name)) return true;
    return false;
}

cJSON *db_get_descendants(oneM2MPrimitive *o2pt, RTNode *target_node, bool is_discover)
{
    logger("DB", LOG_LEVEL_DEBUG, "call db_get_descendants");
    if (!o2pt || !target_node) return NULL;

    cJSON *fc = o2pt->fc;
    int rcn = o2pt->rcn;
    cJSON *result = cJSON_CreateArray();
    if (!fc) return result;

    bool need_details = (rcn == RCN_ATTRIBUTES_AND_CHILD_RESOURCES ||
                         rcn == RCN_CHILD_RESOURCES);
    bool has_res_fc = desc_has_res_fc(fc);

    int lim = DEFAULT_DISCOVERY_LIMIT + 1; // +1 to detect whether more results exist
    int ofst = 0;
    cJSON *p;
    if ((p = cJSON_GetObjectItem(fc, "lim")) && cJSON_IsNumber(p)) {
        int req_lim = (int)p->valuedouble;
        lim = (req_lim > DEFAULT_DISCOVERY_LIMIT) ? req_lim : DEFAULT_DISCOVERY_LIMIT + 1;
    }
    if ((p = cJSON_GetObjectItem(fc, "ofst")) && cJSON_IsNumber(p))
        ofst = (int)p->valuedouble;
    const char *sort_dir = (DEFAULT_DISCOVERY_SORT == SORT_DESC) ? "DESC" : "ASC";

    int fo = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(fc, "fo"));
    const char *fo_str = (fo == FO_OR) ? "OR" : "AND";

    char *esc_uri = desc_escape(target_node->uri);
    char *general_where = desc_build_general_where(o2pt, fc, esc_uri, is_discover, fo_str);
    free(esc_uri);

    size_t cap = 4096;
    char *sql = calloc(1, cap);
    char buf[512];
    sqlite3_stmt *stmt = NULL;
    int rc;

    sqlite_lock();

    if (!has_res_fc) {
        desc_strcat(&sql, &cap, "SELECT id, ty, rn, ri, uri FROM general ");
        desc_strcat(&sql, &cap, general_where);
    } else {
        /* Collect the resource types present, then join each with its own table */
        char *ty_sql = malloc(strlen(general_where) + 64);
        sprintf(ty_sql, "SELECT DISTINCT ty FROM general %s;", general_where);
        rc = sqlite3_prepare_v2(db, ty_sql, -1, &stmt, NULL);
        free(ty_sql);
        if (rc != SQLITE_OK) {
            logger("DB", LOG_LEVEL_ERROR, "desc ty query failed: %s", sqlite3_errmsg(db));
            goto cleanup;
        }
        int joined = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int ty = sqlite3_column_int(stmt, 0);
            if (!desc_fc_needs_ty(fc, ty)) continue;
            char *tbl = get_table_name(ty);
            if (!tbl) continue;
            char *res_where = desc_build_res_where(fc, ty, fo_str);
            snprintf(buf, sizeof(buf), "%sSELECT id, ty, rn, ri, uri FROM general JOIN %s USING(id) ",
                     joined ? " UNION ALL " : "", tbl);
            desc_strcat(&sql, &cap, buf);
            desc_strcat(&sql, &cap, general_where);
            desc_strcat(&sql, &cap, res_where);
            free(res_where);
            joined++;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
        if (joined == 0) goto cleanup;
    }

    {
        char *inner = sql;
        size_t cap2 = strlen(inner) + 256;
        sql = calloc(1, cap2);
        snprintf(sql, cap2, "SELECT ri, ty, rn, uri FROM (%s) ORDER BY id %s LIMIT %d OFFSET %d;",
                 inner, sort_dir, lim, ofst);
        free(inner);
    }
    logger("DB", LOG_LEVEL_DEBUG, "desc SQL: %s", sql);

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        logger("DB", LOG_LEVEL_ERROR, "desc query failed: %s", sqlite3_errmsg(db));
        goto cleanup;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *ri  = (const char *)sqlite3_column_text(stmt, 0);
        int ty          = sqlite3_column_int(stmt, 1);
        const char *rn  = (const char *)sqlite3_column_text(stmt, 2);
        const char *uri = (const char *)sqlite3_column_text(stmt, 3);
        const char *id_val = (o2pt->drt == DRT_STRUCTURED) ? uri : ri;

        if (need_details) {
            cJSON *obj = db_get_resource((char *)ri, ty);
            if (!obj) continue;
            cJSON *pi = cJSON_GetObjectItem(obj, "pi");
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "ri", ri ? ri : "");
            cJSON_AddStringToObject(item, "pi", cJSON_IsString(pi) ? pi->valuestring : "");
            cJSON_AddNumberToObject(item, "ty", ty);
            cJSON_AddItemToObject(item, "obj", obj);
            cJSON_AddItemToArray(result, item);
        } else if (rcn == RCN_DISCOVERY_RESULT_REFERENCES) {
            cJSON_AddItemToArray(result, cJSON_CreateString(id_val ? id_val : ""));
        } else {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "nm", rn ? rn : "");
            cJSON_AddNumberToObject(item, "typ", ty);
            cJSON_AddStringToObject(item, "val", id_val ? id_val : "");
            cJSON_AddItemToArray(result, item);
        }
    }

cleanup:
    if (stmt) sqlite3_finalize(stmt);
    sqlite_unlock();
    free(sql);
    free(general_where);
    return result;
}
