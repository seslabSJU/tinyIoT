#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <db.h>
#include "onem2m.h"
#include "dbmanager.h"
#include "logger.h"
#include "util.h"

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

    ret = dbp->open(dbp, NULL, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664);
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

/* DB init */
int init_dbp(){
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

    return 1;
}

int close_dbp(){
    resourceDBp->close(resourceDBp, 0);
    subDBp->close(subDBp, 0);
    grpDBp->close(grpDBp, 0);
    acpDBp->close(acpDBp, 0);
    return 1;
}

// /*DB Display*/
// int db_display(char* database)
// {
    
//     DBC* dbcp;
//     DBT key, data;
//     int close_db, close_dbc, ret;

//     close_db = close_dbc = 0;

//     /* Open the database. */
//     if ((ret = db_create(&dbp, NULL, 0)) != 0) {
//         fprintf(stderr,
//             "%s: db_create: %s\n", database, db_strerror(ret));
//         return -1;
//     }
//     close_db = 1;

//     /* Turn on additional error output. */
//     dbp->set_errfile(dbp, stderr);
//     dbp->set_errpfx(dbp, database);

//     /* Open the database. */
//     if ((ret = dbp->open(dbp, NULL, database, NULL,
//         DB_UNKNOWN, DB_RDONLY, 0)) != 0) {
//         dbp->err(dbp, ret, "%s: DB->open", database);
//         goto err;
//     }

//     /* Acquire a cursor for the database. */
//     if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
//         dbp->err(dbp, ret, "DB->cursor");
//         goto err;
//     }
//     close_dbc = 1;

//     /* Initialize the key/data return pair. */
//     memset(&key, 0, sizeof(key));
//     memset(&data, 0, sizeof(data));

//     /* Walk through the database and print out the key/data pairs. */
//     while ((ret = dbcp->get(dbcp, &key, &data, DB_NEXT)) == 0) {
//         if (strncmp(key.data, "ty", key.size) == 0 ||
//             strncmp(key.data, "st", key.size) == 0 ||
//             strncmp(key.data, "cni", key.size) == 0 ||
//             strncmp(key.data, "cbs", key.size) == 0 ||
//             strncmp(key.data, "cs", key.size) == 0
//             ){
//             printf("%.*s : %d\n", (int)key.size, (char*)key.data, *(int*)data.data);
//         }
//         else if (strncmp(key.data, "rr", key.size) == 0) {
//             printf("%.*s : ", (int)key.size, (char*)key.data);
//             if (*(bool*)data.data == true)
//                 printf("true\n");
//             else
//                 printf("false\n");
//         }

//         else {
//             printf("%.*s : %.*s\n",
//                 (int)key.size, (char*)key.data,
//                 (int)data.size, (char*)data.data);
//         }
//     }
//     if (ret != DB_NOTFOUND) {
//         dbp->err(dbp, ret, "DBcursor->get");
//         printf("Cursor ERROR\n");
//         return -1;
//     }


// err:    if (close_dbc && (ret = dbcp->close(dbcp)) != 0)
// dbp->err(dbp, ret, "DBcursor->close");
// if (close_db && (ret = dbp->close(dbp, 0)) != 0)
// fprintf(stderr,
//     "%s: DB->close: %s\n", database, db_strerror(ret));
// return -1;
// }


int db_store_cse(CSE *cse_object) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_cse");

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

    //dbp = DB_CREATE_(dbp);
    //DB_OPEN(DATABASE);
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
    // dbp->close(dbp, 0); 

    return 1;
}

int db_store_ae(AE *ae_object) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_ae");
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
    
    return 1;
}

int db_store_cnt(CNT *cnt_object) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_cnt");
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
    
    return 1;
}

int db_store_cin(CIN *cin_object) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_cin");

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
    
    return 1;
}

int db_store_grp(GRP *grp_object){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_grp");
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
    return 1;
}

int db_store_sub(SUB *sub_object) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_sub");
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

    return 1;
}

int db_store_acp(ACP *acp_object) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_store_acp");
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
    return 1;
}

CSE* db_get_cse(char* ri) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_cse");

    //struct to return
    CSE* new_cse= NULL;

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
    // if (dbp != NULL)
    //     dbp->close(dbp, 0);
    
    return new_cse;
}

AE* db_get_ae(char* ri) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_ae");

    //struct to return
    AE* new_ae = NULL;
    
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
    // if (dbp != NULL)
    //     dbp->close(dbp, 0);
    
    return new_ae;
}

CNT* db_get_cnt(char* ri) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_cnt");

    //struct to return
    CNT* new_cnt = NULL;

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
    // if (dbp != NULL)
    //     dbp->close(dbp, 0);

    return new_cnt;
}

CIN* db_get_cin(char* ri) {
    //struct to return
    CIN* new_cin = NULL;

    
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
    // if (dbp != NULL)
    //     dbp->close(dbp, 0);
    //
    return new_cin;
}

SUB* db_get_sub(char* ri) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_sub");

    //store AE
    SUB* new_sub = NULL;
    
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


    return new_sub;
}

ACP* db_get_acp(char* ri) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_acp");

    //struct to return
    ACP* new_acp = NULL;
    
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
    // if (dbp != NULL)
    //     dbp->close(dbp, 0);
    

    return new_acp;
}

GRP *db_get_grp(char* ri) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_grp");

    GRP *new_grp = calloc(1, sizeof(GRP));

    
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
    // if (dbp != NULL)
    //     dbp->close(dbp, 0);
    

    return new_grp;
}

int db_delete_onem2m_resource(char* ri) {
    logger("DB", LOG_LEVEL_DEBUG, "Delete [RI] %s",ri);
    
    DBC* dbcp;
    DBT key, data;
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
    // if (dbp != NULL)
    //     dbp->close(dbp, 0);
    
    
    /* Delete Success */
    return 1;
}

int db_delete_sub(char* ri) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_delete_sub");
    
    DBC* dbcp;
    DBT key, data;
    int ret;

    // /* Open the database. */
    // if ((ret = db_create(&dbp, NULL, 0)) != 0) {
    //     fprintf(stderr,
    //         "%s: db_create: %s\n", database, db_strerror(ret));
    //     return 0;
    // }

    // ret = dbp->open(dbp, NULL, database, NULL, DB_BTREE, DB_CREATE, 0664);
    // if (ret) {
    //     dbp->err(dbp, ret, "%s", database);
    //     return 0;
    //     exit(1);
    // }

    // /* Acquire a cursor for the database. */
    // if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
    //     dbp->err(dbp, ret, "DB->cursor");
    //     return 0;
    //     exit(1);
    // }

    dbcp = DB_GET_CURSOR(subDBp);

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    int cnt = 0;
    int flag = 0;
    int struct_size = 10;

    // DBC* dbcp0;
    // if ((ret = dbp->cursor(dbp, NULL, &dbcp0, 0)) != 0) {
    //     dbp->err(dbp, ret, "DB->cursor");
    //     return 0;
    // }
    // while ((ret = dbcp0->get(dbcp0, &key, &data, DB_NEXT)) == 0) {
    //     cnt++;
    //     if (strncmp(data.data, ri, data.size) == 0) {
    //         flag=1;
    //         break;
    //     }
    // }
    // if (cnt == 0 || flag==0) {
    //     logger("DB", LOG_LEVEL_DEBUG, "Data does not exist");
    //     return 0;
    // }

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

    /* Cursors must be closed */
    // if (dbcp0 != NULL)
    //     dbcp0->close(dbcp0);
    if (dbcp != NULL)
        dbcp->close(dbcp);
    // if (dbp != NULL)
    //     dbp->close(dbp, 0);
    
    
    /* Delete Success */
    return 1;
}

int db_delete_acp(char* ri) {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_delete_acp");
    
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
    // if (dbp != NULL)
    //     dbp->close(dbp, 0);
    
    /* Delete Success */
    
    return 1;
}

int db_delete_grp(char *ri){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_delete_grp");
    
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
    // if (dbp != NULL)
    //     dbp->close(dbp, 0);
    
    
    /* Delete Success */
    return 1;
}

RTNode* db_get_all_cse() {
    fprintf(stderr,"\x1b[92m[Get All CSE]\x1b[0m\n");
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
    // if (dbp != NULL)
    //     dbp->close(dbp, 0);    
    return head;
}

RTNode* db_get_all_ae_rtnode() {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_ae_rtnode");
    const const char* TYPE = "C";
    
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
    // if (dbp != NULL)
    //     dbp->close(dbp, 0);    
    return head;
}

RTNode* db_get_all_cnt_rtnode() {
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_cnt_rtnode");
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
    // if (dbp != NULL)
    //     dbp->close(dbp, 0);    
    return head;
}

RTNode* db_get_all_sub_rtnode(){
    logger("DB", LOG_LEVEL_DEBUG, "Call db_get_all_sub_rtnode");
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

    return head;
}

RTNode* db_get_all_acp_rtnode() {
    const char* TYPE = "1-";
    
    DBC* dbcp;
    DBT key, data;
    int ret;

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
    // if (dbp != NULL)
    //     dbp->close(dbp, 0);    
    return head;
}


RTNode* db_get_all_grp_rtnode(){
    logger("DB", LOG_LEVEL_DEBUG,"Call db_get_all_grp_rtnode");
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
    // if (dbp != NULL)
    //     dbp->close(dbp, 0);    

    return head;
}

RTNode* db_get_cin_rtnode_list(RTNode *parent_rtnode) {
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
         
    // if (dbp != NULL)
    //     dbp->close(dbp, 0); 

    return head;
}