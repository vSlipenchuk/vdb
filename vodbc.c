/*
  Simple vdb ODBC lawer
*/


#include "vdb.h"
#include <stdio.h>
#include <sql.h>
#include <sqlext.h>

#define Version "1.0.0.0"
#define Description "ODBC vdb driver"

/*
  Версии 1.0.0.0 - 30авг2008 - первая версия (в режиме автокомита)
        1. нету - bind (сак - нужно пересобирать текст, как в ибазе!),
        2. нету blob (может их вообще нету?)
        3. может не все типы реализованы??? - не знаю...
*/

typedef struct { // Коннектор для ODBC
    SQLHENV env; // Enviroment handle
    SQLHDBC db; // Connection Handle
    SQLHSTMT stmt; // Statement SQL ???
    char state[8]; // State On Errors
    int err_code; // LastErrorCode
    int binded; // If a data binded???
    int sqlType; // Compiled SQL Type (>0 for specs 1 = commi1, -1 rollback)
    SQLINTEGER native_error_code; // On Error - native error code
    } dbo;


int dbo_error(database *db) {
dbo *dbo = db->h; // Gets a handler
int err; void *h; int typ;
SQLSMALLINT sz = 0;
if (dbo->stmt) { h = dbo->stmt; typ = SQL_HANDLE_STMT;}
 else if (dbo->db) { h = dbo->db; typ = SQL_HANDLE_DBC;}
  else { h = dbo->env; typ = SQL_HANDLE_ENV;}
err = SQLGetDiagRec( typ, h,
  1, // Only First error
  dbo->state, // Five char state error
  &dbo->native_error_code,  // Native Error code for a message
  db->error, sizeof(db->error), // Message Text
  &sz); // Strange result len???
if (err) sprintf(db->error,"SQL_ERROR:%d",dbo->err_code);
else { // zero term
    if (sz>=sizeof(db->error)) sz = sizeof(db->error)-1;
    db->error[sz]=0;//?
    }
return 0;
}

int dbo_done(database *db) {
dbo *dbo = db->h; // Gets a handler
if (!dbo) return 0; // ???
if (dbo->stmt) { SQLFreeHandle(SQL_HANDLE_STMT,dbo->stmt); dbo->stmt=0;}
if (dbo->db)   { SQLDisconnect(dbo->db); SQLFreeHandle(SQL_HANDLE_DBC,dbo->db); dbo->db=0;}
if (dbo->env)  { SQLFreeHandle(SQL_HANDLE_ENV,dbo->env); dbo->env=0;}
db->h = 0; free(dbo);
return 1;
}

int dbo_compile(database *db, unsigned char *sql) { // opens cursor & compile it???
dbo *dbo = db->h; // Gets a handler
db->in.count = 0;
dbo->binded = 0; // if we have them before
while(*sql && *sql<=32) sql++;
if (!dbo->stmt) SQLAllocHandle(SQL_HANDLE_STMT,dbo->db,&dbo->stmt); // Alloc it
if (db->fetchable) {
    SQLCloseCursor(dbo->stmt);
    db->fetchable = 0;
    }
if (stricmp(sql,"commit")==0) {  dbo->sqlType = 1; return 1;  }
 else if (stricmp(sql,"rollback")==0) { dbo->sqlType = -1; return 1;}
  else dbo->sqlType = 0; // Native, do again
dbo->err_code = SQLPrepare(dbo->stmt,sql,SQL_NTS);
if (SQL_SUCCESS!=dbo->err_code) return dbo_error(db); // Fail
return 1;
}

int dbo_commit(database *db) {
dbo *dbo = db->h; // Gets a handler
dbo->err_code = SQLEndTran(SQL_HANDLE_DBC,dbo->db,SQL_COMMIT);
if (dbo->err_code!=SQL_SUCCESS) return dbo_error(db);
return 1;
}

int dbo_rollback(database *db) {
dbo *dbo = db->h; // Gets a handler
dbo->err_code = SQLEndTran(SQL_HANDLE_DBC,dbo->db,SQL_ROLLBACK);
if (dbo->err_code!=SQL_SUCCESS) return dbo_error(db);
return 1;
}

int dbo_exec(database *db) {
dbo *dbo = db->h; // Gets a handler
db_col *c; int i;
i = dbo->sqlType; dbo->sqlType = 0;
if (i) { // spec type!!!
    if (i == 1 ) return dbo_commit(db);
      else if (i == -1) return dbo_rollback(db);
    sprintf(db->error,"vdb odbc sqlType%d unknown",i);
    return 0;
    }
if (db->in.count && !dbo->binded) { // Have to bind them all
    for(i=0;i<db->in.count;i++) {
        c = db->in.cols + i;
        }
    dbo->binded=1; // done bind
    }
for(i=0;i<db->in.count;i++) { // Bind In Params
    c = db->in.cols + i;
    if (c->type == dbDate) { // Have to decode !!!
        int Y[8]; TIMESTAMP_STRUCT *t = (void*)c->dbvalue;
        memset(Y,0,sizeof(Y));
        dt_decode(*(double*)c->value,Y,Y+1,Y+2,Y+3,Y+4,Y+5);
        t->year = Y[0]; t->month = Y[1]; t->day = Y[2];
        t->hour = Y[3]; t->minute = Y[4]; t->second = Y[5];
        }
    }
dbo->err_code = SQLExecute(dbo->stmt);
if (SQL_SUCCESS!=dbo->err_code) return dbo_error(db); // Fail
return 1;
}

int dbo_fetch(database *db) {
dbo *dbo = db->h; // Gets a handler
int i;
if (!db->fetchable) return 0; // Not  a fetchable???
dbo->err_code = SQLFetch(dbo->stmt);
if (dbo->err_code!=SQL_SUCCESS) {
    SQLCloseCursor(dbo->stmt);
    db->fetchable = 0;
    return dbo_error(db);
    }
// OK - fetched???
for(i=0;i<db->out.count;i++) {
    db_col *c;
    c = db->out.cols+i;
    if (c->null <=0 ) continue; // Real Null!!!
    c->len = c->null; c->null=0; // resulted length
    if (c->type == dbDate) { // decoding
        TIMESTAMP_STRUCT *t = (void*)c->dbvalue;
        double d;
        d = dt_encode(t->year,t->month,t->day,t->hour,t->minute,t->second);
        if (!d) c->null = -1; else *(double*)c->value = d; // SetIt
        }
   // reset it here???
    //fprintf(stderr,"name=%s null=%d val=%s\n",c->name,c->null,c->value);// Just DO !!!
    }
return 1;
}

int dbo_open(database *db) { ////SQLDescribeCol()    SQLBindCol()    SQLFetch
SQLSMALLINT ncol; int i;
dbo *dbo = db->h; // Gets a handler
dbo->err_code = SQLNumResultCols(dbo->stmt,&ncol);
if (dbo->err_code!=SQL_SUCCESS) return dbo_error(db);
if (ncol<=0) { sprintf(db->error,"zero columns"); return 0;} // Never?
db->out.count = 0; // clear columns anyway ???
for(i=0;i<ncol;i++) { // describe cols
    db_col *c;
    char name[1024]; SQLSMALLINT szName,typ,digs,nullable, vtype;
    SQLUINTEGER size;
    dbo->err_code = SQLDescribeCol(dbo->stmt, i+1, name,sizeof(name)-1, &szName,
       &typ,&size,&digs,&nullable);
    if (dbo->err_code!=SQL_SUCCESS) return dbo_error(db); // Smth?
    name[1024]=0; // IF more...
    switch(typ) { // Тут нам надо определиться - понимаем мы эти типы или нет
    case SQL_CHAR: case SQL_VARCHAR: case SQL_WCHAR: case SQL_WVARCHAR:
        vtype = dbChar; break;
    case SQL_SMALLINT: case SQL_INTEGER: case SQL_TINYINT: case SQL_BIGINT:
      vtype = dbInt; break;
    case SQL_DECIMAL: case SQL_NUMERIC: case SQL_REAL: case SQL_DOUBLE:
      //fprintf(stderr,"numtype %s, digs:%d\n",name,digs);
      vtype = dbDouble; break;
    case SQL_TYPE_DATE: case SQL_TYPE_TIME: case SQL_TYPE_TIMESTAMP:
      vtype = dbDate; size = sizeof(TIMESTAMP_STRUCT); break;
    default:
      sprintf(db->error,"vdb odbctype %d unsupported",typ);
      db->out.count=0;
      return 0;
    }
    //fprintf(stderr,"dbo_open: ColName=%s\n",name);
    c = db_add_col(&db->out,name,vtype,size);
    c->dbtype = typ; // save it for a future
    }
for(i=0;i<ncol;i++) { // Now - bind them !!!
    db_col * c = db->out.cols+i; int typ,len = c->len;
    switch(c->type) { // define target type???
    case dbInt: typ = SQL_C_SLONG; break;
    case dbDouble: typ = SQL_C_DOUBLE; break;
    case dbDate: typ = SQL_C_TIMESTAMP; break;
    default: typ = SQL_C_CHAR; len++; break;
    }
    dbo->err_code = SQLBindCol(dbo->stmt, i+1, typ, c->dbvalue, len, (void*) &c->null);
    if (dbo->err_code!=SQL_SUCCESS) {
        db->out.cols = 0;
        return dbo_error(db);
        }
    }
db->fetchable = 1; // Ready to fetch???
// ok - somth columns here???
return 1;
} // Opens result set here ???

int dbo_fail(database *db) {
dbo *dbo = db->h; // Gets a handler
if (dbo->err_code == SQL_SUCCESS) return 0;
if (dbo->err_code == SQL_SUCCESS_WITH_INFO) return 0;
return 1;
}

int dbo_connect(database *db, uchar *host, uchar *user, uchar *pass) { // Должна быть экспортирована
// Try connect & if ok - returns 1
dbo *d;
dbo_done(db); // Clear If Any
d = malloc(sizeof(dbo)); memset(d,0,sizeof(dbo)); db->h = d; // SetHew Handle
SQLAllocHandle(SQL_HANDLE_ENV,0, &d->env); // Create an enviroment
//printf("ENV=%x\n",d->env);
d->err_code = SQLSetEnvAttr(d->env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3,0);
if (dbo_fail(db)) {
    //printf("Err %d on SetEnv\n",d->err_code);
    dbo_error(db);
    dbo_done(db);
    return 0;
    }
//printf("Try alloc db\n");
d->err_code =SQLAllocHandle(SQL_HANDLE_DBC, d->env, &d->db); // Create a new handle
if (dbo_fail(db)) {
    dbo_error(db);
    dbo_done(db);
    return 0;
    }
//printf("DB=%x\n",d->db);
//printf("...Try connect odbc host:'%s', user:'%s', pass:'%s'\n",host,user,pass);
d->err_code = SQLConnect(d->db,host,strlen(host),user,strlen(user),pass,strlen(pass));
if (dbo_fail(db)) {
    dbo_error(db); // SetMyError and exit
    dbo_done(db);
    return 0;
    }
///* -- не везде это получается ...
d->err_code = SQLSetConnectAttr(d->db,SQL_ATTR_AUTOCOMMIT,(void*)SQL_AUTOCOMMIT_OFF,0);
/*
if (SQL_SUCCESS!=d->err_code) {
    dbo_error(db); // SetMyError and exit
    dbo_done(db);
    return 0;
    }
*/
db->disconnect = dbo_done;
db->compile = dbo_compile;
db->exec = dbo_exec;
db->open = dbo_open;
db->fetch = dbo_fetch;
db->commit = dbo_commit;
db->rollback = dbo_rollback;
//db->bind = dbo_bind;
return 1; //
}

char *prefix() { return "dbo_";}  // Должна быть экспортирована
