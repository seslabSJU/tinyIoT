#include "onem2m.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

int display(char* database)
{
    fprintf(stderr,"[Display] %s \n", database); //DB name print

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int close_db, close_dbc, ret;

    close_db = close_dbc = 0;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return (1);
    }
    close_db = 1;

    /* Turn on additional error output. */
    dbp->set_errfile(dbp, stderr);
    dbp->set_errpfx(dbp, database);

    /* Open the database. */
    if ((ret = dbp->open(dbp, NULL, database, NULL,
        DB_UNKNOWN, DB_RDONLY, 0)) != 0) {
        dbp->err(dbp, ret, "%s: DB->open", database);
        goto err;
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        goto err;
    }
    close_dbc = 1;

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    /* Walk through the database and print out the key/data pairs. */
    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        //int
        if (strncmp(key.data, "ty", key.size) == 0 ||
            strncmp(key.data, "st", key.size) == 0 ||
            strncmp(key.data, "cni", key.size) == 0 ||
            strncmp(key.data, "cbs", key.size) == 0 ||
            strncmp(key.data, "cs", key.size) == 0
            ) {
            fprintf(stderr,"%.*s : %d\n", (int)key.size, (char*)key.data, *(int*)data.data);
        }
        //bool
        else if (strncmp(key.data, "rr", key.size) == 0) {
            fprintf(stderr,"%.*s : ", (int)key.size, (char*)key.data);
            if (*(bool*)data.data == true)
                fprintf(stderr,"true\n");
            else
                fprintf(stderr,"false\n");
        }

        //string
        else {
            fprintf(stderr,"%.*s : %.*s\n",
                (int)key.size, (char*)key.data,
                (int)data.size, (char*)data.data);
        }
    }
    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        fprintf(stderr,"Cursor ERROR\n");
        exit(0);
    }


err:    if (close_dbc && (ret = dbcp->close(dbcp)) != 0)
dbp->err(dbp, ret, "DBcursor->close");
if (close_db && (ret = dbp->close(dbp, 0)) != 0)
fprintf(stderr,
    "%s: DB->close: %s\n", database, db_strerror(ret));
return (0);
}

int Store_CSE(CSE *cse_object)
{
    char* DATABASE = "CSE.db";

    DB* dbp;    // db handle
    DBC* dbcp;
    FILE* error_file_pointer;
    DBT key, data;  // storving key and real data
    int ret;        // template value

    DBT key_ct, key_lt, key_rn, key_ri, key_pi, key_csi, key_ty;
    DBT data_ct, data_lt, data_rn, data_ri, data_pi, data_csi, data_ty;  // storving key and real data

    char* program_name = "my_prog";

    // if input == NULL
    if (cse_object->ri == NULL) cse_object->ri = "";
    if (cse_object->rn == NULL) cse_object->rn = "";
    if (cse_object->pi == NULL) cse_object->pi = "NULL";
    if (cse_object->ty == '\0') cse_object->ty = -1;
    if (cse_object->ct == NULL) cse_object->ct = "";
    if (cse_object->lt == NULL) cse_object->lt = "";
    if (cse_object->csi == NULL) cse_object->csi = "";

    ret = db_create(&dbp, NULL, 0);
    if (ret) {
        fprintf(stderr, "db_create : %s\n", db_strerror(ret));
        printf("File ERROR\n");
        exit(1);
    }

    dbp->set_errfile(dbp, error_file_pointer);
    dbp->set_errpfx(dbp, program_name);

    /*Set duplicate*/
    ret = dbp->set_flags(dbp, DB_DUP);
    if (ret != 0) {
        dbp->err(dbp, ret, "Attempt to set DUPSORT flag failed.");
        printf("Flag Set ERROR\n");
        dbp->close(dbp, 0);
        return(ret);
    }

    /*DB Open*/
    ret = dbp->open(dbp, NULL, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", DATABASE);
        printf("DB Open ERROR\n");
        exit(1);
    }

    /*
  * The DB handle for a Btree database supporting duplicate data
  * items is the argument; acquire a cursor for the database.
  */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        printf("Cursor ERROR");
        exit(1);
    }
 
    /* keyand data must initialize */
    memset(&key_rn, 0, sizeof(DBT));
    memset(&key_ri, 0, sizeof(DBT));
    memset(&key_pi, 0, sizeof(DBT));
    memset(&key_ty, 0, sizeof(DBT));
    memset(&key_ct, 0, sizeof(DBT));
    memset(&key_lt, 0, sizeof(DBT));
    memset(&key_csi, 0, sizeof(DBT));

    memset(&data_rn, 0, sizeof(DBT));
    memset(&data_ri, 0, sizeof(DBT));
    memset(&data_pi, 0, sizeof(DBT));
    memset(&data_ty, 0, sizeof(DBT));
    memset(&data_ct, 0, sizeof(DBT));
    memset(&data_lt, 0, sizeof(DBT));
    memset(&data_csi, 0, sizeof(DBT));

    /* initialize the data to be the first of two duplicate records. */
    data_rn.data = cse_object->rn;
    data_rn.size = strlen(cse_object->rn) + 1;
    key_rn.data = "rn";
    key_rn.size = strlen("rn") + 1;

    data_ri.data = cse_object->ri;
    data_ri.size = strlen(cse_object->ri) + 1;
    key_ri.data = "ri";
    key_ri.size = strlen("ri") + 1;

    data_pi.data = cse_object->pi;
    data_pi.size = strlen(cse_object->pi) + 1;
    key_pi.data = "pi";
    key_pi.size = strlen("pi") + 1;

    data_ct.data = cse_object->ct;
    data_ct.size = strlen(cse_object->ct) + 1;
    key_ct.data = "ct";
    key_ct.size = strlen("ct") + 1;

    data_lt.data = cse_object->lt;
    data_lt.size = strlen(cse_object->lt) + 1;
    key_lt.data = "lt";
    key_lt.size = strlen("lt") + 1;

    data_csi.data = cse_object->csi;
    data_csi.size = strlen(cse_object->csi) + 1;
    key_csi.data = "csi";
    key_csi.size = strlen("csi") + 1;

    data_ty.data = &cse_object->ty;
    data_ty.size = sizeof(cse_object->ty);
    key_ty.data = "ty";
    key_ty.size = strlen("ty") + 1;

    /* CSE -> only one & first */
    if ((ret = dbcp->put(dbcp, &key_ri, &data_ri, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_rn, &data_rn, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_pi, &data_pi, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_ty, &data_ty, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");

    if ((ret = dbcp->put(dbcp, &key_ct, &data_ct, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_lt, &data_lt, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_csi, &data_csi, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");

    dbcp->close(dbcp);
    dbp->close(dbp, 0); //DB close
    return 1;
}

int Store_AE(AE* ae_object) {
	fprintf(stderr,"[Store AE] %s...",ae_object->ri);
    char* DATABASE = "AE.db";
    DB* dbp;    // db handle
    DBC* dbcp;
    FILE* error_file_pointer;
    DBT key_ct, key_lt, key_rn, key_ri, key_pi, key_ty, key_et, key_api, key_rr,key_aei;
    DBT data_ct, data_lt, data_rn, data_ri, data_pi, data_ty, data_et, data_api, data_rr, data_aei;  // storving key and real data
    int ret;        // template value

    char* program_name = "my_prog";

    // if input == NULL
    if (ae_object->ri == NULL) ae_object->ri = "";
    if (ae_object->rn == NULL) ae_object->rn = "";
    if (ae_object->pi == NULL) ae_object->pi = "";
    if (ae_object->ty == '\0') ae_object->ty = -1;
    if (ae_object->ct == NULL) ae_object->ct = "";
    if (ae_object->lt == NULL) ae_object->lt = "";
    if (ae_object->et == NULL) ae_object->et = "";

    if (ae_object->rr == '\0') ae_object->rr = true;
    if (ae_object->api == NULL) ae_object->api = "";
    if (ae_object->aei == NULL) ae_object->aei = "";

    ret = db_create(&dbp, NULL, 0);
    if (ret) {
        fprintf(stderr, "db_create : %s\n", db_strerror(ret));
        fprintf(stderr,"File ERROR\n");
        exit(1);
    }

    dbp->set_errfile(dbp, error_file_pointer);
    dbp->set_errpfx(dbp, program_name);

    /*Set duplicate*/
    ret = dbp->set_flags(dbp, DB_DUP);
    if (ret != 0) {
        dbp->err(dbp, ret, "Attempt to set DUPSORT flag failed.");
        fprintf(stderr,"Flag Set ERROR\n");
        dbp->close(dbp, 0);
        return(ret);
    }

    /*DB Open*/
    ret = dbp->open(dbp, NULL, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664);

    if (ret) {
        dbp->err(dbp, ret, "%s", DATABASE);
        fprintf(stderr,"DB Open ERROR\n");
        exit(1);
    }
    
    /*
* The DB handle for a Btree database supporting duplicate data
* items is the argument; acquire a cursor for the database.
*/
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        fprintf(stderr,"Cursor ERROR\n");
        exit(1);
    }

    /* keyand data must initialize */
    memset(&key_rn, 0, sizeof(DBT));
    memset(&key_ri, 0, sizeof(DBT));
    memset(&key_pi, 0, sizeof(DBT));
    memset(&key_ty, 0, sizeof(DBT));
    memset(&key_ct, 0, sizeof(DBT));
    memset(&key_lt, 0, sizeof(DBT));
    memset(&key_et, 0, sizeof(DBT));
    memset(&key_api, 0, sizeof(DBT));
    memset(&key_rr, 0, sizeof(DBT));
    memset(&key_aei, 0, sizeof(DBT));

    memset(&data_rn, 0, sizeof(DBT));
    memset(&data_ri, 0, sizeof(DBT));
    memset(&data_pi, 0, sizeof(DBT));
    memset(&data_ty, 0, sizeof(DBT));
    memset(&data_ct, 0, sizeof(DBT));
    memset(&data_lt, 0, sizeof(DBT));
    memset(&data_et, 0, sizeof(DBT));
    memset(&data_api, 0, sizeof(DBT));
    memset(&data_rr, 0, sizeof(DBT));
    memset(&data_aei, 0, sizeof(DBT));

    // Store key & data
    data_rn.data = ae_object->rn;
    data_rn.size = strlen(ae_object->rn) + 1;
    key_rn.data = "rn";
    key_rn.size = strlen("rn") + 1;

    data_ri.data = ae_object->ri;
    data_ri.size = strlen(ae_object->ri) + 1;
    key_ri.data = "ri";
    key_ri.size = strlen("ri") + 1;

    data_pi.data = ae_object->pi;
    data_pi.size = strlen(ae_object->pi) + 1;
    key_pi.data = "pi";
    key_pi.size = strlen("pi") + 1;

    data_ty.data = &ae_object->ty;
    data_ty.size = sizeof(ae_object->ty);
    key_ty.data = "ty";
    key_ty.size = strlen("ty") + 1;

    data_ct.data = ae_object->ct;
    data_ct.size = strlen(ae_object->ct) + 1;
    key_ct.data = "ct";
    key_ct.size = strlen("ct") + 1;

    data_lt.data = ae_object->lt;
    data_lt.size = strlen(ae_object->lt) + 1;
    key_lt.data = "lt";
    key_lt.size = strlen("lt") + 1;

    data_et.data = ae_object->et;
    data_et.size = strlen(ae_object->et) + 1;
    key_et.data = "et";
    key_et.size = strlen("et") + 1;

    data_api.data = ae_object->api;
    data_api.size = strlen(ae_object->api) + 1;
    key_api.data = "api";
    key_api.size = strlen("api") + 1;

    data_rr.data = &ae_object->rr;
    data_rr.size = sizeof(ae_object->rr);
    key_rr.data = "rr";
    key_rr.size = strlen("rr") + 1;

    data_aei.data = ae_object->aei;
    data_aei.size = strlen(ae_object->aei) + 1;
    key_aei.data = "aei";
    key_aei.size = strlen("aei") + 1;


    if ((ret = dbcp->put(dbcp, &key_ri, &data_ri, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "DB->cursor");
    if ((ret = dbcp->put(dbcp, &key_rn, &data_rn, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "DB->cursor");
    if ((ret = dbcp->put(dbcp, &key_pi, &data_pi, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "DB->cursor");
    if ((ret = dbcp->put(dbcp, &key_ty, &data_ty, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "DB->cursor");
    if ((ret = dbcp->put(dbcp, &key_ct, &data_ct, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "DB->cursor");
    if ((ret = dbcp->put(dbcp, &key_lt, &data_lt, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "DB->cursor");
    if ((ret = dbcp->put(dbcp, &key_et, &data_et, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "DB->cursor");
    if ((ret = dbcp->put(dbcp, &key_api, &data_api, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "DB->cursor");
    if ((ret = dbcp->put(dbcp, &key_rr, &data_rr, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "DB->cursor");
    if ((ret = dbcp->put(dbcp, &key_aei, &data_aei, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "DB->cursor");


    dbcp->close(dbcp);
    dbp->close(dbp, 0); //DB close
	fprintf(stderr,"OK\n");
    return 1;
}

int Store_CNT(CNT *cnt_object)
{
    char* DATABASE = "CNT.db";
    
    fprintf(stderr,"[Store CNT] %s...",cnt_object->ri);

    DB* dbp;    // db handle
    DBC* dbcp;
    FILE* error_file_pointer;
    DBT key, data;  // storving key and real data
    int ret;        // template value

    DBT key_ct, key_lt, key_rn, key_ri, key_pi, key_ty, key_et, key_cni, key_cbs, key_st;
    DBT data_ct, data_lt, data_rn, data_ri, data_pi, data_ty, data_et, data_cni, data_cbs, data_st;  // storving key and real data

    char* program_name = "my_prog";

    // if input == NULL
    if (cnt_object->ri == NULL) cnt_object->ri = "";
    if (cnt_object->rn == NULL) cnt_object->rn = "";
    if (cnt_object->pi == NULL) cnt_object->pi = "";
    if (cnt_object->ty == '\0') cnt_object->ty = -1;
    if (cnt_object->ct == NULL) cnt_object->ct = "";
    if (cnt_object->lt == NULL) cnt_object->lt = "";
    if (cnt_object->et == NULL) cnt_object->et = "";

    if (cnt_object->cni == '\0') cnt_object->cni = -1;
    if (cnt_object->cbs == '\0') cnt_object->cbs = -1;
    if (cnt_object->st == '\0') cnt_object->st = -1;

    ret = db_create(&dbp, NULL, 0);
    if (ret) {
        fprintf(stderr, "db_create : %s\n", db_strerror(ret));
        fprintf(stderr,"File ERROR\n");
        exit(1);
    }

    dbp->set_errfile(dbp, error_file_pointer);
    dbp->set_errpfx(dbp, program_name);

    /*Set duplicate*/
    ret = dbp->set_flags(dbp, DB_DUP);
    if (ret != 0) {
        dbp->err(dbp, ret, "Attempt to set DUPSORT flag failed.");
        fprintf(stderr,"Flag Set ERROR\n");
        dbp->close(dbp, 0);
        return(ret);
    }

    /*DB Open*/
    ret = dbp->open(dbp, NULL, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", DATABASE);
        fprintf(stderr,"DB Open ERROR\n");
        exit(1);
    }

    /*
  * The DB handle for a Btree database supporting duplicate data
  * items is the argument; acquire a cursor for the database.
  */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        fprintf(stderr,"Cursor ERROR");
        exit(1);
    }
 
    /* keyand data must initialize */
    memset(&key_ct, 0, sizeof(DBT));
    memset(&key_lt, 0, sizeof(DBT));
    memset(&key_rn, 0, sizeof(DBT));
    memset(&key_ri, 0, sizeof(DBT));
    memset(&key_pi, 0, sizeof(DBT));
    memset(&key_ty, 0, sizeof(DBT));
    memset(&key_et, 0, sizeof(DBT));
    memset(&key_cni, 0, sizeof(DBT));
    memset(&key_cbs, 0, sizeof(DBT));
    memset(&key_st, 0, sizeof(DBT));

    memset(&data_ct, 0, sizeof(DBT));
    memset(&data_lt, 0, sizeof(DBT));
    memset(&data_rn, 0, sizeof(DBT));
    memset(&data_ri, 0, sizeof(DBT));
    memset(&data_pi, 0, sizeof(DBT));
    memset(&data_ty, 0, sizeof(DBT));
    memset(&data_et, 0, sizeof(DBT));
    memset(&data_cni, 0, sizeof(DBT));
    memset(&data_cbs, 0, sizeof(DBT));
    memset(&data_st, 0, sizeof(DBT));


    /* initialize the data to be the first of two duplicate records. */
    data_rn.data = cnt_object->rn;
    data_rn.size = strlen(cnt_object->rn) + 1;
    key_rn.data = "rn";
    key_rn.size = strlen("rn") + 1;

    data_ri.data = cnt_object->ri;
    data_ri.size = strlen(cnt_object->ri) + 1;
    key_ri.data = "ri";
    key_ri.size = strlen("ri") + 1;

    data_pi.data = cnt_object->pi;
    data_pi.size = strlen(cnt_object->pi) + 1;
    key_pi.data = "pi";
    key_pi.size = strlen("pi") + 1;

    data_ct.data = cnt_object->ct;
    data_ct.size = strlen(cnt_object->ct) + 1;
    key_ct.data = "ct";
    key_ct.size = strlen("ct") + 1;

    data_lt.data = cnt_object->lt;
    data_lt.size = strlen(cnt_object->lt) + 1;
    key_lt.data = "lt";
    key_lt.size = strlen("lt") + 1;

    data_et.data = cnt_object->et;
    data_et.size = strlen(cnt_object->et) + 1;
    key_et.data = "et";
    key_et.size = strlen("et") + 1;

    data_ty.data = &cnt_object->ty;
    data_ty.size = sizeof(cnt_object->ty);
    key_ty.data = "ty";
    key_ty.size = strlen("ty") + 1;

    data_st.data = &cnt_object->st;
    data_st.size = sizeof(cnt_object->st);
    key_st.data = "st";
    key_st.size = strlen("st") + 1;

    data_cni.data = &cnt_object->cni;
    data_cni.size = sizeof(cnt_object->cni);
    key_cni.data = "cni";
    key_cni.size = strlen("cni") + 1;

    data_cbs.data = &cnt_object->cbs;
    data_cbs.size = sizeof(cnt_object->cbs);
    key_cbs.data = "cbs";
    key_cbs.size = strlen("cbs") + 1;

    /* CNT -> only one & first */
    if ((ret = dbcp->put(dbcp, &key_ri, &data_ri, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_rn, &data_rn, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_pi, &data_pi, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_ty, &data_ty, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");

    if ((ret = dbcp->put(dbcp, &key_ct, &data_ct, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_lt, &data_lt, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_cni, &data_cni, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_st, &data_st, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_et, &data_et, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_cbs, &data_cbs, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");

    dbcp->close(dbcp);
    dbp->close(dbp, 0); //DB close
    fprintf(stderr,"OK\n");
    return 1;
}

int Store_CIN(CIN *cin_object)
{
	fprintf(stderr,"[Store CIN] %s...",cin_object->ri);
    char* DATABASE = "CIN.db";

    DB* dbp;    // db handle
    DBC* dbcp;
    FILE* error_file_pointer;
    DBT key, data;  // storving key and real data
    int ret;        // template value

    DBT key_ct, key_lt, key_rn, key_ri, key_pi, key_ty, key_et, key_st, key_cs, key_con;
    DBT data_ct, data_lt, data_rn, data_ri, data_pi, data_ty, data_et, data_st, data_cs, data_con;  // storving key and real data

    char* program_name = "my_prog";

    // if input == NULL
    if (cin_object->ri == NULL) cin_object->ri = "";
    if (cin_object->rn == NULL) cin_object->rn = "";
    if (cin_object->pi == NULL) cin_object->pi = "";
    if (cin_object->ty == '\0') cin_object->ty = -1;
    if (cin_object->ct == NULL) cin_object->ct = "";
    if (cin_object->lt == NULL) cin_object->lt = "";
    if (cin_object->et == NULL) cin_object->et = "";

    if (cin_object->con == NULL) cin_object->con = "";
    if (cin_object->cs == '\0') cin_object->cs = -1;
    if (cin_object->st == '\0') cin_object->st = -1;
    
    ret = db_create(&dbp, NULL, 0);
    if (ret) {
        fprintf(stderr, "db_create : %s\n", db_strerror(ret));
        printf("File ERROR\n");
        exit(1);
    }

    dbp->set_errfile(dbp, error_file_pointer);
    dbp->set_errpfx(dbp, program_name);

    /*Set duplicate*/
    ret = dbp->set_flags(dbp, DB_DUP);
    if (ret != 0) {
        dbp->err(dbp, ret, "Attempt to set DUPSORT flag failed.");
        printf("Flag Set ERROR\n");
        dbp->close(dbp, 0);
        return(ret);
    }

    /*DB Open*/
    ret = dbp->open(dbp, NULL, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", DATABASE);
        printf("DB Open ERROR\n");
        exit(1);
    }

    /*
  * The DB handle for a Btree database supporting duplicate data
  * items is the argument; acquire a cursor for the database.
  */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        printf("Cursor ERROR");
        exit(1);
    }
 
    // key and data must initialize
    memset(&key_ct, 0, sizeof(DBT));
    memset(&key_lt, 0, sizeof(DBT));
    memset(&key_rn, 0, sizeof(DBT));
    memset(&key_ri, 0, sizeof(DBT));
    memset(&key_pi, 0, sizeof(DBT));
    memset(&key_ty, 0, sizeof(DBT));
    memset(&key_et, 0, sizeof(DBT));
    memset(&key_cs, 0, sizeof(DBT));
    memset(&key_con, 0, sizeof(DBT));
    memset(&key_st, 0, sizeof(DBT));

    memset(&data_ct, 0, sizeof(DBT));
    memset(&data_lt, 0, sizeof(DBT));
    memset(&data_rn, 0, sizeof(DBT));
    memset(&data_ri, 0, sizeof(DBT));
    memset(&data_pi, 0, sizeof(DBT));
    memset(&data_ty, 0, sizeof(DBT));
    memset(&data_et, 0, sizeof(DBT));
    memset(&data_cs, 0, sizeof(DBT));
    memset(&data_con, 0, sizeof(DBT));
    memset(&data_st, 0, sizeof(DBT));


    /* initialize the data to be the first of two duplicate records. */
    data_rn.data = cin_object->rn;
    data_rn.size = strlen(cin_object->rn) + 1;
    key_rn.data = "rn";
    key_rn.size = strlen("rn") + 1;

    data_ri.data = cin_object->ri;
    data_ri.size = strlen(cin_object->ri) + 1;
    key_ri.data = "ri";
    key_ri.size = strlen("ri") + 1;

    data_pi.data = cin_object->pi;
    data_pi.size = strlen(cin_object->pi) + 1;
    key_pi.data = "pi";
    key_pi.size = strlen("pi") + 1;

    data_ct.data = cin_object->ct;
    data_ct.size = strlen(cin_object->ct) + 1;
    key_ct.data = "ct";
    key_ct.size = strlen("ct") + 1;

    data_lt.data = cin_object->lt;
    data_lt.size = strlen(cin_object->lt) + 1;
    key_lt.data = "lt";
    key_lt.size = strlen("lt") + 1;

    data_et.data = cin_object->et;
    data_et.size = strlen(cin_object->et) + 1;
    key_et.data = "et";
    key_et.size = strlen("et") + 1;

    data_con.data = cin_object->con;
    data_con.size = strlen(cin_object->con) + 1;
    key_con.data = "con";
    key_con.size = strlen("con") + 1;

    data_ty.data = &cin_object->ty;
    data_ty.size = sizeof(cin_object->ty);
    key_ty.data = "ty";
    key_ty.size = strlen("ty") + 1;

    data_st.data = &cin_object->st;
    data_st.size = sizeof(cin_object->st);
    key_st.data = "st";
    key_st.size = strlen("st") + 1;

    data_cs.data = &cin_object->cs;
    data_cs.size = sizeof(cin_object->cs);
    key_cs.data = "cs";
    key_cs.size = strlen("cs") + 1;

    /* CIN -> only one & first */
    if ((ret = dbcp->put(dbcp, &key_ri, &data_ri, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_rn, &data_rn, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_pi, &data_pi, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_ty, &data_ty, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");

    if ((ret = dbcp->put(dbcp, &key_ct, &data_ct, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_lt, &data_lt, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_st, &data_st, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_et, &data_et, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");

    if ((ret = dbcp->put(dbcp, &key_cs, &data_cs, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_con, &data_con, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");

    dbcp->close(dbcp);
    dbp->close(dbp, 0); //DB close
    fprintf(stderr,"OK\n");
    return 1;
}

int Store_Sub(Sub *sub_object) {
    fprintf(stderr,"[Store Sub] %s...",sub_object->ri);
    char* DATABASE = "SUB.db";

    DB* dbp;    // db handle
    DBC* dbcp;
    FILE* error_file_pointer;
    DBT key, data;  // storving key and real data
    int ret;        // template value

    DBT key_pi;
    DBT data_rn, data_net, data_nu, data_ri, data_ct,
     data_et, data_lt, data_ty, data_nct, data_sur;  // storving key and real data

    char* program_name = "my_prog";

    // if input == NULL
    if (sub_object->pi == NULL) {
        fprintf(stderr, "The key is NULL\n");
        return 0;
    }
    if (sub_object->rn == NULL) sub_object->rn = "";
    if (sub_object->ri == NULL) sub_object->ri = "";
    if (sub_object->nu == NULL) sub_object->nu = "";
    if (sub_object->net == NULL) sub_object->net = "1";
    if (sub_object->ct == NULL) sub_object->ct = "";
    if (sub_object->et == NULL) sub_object->et = "";
    if (sub_object->lt == NULL) sub_object->lt = "";
    if (sub_object->ty == '\0') sub_object->ty = 23;
    if (sub_object->nct == '\0') sub_object->nct = 1;
    if (sub_object->sur == NULL) sub_object->sur = "";    


    ret = db_create(&dbp, NULL, 0);
    if (ret) {
        fprintf(stderr, "db_create : %s\n", db_strerror(ret));
        fprintf(stderr, "File ERROR\n");
        return 0;
    }

    dbp->set_errfile(dbp, error_file_pointer);
    dbp->set_errpfx(dbp, program_name);

    /*Set duplicate*/
    ret = dbp->set_flags(dbp, DB_DUP);
    if (ret != 0) {
        dbp->err(dbp, ret, "Attempt to set DUPSORT flag failed.");
        fprintf(stderr, "Flag Set ERROR\n");
        dbp->close(dbp, 0);
        return(ret);
    }

    /*DB Open*/
    ret = dbp->open(dbp, NULL, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", DATABASE);
        fprintf(stderr, "DB Open ERROR\n");
        return 0;
    }

    /*
  * The DB handle for a Btree database supporting duplicate data
  * items is the argument; acquire a cursor for the database.
  */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        fprintf(stderr, "Cursor ERROR");
        return 0;
    }

    /* keyand data must initialize */
    memset(&key_pi, 0, sizeof(DBT));

    memset(&data_rn, 0, sizeof(DBT));
    memset(&data_ri, 0, sizeof(DBT));
    memset(&data_nu, 0, sizeof(DBT));
    memset(&data_net, 0, sizeof(DBT));
    memset(&data_ct, 0, sizeof(DBT));
    memset(&data_et, 0, sizeof(DBT));
    memset(&data_lt, 0, sizeof(DBT));
    memset(&data_ty, 0, sizeof(DBT));
    memset(&data_nct, 0, sizeof(DBT));
    memset(&data_sur, 0, sizeof(DBT));

    /* initialize the data to be the first of two duplicate records. */
    key_pi.data = sub_object->pi;
    key_pi.size = strlen(sub_object->pi) + 1;

    data_rn.data = sub_object->rn;
    data_rn.size = strlen(sub_object->rn) + 1;

    data_ri.data = sub_object->ri;
    data_ri.size = strlen(sub_object->ri) + 1;

    data_nu.data = sub_object->nu;
    data_nu.size = strlen(sub_object->nu) + 1;

    data_net.data = sub_object->net;
    data_net.size = strlen(sub_object->net) + 1;

    data_ct.data = sub_object->ct;
    data_ct.size = strlen(sub_object->ct) + 1;

    data_et.data = sub_object->et;
    data_et.size = strlen(sub_object->et) + 1;

    data_lt.data = sub_object->lt;
    data_lt.size = strlen(sub_object->lt) + 1;

    data_ty.data = &sub_object->ty;
    data_ty.size = sizeof(sub_object->ty) + 1;

    data_nct.data = &sub_object->nct;
    data_nct.size = sizeof(sub_object->nct) + 1;

    data_sur.data = sub_object->sur;
    data_sur.size = strlen(sub_object->sur) + 1;


    /* input DB */
    if ((ret = dbcp->put(dbcp, &key_pi, &data_ri, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_pi, &data_rn, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_pi, &data_nu, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_pi, &data_net, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_pi, &data_sur, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");


    if ((ret = dbcp->put(dbcp, &key_pi, &data_ct, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_pi, &data_et, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_pi, &data_lt, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_pi, &data_ty, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");
    if ((ret = dbcp->put(dbcp, &key_pi, &data_nct, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");



    /* DB close */
    dbcp->close(dbcp);
    dbp->close(dbp, 0); 
    fprintf(stderr,"OK\n");
    return 1;
}

CSE* Get_CSE() {
    fprintf(stderr, "[Get CSE]...");

    //store CSE Object
    CSE* new_cse = (CSE*)malloc(sizeof(CSE));

    char* database = "CSE.db";
    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int close_db, close_dbc, ret;

    close_db = close_dbc = 0;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }
    close_db = 1;

    /* Turn on additional error output. */
    dbp->set_errfile(dbp, stderr);
    dbp->set_errpfx(dbp, database);

    /* Open the database. */
    if ((ret = dbp->open(dbp, NULL, database, NULL,
        DB_UNKNOWN, DB_RDONLY, 0)) != 0) {
        dbp->err(dbp, ret, "%s: DB->open", database);
        goto err;
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        goto err;
    }
    close_dbc = 1;

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    //store CSE
    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {

        if (strncmp(key.data, "ri", key.size) == 0) {
            new_cse->ri = malloc(data.size);
            strcpy(new_cse->ri, data.data);
        }
        if (strncmp(key.data, "pi", key.size) == 0) {
            new_cse->pi = malloc(data.size);
            strcpy(new_cse->pi, data.data);
        }
        if (strncmp(key.data, "rn", key.size) == 0) {
            new_cse->rn = malloc(data.size);
            strcpy(new_cse->rn, data.data);
        }
        if (strncmp(key.data, "ty", key.size) == 0) {
            new_cse->ty = *(int*)data.data;
        }
        if (strncmp(key.data, "csi", key.size) == 0) {
            new_cse->csi = malloc(data.size);
            strcpy(new_cse->csi, data.data);
        }
        if (strncmp(key.data, "lt", key.size) == 0) {
            new_cse->lt = malloc(data.size);
            strcpy(new_cse->lt, data.data);
        }
        if (strncmp(key.data, "ct", key.size) == 0) {
            new_cse->ct = malloc(data.size);
            strcpy(new_cse->ct, data.data);
        }
    }

    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        printf("Cursor ERROR\n");
        exit(0);
    }

err:    if (close_dbc && (ret = dbcp->close(dbcp)) != 0)
dbp->err(dbp, ret, "DBcursor->close");
if (close_db && (ret = dbp->close(dbp, 0)) != 0)
fprintf(stderr,
    "%s: DB->close: %s\n", database, db_strerror(ret));
    fprintf(stderr,"OK\n");
return new_cse;
}

AE* Get_AE(char* ri) {
    fprintf(stderr,"[Get AE] %s...", ri);

    //store AE
    AE* new_ae = (AE*)malloc(sizeof(AE));

    char* database = "AE.db";

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int ret;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }
	
    /* Open the database. */
    ret = dbp->open(dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", database);
        exit(1);
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int idx = 0;
    int flag = 0;
    // žî¹øÂ° AEÀÎÁö Ã£±â À§ÇÑ Ä¿Œ­
    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }
    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "ri", key.size) == 0) {
            idx++;
            if (strncmp(data.data, ri, data.size) == 0) {
                flag = 1;
                new_ae->ri = malloc(data.size);
                strcpy(new_ae->ri, data.data);
                break;
            }
        }
    }
    if (flag == 0) {
        fprintf(stderr,"Not Found\n");
        return NULL;
        //exit(1);
    }

    int cnt_rn = 0;
    int cnt_pi = 0;
    int cnt_ty = 0;
    int cnt_et = 0;
    int cnt_lt = 0;
    int cnt_ct = 0;
    int cnt_api = 0;
    int cnt_aei = 0;
    int cnt_rr = 0;
    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "rn", key.size) == 0) {
            cnt_rn++;
            if (cnt_rn == idx) {
                new_ae->rn = malloc(data.size);
                strcpy(new_ae->rn, data.data);
            }
        }
        if (strncmp(key.data, "pi", key.size) == 0) {
            cnt_pi++;
            if (cnt_pi == idx) {
                new_ae->pi = malloc(data.size);
                strcpy(new_ae->pi, data.data);
            }
        }
        if (strncmp(key.data, "api", key.size) == 0) {
            cnt_api++;
            if (cnt_api == idx) {
                new_ae->api = malloc(data.size);
                strcpy(new_ae->api, data.data);
            }
        }
        if (strncmp(key.data, "aei", key.size) == 0) {
            cnt_aei++;
            if (cnt_aei == idx) {
                new_ae->aei = malloc(data.size);
                strcpy(new_ae->aei, data.data);
            }
        }
        if (strncmp(key.data, "et", key.size) == 0) {
            cnt_et++;
            if (cnt_et == idx) {
                new_ae->et = malloc(data.size);
                strcpy(new_ae->et, data.data);
            }
        }
        if (strncmp(key.data, "lt", key.size) == 0) {
            cnt_lt++;
            if (cnt_lt == idx) {
                new_ae->lt = malloc(data.size);
                strcpy(new_ae->lt, data.data);
            }
        }
        if (strncmp(key.data, "ct", key.size) == 0) {
            cnt_ct++;
            if (cnt_ct == idx) {
                new_ae->ct = malloc(data.size);
                strcpy(new_ae->ct, data.data);
            }
        }
        if (strncmp(key.data, "ty", key.size) == 0) {
            cnt_ty++;
            if (cnt_ty == idx) {
                new_ae->ty = *(int*)data.data;
            }
        }
        if (strncmp(key.data, "rr", key.size) == 0) {
            cnt_rr++;
            if (cnt_rr == idx) {
                new_ae->rr = *(bool*)data.data;
            }
        }
    }
    //fprintf(stderr,"[%d]\n",idx);

    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        fprintf(stderr,"Cursor ERROR\n");
        exit(0);
    }
    
    fprintf(stderr,"OK\n");

    return new_ae;
}

CNT* Get_CNT(char* ri) {
    fprintf(stderr,"[Get CNT] %s...", ri);

    //store CNT
    CNT* new_cnt = (CNT*)malloc(sizeof(CNT));

    char* database = "CNT.db";

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int close_db, close_dbc, ret;

    close_db = close_dbc = 0;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }
    close_db = 1;

    /* Turn on additional error output. */
    dbp->set_errfile(dbp, stderr);
    dbp->set_errpfx(dbp, database);

    /* Open the database. */
    if ((ret = dbp->open(dbp, NULL, database, NULL,
        DB_UNKNOWN, DB_RDONLY, 0)) != 0) {
        dbp->err(dbp, ret, "%s: DB->open", database);
        goto err;
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        goto err;
    }
    close_dbc = 1;

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int idx = 0;
    // žî¹øÂ° CNTÀÎÁö Ã£±â À§ÇÑ Ä¿Œ­
    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        goto err;
    }
    close_dbc = 1;
    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "ri", key.size) == 0) {
            idx++;
            if (strncmp(data.data, ri, data.size) == 0) {
                new_cnt->ri = malloc(data.size);
                strcpy(new_cnt->ri, data.data);
                break;
            }
        }
    }

    int cnt_rn = 0;
    int cnt_pi = 0;
    int cnt_ty = 0;
    int cnt_et = 0;
    int cnt_lt = 0;
    int cnt_ct = 0;
    int cnt_st = 0;
    int cnt_cni = 0;
    int cnt_cbs = 0;

    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "rn", key.size) == 0) {
            cnt_rn++;
            if (cnt_rn == idx) {
                new_cnt->rn = malloc(data.size);
                strcpy(new_cnt->rn, data.data);
            }
        }
        if (strncmp(key.data, "pi", key.size) == 0) {
            cnt_pi++;
            if (cnt_pi == idx) {
                new_cnt->pi = malloc(data.size);
                strcpy(new_cnt->pi, data.data);
            }
        }
        if (strncmp(key.data, "et", key.size) == 0) {
            cnt_et++;
            if (cnt_et == idx) {
                new_cnt->et = malloc(data.size);
                strcpy(new_cnt->et, data.data);
            }
        }
        if (strncmp(key.data, "lt", key.size) == 0) {
            cnt_lt++;
            if (cnt_lt == idx) {
                new_cnt->lt = malloc(data.size);
                strcpy(new_cnt->lt, data.data);
            }
        }
        if (strncmp(key.data, "ct", key.size) == 0) {
            cnt_ct++;
            if (cnt_ct == idx) {
                new_cnt->ct = malloc(data.size);
                strcpy(new_cnt->ct, data.data);
            }
        }
        if (strncmp(key.data, "ty", key.size) == 0) {
            cnt_ty++;
            if (cnt_ty == idx) {
                new_cnt->ty = *(int*)data.data;
            }
        }
        if (strncmp(key.data, "st", key.size) == 0) {
            cnt_st++;
            if (cnt_st == idx) {
                new_cnt->st = *(int*)data.data;
            }
        }
        if (strncmp(key.data, "cni", key.size) == 0) {
            cnt_cni++;
            if (cnt_cni == idx) {
                new_cnt->cni = *(int*)data.data;
            }
        }
        if (strncmp(key.data, "cbs", key.size) == 0) {
            cnt_cbs++;
            if (cnt_cbs == idx) {
                new_cnt->cbs = *(int*)data.data;
            }
        }
    }
    //fprintf(stderr,"[%d]\n",idx);

    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        fprintf(stderr,"Cursor ERROR\n");
        exit(0);
    }

err:    if (close_dbc && (ret = dbcp->close(dbcp)) != 0)
dbp->err(dbp, ret, "DBcursor->close");
if (close_db && (ret = dbp->close(dbp, 0)) != 0)
fprintf(stderr,
    "%s: DB->close: %s\n", database, db_strerror(ret));
fprintf(stderr,"OK\n");
return new_cnt;
}

CIN* Get_CIN(char* ri) {
    fprintf(stderr,"[Get CIN] %s...", ri);

    //store CIN
    CIN* new_cin = (CIN*)malloc(sizeof(CIN));

    char* database = "CIN.db";

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int close_db, close_dbc, ret;

    close_db = close_dbc = 0;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }
    close_db = 1;

    /* Turn on additional error output. */
    dbp->set_errfile(dbp, stderr);
    dbp->set_errpfx(dbp, database);

    /* Open the database. */
    if ((ret = dbp->open(dbp, NULL, database, NULL,
        DB_UNKNOWN, DB_RDONLY, 0)) != 0) {
        dbp->err(dbp, ret, "%s: DB->open", database);
        goto err;
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        goto err;
    }
    close_dbc = 1;

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int idx = 0;
    // žî¹øÂ° CINÀÎÁö Ã£±â À§ÇÑ Ä¿Œ­
    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        goto err;
    }
    close_dbc = 1;
    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "ri", key.size) == 0) {
            idx++;
            if (strncmp(data.data, ri, data.size) == 0) {
                new_cin->ri = malloc(data.size);
                strcpy(new_cin->ri, data.data);
                break;
            }
        }
    }

    int cin_rn = 0;
    int cin_pi = 0;
    int cin_ty = 0;
    int cin_et = 0;
    int cin_lt = 0;
    int cin_ct = 0;
    int cin_st = 0;
    int cin_cs = 0;
    int cin_con = 0;

    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "rn", key.size) == 0) {
            cin_rn++;
            if (cin_rn == idx) {
                new_cin->rn = malloc(data.size);
                strcpy(new_cin->rn, data.data);
            }
        }
        if (strncmp(key.data, "pi", key.size) == 0) {
            cin_pi++;
            if (cin_pi == idx) {
                new_cin->pi = malloc(data.size);
                strcpy(new_cin->pi, data.data);
            }
        }
        if (strncmp(key.data, "et", key.size) == 0) {
            cin_et++;
            if (cin_et == idx) {
                new_cin->et = malloc(data.size);
                strcpy(new_cin->et, data.data);
            }
        }
        if (strncmp(key.data, "lt", key.size) == 0) {
            cin_lt++;
            if (cin_lt == idx) {
                new_cin->lt = malloc(data.size);
                strcpy(new_cin->lt, data.data);
            }
        }
        if (strncmp(key.data, "ct", key.size) == 0) {
            cin_ct++;
            if (cin_ct == idx) {
                new_cin->ct = malloc(data.size);
                strcpy(new_cin->ct, data.data);
            }
        }
        if (strncmp(key.data, "con", key.size) == 0) {
            cin_con++;
            if (cin_con == idx) {
                new_cin->con = malloc(data.size);
                strcpy(new_cin->con, data.data);
            }
        }

        if (strncmp(key.data, "ty", key.size) == 0) {
            cin_ty++;
            if (cin_ty == idx) {
                new_cin->ty = *(int*)data.data;
            }
        }
        if (strncmp(key.data, "st", key.size) == 0) {
            cin_st++;
            if (cin_st == idx) {
                new_cin->st = *(int*)data.data;
            }
        }
        if (strncmp(key.data, "cs", key.size) == 0) {
            cin_cs++;
            if (cin_cs == idx) {
                new_cin->cs = *(int*)data.data;
            }
        }
    }
    //fprintf(stderr,"[%d]\n",idx);

    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        fprintf(stderr,"Cursor ERROR\n");
        exit(0);
    }

err:    if (close_dbc && (ret = dbcp->close(dbcp)) != 0)
dbp->err(dbp, ret, "DBcursor->close");
if (close_db && (ret = dbp->close(dbp, 0)) != 0)
fprintf(stderr,
    "%s: DB->close: %s\n", database, db_strerror(ret));
fprintf(stderr,"OK\n");
return new_cin;
}

Sub* Get_Sub(char* ri) {
    fprintf(stderr,"[Get Sub] %s...", ri);
    char* database = "SUB.db";

    //store AE
    Sub* new_sub = (Sub*)malloc(sizeof(Sub));

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int ret;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }

    ret = dbp->open(dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", database);
        exit(1);
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int cnt = 0;
    int flag = 0;
    int struct_size = 10;

    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }
    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        cnt++;
        if (strncmp(data.data, ri, data.size) == 0) {
            flag=1;
            break;
        }
    }
    if (cnt == 0 || flag==0) {
        fprintf(stderr, "Data not exist\n");
        return NULL;
        exit(1);
    }


    new_sub->pi = malloc(data.size);
    strcpy(new_sub->pi, data.data);

    int idx = -1;
    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(data.data, ri, data.size) == 0) {
            idx=0;
        }
        switch (idx) {
            case 0:
                new_sub->ri = malloc(data.size);
                strcpy(new_sub->ri, data.data);

                idx++;
                break;
            case 1:
                new_sub->rn = malloc(data.size);
                strcpy(new_sub->rn, data.data);

                idx++;
                break;
            case 2:
                new_sub->nu = malloc(data.size);
                strcpy(new_sub->nu, data.data);

                idx++;
                break;
            case 3:
                new_sub->net = malloc(data.size);
                strcpy(new_sub->net, data.data);

                idx++;
                break;
            case 4:
                new_sub->sur = malloc(data.size);
                strcpy(new_sub->sur, data.data);

                idx++;
                break;
            case 5:
                new_sub->ct = malloc(data.size);
                strcpy(new_sub->ct, data.data);

                idx++;
                break;
            case 6:
                new_sub->et = malloc(data.size);
                strcpy(new_sub->et, data.data);

                idx++;
                break;
            case 7:
                new_sub->lt = malloc(data.size);
                strcpy(new_sub->lt, data.data);

                idx++;
                break;
            case 8:
                new_sub->ty = *(int*)data.data;

                idx++;
                break;
            case 9:
                new_sub->nct = *(int*)data.data;

                idx++;
                break;
            default:
                idx=-1;
        }

    }
    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        fprintf(stderr, "Cursor ERROR\n");
        exit(0);
    }

        /* Cursors must be closed */
    if (dbcp0 != NULL)
        dbcp0->close(dbcp0);
    if (dbcp != NULL)
        dbcp->close(dbcp);
    if (dbp != NULL)
        dbp->close(dbp, 0);
    fprintf(stderr,"OK\n");
    return new_sub;
}

int Update_AE_DB(AE* ae_object) {
    fprintf(stderr,"[Update AE] %s...", ae_object->ri);
    /* ri NULL ERROR*/
    if(ae_object->ri==NULL){
        fprintf(stderr,"ri NULL ERROR\n");
        return 0;
    }

    /* Not NULL:0, NULL:1 */
    int rn_f=0, pi_f=0, ct_f=0, lt_f=0, et_f=0, api_f=0, aei_f=0, ty_f=0, rr_f=0;

    if(ae_object->rn==NULL) rn_f=1;
    if(ae_object->pi==NULL) pi_f=1;    
    if(ae_object->ct==NULL) ct_f=1;
    if(ae_object->lt==NULL) lt_f=1;
    if(ae_object->et==NULL) et_f=1;
    if(ae_object->api==NULL) api_f=1;
    if(ae_object->aei==NULL) aei_f=1;
    if(ae_object->ty==0) ty_f=1;  
    if(ae_object->rr==false) rr_f=1;      

    char* database = "AE.db";

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int ret;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }

    ret = dbp->open(dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", database);
        return 0;
        exit(1);
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        return 0;
        exit(1);
    }

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int cnt = 0;
    int idx = 0;

    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        return 0;
        exit(1);
    }
    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "ri", key.size) == 0) {
            idx++;
            if (strncmp(data.data, ae_object->ri, data.size) == 0) {
                cnt++; 
                break;
            }
        }
    }

    if (cnt == 0) {
        fprintf(stderr, "Data not exist\n");
        return 0;
        exit(1);
    }

    int cnt_rn = 0,cnt_pi = 0,cnt_et = 0,cnt_lt = 0,cnt_ct = 0,cnt_api = 0,cnt_aei = 0;
    int cnt_rr = 0,cnt_ty = 0;
    
    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "rn", key.size) == 0 && rn_f==0) {
            cnt_rn++;
            if (cnt_rn == idx) {
                data.size = strlen(ae_object->rn) + 1;
                strcpy(data.data, ae_object->rn);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
        if (strncmp(key.data, "pi", key.size) == 0 && pi_f==0) {
            cnt_pi++;
            if (cnt_pi == idx) {
                data.size = strlen(ae_object->pi) + 1;
                strcpy(data.data, ae_object->pi);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
        if (strncmp(key.data, "api", key.size) == 0 && api_f==0) {
            cnt_api++;
            if (cnt_api == idx) {
                data.size = strlen(ae_object->api) + 1;
                strcpy(data.data, ae_object->api);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
        if (strncmp(key.data, "aei", key.size) == 0 && aei_f==0) {
            cnt_aei++;
            if (cnt_aei == idx) {
                data.size = strlen(ae_object->aei) + 1;
                strcpy(data.data, ae_object->aei);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
        if (strncmp(key.data, "et", key.size) == 0 && et_f==0) {
            cnt_et++;
            if (cnt_et == idx) {
                data.size = strlen(ae_object->et) + 1;
                strcpy(data.data, ae_object->et);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
        if (strncmp(key.data, "lt", key.size) == 0 && lt_f==0) {
            cnt_lt++;
            if (cnt_lt == idx) {
                data.size = strlen(ae_object->lt) + 1;
                strcpy(data.data, ae_object->lt);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
        if (strncmp(key.data, "ct", key.size) == 0 && ct_f==0) {
            cnt_ct++;
            if (cnt_ct == idx) {
                data.size = strlen(ae_object->ct) + 1;
                strcpy(data.data, ae_object->ct);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
        if (strncmp(key.data, "ty", key.size) == 0 && ty_f==0) {
            cnt_ty++;
            if (cnt_ty == idx) {
                *(int*)data.data = ae_object->ty;
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
        if (strncmp(key.data, "rr", key.size) == 0 && rr_f==0) {
            cnt_rr++;
            if (cnt_rr == idx) {
                *(bool*)data.data = ae_object->rr;
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
    }

    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        printf("Cursor ERROR\n");
        return 0;
    }

    /* DB close */
    dbcp->close(dbcp);
    dbcp->close(dbcp0);
    dbp->close(dbp, 0); 
    fprintf(stderr,"OK\n");
    return 1;
}

int Update_CNT_DB(CNT* cnt_object) {
    fprintf(stderr,"[Update CNT] %s...", cnt_object->ri);
    /* ri NULL ERROR*/
    if(cnt_object->ri==NULL){
        fprintf(stderr,"ri NULL ERROR\n");
        return 0;
    }

    /* Not NULL:0, NULL:1 */
    int rn_f=0, pi_f=0, ct_f=0, lt_f=0, et_f=0, st_f=0, cni_f=0, ty_f=0, cbs_f=0;

    if(cnt_object->rn==NULL) rn_f=1;
    if(cnt_object->pi==NULL) pi_f=1;    
    if(cnt_object->ct==NULL) ct_f=1;
    if(cnt_object->lt==NULL) lt_f=1;
    if(cnt_object->et==NULL) et_f=1;
    if(cnt_object->st==0) st_f=1;
    if(cnt_object->ty==0) ty_f=1; 
    if(cnt_object->cni==0) cni_f=1;
    if(cnt_object->cbs==0) cbs_f=1;   

    char* database = "CNT.db";

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int ret;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }

    ret = dbp->open(dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", database);
        exit(1);
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        return 0;
        exit(1);
    }

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int cnt = 0;
    int idx = 0;

    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        return 0;
        exit(1);
    }
    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "ri", key.size) == 0) {
            idx++;
            if (strncmp(data.data, cnt_object->ri, data.size) == 0) {
                cnt++; 
                break;
            }
        }
    }

    if (cnt == 0) {
        fprintf(stderr, "Data not exist\n");
        return 0;
        exit(1);
    }

    int cnt_rn = 0;
    int cnt_pi = 0;
    int cnt_ty = 0;
    int cnt_et = 0;
    int cnt_lt = 0;
    int cnt_ct = 0;
    int cnt_cni = 0;
    int cnt_cbs = 0;
    int cnt_st = 0;

    
    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0 && rn_f==0) {
        if (strncmp(key.data, "rn", key.size) == 0) {
            cnt_rn++;
            if (cnt_rn == idx) {
                data.size = strlen(cnt_object->rn) + 1;
                strcpy(data.data, cnt_object->rn);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }

        if (strncmp(key.data, "pi", key.size) == 0 && pi_f==0) {
            cnt_pi++;
            if (cnt_pi == idx) {
                data.size = strlen(cnt_object->pi) + 1;
                strcpy(data.data, cnt_object->pi);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
        if (strncmp(key.data, "et", key.size) == 0 && et_f==0) {
            cnt_et++;
            if (cnt_et == idx) {
                data.size = strlen(cnt_object->et) + 1;
                strcpy(data.data, cnt_object->et);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
        if (strncmp(key.data, "lt", key.size) == 0 && lt_f==0) {
            cnt_lt++;
            if (cnt_lt == idx) {
                data.size = strlen(cnt_object->lt) + 1;
                strcpy(data.data, cnt_object->lt);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
        if (strncmp(key.data, "ct", key.size) == 0 && ct_f==0) {
            cnt_ct++;
            if (cnt_ct == idx) {
                data.size = strlen(cnt_object->ct) + 1;
                strcpy(data.data, cnt_object->ct);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
        if (strncmp(key.data, "ty", key.size) == 0 && ty_f==0) {
            cnt_ty++;
            if (cnt_ty == idx) {
                *(int*)data.data = cnt_object->ty;
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
        if (strncmp(key.data, "cni", key.size) == 0 && cni_f==0) {
            cnt_cni++;
            if (cnt_cni == idx) {
                *(int*)data.data = cnt_object->cni;
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
        if (strncmp(key.data, "cbs", key.size) == 0 && cbs_f==0) {
            cnt_cbs++;
            if (cnt_cbs == idx) {
                *(int*)data.data = cnt_object->cbs;
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
        if (strncmp(key.data, "st", key.size) == 0 && st_f==0) {
            cnt_st++;
            if (cnt_st == idx) {
                *(int*)data.data = cnt_object->st;
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
        }
    }

    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        printf("Cursor ERROR\n");
        return 0;
        exit(0);
    }

    /* DB close */
    dbcp->close(dbcp);
    dbcp->close(dbcp0);
    dbp->close(dbp, 0); 
    fprintf(stderr,"OK\n");
    return 1;
}

int Update_Sub_DB(Sub *sub_object) {

    /* ri NULL ERROR*/
    if(sub_object->ri==NULL){
        fprintf(stderr,"ri NULL ERROR\n");
        return 0;
    }

    /* Not NULL:0, NULL:1 */
    int rn_f=0, pi_f=0, ct_f=0, lt_f=0, et_f=0, net_f=0,sur_f=0,nct_f=0,nu_f=0, ty_f=0;

    if(sub_object->rn==NULL) rn_f=1;
    if(sub_object->pi==NULL) pi_f=1;    
    if(sub_object->ct==NULL) ct_f=1;
    if(sub_object->lt==NULL) lt_f=1;
    if(sub_object->et==NULL) et_f=1;
    if(sub_object->nu==NULL) nu_f=1;
    if(sub_object->net==NULL) net_f=1;
    if(sub_object->sur==NULL) sur_f=1;
    if(sub_object->ty==0) ty_f=1; 
    if(sub_object->nct==0) nct_f=1;


    char* database = "SUB.db";

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int ret;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }

    ret = dbp->open(dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", database);
        return 0;
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        return 0;
    }

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int cnt = 0;
    int flag = 0;
    int struct_size = 10;

    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        return 0;
        exit(1);
    }
    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        cnt++;
        if (strncmp(data.data, sub_object->ri, data.size) == 0) {
            flag=1;
            break;
        }
    }
    if (cnt == 0 || flag==0) {
        fprintf(stderr, "Data not exist\n");
        return 0;
    }

    int idx = -1;
    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(data.data, sub_object->ri, data.size) == 0) {
            idx=0;
        }
        if(idx!=-1 && idx < struct_size){
            // idx==0 -> ri
            if(idx==1 && rn_f==0) {
                data.size = strlen(sub_object->rn) + 1;
                strcpy(data.data, sub_object->rn);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
            if(idx==2 && nu_f==0) {
                data.size = strlen(sub_object->nu) + 1;
                strcpy(data.data, sub_object->nu);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
            if(idx==3 && net_f==0) {
                data.size = strlen(sub_object->net) + 1;
                strcpy(data.data, sub_object->net);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }            
            if(idx==4 && sur_f==0) {
                data.size = strlen(sub_object->sur) + 1;
                strcpy(data.data, sub_object->sur);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
            if(idx==5 && ct_f==0) {
                data.size = strlen(sub_object->ct) + 1;
                strcpy(data.data, sub_object->ct);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
            if(idx==6 && lt_f==0) {
                data.size = strlen(sub_object->lt) + 1;
                strcpy(data.data, sub_object->lt);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
            if(idx==7 && et_f==0) {
                data.size = strlen(sub_object->et) + 1;
                strcpy(data.data, sub_object->et);
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
            if(idx==8 && ty_f==0) {
                *(int*)data.data=sub_object->ty;
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
            if(idx==9 && nct_f==0) {
                *(int*)data.data=sub_object->nct;
                dbcp->put(dbcp, &key, &data, DB_CURRENT);
            }
            idx++;
        }
    }
    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        fprintf(stderr, "Cursor ERROR\n");
        return 0;
    }

    /* Cursors must be closed */
    if (dbcp0 != NULL)
        dbcp0->close(dbcp0);
    if (dbcp != NULL)
        dbcp->close(dbcp);
    if (dbp != NULL)
        dbp->close(dbp, 0);
        
    /* Delete Success */
    return 1;
}

int Delete_AE(char* ri) {
    fprintf(stderr,"[Delete AE] %s...", ri);
    char* database = "AE.db";

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int ret;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }

    /* Open the database. */
    ret = dbp->open(dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", database);
        return 0;
    }


    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int idx = 0;
    int flag = 0;
    // ���° AE���� ã�� ���� Ŀ��
    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        return 0;
    }

    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "ri", key.size) == 0) {
            idx++;
            if (strncmp(data.data, ri, data.size) == 0) {
                flag = 1;
                dbcp0->del(dbcp0, 0);
                break;
            }
        }
    }
    if (flag == 0) {
        printf("Not Found\n");
        return 0;
    }

    // �ش� index�� ������ �� ã�� ����
    int cnt_rn = 0;
    int cnt_pi = 0;
    int cnt_ty = 0;
    int cnt_et = 0;
    int cnt_lt = 0;
    int cnt_ct = 0;
    int cnt_api = 0;
    int cnt_aei = 0;
    int cnt_rr = 0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        return 0;
    }
    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "rn", key.size) == 0) {
            cnt_rn++;
            if (cnt_rn == idx) 
                dbcp->del(dbcp, 0);
        }
        if (strncmp(key.data, "pi", key.size) == 0) {
            cnt_pi++;
            if (cnt_pi == idx) 
                dbcp->del(dbcp, 0);
        }
        if (strncmp(key.data, "api", key.size) == 0) {
            cnt_api++;
            if (cnt_api == idx) 
                dbcp->del(dbcp, 0);
        }
        if (strncmp(key.data, "aei", key.size) == 0) {
            cnt_aei++;
            if (cnt_aei == idx)
                dbcp->del(dbcp, 0);
        }
        if (strncmp(key.data, "et", key.size) == 0) {
            cnt_et++;
            if (cnt_et == idx) 
                dbcp->del(dbcp, 0);
        }
        if (strncmp(key.data, "lt", key.size) == 0) {
            cnt_lt++;
            if (cnt_lt == idx) 
                dbcp->del(dbcp, 0);
        }
        if (strncmp(key.data, "ct", key.size) == 0) {
            cnt_ct++;
            if (cnt_ct == idx) 
                dbcp->del(dbcp, 0);
        }
        if (strncmp(key.data, "ty", key.size) == 0) {
            cnt_ty++;
            if (cnt_ty == idx) 
                dbcp->del(dbcp, 0);
        }
        if (strncmp(key.data, "rr", key.size) == 0) {
            cnt_rr++;
            if (cnt_rr == idx) 
                dbcp->del(dbcp, 0);
        }
    }


    /* Cursors must be closed */
    if (dbcp0 != NULL)
        dbcp0->close(dbcp0);
    if (dbcp != NULL)
        dbcp->close(dbcp);
    if (dbp != NULL)
        dbp->close(dbp, 0);
    fprintf(stderr,"OK\n");
    /* Delete Success */
    return 1;
}

int Delete_CNT(char* ri) {
    fprintf(stderr,"[Delete CNT] %s...", ri);
    char* database = "CNT.db";

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int ret;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }

    /* Open the database. */
    ret = dbp->open(dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", database);
        return 0;
    }


    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int idx = 0;
    int flag = 0;
    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        return 0;
    }

    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "ri", key.size) == 0) {
            idx++;
            if (strncmp(data.data, ri, data.size) == 0) {
                flag = 1;
                dbcp0->del(dbcp0, 0);
                break;
            }
        }
    }
    if (flag == 0) {
        fprintf(stderr,("Not Found\n"));
        return 0;
    }

    // �ش� index�� ������ �� ã�� ����
    int cnt_rn = 0;
    int cnt_pi = 0;
    int cnt_ty = 0;
    int cnt_et = 0;
    int cnt_lt = 0;
    int cnt_ct = 0;
    int cnt_st = 0;
    int cnt_cni = 0;
    int cnt_cbs = 0;

    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        return 0;
        exit(1);
    }
    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "rn", key.size) == 0) {
            cnt_rn++;
            if (cnt_rn == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "pi", key.size) == 0) {
            cnt_pi++;
            if (cnt_pi == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "et", key.size) == 0) {
            cnt_et++;
            if (cnt_et == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "lt", key.size) == 0) {
            cnt_lt++;
            if (cnt_lt == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "ct", key.size) == 0) {
            cnt_ct++;
            if (cnt_ct == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "ty", key.size) == 0) {
            cnt_ty++;
            if (cnt_ty == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "st", key.size) == 0) {
            cnt_st++;
            if (cnt_st == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "cbs", key.size) == 0) {
            cnt_cbs++;
            if (cnt_cbs == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "cni", key.size) == 0) {
            cnt_cni++;
            if (cnt_cni == idx) {
                dbcp->del(dbcp, 0);
            }
        }

    }

    /* Cursors must be closed */
    if (dbcp0 != NULL)
        dbcp0->close(dbcp0);
    if (dbcp != NULL)
        dbcp->close(dbcp);
    if (dbp != NULL)
        dbp->close(dbp, 0);
    fprintf(stderr,"OK\n");
    /* Delete Success */
    return 1;
}

int Delete_CIN(char* ri) {
    char* database = "CIN.db";

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int ret;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }

    /* Open the database. */
    ret = dbp->open(dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", database);
        return 0;
    }


    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int idx = 0;
    int flag = 0;

    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        return 0;
    }

    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "ri", key.size) == 0) {
            idx++;
            if (strncmp(data.data, ri, data.size) == 0) {
                flag = 1;
                dbcp0->del(dbcp0, 0);
                break;
            }
        }
    }
    if (flag == 0) {
        printf("Not Found\n");
        return 0;
    }

    int cin_rn = 0;
    int cin_pi = 0;
    int cin_ty = 0;
    int cin_et = 0;
    int cin_lt = 0;
    int cin_ct = 0;
    int cin_st = 0;
    int cin_cs = 0;
    int cin_con = 0;
    int cin_csi = 0;

    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        return 0;
        exit(1);
    }
    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "rn", key.size) == 0) {
            cin_rn++;
            if (cin_rn == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "pi", key.size) == 0) {
            cin_pi++;
            if (cin_pi == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "et", key.size) == 0) {
            cin_et++;
            if (cin_et == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "lt", key.size) == 0) {
            cin_lt++;
            if (cin_lt == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "ct", key.size) == 0) {
            cin_ct++;
            if (cin_ct == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "con", key.size) == 0) {
            cin_con++;
            if (cin_con == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "csi", key.size) == 0) {
            cin_csi++;
            if (cin_csi == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "ty", key.size) == 0) {
            cin_ty++;
            if (cin_ty == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "st", key.size) == 0) {
            cin_st++;
            if (cin_st == idx) {
                dbcp->del(dbcp, 0);
            }
        }
        if (strncmp(key.data, "cs", key.size) == 0) {
            cin_cs++;
            if (cin_cs == idx) {
                dbcp->del(dbcp, 0);
            }
        }
    }

    /* Cursors must be closed */
    if (dbcp0 != NULL)
        dbcp0->close(dbcp0);
    if (dbcp != NULL)
        dbcp->close(dbcp);
    if (dbp != NULL)
        dbp->close(dbp, 0);
    
    return 1;
}

int Delete_Sub(char* ri) {
    fprintf(stderr,"[Delete Sub] %s...", ri);
    char* database = "SUB.db";

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int ret;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }

    ret = dbp->open(dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", database);
        return 0;
        exit(1);
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        return 0;
        exit(1);
    }

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int cnt = 0;
    int flag = 0;
    int struct_size = 10;

    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        return 0;
    }
    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        cnt++;
        if (strncmp(data.data, ri, data.size) == 0) {
            flag=1;
            break;
        }
    }
    if (cnt == 0 || flag==0) {
        fprintf(stderr, "Data not exist\n");
        return 0;
    }

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
        dbp->err(dbp, ret, "DBcursor->get");
        fprintf(stderr, "Cursor ERROR\n");
        return 0;
    }

    /* Cursors must be closed */
    if (dbcp0 != NULL)
        dbcp0->close(dbcp0);
    if (dbcp != NULL)
        dbcp->close(dbcp);
    if (dbp != NULL)
        dbp->close(dbp, 0);
    
    fprintf(stderr,"OK\n");
    /* Delete Success */
    return 1;
}

Node* Get_All_AE() {
    fprintf(stderr, "[Get All AE]...");

    char* database = "AE.db";

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int ret;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }

    ret = dbp->open(dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", database);
        exit(1);
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int cnt = 0;
    // žî¹øÂ° AEÀÎÁö Ã£±â À§ÇÑ Ä¿Œ­
    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }
    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "ri", key.size) == 0) {
            cnt++; // ¿ÀºêÁ§Æ® °³Œö
        }
    }
    //fprintf(stderr, "<%d>\n",cnt);

    if (cnt == 0) {
        fprintf(stderr, "Data not exist\n");
        return NULL;
        exit(1);
    }

    // cnt °³ŒöžžÅ­ µ¿ÀûÇÒŽç
    Node* head = (Node *)calloc(1,sizeof(Node));
    Node* node_ri;
    Node* node_pi;
    Node* node_rn;
    Node* node_ty;

    node_ri = node_pi = node_rn = node_ty = head;

    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "pi", key.size) == 0) {
            node_pi->pi = malloc(data.size);
            strcpy(node_pi->pi, data.data);
            node_pi->siblingRight = (Node *)calloc(1,sizeof(Node));
            node_pi->siblingRight->siblingLeft = node_pi;
            node_pi = node_pi->siblingRight;
        }
        if (strncmp(key.data, "ri", key.size) == 0) {
            node_ri->ri = malloc(data.size);
            strcpy(node_ri->ri, data.data);
            node_ri = node_ri->siblingRight;

        }
        if (strncmp(key.data, "rn", key.size) == 0) {
            node_rn->rn = malloc(data.size);
            strcpy(node_rn->rn, data.data);
            node_rn = node_rn->siblingRight;
        }
        if (strncmp(key.data, "ty", key.size) == 0) {
            node_ty->ty = *(int*)data.data;
            node_ty = node_ty->siblingRight;
        }
    }

    if(node_pi->siblingLeft) node_pi->siblingLeft->siblingRight = NULL;
    else head = NULL;
    Free_Node(node_pi);
    node_ri = node_pi = node_rn = node_ty = NULL;

    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        fprintf(stderr, "Cursor ERROR\n");
        exit(0);
    }
    
    fprintf(stderr,"OK\n");

    return head;
}

Node* Get_All_CNT() {
    fprintf(stderr, "[Get All CNT]...");

    char* database = "CNT.db";

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int ret;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }

    ret = dbp->open(dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", database);
        exit(1);
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int cnt = 0;
    // žî¹øÂ° CNTÀÎÁö Ã£±â À§ÇÑ Ä¿Œ­
    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }
    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "ri", key.size) == 0) {
            cnt++; // ¿ÀºêÁ§Æ® °³Œö
        }
    }
    //fprintf(stderr, "<%d>\n",cnt);

    if (cnt == 0) {
        fprintf(stderr, "Data not exist\n");
        return NULL;
        exit(1);
    }

    // cnt °³ŒöžžÅ­ µ¿ÀûÇÒŽç
    Node* head = (Node *)calloc(1,sizeof(Node));
    Node* node_ri;
    Node* node_pi;
    Node* node_rn;
    Node* node_ty;

    node_ri = node_pi = node_rn = node_ty = head;

    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "pi", key.size) == 0) {
            node_pi->pi = malloc(data.size);
            strcpy(node_pi->pi, data.data);
            node_pi->siblingRight = (Node *)calloc(1,sizeof(Node));
            node_pi->siblingRight->siblingLeft = node_pi;
            node_pi = node_pi->siblingRight;
        }
        if (strncmp(key.data, "ri", key.size) == 0) {
            node_ri->ri = malloc(data.size);
            strcpy(node_ri->ri, data.data);
            node_ri = node_ri->siblingRight;

        }
        if (strncmp(key.data, "rn", key.size) == 0) {
            node_rn->rn = malloc(data.size);
            strcpy(node_rn->rn, data.data);
            node_rn = node_rn->siblingRight;
        }
        if (strncmp(key.data, "ty", key.size) == 0) {
            node_ty->ty = *(int*)data.data;
            node_ty = node_ty->siblingRight;
        }
    }

    if(node_pi->siblingLeft) node_pi->siblingLeft->siblingRight = NULL;
    else head = NULL;
    Free_Node(node_pi);
    node_ri = node_pi = node_rn = node_ty = NULL;

    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        fprintf(stderr, "Cursor ERROR\n");
        exit(0);
    }
    
    fprintf(stderr,"OK\n");

    return head;
}

Node* Get_All_CIN() {
    fprintf(stderr, "[Get All CIN]...");

    char* database = "CIN.db";

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int ret;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }

    ret = dbp->open(dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", database);
        exit(1);
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int cnt = 0;
    // žî¹øÂ° CINÀÎÁö Ã£±â À§ÇÑ Ä¿Œ­
    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }
    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "ri", key.size) == 0) {
            cnt++; // ¿ÀºêÁ§Æ® °³Œö
        }
    }
    //fprintf(stderr, "<%d>\n",cnt);

    if (cnt == 0) {
        fprintf(stderr, "Data not exist\n");
        return NULL;
        exit(1);
    }

    // cnt °³ŒöžžÅ­ µ¿ÀûÇÒŽç
    Node* head = (Node *)calloc(1,sizeof(Node));
    Node* node_ri;
    Node* node_pi;
    Node* node_rn;
    Node* node_ty;

    node_ri = node_pi = node_rn = node_ty = head;

    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "pi", key.size) == 0) {
            node_pi->pi = malloc(data.size);
            strcpy(node_pi->pi, data.data);
            node_pi->siblingRight = (Node *)calloc(1,sizeof(Node));
            node_pi->siblingRight->siblingLeft = node_pi;
            node_pi = node_pi->siblingRight;
        }
        if (strncmp(key.data, "ri", key.size) == 0) {
            node_ri->ri = malloc(data.size);
            strcpy(node_ri->ri, data.data);
            node_ri = node_ri->siblingRight;

        }
        if (strncmp(key.data, "rn", key.size) == 0) {
            node_rn->rn = malloc(data.size);
            strcpy(node_rn->rn, data.data);
            node_rn = node_rn->siblingRight;
        }
        if (strncmp(key.data, "ty", key.size) == 0) {
            node_ty->ty = *(int*)data.data;
            node_ty = node_ty->siblingRight;
        }
    }

    if(node_pi->siblingLeft) node_pi->siblingLeft->siblingRight = NULL;
    else head = NULL;
    Free_Node(node_pi);
    node_ri = node_pi = node_rn = node_ty = NULL;

    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        fprintf(stderr, "Cursor ERROR\n");
        exit(0);
    }
	
    fprintf(stderr,"OK\n");	
	
    return head;
}

Node* Get_CIN_Period(char* start_time, char* end_time) {
    //start_time°ú end_timeÀº [20220807T215215] ÇüÅÂ
    fprintf(stderr, "[Get_CIN_Period] <%s ~ %s>...",start_time, end_time);

    // start_ri¿Í end_riŽÂ [4-20220808T113154] ÇüÅÂ
    char* start_ri = (char*)calloc(18, sizeof(char));
    char* end_ri = (char*)calloc(18, sizeof(char));

    strcat(start_ri, "4-");
    strcat(start_ri, start_time);

    strcat(end_ri, "4-");
    strcat(end_ri, end_time);

    //fprintf(stderr, "<%s ~ %s>\n", start_ri, end_ri);

    // DB °ü·Ã ÇÔŒö
    char* database = "CIN.db";

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int ret;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }

    ret = dbp->open(dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", database);
        exit(1);
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int cnt = 0;
    int idx = 0;
    int* arr = NULL;

    // ¿ÀºêÁ§Æ®°¡ žî°³ÀÎÁö Ã£±â À§ÇÑ Ä¿Œ­
    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }
    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "ri", key.size) == 0) {
            cnt++; // ¿ÀºêÁ§Æ® °³Œö
        }
    }
    if (cnt == 0) {
        fprintf(stderr, "Data not exist\n");
        return NULL;
        exit(1);
    }
    //fprintf(stderr, "<%d>\n", cnt);

    //¿ÀºêÁ§Æ® °³ŒöžžÅ­ µ¿ÀûÇÒŽç
    arr = (int*)calloc(cnt, sizeof(int));
    //for (int i = 0; i < cnt; i++) arr[i] = 0;

    // ÇØŽçÇÏŽÂ ¿ÀºêÁ§Æ®°¡ žî°³ÀÎÁö Ã£±â À§ÇÑ Ä¿Œ­
    DBC* dbcp1;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp1, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }
    // ÇØŽçÇÏŽÂ ¿ÀºêÁ§Æ® ¹è¿­¿¡ 1·Î Ç¥œÃ 0 1 1 1 0 <- µÎ¹øÂ° ŒŒ¹øÂ° ³×¹øÂ° ¿ÀºêÁ§Æ®°¡ ÇØŽç
    while ((ret = dbcp1->get(dbcp1, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "ri", key.size) == 0) {
            //9 : day, 16: time
            if (strncmp(start_ri, data.data, 16) <= 0 && strncmp(end_ri, data.data, 16) >= 0)
                arr[idx] = 1;
            idx++;
        }
    }
    //ÇØŽçÇÏŽÂ ¿ÀºêÁ§Æ®°¡ ŸøÀœ
    if (idx == 0) {
        fprintf(stderr, "Data not exist\n");
        return NULL;
        exit(1);
    }
    //for (int i = 0; i < cnt; i++) printf("%d ", arr[i]);


    Node* head = (Node *)calloc(1,sizeof(Node));
    Node* node_ri;
    Node* node_pi;
    Node* node_rn;
    Node* node_ty;

    node_ri = node_pi = node_rn = node_ty = head;


    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "pi", key.size) == 0) {
            if (arr[idx % cnt]) {
                //printf("[%d]", idx % cnt);
                node_pi->pi = malloc(data.size);
                strcpy(node_pi->pi, data.data);
                node_pi->siblingRight = (Node *)calloc(1,sizeof(Node));
                node_pi->siblingRight->siblingLeft = node_pi;
                node_pi = node_pi->siblingRight;
            }
            idx++;
        }
        if (strncmp(key.data, "ri", key.size) == 0) {
            if (arr[idx % cnt]) {
                node_ri->ri = malloc(data.size);
                strcpy(node_ri->ri, data.data);
                node_ri = node_ri->siblingRight;
            }
            idx++;
        }
        if (strncmp(key.data, "rn", key.size) == 0) {
            if (arr[idx % cnt]) {
                node_rn->rn = malloc(data.size);
                strcpy(node_rn->rn, data.data);
                node_rn = node_rn->siblingRight;
            }
            idx++;
        }
        if (strncmp(key.data, "ty", key.size) == 0) {
            if (arr[idx % cnt]) {
                node_ty->ty = *(int*)data.data;
                node_ty = node_ty->siblingRight;
            }
            idx++;
        }
        
    }
    // for (int i = 0; i < cnt; i++) printf("%d ", arr[i]);

    if(node_pi->siblingLeft) node_pi->siblingLeft->siblingRight = NULL;
    else head = NULL;
    Free_Node(node_pi);
    node_ri = node_pi = node_rn = node_ty = NULL;

    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        fprintf(stderr, "Cursor ERROR\n");
        exit(0);
    }

fprintf(stderr,"OK\n");
    return head;
}

Node* Get_CIN_Pi(char* pi) {
    char* database = "CIN.db";

    DB* dbp;
    DBC* dbcp;
    DBT key, data;
    int ret;

    /* Open the database. */
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
        fprintf(stderr,
            "%s: db_create: %s\n", database, db_strerror(ret));
        return 0;
    }

    ret = dbp->open(dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", database);
        exit(1);
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int cnt = 0;
    int idx = 0;
    int* arr = NULL;
    int sum = 0;
    // ¿ÀºêÁ§Æ®°¡ žî°³ÀÎÁö Ã£±â À§ÇÑ Ä¿Œ­
    DBC* dbcp0;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }
    while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "ri", key.size) == 0) {
            cnt++; // ¿ÀºêÁ§Æ® °³Œö
        }
    }
    if (cnt == 0) {
        fprintf(stderr, "Data not exist\n");
        return NULL;
        exit(1);
    }

    //¿ÀºêÁ§Æ® °³ŒöžžÅ­ µ¿ÀûÇÒŽç
    arr = (int*)calloc(cnt, sizeof(int));
    //for (int i = 0; i < cnt; i++) arr[i] = 0;

    // ÇØŽçÇÏŽÂ ¿ÀºêÁ§Æ®°¡ žî°³ÀÎÁö Ã£±â À§ÇÑ Ä¿Œ­
    DBC* dbcp1;
    if ((ret = dbp->cursor(dbp, NULL, &dbcp1, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        exit(1);
    }

    // ÇØŽçÇÏŽÂ ¿ÀºêÁ§Æ® ¹è¿­¿¡ 1·Î Ç¥œÃ 0 1 1 1 0 <- µÎ¹øÂ° ŒŒ¹øÂ° ³×¹øÂ° ¿ÀºêÁ§Æ®°¡ ÇØŽç
    while ((ret = dbcp1->get(dbcp1, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "pi", key.size) == 0) {
            if (strcmp(pi,data.data)== 0)
                arr[idx] = 1;
            idx++;
        }
    }

    //ÇØŽçÇÏŽÂ ¿ÀºêÁ§Æ®°¡ ŸøÀœ
    for (int i = 0; i < cnt; i++) {
        sum += arr[i];
    }
    if (sum == 0) {
        fprintf(stderr, "Data not exist\n");
        return NULL;
        exit(1);
    }

    Node* head = (Node *)calloc(1,sizeof(Node));
    Node* node_ri;
    Node* node_pi;
    Node* node_rn;
    Node* node_ty;

    node_ri = node_pi = node_rn = node_ty = head;

    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, "pi", key.size) == 0) {
            if (arr[idx % cnt]) {
                //printf("[%d]", idx % cnt);
                node_pi->pi = malloc(data.size);
                strcpy(node_pi->pi, data.data);
                node_pi->siblingRight = (Node *)calloc(1,sizeof(Node));
                node_pi->siblingRight->siblingLeft = node_pi;
                node_pi = node_pi->siblingRight;
            }
            idx++;
        }
        if (strncmp(key.data, "ri", key.size) == 0) {
            if (arr[idx % cnt]) {
                node_ri->ri = malloc(data.size);
                strcpy(node_ri->ri, data.data);
                node_ri = node_ri->siblingRight;
            }
            idx++;
        }
        if (strncmp(key.data, "rn", key.size) == 0) {
            if (arr[idx % cnt]) {
                node_rn->rn = malloc(data.size);
                strcpy(node_rn->rn, data.data);
                node_rn = node_rn->siblingRight;
            }
            idx++;
        }
        if (strncmp(key.data, "ty", key.size) == 0) {
            if (arr[idx % cnt]) {
                node_ty->ty = *(int*)data.data;
                node_ty = node_ty->siblingRight;
            }
            idx++;
        }
    }

    if(node_pi->siblingLeft) node_pi->siblingLeft->siblingRight = NULL;
    else head = NULL;
    Free_Node(node_pi);
    node_ri = node_pi = node_rn = node_ty = NULL;

    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        fprintf(stderr, "Cursor ERROR\n");
        exit(0);
    }

    return head;
}

int Store_Label(char* label, char* uri) {
    char* DATABASE = "LABEL.db";

    DB* dbp;    // db handle
    DBC* dbcp;
    FILE* error_file_pointer;
    DBT key, data;  // storving key and real data
    int ret;        // template value

    DBT key_label;
    DBT data_uri;  // storving key and real data

    char* program_name = "my_prog";

    // if input is NULL => return -1
    if (label == NULL) {
        fprintf(stderr, "Label is empty\n");
        return -1;
    }
    if (uri == NULL) {
        fprintf(stderr, "URI is empty\n");
        return -1;
    }


    ret = db_create(&dbp, NULL, 0);
    if (ret) {
        fprintf(stderr, "db_create : %s\n", db_strerror(ret));
        printf("File ERROR\n");
        exit(1);
    }

    dbp->set_errfile(dbp, error_file_pointer);
    dbp->set_errpfx(dbp, program_name);

    /*DB Open*/
    ret = dbp->open(dbp, NULL, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", DATABASE);
        printf("DB Open ERROR\n");
        exit(1);
    }

    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        printf("Cursor ERROR");
        exit(1);
    }

    /* keyand data must initialize */
    memset(&key_label, 0, sizeof(DBT));
    memset(&data_uri, 0, sizeof(DBT));

    /* initialize the data to be the first of two duplicate records. */
    key_label.data = label;
    key_label.size = strlen(label) + 1;
    data_uri.data = uri;
    data_uri.size = strlen(uri) + 1;

    /* input DB */
    if ((ret = dbcp->put(dbcp, &key_label, &data_uri, DB_KEYLAST)) != 0)
        dbp->err(dbp, ret, "db->cursor");

    dbcp->close(dbcp);
    dbp->close(dbp, 0); //DB close

    return 1;
}

/*
 Function name: Label_To_URI
 Input: string label
 Return: string uri
 Description:
 It receives the Label value as an argument from the <key: Label, value:URI> type DB 
 and returns the URI.
*/
char* Label_To_URI(char* label) {
    char* DATABASE = "LABEL.db";

    DB* dbp;    // db handle
    DBC* dbcp;
    FILE* error_file_pointer;
    DBT key, data;  // storving key and real data
    int ret;        // template value


    char* program_name = "my_prog";

    // if input is NULL => return -1
    if (label == NULL) {
        fprintf(stderr, "Label is empty\n");
        exit(-1);
    }

    ret = db_create(&dbp, NULL, 0);
    if (ret) {
        fprintf(stderr, "db_create : %s\n", db_strerror(ret));
        printf("File ERROR\n");
        exit(1);
    }

    dbp->set_errfile(dbp, error_file_pointer);
    dbp->set_errpfx(dbp, program_name);

    /*DB Open*/
    ret = dbp->open(dbp, NULL, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", DATABASE);
        printf("DB Open ERROR\n");
        exit(1);
    }

    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        printf("Cursor ERROR");
        exit(1);
    }

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    /* 0 if LABEL does not exist, 1 if present */
    int flag = 0;

    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(key.data, label, key.size) == 0) {
            flag=1;
            return (char*)data.data;

            //DB close
            dbcp->close(dbcp);
            dbp->close(dbp, 0); 
            break;
        }
    }
    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        printf("Cursor ERROR\n");
        exit(0);
    }
    if (flag == 0)
        return NULL;

    dbcp->close(dbcp);
    dbp->close(dbp, 0); //DB close
}

/*
 Function name: URI_To_Label
 Input: string URI
 Return: string label
 Description:
 It receives the URI value as an argument from the <key: Label, value:URI> type DB
 and returns the label.
*/
char* URI_To_Label(char* uri) {
    char* DATABASE = "LABEL.db";

    DB* dbp;    // db handle
    DBC* dbcp;
    FILE* error_file_pointer;
    DBT key, data;  // storving key and real data
    int ret;        // template value


    char* program_name = "my_prog";

    // if input is NULL => return -1
    if (uri == NULL) {
        fprintf(stderr, "URI is empty\n");
        exit(-1);
    }

    ret = db_create(&dbp, NULL, 0);
    if (ret) {
        fprintf(stderr, "db_create : %s\n", db_strerror(ret));
        printf("File ERROR\n");
        exit(1);
    }

    dbp->set_errfile(dbp, error_file_pointer);
    dbp->set_errpfx(dbp, program_name);

    /*DB Open*/
    ret = dbp->open(dbp, NULL, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664);
    if (ret) {
        dbp->err(dbp, ret, "%s", DATABASE);
        printf("DB Open ERROR\n");
        exit(1);
    }

    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        dbp->err(dbp, ret, "DB->cursor");
        printf("Cursor ERROR");
        exit(1);
    }

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    /* 0 if LABEL does not exist, 1 if present */
    int flag = 0;

    while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
        if (strncmp(data.data, uri, data.size) == 0) {
            flag = 1;
            return (char*)key.data;

            //DB close
            dbcp->close(dbcp);
            dbp->close(dbp, 0);
            break;
        }
    }
    if (ret != DB_NOTFOUND) {
        dbp->err(dbp, ret, "DBcursor->get");
        printf("Cursor ERROR\n");
        exit(0);
    }
    if (flag == 0)
        return NULL;

    dbcp->close(dbcp);
    dbp->close(dbp, 0); //DB close
}

