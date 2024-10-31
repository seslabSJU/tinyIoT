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

#include "sqlite/sqlite3.h"
sqlite3 *db;
extern cJSON *ATTRIBUTES;
extern ResourceTree *rt;

/* DB init */
int init_dbp()
{
    sqlite3_stmt *res;
    char *sql = NULL, *err_msg = NULL;

    // Open (or create) DB File
    int rc = sqlite3_open("data.db", &db);

    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Cannot open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    // Setup Tables
    sql = calloc(1, 1024);

    strcpy(sql, "PRAGMA foreign_keys = ON;");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "foreign_keys error: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS general ( id INTEGER PRIMARY KEY AUTOINCREMENT, \
        rn VARCHAR(60), ri VARCHAR(40), pi VARCHAR(40), ct VARCHAR(30), et VARCHAR(30), lt VARCHAR(30), \
        uri VARCHAR(255), acpi VARCHAR(255), lbl VARCHAR(255), ty INT, memberOf VARCHAR(255) );");

    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[general]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS csr ( id INTEGER, \
        cst INT, poa VARCHAR(200), cb VARCHAR(200), csi VARCHAR(200), mei VARCHAR(45), \
        tri VARCHAR(45), rr INT, nl VARCHAR(45), srv VARCHAR(45), dcse VARCHAR(200), csz VARCHAR(100), \
        CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[csr]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS ae ( id INTEGER, \
        api VARCHAR(45), aei VARCHAR(200), rr INT, poa VARCHAR(255), apn VARCHAR(100), srv VARCHAR(45), at VARCHAR(200), aa VARCHAR(100), ast INT, \
        CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE  );");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[ae]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS cnt ( id INTEGER, \
        cr VARCHAR(40), mni INT, mbs INT, mia INT, st INT, cni INT, cbs INT, li VARCHAR(45), oref VARCHAR(45), disr VARCHAR(45), at VARCHAR(200), aa VARCHAR(100), ast INT, \
        CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[cnt]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS cin ( id INTEGER, \
        cs INT, cr VARCHAR(45), cnf VARCHAR(45), oref VARCHAR(45), con TEXT, st INT, at VARCHAR(200), aa VARCHAR(100), ast INT, \
        CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[cin]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS acp ( id INTEGER, pv VARCHAR(60), pvs VARCHAR(100), at VARCHAR(200), aa VARCHAR(100), ast INT, \
                CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[acp]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS sub ( id INTEGER, \
        enc VARCHAR(45), exc INT, nu VARCHAR(200), gpi VARCHAR(45), nfu VARCHAR(45), bn VARCHAR(45), rl VARCHAR(45), \
        sur VARCHAR(200), nct VARCHAR(45), net VARCHAR(45), cr VARCHAR(45), su VARCHAR(45), at VARCHAR(200), aa VARCHAR(100), ast INT, \
        CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[sub]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS grp ( id INTEGER, \
        cr VARCHAR(45), mt INT, cnm INT, mnm INT, mid TEXT, macp TEXT, mtv INT, csy INT, gn VARCHAR(30), at VARCHAR(200), aa VARCHAR(100), ast INT,\
        CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[grp]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS cb ( id INTEGER, \
        cst INT, csi VARCHAR(45), srt VARCHAR(100), poa VARCHAR(200), nl VARCHAR(45), ncp VARCHAR(45), srv VARCHAR(45), rr INT, at VARCHAR(200), aa VARCHAR(100), ast INT, \
        CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE  );");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[cb]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS cbA ( id INTEGER, \
        cst INT, lnk VARCHAR(100), csi VARCHAR(45), srt VARCHAR(100), poa VARCHAR(200), nl VARCHAR(45), ncp VARCHAR(45), srv VARCHAR(45), rr INT, \
        at VARCHAR(200), aa VARCHAR(100), ast INT,\
        CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE  );");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[cbA]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS aeA ( id INTEGER, \
        api VARCHAR(45), lnk VARCHAR(100), aei VARCHAR(200), rr INT, poa VARCHAR(255), apn VARCHAR(100), srv VARCHAR(45), at VARCHAR(200), aa VARCHAR(100), ast INT, \
        CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES general(id) ON DELETE CASCADE );");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[aeA]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    free(sql);
    return 1;
}

int close_dbp()
{
    if (db)
        sqlite3_close_v2(db);
    db = NULL;
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
    char *colname = NULL;
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
            cJSON_AddNumberToObject(resource, colname, sqlite3_column_int(stmt, col));
            break;
        }
    }

    sqlite3_finalize(stmt);
    free(sql);
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
    char *colname = NULL;
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
            cJSON_AddNumberToObject(resource, colname, sqlite3_column_int(stmt, col));
            break;
        }
    }

    sqlite3_finalize(stmt);
    free(sql);
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
    sqlite3_mutex_enter(sqlite3_db_mutex(db));
    char *sql = NULL, *err_msg = NULL;
    cJSON *pjson = NULL;
    cJSON *GENERAL_ATTR = cJSON_GetObjectItem(ATTRIBUTES, "general");
    int general_cnt = cJSON_GetArraySize(GENERAL_ATTR);

    ResourceType ty = cJSON_GetObjectItem(obj, "ty")->valueint;

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

    sqlite3_stmt *stmt;
    int rc = 0;
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &err_msg);

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_DEBUG, "%s", sql);
        free(sql);
        logger("DB", LOG_LEVEL_ERROR, "prepare error store_resource");
        sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
        sqlite3_finalize(stmt);
        sqlite3_mutex_leave(sqlite3_db_mutex(db));
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
        free(sql);
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert SQL: %s, msg : %s", sql, err_msg);
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
        sqlite3_mutex_leave(sqlite3_db_mutex(db));
        return -1;
    }

    sqlite3_finalize(stmt);

    cJSON *specific_attr = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(ty));

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

    logger("DB", LOG_LEVEL_DEBUG, "SQL: %s", sql);

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "prepare error %d", rc);
        free(sql);
        sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
        sqlite3_mutex_leave(sqlite3_db_mutex(db));
        return -1;
    }

    db_test_and_bind_value(stmt, 1, cJSON_GetObjectItem(obj, "ri"));

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
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert SQL: %s, msg : %s", sql, err_msg);
        free(sql);
        sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
        sqlite3_finalize(stmt);
        sqlite3_mutex_leave(sqlite3_db_mutex(db));
        return -1;
    }
    sqlite3_exec(db, "END TRANSACTION", NULL, NULL, &err_msg);
    if (err_msg)
    {
        logger("DB", LOG_LEVEL_ERROR, "Error: %s", err_msg);
    }

    sqlite3_finalize(stmt);
    free(sql);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return 1;
}

int db_update_resource(cJSON *obj, char *ri, ResourceType ty)
{
    sqlite3_mutex_enter(sqlite3_db_mutex(db));
    logger("DB", LOG_LEVEL_DEBUG, "Call db_update_resource [RI]: %s", ri);
    char *sql = NULL;
    cJSON *pjson = NULL;
    cJSON *GENERAL_ATTR = cJSON_GetObjectItem(ATTRIBUTES, "general");
    int general_cnt = cJSON_GetArraySize(GENERAL_ATTR);
    char *err_msg = NULL;

    int rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Failed Begin Transaction: %s", err_msg);
        sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
        sqlite3_mutex_leave(sqlite3_db_mutex(db));
        return 0;
    }
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
            sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
            sqlite3_mutex_leave(sqlite3_db_mutex(db));
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
            sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
            sqlite3_mutex_leave(sqlite3_db_mutex(db));
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
            sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
            sqlite3_mutex_leave(sqlite3_db_mutex(db));
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
            sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
            sqlite3_mutex_leave(sqlite3_db_mutex(db));
            return 0;
        }

        sqlite3_finalize(stmt);
    }

    sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
    free(sql);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
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
    sqlite3_mutex_enter(sqlite3_db_mutex(db));

    // sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &err_msg);
    sprintf(sql, "DELETE FROM general WHERE uri LIKE '%s/%%' OR ri='%s';", get_uri_rtnode(rtnode), ri);
    logger("DB", LOG_LEVEL_DEBUG, "SQL : %s", sql);
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource from %s/ msg : %s", tableName, err_msg);
        sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &err_msg);
        sqlite3_mutex_leave(sqlite3_db_mutex(db));
        return 0;
    }
    sqlite3_mutex_leave(sqlite3_db_mutex(db));

    return 1;
}

int db_delete_one_cin_mni(RTNode *cnt)
{
    char sql[1024] = {0};
    char *err_msg;
    sqlite3_stmt *res;
    int rc;

    char *latest_ri = NULL;
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
    char *colname = NULL;
    char sql[1024] = {0};
    char buf[256] = {0};

    sprintf(sql, "SELECT * FROM general WHERE ty != 4 AND ty != 5;");
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
                    cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res2, col)));
                    break;
                }
            }
            rc = sqlite3_step(res2);
        }
        sqlite3_finalize(res2);

        cJSON_DeleteItemFromObject(json, "id");

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

    char buf[256] = {0};
    char *pi = get_ri_rtnode(parent_rtnode);
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json, *root;
    sqlite3_stmt *res = NULL;
    char *colname = NULL;
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
    char *colname;
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
    char *ord = NULL, *colname;
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
    char *colname = NULL;
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
        sprintf(buf, " ri IN (SELECT ri FROM cin WHERE cs >= %d) ", pjson->valueint);
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }

    if ((pjson = cJSON_GetObjectItem(fc, "szb")))
    {
        sprintf(buf, " ri IN (SELECT ri FROM cin WHERE cs < %d) ", pjson->valueint);
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }

    if ((pjson = cJSON_GetObjectItem(fc, "sts")))
    {
        sprintf(buf, " ri IN (SELECT ri FROM cnt WHERE st < %d) ", pjson->valueint);
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }

    if ((pjson = cJSON_GetObjectItem(fc, "stb")))
    {
        sprintf(buf, " ri IN (SELECT ri FROM cnt WHERE st >= %d) ", pjson->valueint);
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
    char *colname = NULL;
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
    char *colname = NULL;
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