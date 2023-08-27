#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "onem2m.h"
#include "dbmanager.h"
#include "logger.h"
#include "jsonparser.h"
#include "util.h"

#include "sqlite/sqlite3.h"
sqlite3* db;
extern cJSON *ATTRIBUTES;

/* DB init */
int init_dbp(){
    sqlite3_stmt* res;
    char *sql = NULL, *err_msg = NULL;

    // Open (or create) DB File
    int rc = sqlite3_open("data.db", &db);
    
    if( rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    // Setup Tables
    sql = calloc(1, 1024);
    
    strcpy(sql, "CREATE TABLE IF NOT EXISTS general ( \
        rn VARCHAR(60), ri VARCHAR(40), pi VARCHAR(40), ct VARCHAR(30), et VARCHAR(30), lt VARCHAR(30), \
        uri VARCHAR(255), acpi VARCHAR(255), lbl VARCHAR(255), ty INT );");
    
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[general]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS csr ( \
        ri VARCHAR(200), cst VARCHAR(45), poa VARCHAR(200), cb VARCHAR(200), csi VARCHAR(200), mei VARCHAR(45), \
        tri VARCHAR(45), rr INT, nl VARCHAR(45), srv VARCHAR(45));");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[csr]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS ae ( \
        ri VARCHAR(40), api VARCHAR(45), aei VARCHAR(200), rr VARCHAR(10), poa VARCHAR(255), apn VARCHAR(100), srv VARCHAR(45));");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[ae]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS cnt ( \
        ri VARCHAR(40), cr VARCHAR(40), mni INT, mbs INT, mia INT, st INT, cni INT, cbs INT, li VARCHAR(45), oref VARCHAR(45), disr VARCHAR(45));");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[cnt]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS cin ( \
        ri VARCHAR(40), cs INT, cr VARCHAR(45), cnf VARCHAR(45), oref VARCHAR(45), con TEXT);");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[cin]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS acp ( ri VARCHAR(40), pv VARCHAR(60), pvs VARCHAR(100));");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[acp]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS sub ( \
        ri VARCHAR(40), enc VARCHAR(45), exc VARCHAR(45), nu VARCHAR(200), gpi VARCHAR(45), nfu VARCAHR(45), bn VARCHAR(45), rl VARCHAR(45), \
        sur VARCHAR(200), nct VARCHAR(45), net VARCHAR(45), cr VARCHAR(45), su VARCHAR(45));");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[sub]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS grp ( \
        ri VARCHAR(40), cr VARCHAR(45), mt INT, cnm INT, mnm INT, mid TEXT, macp TEXT, mtv INT, csy INT, gn VARCAHR(30));");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[grp]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS cb ( \
        ri VARCHAR(40), cst VARCHAR(45), csi VARCHAR(45), srt VARCHAR(100), poa VARCHAR(200), nl VARCHAR(45), ncp VARCHAR(45), srv VARCHAR(45), rr INT);");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table[cb]: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    free(sql);
    return 1;
}


int close_dbp(){
    if(db)
        sqlite3_close(db);
    db = NULL;
    return 1;
}

int db_store_cse(CSE *cse_object){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_cse");

    int rc = 0;
    char sql[512] = {0}, err_msg = NULL;

     // if input == NULL
    if (cse_object->ri == NULL) {
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }
    if (cse_object->rn == NULL) cse_object->rn = " ";
    if (cse_object->pi == NULL) cse_object->pi = " ";
    if (cse_object->ty == '\0') cse_object->ty = 0;
    if (cse_object->ct == NULL) cse_object->ct = " ";
    if (cse_object->lt == NULL) cse_object->lt = " ";
    if (cse_object->csi == NULL) cse_object->csi = " ";


    sprintf(sql, "INSERT INTO general (rn, ri, pi, ct, lt, ty, uri) VALUES ('%s', '%s', '%s', '%s', '%s', %d, '%s');",
                cse_object->rn, cse_object->ri, cse_object->pi, cse_object->ct,
                cse_object->lt, cse_object->ty, cse_object->uri);
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert SQL: %s, msg : %s", sql, err_msg);
        return 0;
    }

    sprintf(sql, "INSERT INTO cb (ri, csi, srt, srv) VALUES('%s', '%s', '%s', '%s');",
                cse_object->ri, cse_object->csi, cse_object->srt, cse_object->srv);
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert SQL: %s, msg : %s", sql, err_msg);
        return 0;
    }
    return 1;
}

char *get_table_name(ResourceType ty){
    char * tableName = NULL;
    switch(ty){
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
    }
    return tableName;
}

cJSON *db_get_resource(char *ri, ResourceType ty){
    char *sql = NULL;
    cJSON *resource = NULL;

    sql = malloc(1024);

    sprintf(sql, "SELECT * FROM general, %s WHERE general.ri='%s' and %s.ri='%s';", get_table_name(ty), ri, get_table_name(ty), ri);

    sqlite3_stmt *stmt;
    int rc = 0;
    char *err_msg = NULL;

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        free(sql);
        logger("DB", LOG_LEVEL_ERROR, "prepare error");
        return NULL;
    }

    if(sqlite3_step(stmt) != SQLITE_ROW){
        free(sql);
        return NULL;
    }

    resource = cJSON_CreateObject();
    int cols = sqlite3_column_count(stmt);
    char *colname = NULL;
    int bytes = 0;
    int coltype = 0;
    char buf[256] = {0};
    cJSON *arr = NULL;
    
    for(int col = 0 ; col < cols; col++){
        
        colname = sqlite3_column_name(stmt, col);
        bytes = sqlite3_column_bytes(stmt, col);
        coltype = sqlite3_column_type(stmt, col);

        if(bytes == 0) continue;
        if(cJSON_GetObjectItem(resource, colname)) continue;
        switch(coltype){
            case SQLITE_TEXT:
                memset(buf, 0, 256);
                strncpy(buf, sqlite3_column_text(stmt, col), bytes);
                arr = cJSON_Parse(buf);
                if(arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)){
                    cJSON_AddItemToObject(resource, colname, arr);
                }else{
                    cJSON_AddItemToObject(resource, colname, cJSON_CreateString(buf));
                }
                break;
            case SQLITE_INTEGER:
                cJSON_AddNumberToObject(resource, colname, sqlite3_column_int(stmt, col));
                break;
        }
    }

    return resource;

}

int db_store_resource(cJSON *obj, char *uri){
    char *sql = NULL, *err_msg = NULL;
    cJSON *pjson = NULL;
    cJSON *GENERAL_ATTR = cJSON_GetObjectItem(ATTRIBUTES, "general");
    int general_cnt = cJSON_GetArraySize(GENERAL_ATTR);

    ResourceType ty = cJSON_GetObjectItem(obj, "ty")->valueint;


    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg);

    sql = malloc(1024);
    sprintf(sql, "INSERT INTO general (");
    for(int i = 0 ; i < general_cnt ; i++){
        strcat(sql, cJSON_GetArrayItem(GENERAL_ATTR, i)->string);
        strcat(sql, ",");
    }

    sql[strlen(sql)-1] = ')';
    strcat(sql, " VALUES(");

    for(int i = 0 ; i < general_cnt ; i++){
        strcat(sql, "?,");
    }

    sql[strlen(sql)-1] = ')';
    strcat(sql, ";");

    sqlite3_stmt *stmt;
    int rc = 0;

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        free(sql);
        logger("DB", LOG_LEVEL_ERROR, "prepare error");
        return 0;
    }

    for(int i = 0 ; i < general_cnt ; i++){
        if(strcmp(cJSON_GetArrayItem(GENERAL_ATTR, i)->string, "uri") == 0){
            sqlite3_bind_text(stmt, i+1, uri, strlen(uri), SQLITE_STATIC);
            continue;
        }
        pjson = cJSON_GetObjectItem(obj, cJSON_GetArrayItem(GENERAL_ATTR, i)->string);
        if(pjson){
            db_test_and_bind_value(stmt, i+1, pjson);
        }else{
            sqlite3_bind_null(stmt, i+1);
        }
    }

    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE){
        free(sql);
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert SQL: %s, msg : %s", sql, err_msg);
        return 0;
    }

    sqlite3_finalize(stmt);

    cJSON *specific_attr = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(ty));

    sql[0] = '\0';
    sprintf(sql, "INSERT INTO %s (", get_table_name(ty));
    for(int i = 0 ; i < cJSON_GetArraySize(specific_attr) ; i++){
        strcat(sql, cJSON_GetArrayItem(specific_attr, i)->string);
        strcat(sql, ",");
    }

    sql[strlen(sql)-1] = ')';
    strcat(sql, " VALUES(");

    for(int i = 0 ; i < cJSON_GetArraySize(specific_attr) ; i++){
        strcat(sql, "?,");
    }

    sql[strlen(sql)-1] = ')';
    strcat(sql, ";");
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "prepare error");
        free(sql);
        return 0;
    }

    for(int i = 0 ; i < cJSON_GetArraySize(specific_attr) ; i++){
        pjson = cJSON_GetObjectItem(obj, cJSON_GetArrayItem(specific_attr, i)->string);
        if(pjson){
            db_test_and_bind_value(stmt, i+1, pjson);
        }else{
            sqlite3_bind_null(stmt, i+1);
        }
    }

    
    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert SQL: %s, msg : %s", sql, err_msg);
        free(sql);
        return 0;
    }
    
    sqlite3_finalize(stmt);
    free(sql);
    sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
    return 1;
}

int db_update_resource(cJSON *obj, char *ri, ResourceType ty){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_update_resource [RI]: %s", ri);
    char *sql = NULL;
    cJSON *pjson = NULL;
    cJSON *GENERAL_ATTR = cJSON_GetObjectItem(ATTRIBUTES, "general");
    int general_cnt = cJSON_GetArraySize(GENERAL_ATTR);
    char *err_msg = NULL;

    int rc = sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed Begin Transaction: %s", err_msg);
    }
    int idx = 0;
    sql = malloc(1024);
    sprintf(sql, "UPDATE general SET ");
    for(int i = 0 ; i < general_cnt ; i++){
        char *attr = cJSON_GetArrayItem(GENERAL_ATTR, i)->string;
        if(cJSON_GetObjectItem(obj, attr)){
            strcat(sql, attr);
            strcat(sql, "=?,");
            idx ++;
        }
    }

    sql[strlen(sql)-1] = '\0';
    strcat(sql, " WHERE ri=?;");

    sqlite3_stmt *stmt;
    rc = 0;

    if(idx > 0){
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if(rc != SQLITE_OK){
            free(sql);
            logger("DB", LOG_LEVEL_ERROR, "prepare error");
            return 0;
        }
        int idx = 1;
        for(int i = 0 ; i < general_cnt ; i++){
            pjson = cJSON_GetObjectItem(obj, cJSON_GetArrayItem(GENERAL_ATTR, i)->string);
            if(pjson){
                db_test_and_bind_value(stmt, idx, pjson);
                idx++;
            }
        }
        sqlite3_bind_text(stmt, idx, ri, -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE){
            free(sql);
            logger("DB", LOG_LEVEL_ERROR, "Failed Update SQL: %s, msg : %s", sql, err_msg);
            return 0;
        }

        sqlite3_finalize(stmt);
    }

    cJSON *specific_attr = cJSON_GetObjectItem(ATTRIBUTES, get_resource_key(ty));

    sql[0] = '\0';
    idx = 0;
    sprintf(sql, "UPDATE %s SET ", get_table_name(ty));
    for(int i = 0 ; i < cJSON_GetArraySize(specific_attr) ; i++){
        char *attr = cJSON_GetArrayItem(specific_attr, i)->string;
        if(cJSON_GetObjectItem(obj, attr)){
            strcat(sql, attr);
            strcat(sql, "=?,");
            idx ++;
        }
    }

    sql[strlen(sql)-1] = '\0';
    strcat(sql, " WHERE ri=?;");
    if(idx > 0){
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if(rc != SQLITE_OK){
            logger("DB", LOG_LEVEL_ERROR, "prepare error");
            free(sql);
            return 0;
        }

        idx = 1;
        for(int i = 0 ; i < cJSON_GetArraySize(specific_attr) ; i++){
            pjson = cJSON_GetObjectItem(obj, cJSON_GetArrayItem(specific_attr, i)->string);
            if(pjson){
                db_test_and_bind_value(stmt, idx, pjson);
                idx++;
            }
        }
        sqlite3_bind_text(stmt, idx, ri, -1, SQLITE_TRANSIENT);
        
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE){
            logger("DB", LOG_LEVEL_ERROR, "Failed Insert SQL: %s, msg : %s", sql, err_msg);
            free(sql);
            return 0;
        }
        
        sqlite3_finalize(stmt);
    }
    
    sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
    free(sql);
    return 1;
}

void db_test_and_bind_value(sqlite3_stmt *stmt, int index, cJSON *obj){
    if(!obj) return;
    char *temp = NULL;
    switch(obj->type){
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

void db_test_and_set_bind_text(sqlite3_stmt *stmt, int index, char* context){
    if(context)
        sqlite3_bind_text(stmt, index, context, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, index);
}

void db_test_and_set_bind_int(sqlite3_stmt *stmt, int index, int value){
    if(value >= 0)
        sqlite3_bind_int(stmt, index, value);
    else   
        sqlite3_bind_null(stmt, index);
}


int db_delete_onem2m_resource(RTNode *rtnode) {
    logger("DB", LOG_LEVEL_DEBUG, "Delete [RI] %s",get_ri_rtnode(rtnode));
    char sql[1024] = {0};
    char *err_msg;
    char *tableName = NULL;
    int rc; 

    tableName = get_table_name(rtnode->ty);
    if(!tableName){
        logger("DB", LOG_LEVEL_ERROR, "RTNode ty invalid");
        return 0;
    }

    sprintf(sql, "DELETE FROM general WHERE ri='%s';", get_ri_rtnode(rtnode));
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource from general/ msg : %s", err_msg);
        sqlite3_close(db);
        return 0;
    }


    sprintf(sql, "DELETE FROM %s WHERE ri='%s';", tableName, get_ri_rtnode(rtnode));
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource from %s/ msg : %s", tableName, err_msg);
        sqlite3_close(db);
        return 0;
    }

    if(rtnode->ty == RT_AE){
        int childResources[3] = {RT_CNT, RT_SUB, RT_GRP, RT_CIN};

        for(int i = 0 ; i < 3 ; i++){
            sprintf(sql, "DELETE FROM %s WHERE ri IN (SELECT ri FROM general where uri LIKE '%s/%%');", get_table_name(childResources[i]), get_uri_rtnode(rtnode));
            rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
            if(rc != SQLITE_OK){
                logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource from %s/ msg : %s", tableName, err_msg);
                sqlite3_close(db);
                return 0;
            }
        }
    }

    if(rtnode->ty == RT_CNT){
        int childResources[3] = {RT_CNT, RT_SUB, RT_GRP, RT_CIN};

        for(int i = 0 ; i < 3 ; i++){
            sprintf(sql, "DELETE FROM %s WHERE ri IN (SELECT ri FROM general where uri LIKE '%s/%%');", get_table_name(childResources[i]), get_uri_rtnode(rtnode));
            rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
            if(rc != SQLITE_OK){
                logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource from %s/ msg : %s", tableName, err_msg);
                sqlite3_close(db);
                return 0;
            }
        }
    }

    sprintf(sql, "DELETE FROM general WHERE uri LIKE '%s%%';", get_uri_rtnode(rtnode));
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource from %s/ msg : %s", tableName, err_msg);
        sqlite3_close(db);
        return 0;
    }
    return 1;
}


RTNode *db_get_all_csr_rtnode(){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_csr_rtnode");

    int rc = 0;
    int cols = 0;
    int coltype = 0, bytes = 0;
    cJSON *json, *root;
    sqlite3_stmt *res;
    char *colname = NULL;
    char sql[1024] = {0};
    char buf[256] = {0};

    sprintf(sql, "SELECT * FROM general, csr WHERE general.ri = csr.ri;");
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed select");
        return 0;
    }

    RTNode* head = NULL, *rtnode = NULL;

    rc = sqlite3_step(res);
    cols = sqlite3_column_count(res);
    cJSON *arr = NULL;
    while(rc == SQLITE_ROW){
        json = cJSON_CreateObject();
        
        for(int col = 0 ; col < cols; col++){
            
            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);

            if(bytes == 0) continue;
            if(cJSON_GetObjectItem(json, colname))
                continue;

            switch(coltype){
                case SQLITE_TEXT:
                    memset(buf, 0, 256);
                    strncpy(buf, sqlite3_column_text(res, col), bytes);
                    arr = cJSON_Parse(buf);
                    if(arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)){
                        cJSON_AddItemToObject(json, colname, arr);
                    }else{
                        cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                    }
                    break;
                case SQLITE_INTEGER:
                    cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                    break;
            }
        }
        logger("DB", LOG_LEVEL_DEBUG, "csr : %s", cJSON_PrintUnformatted(json));
        if(!head) {
            head = create_rtnode(json,RT_CSR);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(json, RT_CSR);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }  
        rc = sqlite3_step(res);   
    }
    sqlite3_finalize(res);
    return head;
}

RTNode *db_get_all_ae_rtnode(){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_ae_rtnode");

    int rc = 0;
    int cols = 0;
    int coltype = 0, bytes = 0;
    cJSON *json, *root;
    sqlite3_stmt *res;
    char *colname = NULL;
    char sql[1024] = {0};
    char buf[256] = {0};

    sprintf(sql, "SELECT * FROM general, ae WHERE general.ri = ae.ri;");
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed select");
        return 0;
    }

    RTNode* head = NULL, *rtnode = NULL;
    cJSON *arr = NULL;

    rc = sqlite3_step(res);
    cols = sqlite3_column_count(res);
    while(rc == SQLITE_ROW){
        json = cJSON_CreateObject();
        
        for(int col = 0 ; col < cols; col++){
            
            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);

            if(cJSON_GetObjectItem(json, colname))
                continue;

            if(bytes == 0) continue;
            switch(coltype){
                case SQLITE_TEXT:
                    memset(buf, 0, 256);
                    strncpy(buf, sqlite3_column_text(res, col), bytes);
                    arr = cJSON_Parse(buf);
                    if(arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)){
                        cJSON_AddItemToObject(json, colname, arr);
                    }else{
                        cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                    }
                    break;
                case SQLITE_INTEGER:
                    cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                    break;
            }
        }
        if(!head) {
            head = create_rtnode(json,RT_AE);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(json, RT_AE);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }  
        rc = sqlite3_step(res);
    }

    sqlite3_finalize(res); 
    return head;
}

RTNode *db_get_all_cnt_rtnode(){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_cnt_rtnode");

    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json, *root;
    sqlite3_stmt *res;
    char *colname = NULL;
    char sql[1024] = {0}, buf[256]={0};

    sprintf(sql, "SELECT * FROM general, cnt WHERE general.ri = cnt.ri;");
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed select");
        return 0;
    }

    RTNode* head = NULL, *rtnode = NULL;
    cJSON *arr = NULL;
    rc = sqlite3_step(res);
    cols = sqlite3_column_count(res);
    while(rc == SQLITE_ROW){
        json = cJSON_CreateObject();
        root = cJSON_CreateObject();
        
        for(int col = 0 ; col < cols; col++){
            
            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);

            if(bytes == 0) continue;
            if(cJSON_GetObjectItem(json, colname)) continue;

            switch(coltype){
                case SQLITE_TEXT:
                    memset(buf, 0, 256);
                    strncpy(buf, sqlite3_column_text(res, col), bytes);
                    arr = cJSON_Parse(buf);
                    if(arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)){
                        cJSON_AddItemToObject(json, colname, arr);
                    }else{
                        cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                    }
                    break;
                case SQLITE_INTEGER:
                    cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                    break;
            }
        }
        if(!head) {
            head = create_rtnode(json,RT_CNT);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(json, RT_CNT);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }   
        cJSON_Delete(root);
        rc = sqlite3_step(res);
    }

    sqlite3_finalize(res); 
    return head;
}

RTNode *db_get_all_sub_rtnode(){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_sub_rtnode");

    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json, *root;
    sqlite3_stmt *res;
    char *colname = NULL;
    char sql[1024] = {0}, buf[256] = {0};
    cJSON *arr = NULL;

    sprintf(sql, "SELECT * FROM general, sub WHERE general.ri = sub.ri;");
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed select");
        return 0;
    }

    RTNode* head = NULL, *rtnode = NULL;
    rc = sqlite3_step(res);

    while(rc == SQLITE_ROW){
        json = cJSON_CreateObject();
        root = cJSON_CreateObject();
        cols = sqlite3_column_count(res);
        for(int col = 0 ; col < cols; col++){
            
            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);

            if(bytes == 0) continue;
            if(cJSON_GetObjectItem(json, colname)) continue;
            switch(coltype){
                case SQLITE_TEXT:
                    memset(buf, 0, 256);
                    strncpy(buf, sqlite3_column_text(res, col), bytes);
                    arr = cJSON_Parse(buf);
                    if(arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)){
                        cJSON_AddItemToObject(json, colname, arr);
                    }else{
                        cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                    }
                    break;
                case SQLITE_INTEGER:
                    cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                    break;
            }
        }
        if(!head) {
            head = create_rtnode(json,RT_SUB);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(json, RT_SUB);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }   
        cJSON_Delete(root);
        root = NULL;
        rc = sqlite3_step(res);
    }

    sqlite3_finalize(res); 
    return head;
}

RTNode *db_get_all_acp_rtnode(){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_acp_rtnode");

    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json = NULL, *root = NULL;
    sqlite3_stmt *res;
    char *colname = NULL;
    char sql[1024] = {0}, buf[256] = {0};

    sprintf(sql, "SELECT * FROM general, acp WHERE general.ri = acp.ri;");
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed select");
        return 0;
    }

    RTNode* head = NULL, *rtnode = NULL;
    rc = sqlite3_step(res);
    cols = sqlite3_column_count(res);
    cJSON *arr = NULL;

    while(rc == SQLITE_ROW){
        json = cJSON_CreateObject();
        for(int col = 0 ; col < cols; col++){
            
            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);

            if(bytes == 0) continue;
            if(cJSON_GetObjectItem(json, colname)) continue;
            switch(coltype){
                case SQLITE_TEXT:
                    memset(buf, 0, 256);
                    strncpy(buf, sqlite3_column_text(res, col), bytes);
                    arr = cJSON_Parse(buf);
                    if(arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)){
                        cJSON_AddItemToObject(json, colname, arr);
                    }else{
                        cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                    }
                    break;
                case SQLITE_INTEGER:
                    cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                    break;
            }
        }
        if(!head) {
            head = create_rtnode(json,RT_ACP);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(json, RT_ACP);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }   
        rc = sqlite3_step(res);
    }

    sqlite3_finalize(res); 
    return head;
}

RTNode *db_get_all_grp_rtnode(){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_grp_rtnode");

    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json = NULL, *root = NULL;
    sqlite3_stmt *res;
    char *colname = NULL;
    char sql[1024] = {0}, buf[256] = {0};

    sprintf(sql, "SELECT * FROM general, grp WHERE general.ri = grp.ri;");
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed select");
        return 0;
    }

    RTNode* head = NULL, *rtnode = NULL;
    rc = sqlite3_step(res);

    cJSON *arr = NULL;
    while(rc == SQLITE_ROW){
        cols = sqlite3_column_count(res);
        json = cJSON_CreateObject();
        
        for(int col = 0 ; col < cols; col++){
            
            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);

            if(bytes == 0) continue;
            if(cJSON_GetObjectItem(json, colname)) continue;
            switch(coltype){
                case SQLITE_TEXT:
                    memset(buf, 0, 256);
                    strncpy(buf, sqlite3_column_text(res, col), bytes);
                    arr = cJSON_Parse(buf);
                    if(arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)){
                        cJSON_AddItemToObject(json, colname, arr);
                    }else{
                        cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                    }
                    break;
                case SQLITE_INTEGER:
                    cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                    break;
            }
        }
        
        if(!head) {
            head = create_rtnode(json, RT_GRP);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(json, RT_GRP);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }   
        root = NULL;
        rc = sqlite3_step(res);
    }

    sqlite3_finalize(res); 
    return head;
}

RTNode* db_get_cin_rtnode_list(RTNode *parent_rtnode) {
    logger("DB", LOG_LEVEL_DEBUG, "call db_get_cin_rtnode_list");

    char buf[256] = {0};
    char* pi = get_ri_rtnode(parent_rtnode);
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json, *root;
    sqlite3_stmt *res = NULL;
    char *colname = NULL;
    char *sql;

    sql = "SELECT * FROM 'general', 'cin' WHERE general.pi=? AND general.ri=cin.ri ORDER BY ct ASC;";
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed select, %d", rc);
        return 0;
    }
    sqlite3_bind_text(res, 1, pi, -1, NULL);
    
        
    RTNode* head = NULL, *rtnode = NULL;
    rc = sqlite3_step(res);
    cols = sqlite3_column_count(res);
    cJSON *arr = NULL;
    while(rc == SQLITE_ROW){
        json = cJSON_CreateObject();
        
        for(int col = 0 ; col < cols; col++){
            
            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);

            if(bytes == 0) continue;
            switch(coltype){
                case SQLITE_TEXT:
                    memset(buf,0, 256);
                    strncpy(buf, sqlite3_column_text(res, col), bytes);
                    arr = cJSON_Parse(buf);
                    if(arr && (arr->type == cJSON_Array || arr->type == cJSON_Object)){
                        cJSON_AddItemToObject(json, colname, arr);
                    }else{
                        cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                    }
                    break;
                case SQLITE_INTEGER:
                    cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                    break;
            }
        }
        if(!head) {
            head = create_rtnode(json,RT_CIN);
            rtnode = head;
        } else {
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

/**
 * get Latest/Oldest Content Instance
 * Oldest Flag = 0, Latest flag = 1
*/
cJSON *db_get_cin_laol(RTNode *parent_rtnode, int laol){
    char sql[1024] = {0}, buf[256] = {0};
    char *ord = NULL, *colname;
    int rc = 0;
    int cols, bytes, coltype;
    cJSON *json, *root;
    sqlite3_stmt *res = NULL;


    switch(laol){
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

    sprintf(sql, "SELECT * from general, cin WHERE pi='%s' AND general.ri = cin.ri ORDER BY general.lt %s LIMIT 1;", get_ri_rtnode(parent_rtnode), ord);
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed select");
        return 0;
    }

    if(sqlite3_step(res) != SQLITE_ROW){
        return 0;
    }
    cols = sqlite3_column_count(res);
    json = cJSON_CreateObject();
    
    for(int col = 0 ; col < cols; col++){
        
        colname = sqlite3_column_name(res, col);
        bytes = sqlite3_column_bytes(res, col);
        coltype = sqlite3_column_type(res, col);

        if(bytes == 0) continue;
        switch(coltype){
            case SQLITE_TEXT:
                memset(buf,0, 256);
                strncpy(buf, sqlite3_column_text(res, col), bytes);
                cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                break;
            case SQLITE_INTEGER:
                cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                break;
        }
    }

    sqlite3_finalize(res);
    return json;   

}
cJSON* db_get_filter_criteria(char *to, cJSON *fc) {

    logger("DB", LOG_LEVEL_DEBUG, "call db_get_filter_criteria");
    char buf[256] = {0};
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *pjson = NULL, *ptr = NULL;
    cJSON *result = NULL, *json = NULL, *prtjson = NULL, *chjson = NULL, *tmp = NULL;
    sqlite3_stmt *res = NULL;
    char *colname = NULL;
    char sql[1024] = {0};
    int jsize = 0, psize = 0 , chsize = 0;
    int fo = cJSON_GetNumberValue(cJSON_GetObjectItem(fc, "fo"));
    
    sprintf(buf, "SELECT uri, acpi FROM 'general' WHERE uri LIKE '%s/%%' AND ", to);
    strcat(sql, buf);

    if(pjson = cJSON_GetObjectItem(fc, "cra")){
        sprintf(buf, "ct>'%s' ", cJSON_GetStringValue(pjson));
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }
    if(pjson = cJSON_GetObjectItem(fc, "crb")){
        sprintf(buf, "ct<='%s' ", cJSON_GetStringValue(pjson));
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }
    if(pjson = cJSON_GetObjectItem(fc, "exa")){
        sprintf(buf, "et>'%s' ", cJSON_GetStringValue(pjson));
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }
    if(pjson = cJSON_GetObjectItem(fc, "exb")){
        sprintf(buf, "et<='%s' ", cJSON_GetStringValue(pjson));
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }

    if(pjson = cJSON_GetObjectItem(fc, "ms")){
        sprintf(buf, "lt>'%s' ", cJSON_GetStringValue(pjson));
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }
    if(pjson = cJSON_GetObjectItem(fc, "us")){
        sprintf(buf, "lt<='%s' ", cJSON_GetStringValue(pjson));
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }



    if(pjson = cJSON_GetObjectItem(fc, "lbl")){
        if(cJSON_IsArray(pjson)){
            cJSON_ArrayForEach(ptr, pjson){
                sprintf(buf, "lbl LIKE '%%\"%s\"%%' OR ", cJSON_GetStringValue(ptr));
                strcat(sql, buf);
            }
            sql[strlen(sql)- 3] = '\0';
        }else if(cJSON_IsString(pjson)){
            sprintf(buf, "lbl LIKE '%%\"%s\"%%'", cJSON_GetStringValue(pjson));
            strcat(sql, buf);
        }
        filterOptionStr(fo, sql);
    }

    if(pjson = cJSON_GetObjectItem(fc, "ty")){

        strcat(sql, "(");
        
        if(cJSON_IsArray(pjson)){
            cJSON_ArrayForEach(ptr, pjson){
                sprintf(buf, " ty = %d OR", ptr->valueint);
                strcat(sql, buf);
            }
            sql[strlen(sql) -2] = '\0';
        }else if(cJSON_IsNumber(pjson)){
            sprintf(buf, " ty = %d", cJSON_GetNumberValue(pjson));
            strcat(sql, buf);
        }        

        strcat(sql, ") ");
        filterOptionStr(fo, sql);
    }

    if(pjson = cJSON_GetObjectItem(fc, "sza")){
        sprintf(buf, " ri IN (SELECT ri FROM 'cin' WHERE cs >= %d) ", cJSON_GetNumberValue(pjson));
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }

    if(pjson = cJSON_GetObjectItem(fc, "szb")){
        sprintf(buf, " ri IN (SELECT ri FROM 'cin' WHERE cs < %d) ", cJSON_GetNumberValue(pjson));
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }

    if(pjson = cJSON_GetObjectItem(fc, "sts")){
        sprintf(buf, " ri IN (SELECT ri FROM 'cnt' WHERE st < %d) ", cJSON_GetNumberValue(pjson));
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }

    if(pjson = cJSON_GetObjectItem(fc, "stb")){
        sprintf(buf, " ri IN (SELECT ri FROM 'cnt' WHERE st >= %d) ", cJSON_GetNumberValue(pjson));
        strcat(sql, buf);
        filterOptionStr(fo, sql);
    }

    if(fo == FO_AND){
        sql[strlen(sql) - 4] = '\0';
    }else if(fo == FO_OR){
        sql[strlen(sql) - 3] = '\0';
    }
    strcat(sql, ";");

    

    logger("DB", LOG_LEVEL_DEBUG, "%s", sql);
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed select, %d", rc);
        return 0;
    }
    
    json = cJSON_CreateObject();
    rc = sqlite3_step(res);
    while(rc == SQLITE_ROW){
        bytes = sqlite3_column_bytes(res, 0);
        if(bytes == 0){
            logger("DB", LOG_LEVEL_ERROR, "empty URI");
            break;
        }
        logger("DB", LOG_LEVEL_DEBUG, "%s", sqlite3_column_text(res, 0));
        memset(buf,0, 256);
        strncpy(buf, sqlite3_column_text(res, 0), bytes);
        if( sqlite3_column_bytes(res, 1) > 0){
            cJSON_AddItemToObject(json, buf, cJSON_Parse(sqlite3_column_text(res, 1)));        
        }else{
            cJSON_AddItemToObject(json, buf, cJSON_CreateNull());
        }
        rc = sqlite3_step(res);
    }
    sqlite3_finalize(res); 

    logger("DB", LOG_LEVEL_DEBUG, "json\n%s", cJSON_PrintUnformatted(json));

    if(cJSON_GetObjectItem(fc, "pty") || cJSON_GetObjectItem(fc, "palb") || cJSON_GetObjectItem(fc, "patr")){
        logger("DB", LOG_LEVEL_DEBUG, "find parent");
        prtjson = db_get_parent_filter_criteria(to, fc);
        logger("DB", LOG_LEVEL_DEBUG, "%s", cJSON_Print(prtjson));
        cjson_merge_objs_by_operation(json, prtjson, fo);
        cJSON_Delete(prtjson);
        prtjson = NULL;
    }
    if(cJSON_GetObjectItem(fc, "chty") || cJSON_GetObjectItem(fc, "clbl") || cJSON_GetObjectItem(fc, "catr")){
        logger("DB", LOG_LEVEL_DEBUG, "find child");
        chjson = db_get_child_filter_criteria(to, fc);

        cjson_merge_objs_by_operation(json, chjson, fo);

        cJSON_Delete(chjson);
        chjson = NULL;
        logger("DB", LOG_LEVEL_DEBUG, "%s", cJSON_Print(json));    
    }
    
    return json;
}

bool do_uri_exist(cJSON *list, char *uri){
    char *ptr = NULL;
    int lsize = cJSON_GetArraySize(list);
    for(int i = 0 ; i < lsize ; i++ ){
        ptr = cJSON_GetArrayItem(list, i)->valuestring;
        if(!strcmp(ptr, uri)){
            return true;
        }
    }
    return false;
}

void cjson_merge_objs_by_operation(cJSON* obj1, cJSON* obj2, FilterOperation fo){
    cJSON *result = cJSON_CreateArray();
    cJSON *ptr = NULL;
    cJSON *pjson = NULL;
    int obj1size, obj2size;
    char buf[256] = {0};
    bool next = true;

    obj1size = cJSON_GetArraySize(obj1);
    obj2size = cJSON_GetArraySize(obj2);
    ptr = obj1->child;
    while (ptr){
        next = true;
        strcpy(buf, ptr->string);
        pjson = cJSON_GetObjectItem(obj2, buf);
        switch(fo){
            case FO_AND:
                if(!pjson){
                    ptr = ptr->next;
                    next = false;
                    cJSON_DeleteItemFromObject(obj1, buf);
                }
                break;
            case FO_OR:
                if(!pjson){
                    cJSON_AddItemToObject(obj1, buf, cJSON_Duplicate(ptr, 1));
                }
                break;
        }
        if(next)
            ptr = ptr->next;
    }

    ptr = obj2->child;
    while(ptr){
        strcpy(buf, ptr->string);
        pjson = cJSON_GetObjectItem(obj1, buf);
        switch(fo){
            case FO_AND:
                break;
            case FO_OR:
                if(!pjson){
                     cJSON_AddItemToObject(obj2, buf, cJSON_Duplicate(ptr, 1));
                }
                break;
        }
        ptr = ptr->next;
    }
}

cJSON *db_get_parent_filter_criteria(char *to, cJSON *fc){
    logger("DB", LOG_LEVEL_DEBUG, "call db_get_parent_filter_criteria");
    char buf[256] = {0};
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json, *pjson = NULL, *ptr = NULL;
    cJSON *chdjson;
    sqlite3_stmt *res = NULL;
    char *colname = NULL;
    char sql[1024] = {0};
    int jsize = 0;
    int fo = cJSON_GetNumberValue(cJSON_GetObjectItem(fc, "fo"));

    sprintf(sql, "SELECT uri FROM 'general' WHERE uri LIKE '%s/%%' AND ", to);

    if(pjson = cJSON_GetObjectItem(fc, "pty")){

        strcat(sql, "(");
        
        if(cJSON_IsArray(pjson)){
            cJSON_ArrayForEach(ptr, pjson){
                sprintf(buf, " ty = %d OR", ptr->valueint);
                strcat(sql, buf);
            }
            sql[strlen(sql) -2] = '\0';
        }else if(cJSON_IsNumber(pjson)){
            sprintf(buf, " ty = %d", pjson->valueint);
            strcat(sql, buf);
        }        

        strcat(sql, ") ");
        filterOptionStr(fo, sql);
    }
    
    if(pjson = cJSON_GetObjectItem(fc, "palb")){
        cJSON_ArrayForEach(ptr, pjson){
            sprintf(buf, "lbl LIKE '%%\"%s\"%%' OR ", cJSON_GetStringValue(ptr));
            strcat(sql, buf);
        }
        sql[strlen(sql)- 3] = '\0';
        filterOptionStr(fo, sql);
    }

    // if(fc->patr){ //TODO - patr
    //     sprintf(buf, "%s LIKE %s",);
    // }
    
    if(fo == FO_AND){
        sql[strlen(sql)- 4] = '\0';
    }else if(fo == FO_OR){
        sql[strlen(sql) - 3] = '\0';
    }

    strcat(sql, ";");

    logger("DB", LOG_LEVEL_DEBUG, "%s", sql);
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed select, %d", rc);
        return 0;
    }
    json = cJSON_CreateArray();
    rc = sqlite3_step(res);
    while(rc == SQLITE_ROW){
        bytes = sqlite3_column_bytes(res, 0);
        if(bytes == 0){
            logger("DB", LOG_LEVEL_ERROR, "empty URI");
            cJSON_AddItemToArray(json, cJSON_CreateString("Internal Server ERROR"));
        }else{
            memset(buf,0, 256);
            strncpy(buf, sqlite3_column_text(res, 0), bytes);
            cJSON_AddItemToArray(json, cJSON_CreateString(buf));
        }
        rc = sqlite3_step(res);
    }
    sqlite3_finalize(res);

    jsize = cJSON_GetArraySize(json);
    chdjson = cJSON_CreateObject();

    for(int i = 0 ; i < jsize ; i++){
        sprintf(sql, "SELECT uri,acpi FROM general WHERE uri LIKE '%s/%%' AND uri NOT LIKE '%s/%%/%%';", 
                        cJSON_GetArrayItem(json, i)->valuestring, cJSON_GetArrayItem(json, i)->valuestring);

        if(rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL)){
            logger("DB", LOG_LEVEL_ERROR, "Failed select, %d", rc);
            return 0;
        }
        rc = sqlite3_step(res);
        while(rc == SQLITE_ROW){
            bytes = sqlite3_column_bytes(res, 0);
            if(bytes == 0){
                logger("DB", LOG_LEVEL_ERROR, "empty URI");
                cJSON_AddItemToArray(chdjson, cJSON_CreateString("Internal Server ERROR"));
            }

            memset(buf,0, 256);
            strncpy(buf, sqlite3_column_text(res, 0), bytes);
            if( sqlite3_column_bytes(res, 1) > 0){
                cJSON_AddItemToObject(chdjson, buf, cJSON_Parse(sqlite3_column_text(res, 1)));        
            }else{
                cJSON_AddItemToObject(chdjson, buf, cJSON_CreateNull());
            }
            rc = sqlite3_step(res);
        }
        sqlite3_finalize(res);
    }
    cJSON_Delete(json);

   
    return chdjson;
}

cJSON *db_get_child_filter_criteria(char *to, cJSON *fc){
    logger("DB", LOG_LEVEL_DEBUG, "call db_get_child_filter_criteria");
    char buf[256] = {0};
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json;
    cJSON *ptrjson;
    cJSON *pjson = NULL, *ptr = NULL;
    sqlite3_stmt *res = NULL;
    char *turi = NULL, *cptr = NULL;
    char sql[1024] = {0};
    int jsize = 0;
    int fo = cJSON_GetNumberValue(cJSON_GetObjectItem(fc, "fo"));

    strcat(sql, "SELECT uri, acpi FROM 'general' WHERE ");

    if(pjson = cJSON_GetObjectItem(fc, "chty")){
        if(cJSON_IsArray(pjson)){
            cJSON_ArrayForEach(ptr, pjson){
                sprintf(buf, " ty = %d OR", cJSON_GetNumberValue(ptr));
                strcat(sql, buf);
            }
            sql[strlen(sql) -2] = '\0';
        }else{
            sprintf(buf, " ty = %d ", cJSON_GetNumberValue(pjson));
            strcat(sql, buf);
        }
        filterOptionStr(fo, sql);
    }
    
    if(pjson = cJSON_GetObjectItem(fc, "clbl")){
        cJSON_ArrayForEach(ptr, pjson){
            sprintf(buf, "lbl LIKE '%%,%s,%%' OR ", cJSON_GetStringValue(ptr));
            strcat(sql, buf);
        }
        sql[strlen(sql)- 3] = '\0';
        filterOptionStr(fo, sql);
    }

    // if(fc->catr){ //TODO - catr
    //     sprintf(buf, "%s LIKE %s",);
    // }

    if(fo == FO_AND){
        sql[strlen(sql)- 4] = '\0';
    }else if(fo == FO_OR){
        sql[strlen(sql) - 3] = '\0';
    }

    sprintf(buf, " AND uri LIKE '%s/%%';", to);
    strcat(sql, buf);

    logger("DB", LOG_LEVEL_DEBUG, "%s", sql);
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed select, %d", rc);
        return 0;
    }
    json = cJSON_CreateObject();
    rc = sqlite3_step(res);
    while(rc == SQLITE_ROW){
        bytes = sqlite3_column_bytes(res, 0);
        if(bytes == 0){
            logger("DB", LOG_LEVEL_ERROR, "empty URI");
            cJSON_AddItemToArray(json, cJSON_CreateString("Internal Server ERROR"));
        }

        memset(buf,0, 256);
        strncpy(buf, sqlite3_column_text(res, 0), bytes);

        cptr = strrchr(buf, '/');
        *cptr = '\0';

        if( sqlite3_column_bytes(res, 1) > 0){
            cJSON_AddItemToObject(json, buf, cJSON_Parse(sqlite3_column_text(res, 1)));        
        }else{
            cJSON_AddItemToObject(json, buf, cJSON_CreateNull());
        }   
        rc = sqlite3_step(res);
    }
    sqlite3_finalize(res);   
    return json;
}