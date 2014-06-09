/*

 SQL lite module - реализация драйвера vdb

  Версии:
  3.0.10.12 -  кодировка по умолчанию UTF8, удалено декодирование в win1251 (undef SQL_UTF8)

  15 сентября 2009
  3.0.10.11 - убрана страшная бага оценки нахождения в транзакции.
              время сделано под C# - так и будет теперь...
  3.0.10.10 - встроена UTF8 как основной язык БД (и след - переконвертации в тексте)

 ToDo:
  0 !!!
  1. +Заменить хранение дат на намбер - возможность +-...
  ----
  15 - сентября 2009 - поддержка UTF8 при компиляции, выборках и связываниях
  1. +Типизированные возвращаемые значения - по дефолту varchar(4000)
но если колонка селекта забирается из таблицы - по названиям типов колонок восстанавливаются
возвращаемые типы (INTEGER,NUMBER,CHAR,DATE,BLOB)
  2. +bind variables входные сделаны. Выходные - пока непонятно, могут ли быть?
  3. +транзакции - commit & rollback сейчас возвращают ОК (BEGIN,COMMIT,END)
  4.


  Баги - особенности:
  1. sqlite_column_type (если не было фетча)=1 - это нормально?
  короче - видимо, единственный способ получить тип - это
  const char * sqlite3_column_decltype(sqlite3_stmt *, int iCol) - сейчас им
пользуемся (и дальше - разбор текстовой строки. Дурь какая-то?)
  2. кажется залечил? - вываливается - если было были селекты и потоп - переконнект
Нужно проверять на dbleack.v8
  3. если данные недофетчены - их обязательно нужно закрыть (финализировать)

*/

#include "vdb.h"
#include <sqlite3.h>
#include <time.h>
#include <stdio.h>
#include "coders.h"
#include "vos.h"

#define printf

//#define SQL_UTF8 1

// Возвращаем строку, в sqlite нету типа DATE
static void vs_sysdate( sqlite3_context *context,  int argc,  sqlite3_value **argv){
  time_t t;
  //char *zFormat = (char *)sqlite3_user_data(context);
  char zBuf[20];
  struct tm sNow;
  time(&t);
  sNow = *localtime(&t);
  //sprintf(zBuf,"%d.%02d.%d %02d:%02d:%02d",sNow.tm_mday,sNow.tm_mon+1,sNow.tm_year+1900,
    //sNow.tm_hour,sNow.tm_min,sNow.tm_sec);
  sprintf(zBuf,"%04d-%02d-%02d %02d:%02d:%02d",sNow.tm_year+1900,sNow.tm_mon+1,sNow.tm_mday,
       sNow.tm_hour,sNow.tm_min,sNow.tm_sec);
  //strftime(zBuf, 20, zFormat, &sNow);
  // printf("SysCalled\n");
  sqlite3_result_text(context,zBuf, -1, SQLITE_TRANSIENT);
}


static void vs_nvl( sqlite3_context *context,  int argc,  sqlite3_value **argv){
char  *p1=sqlite3_value_text(argv[0]),
      *p2=sqlite3_value_text(argv[1]);
  if (!p1 || !*p1) sqlite3_result_text(context,p2, -1, SQLITE_TRANSIENT);
        else sqlite3_result_text(context,p1, -1, SQLITE_TRANSIENT);
}


int sq_done(database *db) {
if (db->s) {  sqlite3_finalize(db->s); db->s=0;}
if (db->h) {  sqlite3_close(db->h); db->h = 0; }
return 1;
}

#define strCpy(A,B) {strncpy(A,B,sizeof(A)-1); A[sizeof(A)-1]=0;}

int sq_error(database *db) {
strCpy(db->error,sqlite3_errmsg(db->h));
return 0;
}

int sqlite_trans(database *db) { // Если включен автокоммит (!=0) - транзакции - нету...
if ( sqlite3_get_autocommit(db->h) ) return 0;
return 1;
}

int sq_commit(database *db) {
if (db->fetchable && db->s)  {
   sqlite3_finalize(db->s); db->s=0; db->fetchable=0;}
//return db_exec_once(db,"commit");
db->transaction = sqlite_trans(db);
sqlite3_exec(db->h,"commit",0,0,0);
db->transaction = 0; // Stopped transaction
if (db->s) sqlite3_finalize(db->s); db->s = 0;
return 1;
}

int sq_rollback(database *db) {
if (db->fetchable && db->s)  {
   sqlite3_finalize(db->s); db->s=0; db->fetchable=0;}
db->transaction = sqlite_trans(db);
//if (db->transaction)
sqlite3_exec(db->h,"rollback",0,0,0);
if (db->s) {  sqlite3_finalize(db->s); db->s=0;}
db->transaction = 0; // Stopped transaction
return 1;
}

int sq_compile(database *db, char *sql) {
char *upd; int err,is_select;  int len;
db_blob_handle *buf = &db->buf; // temp buffer for compile

#ifdef SQL_UTF8
len = strlen(sql)+1;
buf->len=0;
db_add_blob(buf,0,10+len*10); // ensure a size
encode_utf8(buf->data,sql,-1); // encode it her
sql = buf->data; // fix a new sql
printf("Encoded SQL:'%s'\n",sql);
#endif

if (db->fetchable && db->s)  {
   sqlite3_finalize(db->s); db->s=0; db->fetchable=0;}

db->in.count = 0; // clear in variables
upd = strstr(sql,"for update"); // if have - really transaction starts...
is_select = strncmp(sql,"select",6)==0;
if (is_select && upd) memcpy(upd,"          ",10); // replace this
                else upd = 0;
printf("check transaction?\n");
db->transaction = sqlite_trans(db);
printf("check transaction res = %d?\n",db->transaction);
//if (db->s) {  sqlite3_finalize(db->s); db->s=0;}
if (!db->transaction &&
    strncmp(sql,"commit",6)!=0 && strncmp(sql,"rollback",8)!=0
    &&  ( upd || !is_select )) { // Start transaction -)))
   printf("openk transaction?\n");
   err = sqlite3_exec(db->h,"BEGIN",0,0,0);
   printf("errs of open = %d\n",err);
   if (err) {
        if (upd) memcpy(upd,"for update",10); // ret back -))
        return sq_error(db);
        }
   printf("sqlite -- opened transaction on SQL=%s OK\n",sql);
   db->transaction = sqlite_trans(db);
   }
printf("sq - begin compile '%s'\n",sql);
err = sqlite3_prepare_v2(db->h, sql, -1, (void*)&db->s, 0);
printf("sqlite3_prepare:%d Encoded SQL:'%s'\n",err,sql);
if (upd) memcpy(upd,"for update",10); // ret back -))
if (err) return sq_error(db);
printf("sqlite prepared ok %d bind variables on %s\n", sqlite3_bind_parameter_count(db->s),sql);
while(*sql && *sql<=32) sql++;
if (strnicmp(sql,"commit",6)==0 || strnicmp(sql,"rollback",8)==0) { // done it after exec???
  db->transaction = 0;
  }
//printf("sq - compiled ok\n");
return 1; // OK
}




int sq_bind(database *db, char *name, int typ, int *null, void *data, int len) {
int i = 0;
char nam[80];
db_col *c;
if (name[0]!=':') { strncpy(nam+1,name,40); nam[0]=':'; nam[40]=0; name=nam;};
if (!db->s) i=0;
 else i = sqlite3_bind_parameter_index(db->s,name);
//printf("sq idx=%d for '%s'\n",i,name);
if (!i) { sprintf(db->error,"sqlite: bind %s not found",name); return 0;}
c=db_add_col(&db->in,name,typ, 8);
if (!c) return 0; // No memory ???
c->dbflag = typ>>16; // flags?
c->type = typ & 0xFF;   // Сюда запоминаем тип
c->value = (void*)data; // Сюда запоминаем - откуда брать данные
c->len = len; // Сюда запоминаем откуда брать длину
c->dbtype = i; // Самое главное - индекс !!!
sprintf(db->error,"sqlite: unknown type %d",typ);
return 1;
}

int sq_decl_number(db_col *c, unsigned char *s) {
int n1,n2=0;
while(*s && *s<=32) s++; if (*s=='(') s++;
c->len = 0;
if (sscanf(s,"%d,%d",&n1,&n2)>0) c->len = n2; // scale
c->type = dbNumber;
return 1;
}

int sq_decl_char(db_col *c, unsigned char *s) {
int n1=0;
while(*s && *s!='(') s++;
if (*s=='(') s++; c->len = 0;
if (sscanf(s,"%d",&n1)>0) c->len = n1; // scale
if (c->len<=0) c->len = 4000; // def char ???
c->type = dbChar;
return 1;
}


int sq_decl(db_col *c,unsigned char *s)  {
if (!s) return 0;
printf("col=%s declared as %s\n",c->name,s);
while(*s && *s<=32) s++;
if (strnicmp(s,"integer",7)==0) c->type = dbInt;
 else if (strnicmp(s,"decimal",7)==0) sq_decl_number(c,s+7);
  else if (strnicmp(s,"number",6)==0) sq_decl_number(c,s+6);
   else if (strnicmp(s,"char",4)==0) sq_decl_char(c,s+4);
    else if (strnicmp(s,"varchar",7)==0) sq_decl_char(c,s+7);
     else if (strnicmp(s,"blob",4)==0)  c->type = dbBlob;
      else if (strnicmp(s,"date",4)==0) c->type = dbDate;
return 1;
}

int sq_open(database *db) { // Fills resulted columns
int j,coln;
if (!db->s) return 0;
coln  = sqlite3_column_count(db->s);
db->out.count = 0; // Clear
if (coln<=0) { strcpy(db->error,"sqlite: empty dataset");}
for(j=0; j<coln; j++) {
        const char *name; db_col *c;
        name   = sqlite3_column_name(db->s, j);
        c = db_add_col(&db->out, (char*)name , dbChar , 10); // Adding a columns
        c->type = dbChar;  c->len = 4000; // default -))
        sq_decl(c, sqlite3_column_decltype(db->s, j)); // try correct data-type
        }
return 1;
}

int sq_exec(database *db) {
int i; db_col *c; int pos[256],ipos=0,len; // Максимальное количество связываемых переменных
db_blob_handle *bh = &db->buf; uchar *data;
if (!db->s) return sq_error(db);
printf("sq_exec - %d bind vars\n", db->in.count);
// Нужно скопировать данные для бинда более одной переменной!!!
 bh->len = 0;
for(i=0,c=db->in.cols;i<db->in.count;i++,c++) { // Перебираем что перекодировать
  if (c->type==dbChar) { // Пора!!!
    int strt;
    len = c->len; data = c->value; if (len<0) len  = strlen(data);
    pos[ipos]=strt = bh->len; // Начало в буфере
     #ifdef SQL_UTF8
    db_blob_add(bh,0,1+len*4); // fore sure
    len =  decode_utf8(bh->data+strt,c->value,len); // decode it here...
     bh->len = strt + len; bh->data[bh->len]=0; bh->len++; // zero term
    #endif
    ipos++;
    }
  }

ipos = 0;
for(i=0,c=db->in.cols;i<db->in.count;i++,c++) { // Делаем перемещение
  db_blob_handle *bh = (void*)c->value; double *d = (void*) c->value;
  char buf[100];
  printf("sq_exec: REBIND name = '%s' idx=%d\n",c->name,c->dbtype);
  switch(c->type) {
  case dbInt: sqlite3_bind_int(db->s,c->dbtype, *(int*)c->value); break;
  case dbNumber: sqlite3_bind_double(db->s,c->dbtype, *(double*)c->value); break;
  case dbDate:  // БИНДИМ как текст "yyyymmddhhnnss"
             if (!*d) buf[0]=0;
             else { int Y[8];
             dt_decode(*d,Y,Y+1,Y+2, Y+3,Y+4,Y+5);
             sprintf(buf,"%04d%02d%02d%02d%02d%02d",Y[0],Y[1],Y[2],Y[3],Y[4],Y[5]);
             }
             sqlite3_bind_text(db->s,c->dbtype, buf, -1,SQLITE_TRANSIENT); break;
  case dbChar: {
              uchar *data = c->value;
              int len = c->len; if (len<0) len = strlen(data);
              #ifdef SQL_UTF8
              data =db->buf.data + pos[ipos]; len = strlen(data);
              ipos++;
              #endif
              sqlite3_bind_text(db->s,c->dbtype, data,len,SQLITE_STATIC);
              break;
              }
  case dbBlob: sqlite3_bind_blob(db->s,c->dbtype, bh->data, bh->len, SQLITE_STATIC); break;
  }
  //
  }
i = sqlite3_step(db->s);
printf("sq_exec_step=%d DONE=%d\n",i,SQLITE_DONE);
if (i == SQLITE_DONE) {

     i = sqlite3_reset(db->s);
     //fprintf(stderr,"sqlite reset=%d\n",i);
     db->fetchable = 0;
     if (!sqlite_trans(db)) { sqlite3_finalize(db->s); db->s=0;}
     return 1; // OK
     }
if (i==SQLITE_ROW) {
   db->fetchable = 2; // open fetchable
   return 1; // ok - new row ready ---)
   }
return sq_error(db);
}

static char sq_empty[2]="";

int sq_fetch(database *db) { // Try to fetch data into columns
int rc,i;
if (!db->s) return sq_error(db);
if (db->fetchable==2) rc=SQLITE_ROW ; // already has a row
    else rc = sqlite3_step(db->s);
printf("sq  - end fetch result=%d expect=%d\n",rc,SQLITE_ROW);
db->fetchable = rc == SQLITE_ROW;
if (0) if (!db->fetchable) {
   fprintf(stderr,"sqlite: finalize (no fetch)\n");
   if (!sqlite_trans(db) && db->s)
         { sqlite3_finalize(db->s); db->s=0;}
   return 0;
   }
if (db->fetchable) { // OK --
     db_col *c; uchar *str;
     for(i=0,c=db->out.cols;i<db->out.count;i++,c++) {
        printf("F:%d\n",i);
        c->null = 0; // no nulls -)))
        if (c->type == dbInt) *(int*)c->value = sqlite3_column_int(db->s, i);
         else if (c->type == dbNumber) *(double*)c->value = sqlite3_column_double(db->s, i);
          else if (c->type == dbDate) {
              char *str;
              str = sqlite3_column_text(db->s, i); // Copy Text Here
              if (!str) str="";
              //printf("DATE=%d\n",str);
              *(double*)c->value = dt_scanf(str,-1); // set it
              c->null =  (*(double*)c->value)==0;
              } else if (c->type == dbChar) {
                 str = (char*) sqlite3_column_text(db->s, i); // Copy Text Here
                 c->null = str==0;
                 if (!str) { c->value = ""; c->len=0;}
                  else { c->value = str; c->len = sqlite3_column_bytes(db->s,i);
//printf("SQL_UTF=%d\n",SQL_UTF8);
                       #ifdef SQL_UTF8
                       c->len = decode_utf8(c->value,c->value,c->len);
                       #endif
                       }
                 }
         else if (c->type = dbBlob) {
             db_blob_handle *bh = (void*)c->value;
             str = sqlite3_column_blob(db->s,i);
             //fprintf(stderr,"dbBlob=%d\n",str);
             c->null = str==0;
             if (!str) { bh->data = ""; bh->len=0;}
                    else { bh->data=str; bh->len =  sqlite3_column_bytes(db->s,i);}
             }
       }
     return 1;
     }
return sq_error(db);
}

char *sq_params[]={
    "sysdate","sysdate()", // Текущая дата -)
    "all_tables"," table_name  from all_tables where owner=user order by 1", // Все таблицы текущего пользователя
    "DATE_COL", "DATE",
    0,0};

int sq_disconnect(database *db) {
if (db->h) {
if (db->transaction) sqlite3_exec(db->h,"rollback",0,0,0);
if (db->s) {  sqlite3_finalize(db->s); db->s=0;}
sqlite3_close(db->h); db->h;
 }
return 1;
}

int sq_connect(database *db , char *host, char *user, char *pass);

vdb_func sq_fun = { // Статическая структура
   "Sqlite", //   char *name; // Database Internal Name
   sq_connect, //int (*connect)() ;
   sq_disconnect, // int (*disconnect)();
   sq_fetch, //int (*fetch)();
   sq_exec, //int (*exec)();
   sq_compile, //int (*compile)();
   sq_open, //int (*open)();
   sq_commit, //int (*commit)();
   sq_rollback, //int (*rollback)();
   sq_bind, //int (*bind)();
   sq_params, //char **params; // Внешний буффер именованных параметров -)
   //int res[512]; // Для расширений -)
   } ;//vdb_func;


int sq_connect(database *db , char *host, char *user, char *pass) {
if (sqlite3_open(host, (void*)&db->h)) 	{  sq_error(db); sq_done(db); return 0; 	}
sqlite3_create_function(db->h,"sysdate",0,SQLITE_UTF8,0, vs_sysdate, 0,0); // Create MyFunction
sqlite3_create_function(db->h,"nvl",2,SQLITE_UTF8,0, vs_nvl, 0,0); // Create MyFunction
db->disconnect = sq_done;
db->compile = sq_compile;
db->open = sq_open;
db->exec = sq_exec;
db->fetch = sq_fetch;
db->bind= sq_bind;
db->commit = sq_commit;
db->rollback = sq_rollback;
db->fun = &sq_fun;
return 1; // OK - connected
}

char *prefix() { return "sq_"; }
