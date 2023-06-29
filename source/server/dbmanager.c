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

#ifdef SQLITE_DB
#include "sqlite/sqlite3.h"
sqlite3* db;
#else
#include <db.h>

DB *resourceDBp;
DB *subDBp;
DB *grpDBp;
DB *acpDBp;

/*DB CREATE*/
DB* DB_CREATE_(DB *dbp){
    int ret;

    ret = db_create(&dbp, NULL, 0);
    if (ret) {
        fprintf(stderr, "db_create : %s\n", db_strerror(ret));
        fprintf(stderr, "File ERROR\n");
        return NULL;
    }
    return dbp;
}

/*DB Open*/
int DB_OPEN(DB* dbp, char* DATABASE){
    int ret;

    ret = dbp->open(dbp, NULL, DATABASE, NULL, DB_BTREE, DB_CREATE, 0644);
    if (ret) {
        dbp->err(dbp, ret, "%s", DATABASE);
        fprintf(stderr, "DB Open ERROR\n");
        return 0;
    }
    return 1;
}

/*DB Get Cursor*/
DBC *DB_GET_CURSOR(DB *dbp){
    DBC *dbcp;
    int ret;
    
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        fprintf(stderr, "Cursor ERROR");
        return NULL;
    }
    return dbcp;
}

#endif

/* DB init */
int init_dbp(){
    #ifdef SQLITE_DB
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
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS ae ( \
        ri VARCHAR(40), api VARCHAR(45), aei VARCHAR(200), rr VARCHAR(10), poa VARCHAR(255), apn VARCHAR(100), srv VARCHAR(45));");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS cnt ( \
        ri VARCHAR(40), cr VARCHAR(40), mni INT, mbs INT, mia INT, st INT, cni INT, cbs INT, li VARCHAR(45), oref VARCHAR(45), disr VARCHAR(45));");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS cin ( \
        ri VARCHAR(40), cs INT, cr VARCHAR(45), cnf VARCHAR(45), oref VARCHAR(45), con TEXT);");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS acp ( ri VARCHAR(40), pvacop VARCHAR(60), pvacor VARCHAR(100), pvsacop VARCHAR(60), pvsacor VARCHAR(100));");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS sub ( \
        ri VARCHAR(40), enc VARCHAR(45), exc VARCHAR(45), nu VARCHAR(200), gpi VARCHAR(45), nfu VARCAHR(45), bn VARCHAR(45), rl VARCHAR(45), \
        sur VARCHAR(200), nct VARCHAR(45), net VARCHAR(45), cr VARCHAR(45), su VARCHAR(45));");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS grp ( \
        ri VARCHAR(40), cr VARCHAR(45), mt INT, cnm INT, mnm INT, mid TEXT, macp TEXT, mtv INT, csy INT, gn VARCAHR(30));");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }

    strcpy(sql, "CREATE TABLE IF NOT EXISTS cb ( \
        ri VARCHAR(40), cst VARCHAR(45), csi VARCHAR(45), srt VARCHAR(100), poa VARCHAR(200), nl VARCHAR(45), ncp VARCHAR(45), srv VARCHAR(45));");
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot create table: %s", err_msg);
        sqlite3_close(db);
        free(sql);
        return 0;
    }


    free(sql);
    #else
    int ret;
    logger("DB", LOG_LEVEL_DEBUG, "Initializing DB Pointer");
    ret = db_create(&resourceDBp, NULL, 0);
    if(ret) {
        fprintf(stderr, "db_create : %s\n", db_strerror(ret));
        fprintf(stderr, "File ERROR\n");
        return 0;   
    }
    ret = db_create(&subDBp, NULL, 0);
    if(ret) {
        fprintf(stderr, "db_create : %s\n", db_strerror(ret));
        fprintf(stderr, "File ERROR\n");
        return 0;   
    }

    ret = db_create(&grpDBp, NULL, 0);
    if(ret) {
        fprintf(stderr, "db_create : %s\n", db_strerror(ret));
        fprintf(stderr, "File ERROR\n");
        return 0;   
    }
    ret = db_create(&acpDBp, NULL, 0);
    if(ret) {
        fprintf(stderr, "db_create : %s\n", db_strerror(ret));
        fprintf(stderr, "File ERROR\n");
        return 0;   
    }

    DB_OPEN(resourceDBp, "RESOURCE.db");
    DB_OPEN(subDBp, "SUB.db");
    DB_OPEN(grpDBp, "GROUP.db");
    DB_OPEN(acpDBp, "ACP.db");

    #endif
    return 1;
}


int close_dbp(){
    #ifdef SQLITE_DB
    if(db)
        sqlite3_close(db);
    db = NULL;
    #else
    resourceDBp->close(resourceDBp, 0);
    subDBp->close(subDBp, 0);
    grpDBp->close(grpDBp, 0);
    acpDBp->close(acpDBp, 0);
    #endif
    return 1;
}

int db_store_cse(CSE *cse_object){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_cse");

    #ifdef SQLITE_DB
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

    sprintf(sql, "INSERT INTO cb (ri, csi) VALUES('%s', '%s');",
                cse_object->ri, cse_object->csi);
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert SQL: %s, msg : %s", sql, err_msg);
        return 0;
    }
    #else
    DBC* dbcp;
    int ret;        // template value

    DBT key_ri;
    DBT data;  // storving key and real data


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


    dbcp = DB_GET_CURSOR(resourceDBp);
    
    /* key and data must initialize */
    memset(&key_ri, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));

    /* initialize the data to be the first of two duplicate records. */
    key_ri.data = cse_object->ri;
    key_ri.size = strlen(cse_object->ri) + 1;

    /* List data excluding 'ri' as strings using delimiters. */
    char str[DB_STR_MAX]= "\0";
    sprintf(str, "%s;%s;%d;%s;%s;%s",
            cse_object->rn,cse_object->pi,cse_object->ty,cse_object->ct,cse_object->lt,cse_object->csi);

    data.data = str;
    data.size = strlen(str) + 1;

    /* input DB */
    if ((ret = dbcp->put(dbcp, &key_ri, &data, DB_KEYLAST)) != 0)
        resourceDBp->err(resourceDBp, ret, "db->cursor");

    /* DB close */
    dbcp->close(dbcp);
    
    #endif
    
    return 1;
}

int db_store_ae(AE *ae_object){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_ae");
  
    #ifdef SQLITE_DB
    char *err_msg = NULL;
    int rc = 0;
    //char sql[1024] = {0};
    char *sql;
    sqlite3_stmt *res;

        // db handle
    char rr[6] ="";

    sql = "INSERT INTO general (rn, ri, pi, ct, et, lt, uri, acpi, lbl, ty) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    // if input == NULL
    if (ae_object->ri == NULL) {
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }

    if(ae_object->rr == false) strcpy(rr, "false");
    else strcpy(rr, "true");

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg);

    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if( rc != SQLITE_OK ){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert msg : %s", sqlite3_errmsg(db));
        return 0;
    }

    db_test_and_set_bind_text(res, 1, ae_object->rn);
    sqlite3_bind_text(res, 2, ae_object->ri, -1, SQLITE_STATIC);
    db_test_and_set_bind_text(res, 3, ae_object->pi);
    db_test_and_set_bind_text(res, 4, ae_object->ct);
    db_test_and_set_bind_text(res, 5, ae_object->et);
    db_test_and_set_bind_text(res, 6, ae_object->lt);
    db_test_and_set_bind_text(res, 7, ae_object->uri);
    db_test_and_set_bind_text(res, 8, ae_object->acpi);
    db_test_and_set_bind_text(res, 9, ae_object->lbl);
    db_test_and_set_bind_int(res, 10, RT_AE);

    rc = sqlite3_step(res);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert msg : %s", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_finalize(res);


    sql = "INSERT INTO ae (ri, api, aei, rr, srv) VALUES(?, ?, ?, ?, ?);";
    sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    sqlite3_bind_text(res, 1, ae_object->ri, -1, SQLITE_STATIC);
    db_test_and_set_bind_text(res, 2, ae_object->api);
    db_test_and_set_bind_text(res, 3, ae_object->aei);
    db_test_and_set_bind_text(res, 4, rr);
    db_test_and_set_bind_text(res, 5, ae_object->srv);


    rc = sqlite3_step(res);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert msg2 : %s", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
    sqlite3_finalize(res);

    #else
    char* blankspace = " ";

        // db handle
    DBC* dbcp;
    int ret;        // template value

    DBT key_ri;
    DBT data;  // storving key and real data
    char rr[6] ="";

    // if input == NULL
    if (ae_object->ri == NULL) {
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }
    if (ae_object->rn == NULL) ae_object->rn = blankspace;
    if (ae_object->pi == NULL) ae_object->pi = blankspace;
    if (ae_object->ty == '\0') ae_object->ty = 0;
    if (ae_object->ct == NULL) ae_object->ct = blankspace;
    if (ae_object->lt == NULL) ae_object->lt = blankspace;
    if (ae_object->et == NULL) ae_object->et = blankspace;
    if (ae_object->api == NULL) ae_object->api = blankspace;
    if (ae_object->aei == NULL) ae_object->aei = blankspace;
    if (ae_object->lbl == NULL) ae_object->lbl = blankspace;
    if (ae_object->srv == NULL) ae_object->srv = blankspace;
    if (ae_object->acpi == NULL) ae_object->acpi = blankspace;
    if (ae_object->origin == NULL) ae_object->origin = blankspace;
    if (ae_object->rr == false) strcpy(rr, "false");
    else strcpy(rr, "true");

    ////dbp = DB_CREATE_(dbp);
    //DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(resourceDBp);
    
    /* key and data must initialize */
    memset(&key_ri, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));

    /* initialize the data to be the first of two duplicate records. */
    key_ri.data = ae_object->ri;
    key_ri.size = strlen(ae_object->ri) + 1;

    /* List data excluding 'ri' as strings using delimiters. */
    char str[DB_STR_MAX]= "\0";
    sprintf(str, "%s;%s;%d;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s",
            ae_object->rn,ae_object->pi,ae_object->ty,ae_object->ct,ae_object->lt,
            ae_object->et,ae_object->api,rr,ae_object->aei,ae_object->lbl,ae_object->srv,ae_object->acpi,ae_object->origin);

    data.data = str;
    data.size = strlen(str) + 1;

    /* input DB */
    if ((ret = dbcp->put(dbcp, &key_ri, &data, DB_KEYLAST)) != 0)
        resourceDBp->err(resourceDBp, ret, "db->cursor");

    /* DB close */
    dbcp->close(dbcp);

    if (ae_object->rn == blankspace) ae_object->rn = NULL;
    if (ae_object->pi == blankspace) ae_object->pi = NULL;
    if (ae_object->ty == '\0') ae_object->ty = 0;
    if (ae_object->ct == blankspace) ae_object->ct = NULL;
    if (ae_object->lt == blankspace) ae_object->lt = NULL;
    if (ae_object->et == blankspace) ae_object->et = NULL;
    if (ae_object->api == blankspace) ae_object->api = NULL;
    if (ae_object->aei == blankspace) ae_object->aei = NULL;
    if (ae_object->lbl == blankspace) ae_object->lbl = NULL;
    if (ae_object->srv == blankspace) ae_object->srv = NULL;
    if (ae_object->acpi == blankspace) ae_object->acpi = NULL;
    if (ae_object->origin == blankspace) ae_object->origin = NULL;
    #endif

    
    return 1;

}

int db_store_cnt(CNT *cnt_object){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_cnt");

    #ifdef SQLITE_DB
    sqlite3_stmt *res;
    char *sql, *err_msg = NULL;
    int rc = 0;
    char attrs[128]={0}, vals[512]={0}, buf[256]={0};

    if (cnt_object->ri == NULL) {
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }

    sql = "INSERT INTO general (rn, ri, pi, ct, et, lt, uri, acpi, lbl, ty) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg);

    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if( rc != SQLITE_OK ){
        logger("DB", LOG_LEVEL_ERROR, "Failed prepare msg : %s", sqlite3_errmsg(db));
        return 0;
    }

    db_test_and_set_bind_text(res, 1, cnt_object->rn);
    sqlite3_bind_text(res, 2, cnt_object->ri, -1, SQLITE_STATIC);
    db_test_and_set_bind_text(res, 3, cnt_object->pi);
    db_test_and_set_bind_text(res, 4, cnt_object->ct);
    db_test_and_set_bind_text(res, 5, cnt_object->et);
    db_test_and_set_bind_text(res, 6, cnt_object->lt);
    db_test_and_set_bind_text(res, 7, cnt_object->uri);
    db_test_and_set_bind_text(res, 8, cnt_object->acpi);
    db_test_and_set_bind_text(res, 9, cnt_object->lbl);
    db_test_and_set_bind_int(res, 10, RT_CNT);

    rc = sqlite3_step(res);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert msg : %s", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_finalize(res);

    sql = "INSERT INTO cnt (ri, mni, mbs, cni, cbs, st) VALUES(?, ?, ?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if( rc != SQLITE_OK ){
        logger("DB", LOG_LEVEL_ERROR, "Failed prepare msg : %s", sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_text(res, 1, cnt_object->ri, -1, SQLITE_STATIC);
    db_test_and_set_bind_int(res, 2, cnt_object->mni);
    db_test_and_set_bind_int(res, 3, cnt_object->mbs);
    db_test_and_set_bind_int(res, 4, cnt_object->cni);
    db_test_and_set_bind_int(res, 5, cnt_object->cbs);
    db_test_and_set_bind_int(res, 6, cnt_object->st);
    
    rc = sqlite3_step(res);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert msg : %s", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
    sqlite3_finalize(res);


    #else
    char* blankspace = " ";

        // db handle
    DBC* dbcp;
    int ret;        // template value

    DBT key_ri;
    DBT data;  // storving key and real data
    
    // if input == NULL
    if (cnt_object->ri == NULL) {
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }
    if (cnt_object->rn == NULL) cnt_object->rn = blankspace;
    if (cnt_object->pi == NULL) cnt_object->pi = blankspace;
    if (cnt_object->ct == NULL) cnt_object->ct = blankspace;
    if (cnt_object->lt == NULL) cnt_object->lt = blankspace;
    if (cnt_object->et == NULL) cnt_object->et = blankspace;
    if (cnt_object->acpi == NULL) cnt_object->acpi = blankspace;
    if (cnt_object->lbl == NULL) cnt_object->lbl = blankspace;

    ////dbp = DB_CREATE_(dbp);
    //DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(resourceDBp);
    
    /* key and data must initialize */
    memset(&key_ri, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));

    /* initialize the data to be the first of two duplicate records. */
    key_ri.data = cnt_object->ri;
    key_ri.size = strlen(cnt_object->ri) + 1;

    /* List data excluding 'ri' as strings using delimiters. */
    char str[DB_STR_MAX]= "\0";
    sprintf(str, "%s;%s;%d;%s;%s;%s;%s;%s;%d;%d;%d;%d;%d",
            cnt_object->rn,cnt_object->pi,cnt_object->ty,cnt_object->ct,cnt_object->lt,cnt_object->et,
            cnt_object->lbl,cnt_object->acpi,cnt_object->cbs,cnt_object->cni,cnt_object->st,cnt_object->mni,cnt_object->mbs);

    data.data = str;
    data.size = strlen(str) + 1;

    /* input DB */
    if ((ret = dbcp->put(dbcp, &key_ri, &data, DB_KEYLAST)) != 0)
        resourceDBp->err(resourceDBp, ret, "db->cursor");

    logger("DB", LOG_LEVEL_DEBUG, "%d", ret);

    /* DB close */
    dbcp->close(dbcp);
    //dbp->close(dbp, 0); 

    if (cnt_object->rn == blankspace) cnt_object->rn = NULL;
    if (cnt_object->pi == blankspace) cnt_object->pi = NULL;
    if (cnt_object->ct == blankspace) cnt_object->ct = NULL;
    if (cnt_object->lt == blankspace) cnt_object->lt = NULL;
    if (cnt_object->et == blankspace) cnt_object->et = NULL;
    if (cnt_object->acpi == blankspace) cnt_object->acpi = NULL;
    if (cnt_object->lbl == blankspace) cnt_object->lbl = NULL;
    #endif

    return 1;
}

int db_store_cin(CIN *cin_object) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_cin");
    
    #ifdef SQLITE_DB
    char *sql = NULL, *err_msg = NULL;
    int rc = 0;
    sqlite3_stmt *res;
    
    // if input == NULL
    if (cin_object->ri == NULL) {
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }


    sql = "INSERT INTO general (rn, ri, pi, ct, et, lt, uri, ty) VALUES(?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg);

    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if( rc != SQLITE_OK ){
        logger("DB", LOG_LEVEL_ERROR, "Failed prepare msg : %s", sqlite3_errmsg(db));
        return 0;
    }

    db_test_and_set_bind_text(res, 1, cin_object->rn);
    sqlite3_bind_text(res, 2, cin_object->ri, -1, SQLITE_STATIC);
    db_test_and_set_bind_text(res, 3, cin_object->pi);
    db_test_and_set_bind_text(res, 4, cin_object->ct);
    db_test_and_set_bind_text(res, 5, cin_object->et);
    db_test_and_set_bind_text(res, 6, cin_object->lt);
    db_test_and_set_bind_text(res, 7, cin_object->uri);
    db_test_and_set_bind_int(res, 8, RT_CIN);

    rc = sqlite3_step(res);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert msg : %s", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_finalize(res);

    sql = "INSERT INTO cin (ri, cs, con) VALUES(?, ?, ?);";
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if( rc != SQLITE_OK ){
        logger("DB", LOG_LEVEL_ERROR, "Failed prepare msg : %s", sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_text(res, 1, cin_object->ri, -1, SQLITE_STATIC);
    db_test_and_set_bind_int(res, 2, cin_object->cs);
    db_test_and_set_bind_text(res, 3, cin_object->con);

    rc = sqlite3_step(res);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert msg : %s", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
    sqlite3_finalize(res);

    #else
        // db handle
    DBC* dbcp;
    int ret;        // template value

    DBT key_ri;
    DBT data;  // storving key and real data
    
    // if input == NULL
    if (cin_object->ri == NULL) {
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }
    if (cin_object->rn == NULL) cin_object->rn = " ";
    if (cin_object->pi == NULL) cin_object->pi = "NULL";
    if (cin_object->ty == '\0') cin_object->ty = 0;
    if (cin_object->ct == NULL) cin_object->ct = " ";
    if (cin_object->lt == NULL) cin_object->lt = " ";
    if (cin_object->et == NULL) cin_object->et = " ";

    if (cin_object->con == NULL) cin_object->con = " ";
    if (cin_object->cs == '\0') cin_object->cs = 0;
    if (cin_object->st == '\0') cin_object->st = 0;

    ////dbp = DB_CREATE_(dbp);
    //DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(resourceDBp);
    
    /* key and data must initialize */
    memset(&key_ri, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));

    
    /* initialize the data to be the first of two duplicate records. */
    key_ri.data = cin_object->ri;
    key_ri.size = strlen(cin_object->ri) + 1;

    
    /* List data excluding 'ri' as strings using delimiters. */
    char str[DB_STR_MAX]= "\0";
    sprintf(str, "%s;%s;%d;%s;%s;%s;%s;%d;%d",
            cin_object->rn,cin_object->pi,cin_object->ty,cin_object->ct,cin_object->lt,cin_object->et,
            cin_object->con,cin_object->cs,cin_object->st);

    data.data = str;
    data.size = strlen(str) + 1;
    /* input DB */
    if ((ret = dbcp->put(dbcp, &key_ri, &data, DB_KEYLAST)) != 0)
        resourceDBp->err(resourceDBp, ret, "db->cursor");

    /* DB close */
    dbcp->close(dbcp);
    //dbp->close(dbp, 0);
    #endif

    return 1;
}

int db_store_grp(GRP *grp_object){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_grp");

    #ifdef SQLITE_DB
    char *sql, *err_msg = NULL;
    int rc = 0;
    sqlite3_stmt *res = NULL;

    if(grp_object->ri == NULL){
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }

    sql = "INSERT INTO general (rn, ri, pi, ct, et, lt, uri, acpi, ty) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg);

    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if( rc != SQLITE_OK ){
        logger("DB", LOG_LEVEL_ERROR, "Failed prepare msg : %s", sqlite3_errmsg(db));
        return 0;
    }

    db_test_and_set_bind_text(res, 1, grp_object->rn);
    sqlite3_bind_text(res, 2, grp_object->ri, -1, SQLITE_STATIC);
    db_test_and_set_bind_text(res, 3, grp_object->pi);
    db_test_and_set_bind_text(res, 4, grp_object->ct);
    db_test_and_set_bind_text(res, 5, grp_object->et);
    db_test_and_set_bind_text(res, 6, grp_object->lt);
    db_test_and_set_bind_text(res, 7, grp_object->uri);
    db_test_and_set_bind_text(res, 8, grp_object->acpi);
    db_test_and_set_bind_int(res, 9, RT_GRP);

    rc = sqlite3_step(res);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert msg : %s", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_finalize(res);

    sql = "INSERT INTO grp(ri, mt, cnm, mnm, mtv, csy, mid) VALUES(?,?,?,?,?,?,?);";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if( rc != SQLITE_OK ){
        logger("DB", LOG_LEVEL_ERROR, "Failed prepare msg : %s", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_bind_text(res, 1, grp_object->ri, -1, SQLITE_STATIC);
    db_test_and_set_bind_int(res, 2, grp_object->mt);
    db_test_and_set_bind_int(res, 3, grp_object->cnm);
    db_test_and_set_bind_int(res, 4, grp_object->mnm);
    db_test_and_set_bind_int(res, 5, grp_object->mtv);
    db_test_and_set_bind_int(res, 6, grp_object->csy);

    //change mid to str
    char strbuf[256] = {0};
    if(grp_object->mid && grp_object->cnm > 0) {
        for(int i = 0 ; i < grp_object->cnm; i++){
            if(grp_object->mid[i]){
                strcat(strbuf, grp_object->mid[i]);
                strcat(strbuf, ",");
            }else
                break;
        }
        sqlite3_bind_text(res, 7, strbuf, -1, NULL);
    }else{
        sqlite3_bind_null(res, 7);
    }

    rc = sqlite3_step(res);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert msg : %s", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
    sqlite3_finalize(res);
    #else
    char *bs = " ";

        // db handle
    DBC* dbcp;
    int ret;        // template value

    DBT key_rn;
    DBT data;  // storving key and real data
    
    // if input == NULL
    
    if(grp_object->rn == NULL) grp_object->rn = bs;
    if(grp_object->acpi == NULL) grp_object->acpi = bs;

    //dbp = DB_CREATE_(dbp);
    //DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(grpDBp);
    
    /* key and data must initialize */
    memset(&key_rn, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));

    
    /* initialize the data to be the first of two duplicate records. */
    key_rn.data = grp_object->ri;
    key_rn.size = strlen(grp_object->ri) + 1;

    
    /* List data excluding 'ri' as strings using delimiters. */
    char str[DB_STR_MAX]= "\0";
    char *strbuf;

    /* mnm , mt, min*/

    strbuf = (char *) malloc(sizeof(char) * 1024);

    sprintf(strbuf, "%s;%s;%s;%s;%s;%s;%d;%d;%d;%d;", 
        grp_object->pi, grp_object->rn, grp_object->ct, grp_object->et, grp_object->lt, 
        grp_object->acpi, grp_object->mnm, grp_object->cnm, grp_object->mt, grp_object->csy);
    strcat(str, strbuf);
    strcat(str, grp_object->mtv ? "1" : "0");
    if(grp_object->mid) {
        for(int i = 0 ; i < grp_object->cnm; i++){
            if(grp_object->mid[i]){
                sprintf(strbuf, ";%s", grp_object->mid[i]);
                strcat(str, strbuf);
            }else
                break;
        }
    }

    data.data = str;
    data.size = strlen(str) + 1;
    /* input DB */
    if ((ret = dbcp->put(dbcp, &key_rn, &data, DB_KEYLAST)) != 0)
        grpDBp->err(grpDBp, ret, "db->cursor");

    /* DB close */
    dbcp->close(dbcp);
    
    if(grp_object->rn == bs) grp_object->rn = NULL;
    if(grp_object->acpi == bs) grp_object->acpi = NULL;
    free(strbuf);
    #endif

    return 1;
}

int db_store_sub(SUB *sub_object) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_sub");
    #ifdef SQLITE_DB
    char *sql, *err_msg = NULL;
    int rc = 0;
    sqlite3_stmt *res;
    
    // if input == NULL
    if (sub_object->ri == NULL) {
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }

    sql = "INSERT INTO general (rn, ri, pi, ct, et, lt, uri, ty) VALUES(?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg);

    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if( rc != SQLITE_OK ){
        logger("DB", LOG_LEVEL_ERROR, "Failed prepare msg : %s", sqlite3_errmsg(db));
        return 0;
    }

    db_test_and_set_bind_text(res, 1, sub_object->rn);
    sqlite3_bind_text(res, 2, sub_object->ri, -1, SQLITE_STATIC);
    db_test_and_set_bind_text(res, 3, sub_object->pi);
    db_test_and_set_bind_text(res, 4, sub_object->ct);
    db_test_and_set_bind_text(res, 5, sub_object->et);
    db_test_and_set_bind_text(res, 6, sub_object->lt);
    db_test_and_set_bind_text(res, 7, sub_object->uri);
    db_test_and_set_bind_int(res, 8, RT_SUB);

    rc = sqlite3_step(res);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert msg : %s", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_finalize(res);


    sql = "INSERT INTO sub (ri, nu, nct, net, sur) VALUES(?, ?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if( rc != SQLITE_OK ){
        logger("DB", LOG_LEVEL_ERROR, "Failed prepare msg : %s", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_bind_text(res, 1, sub_object->ri, -1, SQLITE_STATIC);
    db_test_and_set_bind_text(res, 2, sub_object->nu);
    db_test_and_set_bind_int(res, 3, sub_object->nct);
    db_test_and_set_bind_text(res, 4, sub_object->net);
    db_test_and_set_bind_text(res, 5, sub_object->sur);
    
    rc = sqlite3_step(res);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert msg : %s", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
    sqlite3_finalize(res);

    #else
    char* blankspace = " ";

        // db handle
    DBC* dbcp;
    int ret;        // template value

    DBT key_ri;
    DBT data;  // storving key and real data
    
    // if input == NULL
    if (sub_object->ri == NULL) {
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }
    if (sub_object->rn == NULL) sub_object->rn = blankspace;
    if (sub_object->ri == NULL) sub_object->ri = blankspace;
    if (sub_object->nu == NULL) sub_object->nu = blankspace;
    if (sub_object->net == NULL) sub_object->net = blankspace;
    if (sub_object->ct == NULL) sub_object->ct = blankspace;
    if (sub_object->et == NULL) sub_object->et = blankspace;
    if (sub_object->lt == NULL) sub_object->lt = blankspace;
    if (sub_object->ty == '\0') sub_object->ty = 23;
    if (sub_object->sur == NULL) sub_object->sur = blankspace;    

    dbcp = DB_GET_CURSOR(subDBp);

    /* keyand data must initialize */
    memset(&key_ri, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));
    
    /* initialize the data to be the first of two duplicate records. */
    key_ri.data = sub_object->ri;
    key_ri.size = strlen(sub_object->ri) + 1;

    char str[DB_STR_MAX] = "\0";
    sprintf(str, "%s;%s;%d;%s;%s;%s;%s;%s;%s;%d",sub_object->rn, sub_object->pi, sub_object->ty, 
    sub_object->ct, sub_object->et, sub_object->lt, sub_object->nu, sub_object->net,
    sub_object->sur, sub_object->nct);

    data.data = str;
    data.size = strlen(str) + 1;

    /* DB close */
    dbcp->close(dbcp);
    // dbp->close(dbp, 0); 

    if ((ret = dbcp->put(dbcp, &key_ri, &data, DB_KEYLAST)) != 0)
        subDBp->err(resourceDBp, ret, "db->cursor");
    
    if (sub_object->rn == blankspace) sub_object->rn = NULL;
    if (sub_object->ri == blankspace) sub_object->ri = NULL;
    if (sub_object->nu == blankspace) sub_object->nu = NULL;
    if (sub_object->net == blankspace) sub_object->net = NULL;
    if (sub_object->ct == blankspace) sub_object->ct = NULL;
    if (sub_object->et == blankspace) sub_object->et = NULL;
    if (sub_object->lt == blankspace) sub_object->lt = NULL;
    if (sub_object->ty == '\0') sub_object->ty = 23;
    if (sub_object->sur == blankspace) sub_object->sur = NULL; 
    #endif
    return 1;
}

int db_store_acp(ACP *acp_object) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_acp");

    #ifdef SQLITE_DB
    char *sql, *err_msg = NULL;
    char attrs[256] = {0}, vals[256] = {0};
    char buf[256];
    int rc = 0;
    sqlite3_stmt *res;

    // if input == NULL
    if (acp_object->ri == NULL) {
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }

    sql = "INSERT INTO general (rn, ri, pi, ct, et, lt, lbl, uri, ty) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg);

    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if( rc != SQLITE_OK ){
        logger("DB", LOG_LEVEL_ERROR, "Failed prepare msg : %s", sqlite3_errmsg(db));
        return 0;
    }

    db_test_and_set_bind_text(res, 1, acp_object->rn);
    sqlite3_bind_text(res, 2, acp_object->ri, -1, SQLITE_STATIC);
    db_test_and_set_bind_text(res, 3, acp_object->pi);
    db_test_and_set_bind_text(res, 4, acp_object->ct);
    db_test_and_set_bind_text(res, 5, acp_object->et);
    db_test_and_set_bind_text(res, 6, acp_object->lt);
    db_test_and_set_bind_text(res, 7, acp_object->lbl);
    db_test_and_set_bind_text(res, 8, acp_object->uri);
    db_test_and_set_bind_int(res, 9, RT_ACP);

    rc = sqlite3_step(res);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert msg : %s", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_finalize(res);
    

    sql = "INSERT INTO acp (ri, pvacop, pvacor, pvsacop, pvsacor) VALUES(?, ?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if( rc != SQLITE_OK ){
        logger("DB", LOG_LEVEL_ERROR, "Failed prepare msg : %s", sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_text(res, 1, acp_object->ri, -1, SQLITE_STATIC);
    db_test_and_set_bind_text(res, 2, acp_object->pv_acop);
    db_test_and_set_bind_text(res, 3, acp_object->pv_acor);
    db_test_and_set_bind_text(res, 4, acp_object->pvs_acop);
    db_test_and_set_bind_text(res, 5, acp_object->pvs_acor);

    rc = sqlite3_step(res);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Insert msg : %s", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
    sqlite3_finalize(res);

    #else
    char* blankspace = " ";

        // db handle
    DBC* dbcp;
    int ret;        // template value

    DBT key_ri;
    DBT data;  // storving key and real data


    // if input == NULL
    if (acp_object->ri == NULL) {
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }
    if (acp_object->rn == NULL) acp_object->rn = blankspace;
    if (acp_object->pi == NULL) acp_object->pi = blankspace;
    if (acp_object->ct == NULL) acp_object->ct = blankspace;
    if (acp_object->lt == NULL) acp_object->lt = blankspace;
    if (acp_object->et == NULL) acp_object->et = blankspace;
    if (acp_object->pv_acor == NULL) acp_object->pv_acor = blankspace;       
    if (acp_object->pv_acop == NULL) acp_object->pv_acop = blankspace; 
    if (acp_object->pvs_acop == NULL) acp_object->pvs_acor = blankspace; 
    if (acp_object->pvs_acop == NULL) acp_object->pvs_acop = blankspace; 


    //dbp = DB_CREATE_(dbp);
    // DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(acpDBp);

    /* keyand data must initialize */
    memset(&key_ri, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));

    /* initialize the data to be the first of two duplicate records. */
    key_ri.data = acp_object->ri;
    key_ri.size = strlen(acp_object->ri) + 1;

    /* List data excluding 'ri' as strings using delimiters. */
    char str[DB_STR_MAX]= "\0";
    //strcat(str,acp_object->rn);
    sprintf(str, "%s;%s;%d;%s;%s;%s;%s;%s;%s;%s;%s",
            acp_object->rn,acp_object->pi,acp_object->ty,acp_object->ct,acp_object->lt,
            acp_object->et,acp_object->pv_acor,acp_object->pv_acop,acp_object->pvs_acor,acp_object->pvs_acop,acp_object->lbl);
            
    data.data = str;
    data.size = strlen(str) + 1;

    /* input DB */
    if ((ret = dbcp->put(dbcp, &key_ri, &data, DB_KEYLAST)) != 0)
        acpDBp->err(acpDBp, ret, "db->cursor");

    /* DB close */
    dbcp->close(dbcp);
    // dbp->close(dbp, 0); 

    if (acp_object->rn == blankspace) acp_object->rn = NULL;
    if (acp_object->pi == blankspace) acp_object->pi = NULL;
    if (acp_object->ct == blankspace) acp_object->ct = NULL;
    if (acp_object->lt == blankspace) acp_object->lt = NULL;
    if (acp_object->et == blankspace) acp_object->et = NULL;
    if (acp_object->pv_acor == blankspace) acp_object->pv_acor = NULL;       
    if (acp_object->pv_acop == blankspace) acp_object->pv_acop = NULL; 
    if (acp_object->pvs_acop == blankspace) acp_object->pvs_acor = NULL; 
    if (acp_object->pvs_acop == blankspace) acp_object->pvs_acop = NULL;
    #endif

    return 1;
}

CSE *db_get_cse(char *ri){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_cse");
    CSE* new_cse= NULL;
    #ifdef SQLITE_DB
    int rc = 0;
    int cols = 0;
    sqlite3_stmt *res;
    char *colname;
    int bytes = 0;

    rc = sqlite3_prepare_v2(db, "SELECT * FROM general, cb WHERE general.ri = cb.ri;", -1, &res, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed select, %d", rc);
        return 0;
    }

    if(sqlite3_step(res) != SQLITE_ROW){
        return 0;
    }
    cols = sqlite3_column_count(res);
    new_cse = calloc(1, sizeof(CSE));
    for(int col = 0 ; col < cols; col++){
        colname = sqlite3_column_name(res, col);
        if( (bytes = sqlite3_column_bytes(res, col)) == 0)
            continue;

        if(!strcmp(colname, "rn") ){
            new_cse->rn = calloc(sizeof(char), bytes + 2);
            strncpy(new_cse->rn, sqlite3_column_text(res, col), bytes);
        }else if( !strcmp(colname, "ri") ){
            new_cse->ri = calloc(sizeof(char), bytes + 2);
            strncpy(new_cse->ri, sqlite3_column_text(res, col), bytes);
        }else if( !strcmp(colname, "pi") ){
            new_cse->pi = calloc(sizeof(char), bytes + 2);
            strncpy(new_cse->pi, sqlite3_column_text(res, col), bytes);
        }else if( !strcmp(colname, "ct") ){
            new_cse->ct = calloc(sizeof(char), bytes + 2);
            strncpy(new_cse->ct, sqlite3_column_text(res, col), bytes);
        }else if( !strcmp(colname, "lt") ){
            new_cse->lt = calloc(sizeof(char), bytes + 2);
            strncpy(new_cse->lt, sqlite3_column_text(res, col), bytes);
        }else if( !strcmp(colname, "csi") ){
            new_cse->csi = calloc(sizeof(char), bytes + 2);
            strncpy(new_cse->csi, sqlite3_column_text(res, col), bytes);
        }else if( !strcmp(colname, "uri")){
            new_cse->uri = calloc(sizeof(char), bytes + 2);
            strncpy(new_cse->uri, sqlite3_column_text(res, col), bytes);
        }
    }
    new_cse->ty = RT_CSE;
    

    sqlite3_finalize(res);
    #else
    DBC* dbcp;
    DBT key, data;
    int ret;

    bool flag = false;
    int idx = 0;
    
    ////dbp = DB_CREATE_(dbp);
    // DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(resourceDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    while (!flag && (ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, ri, key.size) == 0) {
            flag=true;
            new_cse= calloc(1,sizeof(CSE));
            // ri = key
            new_cse->ri = malloc((key.size + 1)*sizeof(char));
            strcpy(new_cse->ri, key.data);

            char *ptr = strtok((char*)data.data,DB_SEP);  //split first string
            while (ptr != NULL) { // Split to end of next string

                switch (idx) {
                case 0:
                    if(strcmp(ptr," ")==0) new_cse->rn=NULL; //data is NULL
                    else{
                        new_cse->rn = malloc((strlen(ptr) + 1) *sizeof(char));
                        strcpy(new_cse->rn, ptr);
                    }
                    idx++;
                    break;
                case 1:
                    if(strcmp(ptr," ")==0) new_cse->pi=NULL; //data is NULL
                    else{
                        new_cse->pi = malloc((strlen(ptr)+1)*sizeof(char));
                        strcpy(new_cse->pi, ptr);
                    }
                    idx++;
                    break;
                case 2:
                    if(strcmp(ptr,"0")==0) new_cse->ty=0;
                    else {new_cse->ty = atoi(ptr);}

                    idx++;
                    break;
                case 3:
                    if(strcmp(ptr," ")==0) new_cse->ct=NULL; //data is NULL
                    else{
                        new_cse->ct = malloc((strlen(ptr)+1)*sizeof(char));
                        strcpy(new_cse->ct, ptr);
                    }
                    idx++;
                    break;
                case 4:
                    if(strcmp(ptr," ")==0) new_cse->lt=NULL;
                    else{
                    new_cse->lt = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_cse->lt, ptr);
                    }
                    idx++;
                    break;                
                case 5:
                if(strcmp(ptr," ")==0) new_cse->csi=NULL;
                else{
                    new_cse->csi = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_cse->csi, ptr);
                }
                    idx++;
                    break;             
                default:
                    idx=-1;
                }
                
                ptr = strtok(NULL, DB_SEP); //The delimiter is ,
            }
        }
    }

    /* Cursors must be closed */
    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif

    return new_cse;
}

AE *db_get_ae(char *ri){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_ae");
    AE* new_ae = NULL;

    #ifdef SQLITE_DB
    //struct to return
    int rc = 0;
    int cols = 0;
    sqlite3_stmt *res;
    char *colname = NULL;
    cJSON *json = NULL, *root = NULL;
    int bytes = 0, coltype = 0;
    char sql[1024] = {0}, buf[256] = {0};

    sprintf(sql, "SELECT * FROM general, ae WHERE general.ri='%s' and ae.ri='%s';", ri, ri);
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
    root = cJSON_CreateObject();
    
    for(int col = 0 ; col < cols; col++){
        
        colname = sqlite3_column_name(res, col);
        bytes = sqlite3_column_bytes(res, col);
        coltype = sqlite3_column_type(res, col);
        if(bytes == 0) continue;

        if(!strcmp(colname, "rr")){
            if(!strncmp(sqlite3_column_text(res, col), "true", 4)){
                cJSON_AddItemToObject(json, colname, cJSON_CreateBool(true));
            }else{
                cJSON_AddItemToObject(json, colname, cJSON_CreateBool(false));
            }
            continue;
        }else if(!strcmp(colname, "lbl")){
            memset(buf,0, 256);
            strncpy(buf, sqlite3_column_text(res, col), bytes-1);
            cJSON_AddItemToObject(json, "lbl", cJSON_CreateString(buf+1));
            continue;
        }

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
    cJSON_AddItemToObject(root, "m2m:ae", json);
    new_ae = cjson_to_ae(root);
    cJSON_Delete(root);

    sqlite3_finalize(res);

    #else
    DBC* dbcp;
    DBT key, data;
    int ret;

    bool flag = false;
    int idx = 0;
    
    //dbp = DB_CREATE_(dbp);
    // DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(resourceDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    while (!flag && (ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, ri, key.size) == 0) {
            new_ae = calloc(1,sizeof(AE));
            flag=true;
            // ri = key
            new_ae->ri = malloc((key.size+1)*sizeof(char));
            strcpy(new_ae->ri, key.data);

            char *ptr = strtok((char*)data.data,DB_SEP);  //split first string
            while (ptr != NULL) { // Split to end of next string
                switch (idx) {
                case 0:
                if(strcmp(ptr," ")==0) new_ae->rn=NULL; //data is NULL
                else{
                    new_ae->rn = malloc((strlen(ptr) + 1) * sizeof(char));
                    strcpy(new_ae->rn, ptr);
                }
                    idx++;
                    break;
                case 1:
                if(strcmp(ptr," ")==0) new_ae->pi=NULL; //data is NULL
                    else{
                    new_ae->pi = malloc((strlen(ptr) + 1) * sizeof(char));
                    strcpy(new_ae->pi, ptr);
                    }
                    idx++;
                    break;
                case 2:
                if(strcmp(ptr,"0")==0) new_ae->ty=0;
                else new_ae->ty = atoi(ptr);

                    idx++;
                    break;
                case 3:
                if(strcmp(ptr," ")==0) new_ae->ct=NULL; //data is NULL
                else{
                    new_ae->ct = malloc((strlen(ptr) + 1) * sizeof(char));
                    strcpy(new_ae->ct, ptr);
                }
                    idx++;
                    break;
                case 4:
                if(strcmp(ptr," ")==0) new_ae->lt=NULL; //data is NULL
                else{                
                    new_ae->lt = malloc((strlen(ptr) + 1) * sizeof(char));
                    strcpy(new_ae->lt, ptr);
                }
                    idx++;
                    break;                
                case 5:
                if(strcmp(ptr," ")==0) new_ae->et=NULL; //data is NULL
                else{                
                    new_ae->et = malloc((strlen(ptr) + 1) * sizeof(char));
                    strcpy(new_ae->et, ptr);
                }
                    idx++;
                    break;      
                case 6:
                if(strcmp(ptr," ")==0) new_ae->api=NULL; //data is NULL
                else{                
                    new_ae->api = malloc((strlen(ptr) + 1) * sizeof(char));
                    strcpy(new_ae->api, ptr);
                }
                    idx++;
                    break;      
                case 7:
                    if(strcmp(ptr,"true")==0)
                        new_ae->rr = true;
                    else
                        new_ae->rr = false;

                    idx++;
                    break;      
                case 8:
                if(strcmp(ptr," ")==0) new_ae->aei=NULL; //data is NULL
                else{                
                    new_ae->aei = malloc((strlen(ptr) + 1) * sizeof(char));
                    strcpy(new_ae->aei, ptr);
                }
                    idx++;
                    break; 
                case 9:
                if(strcmp(ptr," ")==0) new_ae->lbl=NULL; //data is NULL
                else{                
                    new_ae->lbl = malloc((strlen(ptr) + 1) * sizeof(char));
                    strcpy(new_ae->lbl, ptr);
                }
                    idx++;
                    break;
                case 10:
                if(strcmp(ptr," ")==0) new_ae->srv=NULL; //data is NULL
                else{                
                    new_ae->srv = malloc((strlen(ptr) + 1) * sizeof(char));
                    strcpy(new_ae->srv, ptr);
                }            
                    idx++;
                    break; 
                case 11:
                if(strcmp(ptr," ")==0) new_ae->acpi=NULL; //data is NULL
                else{                
                    new_ae->acpi = malloc((strlen(ptr) + 1) * sizeof(char));
                    strcpy(new_ae->acpi, ptr);
                }            
                    idx++;
                    break;
                case 12:
                if(strcmp(ptr," ")==0) new_ae->origin=NULL; //data is NULL
                else{                
                    new_ae->origin = malloc((strlen(ptr) + 1) * sizeof(char));
                    strcpy(new_ae->origin, ptr);
                }            
                    idx++;
                    break;                                     
                default:
                    idx=-1;
                }
                
                ptr = strtok(NULL, DB_SEP); //The delimiter is ,
            }
        }
    }

    /* Cursors must be closed */
    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif
    return new_ae;   
}

CNT *db_get_cnt(char *ri){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_cnt");
    CNT* new_cnt = NULL;

    #ifdef SQLITE_DB
    //struct to return
    int rc = 0;
    int cols = 0;
    cJSON *json = NULL, *root = NULL;
    sqlite3_stmt *res;
    char *colname = NULL;
    int bytes = 0, coltype = 0;
    char sql[1024] = {0}, buf[256] = {0};

    sprintf(sql, "SELECT * FROM general, cnt WHERE general.ri='%s' and cnt.ri='%s';", ri, ri);
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
    root = cJSON_CreateObject();
    
    for(int col = 0 ; col < cols; col++){
        
        colname = sqlite3_column_name(res, col);
        bytes = sqlite3_column_bytes(res, col);
        coltype = sqlite3_column_type(res, col);

        if(bytes == 0) continue;

        if(!strcmp(colname, "lbl")){
            memset(buf,0, 256);
            strncpy(buf, sqlite3_column_text(res, col), bytes-1);
            cJSON_AddItemToObject(json, "lbl", cJSON_CreateString(buf+1));
            continue;
        }

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
    cJSON_AddItemToObject(root, "m2m:cnt", json);
    new_cnt = cjson_to_cnt(root);
    cJSON_Delete(root);
    

    sqlite3_finalize(res);
    #else
    DBC* dbcp;
    DBT key, data;
    int ret;

    bool flag = false;
    int idx = 0;
    
    // DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(resourceDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    while (!flag && (ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, ri, key.size) == 0) {
            new_cnt = calloc(1,sizeof(CNT));
            flag=true;
            // ri = key
            new_cnt->ri = malloc((key.size+1)*sizeof(char));
            strcpy(new_cnt->ri, key.data);

            char *ptr = strtok((char*)data.data,DB_SEP);  //split first string
            while (ptr != NULL) { // Split to end of next string
                switch (idx) {
                case 0:
                if(strcmp(ptr," ")==0) new_cnt->rn=NULL; //data is NULL
                    else{
                    new_cnt->rn = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_cnt->rn, ptr);
                    }
                    idx++;
                    break;
                case 1:
                if(strcmp(ptr," ")==0) new_cnt->pi=NULL; //data is NULL
                    else{
                    new_cnt->pi = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_cnt->pi, ptr);
                    }
                    idx++;
                    break;
                case 2:
                    new_cnt->ty = atoi(ptr);
                    idx++;
                    break;
                case 3:
                if(strcmp(ptr," ")==0) new_cnt->ct=NULL; //data is NULL
                    else{
                    new_cnt->ct = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_cnt->ct, ptr);
                    }
                    idx++;
                    break;
                case 4:
                if(strcmp(ptr," ")==0) new_cnt->lt=NULL; //data is NULL
                    else{
                    new_cnt->lt = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_cnt->lt, ptr);
                    }
                    idx++;
                    break;                
                case 5:
                if(strcmp(ptr," ")==0) new_cnt->et=NULL; //data is NULL
                    else{
                    new_cnt->et = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_cnt->et, ptr);
                    }
                    idx++;
                    break;      
                case 6:
                if(strcmp(ptr," ")==0) new_cnt->lbl=NULL; //data is NULL
                    else{
                    new_cnt->lbl = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_cnt->lbl, ptr);
                    }
                    idx++;
                    break;   
                case 7:
                if(strcmp(ptr," ")==0) new_cnt->acpi=NULL; //data is NULL
                    else{
                    new_cnt->acpi = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_cnt->acpi, ptr);
                    }
                    idx++;
                    break;                                           
                case 8:
                    new_cnt->cbs = atoi(ptr);

                    idx++;
                    break;     
                case 9:
                    new_cnt->cni = atoi(ptr);

                    idx++;
                    break;    
                case 10:
                    new_cnt->st = atoi(ptr);

                    idx++;
                    break;
                case 11:
                    new_cnt->mni = atoi(ptr);  
                    idx++;
                    break;
                case 12:
                    new_cnt->mbs = atoi(ptr);
                    idx++;
                    break;                                                                  
                default:
                    idx=-1;
                }         
                ptr = strtok(NULL, DB_SEP); //The delimiter is ;
            }
        }
    }

    /* Cursors must be closed */
    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif

    return new_cnt;   
}

CIN *db_get_cin(char *ri){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_cin");
    CIN* new_cin = NULL;

    #ifdef SQLITE_DB
    //struct to return
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json, *root;
    sqlite3_stmt *res;
    char *colname = NULL;
    char sql[1024] = {0}, buf[256] = {0};

    sprintf(sql, "SELECT * FROM general, cin WHERE general.ri='%s' and cin.ri='%s';", ri, ri);
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
    root = cJSON_CreateObject();
    
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
    cJSON_AddItemToObject(root, "m2m:cin", json);
    new_cin = cjson_to_cin(root);
    cJSON_Delete(json);


    sqlite3_finalize(res);
    #else
    DBC* dbcp;
    DBT key, data;
    int ret;

    int idx = 0;
    bool flag = false;
    
    //dbp = DB_CREATE_(dbp);
    // DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(resourceDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    while (!flag && (ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, ri, key.size) == 0) {
            flag = true;
            new_cin = calloc(1,sizeof(CIN));
            // ri = key
            new_cin->ri = strdup(key.data); //malloc((key.size+1)*sizeof(char));
            //strcpy(new_cin->ri, key.data);

            char *ptr = strtok((char*)data.data, DB_SEP);  //split first string
            while (ptr != NULL) { // Split to end of next string
                switch (idx) {
                case 0:
                if(strcmp(ptr," ")==0) new_cin->rn=NULL; //data is NULL
                    else{
                    new_cin->rn = strdup(ptr);//malloc(strlen(ptr)*sizeof(char));
                    
                    }
                    idx++;
                    break;
                case 1:
                if(strcmp(ptr," ")==0) new_cin->pi=NULL; //data is NULL
                    else{
                    new_cin->pi = strdup(ptr);//malloc(strlen(ptr)*sizeof(char));
                    //strcpy(new_cin->pi, ptr);
                    }
                    idx++;
                    break;
                case 2:
                if(strcmp(ptr,"0")==0) new_cin->ty=0;
                    else
                    new_cin->ty = atoi(ptr);

                    idx++;
                    break;
                case 3:
                if(strcmp(ptr," ")==0) new_cin->ct=NULL; //data is NULL
                    else{
                    new_cin->ct = strdup(ptr);// malloc(strlen(ptr)*sizeof(char));
                    //strcpy(new_cin->ct, ptr);
                    }
                    idx++;
                    break;
                case 4:
                    new_cin->lt = strdup(ptr);// malloc(strlen(ptr)*sizeof(char));
                    //strcpy(new_cin->lt, ptr);

                    idx++;
                    break;                
                case 5:
                if(strcmp(ptr," ")==0) new_cin->et=NULL; //data is NULL
                    else{
                    new_cin->et = strdup(ptr);// malloc(strlen(ptr)*sizeof(char));
                    //strcpy(new_cin->et, ptr);
                    }
                    idx++;
                    break;      
                case 6:
                if(strcmp(ptr," ")==0) new_cin->con=NULL; //data is NULL
                    else{
                    new_cin->con = strdup(ptr);// malloc(strlen(ptr)*sizeof(char));
                    //strcpy(new_cin->con, ptr);
                    }
                    idx++;
                    break;       
                case 7:
                if(strcmp(ptr,"0")==0) new_cin->cs=0;
                    else
                    new_cin->cs = atoi(ptr);

                    idx++;
                    break;            
                case 8:
                if(strcmp(ptr,"0")==0) new_cin->st=0;
                    else
                    new_cin->st = atoi(ptr);

                    idx++;
                    break;                                                                              
                default:
                    idx=-1;
                }
                
                ptr = strtok(NULL, DB_SEP); //The delimiter is ;
            }
        }
    }

    /* Cursors must be closed */
    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif

    return new_cin;   
}

SUB *db_get_sub(char *ri){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_sub");
    SUB* new_sub = NULL;

    #ifdef SQLITE_DB
    //struct to return
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    sqlite3_stmt *res;
    cJSON *json = NULL, *root = NULL;
    char *colname = NULL;
    char *sql, buf[256] = {0};

    sql = "SELECT * FROM general, sub WHERE general.ri=? and sub.ri=?;";
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    sqlite3_bind_text(res, 1, ri, -1, NULL);
    sqlite3_bind_text(res, 2, ri, -1, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed select");
        return 0;
    }

    if(sqlite3_step(res) != SQLITE_ROW){
        return 0;
    }
    cols = sqlite3_column_count(res);
    json = cJSON_CreateObject();
    root = cJSON_CreateObject();
    
    for(int col = 0 ; col < cols; col++){
        
        colname = sqlite3_column_name(res, col);
        bytes = sqlite3_column_bytes(res, col);
        coltype = sqlite3_column_type(res, col);

        if(bytes == 0) continue;
        switch(coltype){
            case SQLITE_TEXT:
                memset(buf, 0, 256);
                strncpy(buf, sqlite3_column_text(res, col), bytes);
                cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                break;
            case SQLITE_INTEGER:
                cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                break;
        }
    }
    cJSON_AddItemToObject(root, "m2m:sub", json);
    new_sub = cjson_to_sub_db(root);
    cJSON_Delete(root);

    sqlite3_finalize(res);
    #else

    DBC* dbcp;
    DBT key, data;
    int ret;

    bool flag = false;
    int idx = 0;

    dbcp = DB_GET_CURSOR(subDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    while (!flag && (ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, ri, key.size) == 0) {
            new_sub = (SUB*)calloc(1, sizeof(SUB));
            flag=true;
            // ri = key
            new_sub->ri = malloc((key.size+1)*sizeof(char));
            strcpy(new_sub->ri, key.data);

            char *ptr = strtok((char*)data.data,DB_SEP);  //split first string
            while (ptr != NULL) { // Split to end of next string
                switch (idx) {
                case 0:
                if(strcmp(ptr," ")==0) new_sub->rn=NULL; //data is NULL
                    else{
                    new_sub->rn = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_sub->rn, ptr);
                    }
                    idx++;
                    break;
                case 1:
                if(strcmp(ptr," ")==0) new_sub->pi=NULL; //data is NULL
                    else{
                    new_sub->pi = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_sub->pi, ptr);
                    }
                    idx++;
                    break;
                case 2:
                    new_sub->ty = atoi(ptr);
                    idx++;
                    break;
                case 3:
                if(strcmp(ptr," ")==0) new_sub->ct=NULL; //data is NULL
                    else{
                    new_sub->ct = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_sub->ct, ptr);
                    }
                    idx++;
                    break;
                case 4:
                if(strcmp(ptr," ")==0) new_sub->et=NULL; //data is NULL
                    else{
                    new_sub->et = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_sub->et, ptr);
                    }
                    idx++;
                    break;                
                case 5:
                if(strcmp(ptr," ")==0) new_sub->lt=NULL; //data is NULL
                    else{
                    new_sub->lt = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_sub->lt, ptr);
                    }
                    idx++;
                    break;      
                case 6:
                if(strcmp(ptr," ")==0) new_sub->nu=NULL; //data is NULL
                    else{
                    new_sub->nu = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_sub->nu, ptr);
                    }
                    idx++;
                    break;   
                case 7:
                if(strcmp(ptr," ")==0) new_sub->net=NULL; //data is NULL
                    else{
                    new_sub->net = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_sub->net, ptr);
                    }
                    idx++;
                    break;                                           
                case 8:
                if(strcmp(ptr," ")==0) new_sub->sur=NULL; //data is NULL
                    else{
                    new_sub->sur = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_sub->sur, ptr);
                    }
                    idx++;
                    break;     
                case 9:
                    new_sub->nct = atoi(ptr);

                    idx++;
                    break;                                                             
                default:
                    idx=-1;
                }         
                ptr = strtok(NULL, DB_SEP); //The delimiter is ;
            }
        }
    }

    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif
    return new_sub;   
}

GRP *db_get_grp(char *ri){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_grp");

    GRP* new_grp = NULL;
    new_grp = calloc(1, sizeof(GRP));
    #ifdef SQLITE_DB
    //struct to return
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *root = NULL, *json = NULL;
    sqlite3_stmt *res;
    char *colname = NULL;
    char sql[1024] = {0}, buf[256] = {0};

    sprintf(sql, "SELECT * FROM 'general', 'grp' WHERE general.ri='%s' and grp.ri='%s';", ri, ri);
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed select");
        return NULL;
    }

    if(sqlite3_step(res) != SQLITE_ROW){
        logger("DB", LOG_LEVEL_ERROR, "Not Found");
        return NULL;
    }

    cols = sqlite3_column_count(res);
    json = cJSON_CreateObject();
    root = cJSON_CreateObject();
    
    for(int col = 0 ; col < cols; col++){
        
        colname = sqlite3_column_name(res, col);
        bytes = sqlite3_column_bytes(res, col);
        coltype = sqlite3_column_type(res, col);

        if(bytes == 0) continue;
        switch(coltype){
            case SQLITE_TEXT:
                memset(buf, 0, 256);
                strncpy(buf, sqlite3_column_text(res, col), bytes);
                if(!strcmp(colname, "mid")){
                    cJSON_AddItemToObject(json, colname, string_to_cjson_string_list_item(buf));
                }else{
                    cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                }
                break;
            case SQLITE_INTEGER:
                cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                break;
        }
    }
    cJSON_AddItemToObject(root, "m2m:grp", json);
    cjson_to_grp(root, new_grp);
    cJSON_Delete(root);

    sqlite3_finalize(res);
    #else
    DBC* dbcp;
    DBT key, data;
    int ret;

    int cnt = 0;
    int flag = 0;
    int idx = 0;

    //dbp = DB_CREATE_(dbp);
    //DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(grpDBp);

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));


    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        cnt++;
        if (strncmp(key.data, ri, key.size) == 0) {
            flag=1;
            new_grp->ri = strdup(key.data);
            
            //(char *) malloc( (key.size+1) *sizeof(char));
            //strcpy(new_grp->rn, key.data);

            char *ptr = strtok((char*)data.data, DB_SEP);  //split first string
            new_grp->pi = strdup(ptr); //rn

            ptr = strtok(NULL, DB_SEP);
            new_grp->rn = strdup(ptr);

            ptr = strtok(NULL, DB_SEP);
            new_grp->ct = strdup(ptr);

            ptr = strtok(NULL, DB_SEP);
            new_grp->et = strdup(ptr);

            ptr = strtok(NULL, DB_SEP);
            new_grp->lt = strdup(ptr);

            ptr = strtok(NULL, DB_SEP);
            if(!strcmp(ptr, " "))
                new_grp->acpi = NULL;
            else   
		        new_grp->acpi = strdup(ptr);

            ptr = strtok(NULL, DB_SEP);
            new_grp->mnm = atoi(ptr);

            ptr = strtok(NULL, DB_SEP);
            new_grp->cnm = atoi(ptr);
            
            ptr = strtok(NULL, DB_SEP); //The delimiter is ;
            new_grp->mt = atoi(ptr);

            ptr = strtok(NULL, DB_SEP); //The delimiter is ;
            new_grp->mtv = atoi(ptr) ? true : false;

            ptr = strtok(NULL, DB_SEP); //The delimiter is ;
            new_grp->csy = atoi(ptr);

            ptr = strtok(NULL, DB_SEP); //The delimiter is ;
            if(ptr){
                new_grp->mid = (char **) malloc(sizeof(char *) * new_grp->mnm);
                for(int i = 0 ; i < new_grp->mnm ; i++){
                    if(ptr){
                        new_grp->mid[i] = strdup(ptr);
                        ptr = strtok(NULL, DB_SEP); //The delimiter is ;
                    }else{
                        new_grp->mid[i] = NULL;
                    }
                }
            }
        }
    }


    if(ret != DB_NOTFOUND){
        grpDBp->err(grpDBp, ret, "DBcursor->get");
        logger("DB", LOG_LEVEL_DEBUG, "Cursor ERROR");
        return NULL;
    }
    if (cnt == 0 || flag==0) {
        logger("DB", LOG_LEVEL_DEBUG, "Data does not exist");
        return NULL;
    }


    /* Cursors must be closed */
    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif

    return new_grp;   
}

ACP *db_get_acp(char *ri){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_acp");
    //struct to return
    ACP* new_acp = NULL;

    #ifdef SQLITE_DB
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    sqlite3_stmt *res;
    cJSON *json = NULL, *root = NULL;
    char *colname = NULL;
    char sql[1024] = {0}, buf[256] = {0};

    sprintf(sql, "SELECT * FROM general, acp WHERE general.ri='%s' and acp.ri='%s';", ri, ri);
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed prepare");
        return 0;
    }

    if(sqlite3_step(res) != SQLITE_ROW){
        return 0;
    }
    cols = sqlite3_column_count(res);

    json = cJSON_CreateObject();
    root = cJSON_CreateObject();
    
    for(int col = 0 ; col < cols; col++){
        
        colname = sqlite3_column_name(res, col);
        bytes = sqlite3_column_bytes(res, col);
        coltype = sqlite3_column_type(res, col);

        if(bytes == 0) continue;


        if(!strcmp(colname, "lbl")){
            memset(buf,0, 256);
            strncpy(buf, sqlite3_column_text(res, col), bytes-1);
            cJSON_AddItemToObject(json, "lbl", cJSON_CreateString(buf+1));
            continue;
        }

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
    cJSON_AddItemToObject(root, "m2m:acp", json);
    new_acp = cjson_to_acp(root);
    cJSON_Delete(root);

    sqlite3_finalize(res);
    #else
    DBC* dbcp;
    DBT key, data;
    int ret;

    bool flag = false;
    int idx = 0;
    
    //dbp = DB_CREATE_(dbp);
    //DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(acpDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    while (!flag && (ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        new_acp = calloc(1,sizeof(ACP));
        if (strncmp(key.data, ri, key.size) == 0) {
            flag=true;
            // ri = key
            new_acp->ri =malloc((key.size+1)*sizeof(char));
            strcpy(new_acp->ri, key.data);

            char *ptr = strtok((char*)data.data, DB_SEP);  //split first string
            while (ptr != NULL) { // Split to end of next string
                switch (idx) {
                case 0:if(strcmp(ptr," ")==0) new_acp->rn=NULL; //data is NULL
                else{
                    new_acp->rn = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_acp->rn, ptr);
                    }
                    idx++;
                    break;
                case 1:
                if(strcmp(ptr," ")==0) new_acp->pi=NULL; //data is NULL
                else{
                    new_acp->pi = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_acp->pi, ptr);
                    }
                    idx++;
                    break;
                case 2:
                if(strcmp(ptr,"0")==0) new_acp->ty=0;
                else new_acp->ty = atoi(ptr);

                    idx++;
                    break;
                case 3:
                if(strcmp(ptr," ")==0) new_acp->ct=NULL; //data is NULL
                else{
                    new_acp->ct = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_acp->ct, ptr);
                    }
                    idx++;
                    break;
                case 4:
                if(strcmp(ptr," ")==0) new_acp->lt=NULL; //data is NULL
                else{
                    new_acp->lt = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_acp->lt, ptr);
                    }
                    idx++;
                    break;                
                case 5:
                if(strcmp(ptr," ")==0) new_acp->et=NULL; //data is NULL
                else{
                    new_acp->et = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_acp->et, ptr);
                }
                    idx++;
                    break;
                case 6:
                if(strcmp(ptr," ")==0) new_acp->pv_acor=NULL; //data is NULL
                else{
                    new_acp->pv_acor = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_acp->pv_acor, ptr);
                }
                    idx++;
                    break;
                case 7:
                if(strcmp(ptr," ")==0) new_acp->pv_acop=NULL; //data is NULL
                else{
                    new_acp->pv_acop = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_acp->pv_acop, ptr);
                }
                    idx++;
                    break;
                case 8:
                if(strcmp(ptr," ")==0) new_acp->pvs_acor=NULL; //data is NULL
                else{
                    new_acp->pvs_acor = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_acp->pvs_acor, ptr);
                }
                    idx++;
                    break;
                case 9:
                if(strcmp(ptr," ")==0) new_acp->pvs_acop=NULL; //data is NULL
                else{
                    new_acp->pvs_acop = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_acp->pvs_acop, ptr);
                }
                    idx++;
                    break;   
                case 10:
                if(strcmp(ptr," ")==0) new_acp->lbl=NULL; //data is NULL
                else{
                    new_acp->lbl = malloc((strlen(ptr)+1)*sizeof(char));
                    strcpy(new_acp->lbl, ptr);
                }
                    idx++;
                    break;              
                default:
                    idx=-1;
                }
                
                ptr = strtok(NULL, DB_SEP); //The delimiter is ;
            }
        }
    }

    /* Cursors must be closed */
    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif
    return new_acp;   
}


#ifdef SQLITE_DB
int db_update_ae(AE *ae_object){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_update_ae");
  //  AE *ae_object = (AE *) rtnode->obj;
    char* blankspace = " ";
    char *err_msg = NULL;
    int rc = 0;
    char attrs[128]={0}, vals[512]={0};
    char buf[128] = {0};
    char *sql;
    sqlite3_stmt *stmt;

        // db handle
    char rr[6] ="";

    // if input == NULL
    if (ae_object->ri == NULL) {
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }

    if(ae_object->rr == false) strcpy(rr, "false");
    else strcpy(rr, "true");

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg);

    sql = "UPDATE general SET rn=?, lt=?, et=?, lbl=?, acpi=? WHERE ri=?";
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed prepare");
        return 0;
    }

    db_test_and_set_bind_text(stmt, 1, ae_object->rn);
    db_test_and_set_bind_text(stmt, 2, ae_object->lt);
    db_test_and_set_bind_text(stmt, 3, ae_object->et);
    db_test_and_set_bind_text(stmt, 4, ae_object->lbl);
    db_test_and_set_bind_text(stmt, 5, ae_object->acpi);
    db_test_and_set_bind_text(stmt, 6, ae_object->ri);
    
  
    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Update SQL: %s, msg : %s", sql, err_msg);
        return 0;
    }

    sqlite3_finalize(stmt);

    sql = "UPDATE ae SET api=?, aei=?, srv=?, rr=? WHERE ri=?";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed prepare");
        sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
        return 0;
    }

    db_test_and_set_bind_text(stmt, 1, ae_object->api);
    db_test_and_set_bind_text(stmt, 2, ae_object->aei);
    db_test_and_set_bind_text(stmt, 3, ae_object->srv);
    db_test_and_set_bind_text(stmt, 4, rr);
    db_test_and_set_bind_text(stmt, 5, ae_object->ri);
      
    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Update SQL: %s, msg : %s", sql, err_msg);
        sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
        return 0;
    }

    sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
    sqlite3_finalize(stmt);
    
    return 1;
}

int db_update_cnt(CNT *cnt_object){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_update_cnt");
    char *sql, *err_msg = NULL;
    int rc = 0;
    sqlite3_stmt *stmt;
    char buf[256]={0};

    if (cnt_object->ri == NULL) {
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg);
    sql = "UPDATE general SET rn=?, lbl=?, lt=?, et=?, acpi=? WHERE ri=?";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "prepare error");
        return 0;
    }

    db_test_and_set_bind_text(stmt, 1, cnt_object->rn);
    db_test_and_set_bind_text(stmt, 2, cnt_object->lbl);
    db_test_and_set_bind_text(stmt, 3, cnt_object->lt);
    db_test_and_set_bind_text(stmt, 4, cnt_object->et);
    db_test_and_set_bind_text(stmt, 5, cnt_object->acpi);
    db_test_and_set_bind_text(stmt, 6, cnt_object->ri);


    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed UPDATE SQL: %s, msg : %s", sql, err_msg);
        return 0;
    }

    sqlite3_finalize(stmt);

    sql = "UPDATE cnt SET mni=?, mbs=?, st=?, cni=?, cbs=? WHERE ri=?;";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "prepare error");
        return 0;
    }
    db_test_and_set_bind_int(stmt, 1, cnt_object->mni);
    db_test_and_set_bind_int(stmt, 2, cnt_object->mbs);
    db_test_and_set_bind_int(stmt, 3, cnt_object->st);
    db_test_and_set_bind_int(stmt, 4, cnt_object->cni);
    db_test_and_set_bind_int(stmt, 5, cnt_object->cbs);
    db_test_and_set_bind_text(stmt, 6, cnt_object->ri);

    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed UPDATE SQL: %s, msg : %s", sql, err_msg);
        return 0;
    }

    sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
    sqlite3_finalize(stmt);

    return 1;
}

int db_update_acp(ACP *acp_object){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_update_acp");
    char *sql, *err_msg = NULL;
    int rc = 0;
    char buf[256]={0};
    sqlite3_stmt *stmt;

    if (acp_object->ri == NULL) {
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg);

    sql = "UPDATE general SET rn=?, lt=?, et=?, lbl=? WHERE ri=?;";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "prepare error");
        sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
        return 0;
    }

    db_test_and_set_bind_text(stmt, 1, acp_object->rn);
    db_test_and_set_bind_text(stmt, 2, acp_object->lt);
    db_test_and_set_bind_text(stmt, 3, acp_object->et);
    db_test_and_set_bind_text(stmt, 4, acp_object->lbl);
    db_test_and_set_bind_text(stmt, 5, acp_object->ri);

    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Update SQL: %s, msg : %s", sql, err_msg);
        sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
        return 0;
    }

    sqlite3_finalize(stmt);
    
    sql = "UPDATE acp SET pvacop=?, pvacor=?, pvsacop=?, pvsacor=? WHERE ri=?;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "prepare error");
        sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
        return 0;
    }

    db_test_and_set_bind_text(stmt, 1, acp_object->pv_acop);
    db_test_and_set_bind_text(stmt, 2, acp_object->pv_acor);
    db_test_and_set_bind_text(stmt, 3, acp_object->pvs_acop);
    db_test_and_set_bind_text(stmt, 4, acp_object->pvs_acor);
    db_test_and_set_bind_text(stmt, 5, acp_object->ri);

    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Update SQL: %s, msg : %s", sql, err_msg);
        sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
        return 0;
    }
    sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
    sqlite3_finalize(stmt);

    return 1;
}

int db_update_grp(GRP *grp_object){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_update_grp");
    char *sql, *err_msg = NULL;
    int rc = 0;
    char buf[256]={0};
    sqlite3_stmt *stmt;

    if (grp_object->ri == NULL) {
        fprintf(stderr, "ri is NULL\n");
        return -1;
    }

    for(int i = 0 ; i < grp_object->cnm ; i++){
        strcat(buf, grp_object->mid[i]);
        strcat(buf, ",");
    }
    buf[strlen(buf)-1] = '\0';

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg);

    sql = "UPDATE general SET rn=?, lt=?, et=? WHERE ri=?;";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "prepare error");
        sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
        return 0;
    }

    db_test_and_set_bind_text(stmt, 1, grp_object->rn);
    db_test_and_set_bind_text(stmt, 2, grp_object->lt);
    db_test_and_set_bind_text(stmt, 3, grp_object->et);
    db_test_and_set_bind_text(stmt, 4, grp_object->ri);

    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Update SQL: %s, msg : %s", sql, err_msg);
        sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
        return 0;
    }

    sqlite3_finalize(stmt);

    sql = "UPDATE grp SET mt=?, cnm=?, mnm=?, mtv=?, csy=?, mid=? WHERE ri=?;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "prepare error");
        sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
        return 0;
    }
    
    db_test_and_set_bind_int(stmt, 1, grp_object->mt);
    db_test_and_set_bind_int(stmt, 2, grp_object->cnm);
    db_test_and_set_bind_int(stmt, 3, grp_object->mnm);
    db_test_and_set_bind_int(stmt, 4, grp_object->mtv);
    db_test_and_set_bind_int(stmt, 5, grp_object->csy);
    db_test_and_set_bind_text(stmt, 6, buf);
    db_test_and_set_bind_text(stmt, 7, grp_object->ri);
    
    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE){
        logger("DB", LOG_LEVEL_ERROR, "Failed Update SQL: %s, msg : %s", sql, err_msg);
        sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
        return 0;
    }
    sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &err_msg);
    sqlite3_finalize(stmt);

    return 1;
}

char *get_table_name(ResourceType ty){
    char * tableName = NULL;
    switch(ty){
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
    }
    return tableName;
}

void db_test_and_set_bind_text(sqlite3_stmt *stmt, int index, char* context){
    if(context)
        sqlite3_bind_text(stmt, index, context, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, index);
}

void db_test_and_set_bind_int(sqlite3_stmt *stmt, int index, int value){
    if(value >= 0)
        sqlite3_bind_int(stmt, index, value);
    else   
        sqlite3_bind_null(stmt, index);
}

#endif
#ifdef BERKELEY_DB
int db_delete_child_cin(RTNode *rtnode){
    DBC* dbcp;
    DBT key, data;

	int ret=0;
    char* result = NULL;
    char* ri = get_ri_rtnode(rtnode);

    dbcp = DB_GET_CURSOR(resourceDBp);
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    while((ret = dbcp->get(dbcp, &key, &data, DB_NEXT))==0)
    {
        result = strstr((char*)data.data, ri);

        if(result != NULL)
        {
            ret = dbcp->del(dbcp, 0);
            if(ret!=0)
            {
                break;
            }
        }
        else
        {

        }
    }
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    if(ret != DB_NOTFOUND && ret != 0)
    {
        fprintf(stderr, "Error occurred while iterating through the DB\n");
        return 1;
    }
    return 0;
}
#endif

// int db_update_onem2m_resource(RTNode *rtnode){
//     logger("DB", LOG_LEVEL_DEBUG, "Update [RI] %s", get_ri_rtnode(rtnode));
//     char sql[1024] = {0};
//     char *errmsg;
//     switch(rtnode->ty){
//         case RT_AE:
//             break;
//         case RT_CNT:
//             break;
//         case RT_CIN:
//             break;
//         case RT_ACP:
//             break;
//         case RT_GRP:
//             break;
//         case RT_SUB:
//             break;
//     }
// }

int db_delete_onem2m_resource(RTNode *rtnode) {
    logger("DB", LOG_LEVEL_DEBUG, "Delete [RI] %s",get_ri_rtnode(rtnode));

    #ifdef SQLITE_DB
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

    if(rtnode->ty == RT_CNT){

        sprintf(sql, "DELETE FROM cin WHERE ri IN (SELECT ri FROM general where uri LIKE '%s%%');", get_uri_rtnode(rtnode));
        rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
        if(rc != SQLITE_OK){
            logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource from %s/ msg : %s", tableName, err_msg);
            sqlite3_close(db);
            return 0;
        }
        
        sprintf(sql, "DELETE FROM general WHERE uri LIKE '%s%%';", get_uri_rtnode(rtnode));
        rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
        if(rc != SQLITE_OK){
            logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource from %s/ msg : %s", tableName, err_msg);
            sqlite3_close(db);
            return 0;
        }

    }
    #else
    DBC* dbcp;
    DBT key, data;
    char *ri = get_ri_rtnode(rtnode);
    int ret;
    bool flag = false;

    //dbp = DB_CREATE_(dbp);
    //DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(resourceDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    while (!flag && (ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strcmp(key.data, ri) == 0) {
            flag = true;
            dbcp->del(dbcp, 0);
            break;
        }
    }

    /* Cursors must be closed */
    if (dbcp != NULL)
        dbcp->close(dbcp);

    if (!flag) {
        fprintf(stderr, "DB data not found\n");
        return -1;
    }
    #endif

    return 1;
}

int db_delete_sub(char* ri) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_delete_sub");
    
    #ifdef SQLITE_DB
    char sql[1024] = {0};
    char *err_msg;
    int rc; 

    sprintf(sql, "DELETE FROM sub WHERE ri='%s';", ri);
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource %s", err_msg);
        sqlite3_close(db);
        return 0;
    }

    sprintf(sql, "DELETE FROM general WHERE ri='%s';", ri);
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource %s", err_msg);
        sqlite3_close(db);
        return 0;
    }
    #else
    DBC* dbcp;
    DBT key, data;
    int ret;

    dbcp = DB_GET_CURSOR(subDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int cnt = 0;
    int flag = 0;
    int struct_size = 10;

    int idx = -1;
    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(data.data, ri, data.size) == 0) {
            idx=0;
        }
        if(idx!=-1 && idx < struct_size){
            dbcp->del(dbcp, 0);
            idx++;
        }
    }
    if (ret != DB_NOTFOUND) {
        subDBp->err(subDBp, ret, "DBcursor->get");
        fprintf(stderr, "Cursor ERROR\n");
        return 0;
    }

    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif

    return 1;
}

int db_delete_acp(char* ri) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_delete_acp");
    #ifdef SQLITE_DB
    char sql[1024] = {0};
    char *err_msg;
    int rc; 

    sprintf(sql, "DELETE FROM acp WHERE ri='%s';", ri);
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource %s", err_msg);
        sqlite3_close(db);
        return 0;
    }

    sprintf(sql, "DELETE FROM general WHERE ri='%s';", ri);
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource %s", err_msg);
        sqlite3_close(db);
        return 0;
    }
    #else
    DBC* dbcp;
    DBT key, data;
    int ret;
    int flag = 0;

    //dbp = DB_CREATE_(dbp);
    //DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(acpDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, ri, key.size) == 0) {
            flag = 1;
            dbcp->del(dbcp, 0);
            break;
        }
    }
    if (flag == 0) {
        fprintf(stderr, "Not Found\n");
        return -1;
    }

    /* Cursors must be closed */
    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif

    return 1;
}

int db_delete_grp(char* ri) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_delete_grp");
    #ifdef SQLITE_DB
    char sql[1024] = {0};
    char *err_msg;
    int rc; 

    sprintf(sql, "DELETE FROM grp WHERE ri='%s';", ri);
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource %s", err_msg);
        sqlite3_close(db);
        return 0;
    }

    sprintf(sql, "DELETE FROM general WHERE ri='%s';", ri);
    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Cannot delete resource %s", err_msg);
        sqlite3_close(db);
        return 0;
    }
    #else
    DBC* dbcp;
    DBT key, data;
    int ret;
    int flag = 0;

    //dbp = DB_CREATE_(dbp);
    //DB_OPEN( database);
    dbcp = DB_GET_CURSOR(grpDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, ri, key.size) == 0) {
            flag = 1;
            dbcp->del(dbcp, 0);
            break;
        }
    }
    if (flag == 0) {
        fprintf(stderr, "Not Found\n");
        return -1;
    }

    /* Cursors must be closed */
    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif

    return 1;
}

RTNode* db_get_all_cse() {
    fprintf(stderr,"\x1b[92m[Get All CSE]\x1b[0m\n");
    #ifdef SQLITE_DB
    int rc = 0;
    int cols = 0;
    
    sqlite3_stmt *res;
    char *colname = NULL;
    char sql[1024] = {0};

    sprintf(sql, "SELECT * FROM general, cb WHERE general.ri = cb.ri;");
    rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL);
    if(rc != SQLITE_OK){
        logger("DB", LOG_LEVEL_ERROR, "Failed select");
        return 0;
    }

    RTNode* head = NULL, *rtnode = NULL;
    rc = sqlite3_step(res);
    cols = sqlite3_column_count(res);
    while(rc == SQLITE_ROW){
        CSE* cse = calloc(1, sizeof(CSE));
        for(int col = 0 ; col < cols; col++){
            colname = sqlite3_column_name(res, col);
            if(!strcmp(colname, "rn") ){
                cse->rn = strdup(sqlite3_column_text(res, col));
            }else if( !strcmp(colname, "ri") ){
                cse->ri = strdup(sqlite3_column_text(res, col));
            }else if( !strcmp(colname, "pi") ){
                cse->pi = strdup(sqlite3_column_text(res, col));
            }else if( !strcmp(colname, "ct") ){
                cse->ct = strdup(sqlite3_column_text(res, col));
            }else if( !strcmp(colname, "lt") ){
                cse->lt = strdup(sqlite3_column_text(res, col));
            }else if( !strcmp(colname, "csi") ){
                cse->csi = strdup(sqlite3_column_text(res, col));
            }
        }
        cse->ty = RT_CSE;
        if(!head) {
            head = create_rtnode(cse,RT_CSE);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(cse,RT_CSE);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }   
        rc = sqlite3_step(res);
    }

    // if (dbp != NULL)
    //     dbp->close(dbp, 0);   
    sqlite3_finalize(res); 

    #else
    const char* TYPE = "5-";
    
    DBC* dbcp;
    DBT key, data;
    int ret;

    //dbp = DB_CREATE_(dbp);
    //DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(resourceDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int cse = 0;
    DBC* dbcp0;
    dbcp0 = DB_GET_CURSOR(resourceDBp);
    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, TYPE , 2) == 0) 
            cse++;
    }

    if (cse == 0) {
        logger("DB", LOG_LEVEL_DEBUG, "CSE does not exist");
        return NULL;
    }

    dbcp0->close(dbcp0);
    RTNode* head = NULL, *rtnode = NULL;

            logger("DB", LOG_LEVEL_DEBUG, "%d", cse);
    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, TYPE , 2) == 0){
            CSE* cse = db_get_cse((char*)key.data);
            if(!head) {
                head = create_rtnode(cse,RT_CSE);
                rtnode = head;
            } else {
                rtnode->sibling_right = create_rtnode(cse,RT_CSE);
                rtnode->sibling_right->sibling_left = rtnode;
                rtnode = rtnode->sibling_right;
            }   
        }
    }
    
    /* Cursors must be closed */
    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif
    return head;
}

RTNode *db_get_all_ae_rtnode(){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_ae_rtnode");

    #ifdef SQLITE_DB
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

    rc = sqlite3_step(res);
    cols = sqlite3_column_count(res);
    while(rc == SQLITE_ROW){
        json = cJSON_CreateObject();
        root = cJSON_CreateObject();
        
        AE* ae = NULL;
        for(int col = 0 ; col < cols; col++){
            
            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);
            if(!strcmp(colname, "rr")){
                if(!strncmp(sqlite3_column_text(res, col), "true", 4)){
                    cJSON_AddItemToObject(json, colname, cJSON_CreateBool(true));
                }else{
                    cJSON_AddItemToObject(json, colname, cJSON_CreateBool(false));
                }
                continue;
            }

            if(bytes == 0) continue;
            switch(coltype){
                case SQLITE_TEXT:
                    memset(buf, 0, 256);
                    strncpy(buf, sqlite3_column_text(res, col), bytes);
                    cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                    break;
                case SQLITE_INTEGER:
                    cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                    break;
            }
        }
        cJSON_AddItemToObject(root, "m2m:ae", json);
        ae = cjson_to_ae(root);
        if(!head) {
            head = create_rtnode(ae,RT_AE);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(ae, RT_AE);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }  
        cJSON_Delete(root); 
        rc = sqlite3_step(res);
    }

    sqlite3_finalize(res); 
    #else
    char *TYPE = "C";
    DBC* dbcp;
    DBT key, data;
    int ret;

    //dbp = DB_CREATE_(dbp);
    //DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(resourceDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    RTNode* head = NULL, *rtnode = NULL;

    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        
        if (strncmp(key.data, TYPE , 1) == 0){
            AE* ae = db_get_ae((char*)key.data);

            if(!head) {
                head = create_rtnode(ae,RT_AE);
                rtnode = head;
            } else {
                rtnode->sibling_right = create_rtnode(ae,RT_AE);
                rtnode->sibling_right->sibling_left = rtnode;
                rtnode = rtnode->sibling_right;
            }      
        }
    }

    /* Cursors must be closed */
    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif
    return head;
}

RTNode *db_get_all_cnt_rtnode(){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_cnt_rtnode");

    #ifdef SQLITE_DB
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
    rc = sqlite3_step(res);
    cols = sqlite3_column_count(res);
    while(rc == SQLITE_ROW){
        json = cJSON_CreateObject();
        root = cJSON_CreateObject();
        
        CNT* cnt = NULL;
        for(int col = 0 ; col < cols; col++){
            
            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);

            if(bytes == 0) continue;
            switch(coltype){
                case SQLITE_TEXT:
                    memset(buf, 0, 256);
                    strncpy(buf, sqlite3_column_text(res, col), bytes);
                    cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                    break;
                case SQLITE_INTEGER:
                    cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                    break;
            }
        }
        cJSON_AddItemToObject(root, "m2m:cnt", json);
        cnt = cjson_to_cnt(root);
        if(!head) {
            head = create_rtnode(cnt,RT_CNT);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(cnt, RT_CNT);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }   
        cJSON_Delete(root);
        rc = sqlite3_step(res);
    }

    sqlite3_finalize(res); 
    #else
    const char* TYPE = "3-";
    
    DBC* dbcp;
    DBT key, data;
    int ret;

    //dbp = DB_CREATE_(dbp);
    //DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(resourceDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    RTNode* head = NULL, *rtnode = NULL;

    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, TYPE , 2) == 0){
            CNT* cnt = db_get_cnt((char*)key.data);
            if(!head) {
                head = create_rtnode(cnt,RT_CNT);
                rtnode = head;
            } else {
                rtnode->sibling_right = create_rtnode(cnt,RT_CNT);
                rtnode->sibling_right->sibling_left = rtnode;
                rtnode = rtnode->sibling_right;
            }     
        }
    }

    /* Cursors must be closed */
    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif

    return head;
}

RTNode *db_get_all_sub_rtnode(){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_sub_rtnode");

    #ifdef SQLITE_DB
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json, *root;
    sqlite3_stmt *res;
    char *colname = NULL;
    char sql[1024] = {0}, buf[256] = {0};

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
        SUB* sub = NULL;
        for(int col = 0 ; col < cols; col++){
            
            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);

            if(bytes == 0) continue;
            switch(coltype){
                case SQLITE_TEXT:
                    memset(buf, 0, 256);
                    strncpy(buf, sqlite3_column_text(res, col), bytes);
                    cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                    break;
                case SQLITE_INTEGER:
                    cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                    break;
            }
        }
        cJSON_AddItemToObject(root, "m2m:sub", json);
        sub = cjson_to_sub_db(root);
        if(!head) {
            head = create_rtnode(sub,RT_SUB);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(sub, RT_SUB);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }   
        cJSON_Delete(root);
        root = NULL;
        rc = sqlite3_step(res);
    }

    sqlite3_finalize(res); 

    #else
    const char *TYPE = "23-";

    DBC* dbcp;
    DBT key, data;
    int ret;

    dbcp = DB_GET_CURSOR(subDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    RTNode* head = NULL, *rtnode = NULL;
    
    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, TYPE , 3) == 0){
            SUB* sub = db_get_sub((char*)key.data);
            if(!head) {
                head = create_rtnode(sub,RT_SUB);
                rtnode = head;
            } else {
                rtnode->sibling_right = create_rtnode(sub,RT_SUB);
                rtnode->sibling_right->sibling_left = rtnode;
                rtnode = rtnode->sibling_right;
            }     
        }
    }

    /* DB close */
    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif
    return head;
}

RTNode *db_get_all_acp_rtnode(){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_acp_rtnode");

    #ifdef SQLITE_DB
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

    while(rc == SQLITE_ROW){
        json = cJSON_CreateObject();
        root = cJSON_CreateObject();
        ACP* acp = NULL;
        
        for(int col = 0 ; col < cols; col++){
            
            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);

            if(bytes == 0) continue;
            switch(coltype){
                case SQLITE_TEXT:
                    memset(buf, 0, 256);
                    strncpy(buf, sqlite3_column_text(res, col), bytes);
                    cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                    break;
                case SQLITE_INTEGER:
                    cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                    break;
            }
        }
        cJSON_AddItemToObject(root, "m2m:acp", json);
        acp = cjson_to_acp(root);
        if(!head) {
            head = create_rtnode(acp,RT_SUB);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(acp, RT_SUB);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }   
        cJSON_Delete(root);
        rc = sqlite3_step(res);
    }

    sqlite3_finalize(res); 
    #else
    DBC* dbcp;
    DBT key, data;
    int ret;
    char *TYPE = "1-";

    //dbp = DB_CREATE_(dbp);
    //DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(acpDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    RTNode* head = NULL, *rtnode = NULL;

    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, TYPE , 2) == 0){
            ACP* acp = db_get_acp((char*)key.data);
            if(!head) {
                head = create_rtnode(acp,RT_ACP);
                rtnode = head;
            } else {
                rtnode->sibling_right = create_rtnode(acp,RT_ACP);
                rtnode->sibling_right->sibling_left = rtnode;
                rtnode = rtnode->sibling_right;
            }     
        }
    }

    /* Cursors must be closed */
    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif
    return head;
}

RTNode *db_get_all_grp_rtnode(){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_grp_rtnode");

    #ifdef SQLITE_DB
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json = NULL, *root = NULL;
    sqlite3_stmt *res;
    GRP* grp = NULL;
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

    while(rc == SQLITE_ROW){
        cols = sqlite3_column_count(res);
        grp = (GRP*) calloc(1, sizeof(GRP));
        json = cJSON_CreateObject();
        root = cJSON_CreateObject();
        
        for(int col = 0 ; col < cols; col++){
            
            colname = sqlite3_column_name(res, col);
            bytes = sqlite3_column_bytes(res, col);
            coltype = sqlite3_column_type(res, col);

            if(bytes == 0) continue;
            switch(coltype){
                case SQLITE_TEXT:
                    memset(buf, 0, 256);
                    strncpy(buf, sqlite3_column_text(res, col), bytes);
                    if(buf[0] == '['){
                        cJSON_AddItemToObject(json, colname, string_to_cjson_string_list_item(buf));
                    }else{
                        cJSON_AddItemToObject(json, colname, cJSON_CreateString(buf));
                    }
                    break;
                case SQLITE_INTEGER:
                    cJSON_AddItemToObject(json, colname, cJSON_CreateNumber(sqlite3_column_int(res, col)));
                    break;
            }
        }
        cJSON_AddItemToObject(root, "m2m:grp", json);
        cjson_to_grp(root, grp);
        
        if(!head) {
            head = create_rtnode(grp, RT_GRP);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(grp, RT_GRP);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }   
        cJSON_Delete(root);
        root = NULL;
        rc = sqlite3_step(res);
    }

    sqlite3_finalize(res); 
    #else
    const char* TYPE = "9-";
 
    DBC* dbcp;
    DBT key, data;
    int ret;

    //dbp = DB_CREATE_(dbp);
    //DB_OPEN(DATABASE);
    dbcp = DB_GET_CURSOR(grpDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    RTNode* head = NULL;
    RTNode* rtnode = NULL;

    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, TYPE , 2) == 0){
            GRP *grp = db_get_grp((char *) key.data);

            if(!head){
                head = create_rtnode(grp, RT_GRP);
                rtnode = head;
            }else{
                rtnode->sibling_right = create_rtnode(grp, RT_GRP);
                rtnode->sibling_right->sibling_left = rtnode;
                rtnode = rtnode->sibling_right;
            }            
            free_grp(grp);
        }
    }

    /* Cursors must be closed */
    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif

    return head;
}

RTNode* db_get_cin_rtnode_list(RTNode *parent_rtnode) {
    logger("DB", LOG_LEVEL_DEBUG, "call db_get_cin_rtnode_list");
    #ifdef SQLITE_DB
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
    while(rc == SQLITE_ROW){
        CIN* cin = NULL;
        json = cJSON_CreateObject();
        root = cJSON_CreateObject();
        
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
        cJSON_AddItemToObject(root, "m2m:cin", json);
        cin = cjson_to_cin(root);
        if(!head) {
            head = create_rtnode(cin,RT_CIN);
            rtnode = head;
        } else {
            rtnode->sibling_right = create_rtnode(cin, RT_CIN);
            rtnode->sibling_right->sibling_left = rtnode;
            rtnode = rtnode->sibling_right;
        }   
        cJSON_Delete(root);
        json = NULL;
        root = NULL;
        rc = sqlite3_step(res);
    }

    sqlite3_finalize(res); 
    #else
    const char* TYPE = "4-";
    
    DBC* dbcp;
    DBT key, data;
    int ret;

    char buf[256] = {0};
    char* pi = get_ri_rtnode(parent_rtnode);

    //dbp = DB_CREATE_(dbp);
    //DB_OPEN(DATABASE);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int idx = 0;
    
        
    RTNode* head = NULL, *rtnode = NULL;
    dbcp = DB_GET_CURSOR(resourceDBp);
    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        //find CIN
        if (strncmp(key.data, TYPE , 2) == 0){
            CIN *cin = db_get_cin((char*)key.data);
            //find pi
            if(!strcmp(pi, cin->pi)){
                sprintf(buf, "%s/%s", parent_rtnode->uri, cin->rn);
                if(!head) {
                    head = create_rtnode(cin, RT_CIN);
                    head->uri = strdup(buf);
                    rtnode = head;
                } else {
                    rtnode->sibling_right = create_rtnode(cin,RT_CIN);
                    rtnode->sibling_right->uri = strdup(buf);
                    rtnode->sibling_right->sibling_left = rtnode;
                    rtnode = rtnode->sibling_right;
                }
                rtnode->parent = parent_rtnode;
            } else {
                free_cin(cin);
            }
        }
    }

    /* Cursors must be closed */
    if (dbcp != NULL)
        dbcp->close(dbcp);
    #endif
    return head;
}

#ifdef SQLITE_DB
/**
 * get Latest/Oldest Content Instance
 * Oldest Flag = 0, Latest flag = 1
*/
CIN *db_get_cin_laol(RTNode *parent_rtnode, int laol){
    CIN *cin = NULL;

    char sql[1024] = {0}, buf[256] = {0};
    char *orb = NULL, *colname;
    int rc = 0;
    int cols, bytes, coltype;
    cJSON *json, *root;
    sqlite3_stmt *res = NULL;


    switch(laol){
        case 0:
            orb = "DESC";
            break;
        case 1:
            orb = "ASC";
            break;
        default:
            logger("DB", LOG_LEVEL_DEBUG, "Invalid la ol flag");
            return NULL;
    }

    sprintf(sql, "SELECT * from general, cin WHERE pi='%s' AND general.ri = cin.ri ORDER BY general.lt %s LIMIT 1;", get_ri_rtnode(parent_rtnode), orb);
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
    root = cJSON_CreateObject();
    
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
    cJSON_AddItemToObject(root, "m2m:cin", json);
    cin = cjson_to_cin(root);
    cJSON_Delete(root);

    logger("DB", LOG_LEVEL_DEBUG, "%s", cin->ri);

    sqlite3_finalize(res);
    return cin;   

}
cJSON* db_get_filter_criteria(char *to, FilterCriteria *fc) {

    logger("DB", LOG_LEVEL_DEBUG, "call db_get_filter_criteria");
    char buf[256] = {0};
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *result = NULL, *json = NULL, *prtjson = NULL, *chjson = NULL, *tmp = NULL;
    sqlite3_stmt *res = NULL;
    char *colname = NULL;
    char sql[1024] = {0};
    int jsize = 0, psize = 0 , chsize = 0;

    sprintf(buf, "SELECT uri FROM 'general' WHERE uri LIKE '%s/%%' AND ", to);
    strcat(sql, buf);

    if(fc->cra){
        sprintf(buf, "ct>'%s' ", fc->cra);
        strcat(sql, buf);
        filterOptionStr(fc->fo, sql);
    }
    if(fc->crb){
        sprintf(buf, "ct<='%s' ", fc->crb);
        strcat(sql, buf);
        filterOptionStr(fc->fo, sql);
    }
    if(fc->exa){
        sprintf(buf, "et>'%s' ", fc->exa);
        strcat(sql, buf);
        filterOptionStr(fc->fo, sql);
    }
    if(fc->exb){
        sprintf(buf, "et<='%s' ", fc->exb);
        strcat(sql, buf);
        filterOptionStr(fc->fo, sql);
    }

    if(fc->ms){
        sprintf(buf, "lt>'%s' ", fc->ms);
        strcat(sql, buf);
        filterOptionStr(fc->fo, sql);
    }
    if(fc->us){
        sprintf(buf, "lt<='%s' ", fc->us);
        strcat(sql, buf);
        filterOptionStr(fc->fo, sql);
    }

    if(fc->lbl){
        jsize = cJSON_GetArraySize(fc->lbl);
        for(int i = 0 ; i < jsize ; i++){
            sprintf(buf, "lbl LIKE '%%,%s,%%' OR ", cJSON_GetArrayItem(fc->lbl, i)->valuestring);
            strcat(sql, buf);
        }
        sql[strlen(sql)- 3] = '\0';
        filterOptionStr(fc->fo, sql);
    }

    if(fc->ty){
        for(int i = 0 ; i < fc->tycnt ; i++){
            sprintf(buf, " ty = %d OR", fc->ty[i]);
            strcat(sql, buf);
        }
        sql[strlen(sql) -2] = '\0';
        filterOptionStr(fc->fo, sql);
    }

    if(fc->sza){
        sprintf(buf, " ri IN (SELECT ri FROM 'cin' WHERE cs >= %d) ", fc->sza);
        strcat(sql, buf);
        filterOptionStr(fc->fo, sql);
    }

    if(fc->szb){
        sprintf(buf, " ri IN (SELECT ri FROM 'cin' WHERE cs < %d) ", fc->szb);
        strcat(sql, buf);
        filterOptionStr(fc->fo, sql);
    }

    if(fc->sts){
        sprintf(buf, " ri IN (SELECT ri FROM 'cnt' WHERE st < %d) ", fc->sts);
        strcat(sql, buf);
        filterOptionStr(fc->fo, sql);
    }

    if(fc->stb){
        sprintf(buf, " ri IN (SELECT ri FROM 'cnt' WHERE st >= %d) ", fc->stb);
        strcat(sql, buf);
        filterOptionStr(fc->fo, sql);
    }

    if(fc->fo == FO_AND){
        sql[strlen(sql) - 4] = '\0';
    }else if(fc->fo == FO_OR){
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

    if(fc->pty || fc->palb || fc->patr){
        logger("DB", LOG_LEVEL_DEBUG, "find parent");
        prtjson = db_get_parent_filter_criteria(to, fc);
        result = cjson_merge_arrays_by_operation(json, prtjson, fc->fo);
        cJSON_Delete(prtjson);
        prtjson = NULL;
    }

    if(fc->chty || fc->clbl || fc->catr){
        logger("DB", LOG_LEVEL_DEBUG, "find child");
        chjson = db_get_child_filter_criteria(to, fc);
        if(result){
            tmp = cjson_merge_arrays_by_operation(result, chjson, fc->fo);
            cJSON_Delete(result);
            result = tmp;
        }else{
            result = cjson_merge_arrays_by_operation(json, chjson, fc->fo);
        }
        cJSON_Delete(chjson);
        chjson = NULL;
    }
    
    if(!result)
        result = cJSON_Duplicate(json, cJSON_True);
    if(json)
        cJSON_Delete(json);
    if(prtjson)
        cJSON_Delete(prtjson);
    if(chjson)
        cJSON_Delete(chjson);
    
    return result;
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

cJSON *cjson_merge_arrays_by_operation(cJSON* arr1, cJSON* arr2, FilterOperation fo){
    cJSON *result = cJSON_CreateArray();
    int ar1size, ar2size;
    char buf[256] = {0};

    ar1size = cJSON_GetArraySize(arr1);
    ar2size = cJSON_GetArraySize(arr2);
    switch(fo){
        case FO_AND:
            for(int i = 0 ; i < ar1size ; i++){ 
                strcpy(buf, cJSON_GetArrayItem(arr1, i)->valuestring);
                if( do_uri_exist(arr2, buf)){
                    cJSON_AddItemToArray(result, cJSON_CreateString(buf));
                }
            }
            break;
        case FO_OR:
            for(int i = 0 ; i < ar1size ; i++){ 
                strcpy(buf, cJSON_GetArrayItem(arr1, i)->valuestring);
                if(!do_uri_exist(result, buf)){
                    cJSON_AddItemToArray(result, cJSON_CreateString(buf));
                }
            }
            for(int i = 0 ; i < ar2size ; i++){ 
                strcpy(buf, cJSON_GetArrayItem(arr2, i)->valuestring);
                if(!do_uri_exist(result, buf)){
                    cJSON_AddItemToArray(result, cJSON_CreateString(buf));
                }
            }
            break;
    }
    return result;
}

cJSON *db_get_parent_filter_criteria(char *to, FilterCriteria *fc){
    logger("DB", LOG_LEVEL_DEBUG, "call db_get_parent_filter_criteria");
    char buf[256] = {0};
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json;
    cJSON *chdjson;
    sqlite3_stmt *res = NULL;
    char *colname = NULL;
    char sql[1024] = {0};
    int jsize = 0;

    sprintf(sql, "SELECT uri FROM 'general' WHERE uri LIKE '%s/%%' AND ", to);

    if(fc->pty){
        for(int i = 0 ; i < fc->ptycnt ; i++){
            sprintf(buf, " ty = %d OR", fc->pty[i]);
            strcat(sql, buf);
        }
        sql[strlen(sql) -2] = '\0';
        filterOptionStr(fc->fo, sql);
    }
    
    if(fc->palb){
        jsize = cJSON_GetArraySize(fc->palb);
        for(int i = 0 ; i < jsize ; i++){
            sprintf(buf, "lbl LIKE '%%,%s,%%' OR ", cJSON_GetArrayItem(fc->palb, i)->valuestring);
            strcat(sql, buf);
        }
        sql[strlen(sql)- 3] = '\0';
        filterOptionStr(fc->fo, sql);
    }

    // if(fc->patr){ //TODO - patr
    //     sprintf(buf, "%s LIKE %s",);
    // }
    
    if(fc->fo == FO_AND){
        sql[strlen(sql)- 4] = '\0';
    }else if(fc->fo == FO_OR){
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
    chdjson = cJSON_CreateArray();

    for(int i = 0 ; i < jsize ; i++){
        sprintf(sql, "SELECT uri FROM general WHERE uri LIKE '%s/%%' AND uri NOT LIKE '%s/%%/%%';", 
                        cJSON_GetArrayItem(json, i)->valuestring, cJSON_GetArrayItem(json, i)->valuestring);
        logger("DB", LOG_LEVEL_DEBUG, "%s", sql);
        if(rc = sqlite3_prepare_v2(db, sql, -1, &res, NULL)){
            logger("DB", LOG_LEVEL_ERROR, "Failed select, %d", rc);
            return 0;
        }
        rc = sqlite3_step(res);
        while(rc == SQLITE_ROW){
            bytes = sqlite3_column_bytes(res, 0);
            if(bytes == 0){
                logger("DB", LOG_LEVEL_ERROR, "empty URI");
                cJSON_AddItemToArray(json, cJSON_CreateString("Internal Server ERROR"));
            }else{
                memset(buf,0, 256);
                strncpy(buf, sqlite3_column_text(res, 0), bytes);
                cJSON_AddItemToArray(chdjson, cJSON_CreateString(buf));
            }
            rc = sqlite3_step(res);
        }
        sqlite3_finalize(res);
    }
    cJSON_Delete(json);

   
    return chdjson;
}

cJSON *db_get_child_filter_criteria(char *to, FilterCriteria *fc){
    logger("DB", LOG_LEVEL_DEBUG, "call db_get_child_filter_criteria");
    char buf[256] = {0};
    int rc = 0;
    int cols = 0, bytes = 0, coltype = 0;
    cJSON *json;
    cJSON *ptrjson;
    sqlite3_stmt *res = NULL;
    char *turi = NULL, *cptr = NULL;
    char sql[1024] = {0};
    int jsize = 0;

    strcat(sql, "SELECT uri FROM 'general' WHERE ");

    if(fc->chty){
        for(int i = 0 ; i < fc->chtycnt ; i++){
            sprintf(buf, " ty = %d OR", fc->chty[i]);
            strcat(sql, buf);
        }
        sql[strlen(sql) -2] = '\0';
        filterOptionStr(fc->fo, sql);
    }
    
    if(fc->clbl){
        jsize = cJSON_GetArraySize(fc->clbl);
        for(int i = 0 ; i < jsize ; i++){
            sprintf(buf, "lbl LIKE '%%,%s,%%' OR ", cJSON_GetArrayItem(fc->clbl, i)->valuestring);
            strcat(sql, buf);
        }
        sql[strlen(sql)- 3] = '\0';
        filterOptionStr(fc->fo, sql);
    }

    // if(fc->catr){ //TODO - catr
    //     sprintf(buf, "%s LIKE %s",);
    // }

    if(fc->fo == FO_AND){
        sql[strlen(sql)- 4] = '\0';
    }else if(fc->fo == FO_OR){
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

    ptrjson = cJSON_CreateArray();
    for(int i = 0 ; i < jsize ; i++){
        turi = strdup(cJSON_GetArrayItem(json, i)->valuestring); 
        cptr = strrchr(turi, '/');
        *cptr = '\0';
        cJSON_AddItemToArray(ptrjson, cJSON_CreateString(turi));
        free(turi);
    }
    cJSON_Delete(json);

   
    return ptrjson;
}

#endif