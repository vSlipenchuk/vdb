#ifndef VDBH
#define VDBH


#include "vtypes.h"

#define vdb_version "2.3.0.0"
#include <time.h>

#ifdef MSWIN
#include "windows.h" // for proc & register functions
#endif

/*

 !!! Compiling in MS-Windows needs #define MSWIN !!!

*/
// #include "dt.h" - now its included ,,,

typedef double date_time;
extern int dt_month_day[2][12];
extern int tzOffsetSeconds; // set by tzInit
extern int tzOffsetSign ;  // '+' или '-'
extern int tzOffsetHour ; //   Без знака
extern int tzOffsetMinutes ; // Без знака

extern char * dt_mon_name[13];
extern uchar *szWkName1[] ; // Имена дней недели (0 - ошибка, 1 - понедельник, 7 - воскресенье)
extern uchar *szMonName[] ; // Имена месяцев (0 - Jan, 11-Dec)

extern uchar *szWDayName0[];

#define one_second (1/(24.*3600))
int 		dt_leap_year(int Year);
int dt_weekday(date_time strt); // 1- понедельник, 7 - воскресенье...
date_time 	date_encode(int Year,int Month,int Day);
date_time 	time_encode(int Hour,int Min,int Sec);
date_time 	dt_encode(int Year,int Month,int Day,int Hour,int Min,int Sec);
void 		date_decode(date_time Date,int *Year,int *Month,int *Day);
void 		time_decode(date_time D,int *Hour,int *Min,int *Sec);
int 		dt_decode(date_time DT,int *Year,int *Month,int *Day,int *Hour,int *Min,int *Sec);
int buf_dt_str(uchar *Buf,int maxSize, char *Fmt, date_time D) ;
char*		dt_str(char *fmt,date_time D);
int buf_dt_str(uchar *Buf,int maxSize, char *Fmt, date_time D);
date_time 	dt_scanf(char *fmt,int len);
char* 		dt_gupta(date_time D);
char* 		dt_rus(date_time D);
int 		dt_cmp(date_time D1,date_time D2);
int 		dt_year4(int year);
date_time 	now();
date_time 	date();

int dt2arr(double date, int *Y); // Extracts date to 7 bytes array ->>>
int dt2tm(double date, struct tm *t); // time.h
char *dt2rfc822(double date, char *out);
void dt_init();
char *dt2rfc822(double date, char *out); // Конвертирует дату в rfc822  -> Wed, 18 Jun 2008 06:00:10 +0400


 /*
 #include "u_pb.h"
 #include "vds.h"
 */

#define Free(A) if(A) free(A); A=0;

enum {dbUnknown,dbInt,dbNumber,dbDate,dbChar,dbBlob};
#define dbDouble dbNumber

enum {dbConnect,dbDisconnect, dbCommit,dbRollback,
      dbCompile,dbOpen, dbFetch,dbExec};


#define DB_STEP_COL    256
#define DB_STEP_BUFFER 300*1024 /* 30k peiece slices */
#define DB_MAX_ERROR   512
#define DB_MAX_NAME    32*4


#define DB_STR_AUTOLEN    1 /* dbflag - используется для автовычисляемых полей */
#define DB_NULL_AUTO      2 /* dbflag - используется для автоматического вычисления нула по содержимому */
#define DB_ALWAYS_NULL  (1 << 16) /* фЛАГ - значение всегда NULL*/
#define DB_OUT          (2 << 16) /* ФЛАГ - аут параметр при подвязывании переменных */

typedef struct { // Определитель "колонка"
     char *name;
     int  null,len;
     int  type; short dbtype,dbflag; // Расширяем под ибазу
     char *value,*dbvalue; // данные в приложении и данные в БД буфере
     } db_col;

#define db_blob_handle t_blob

typedef struct
    {
    int count,capacity;
    void *p;
    db_col *cols;
    int size,len;
    char *data;
    int bsize,blen;
    char *blob;
    }  db_cols;

typedef struct { // Эти функции должен имплементить каждый модуль, умеющий работать с БД
   char *name; // Database Internal Name
   int (*connect)() ;
   int (*disconnect)();
   int (*fetch)();
   int (*exec)();
   int (*compile)();
   int (*open)();
   int (*commit)();
   int (*rollback)();
   int (*bind)();
   char **params; // Внешний буффер именованных параметров -)
   int res[512]; // Для расширений -)
   } vdb_func;

typedef  struct
   {
   void *lib;
   void *h;
   void *c;
   void *s;

   void *p;
   db_cols in;
   db_cols out;
   int  err_code;

   char error[DB_MAX_ERROR];

   int (*connect)() ;
   int (*disconnect)();
   int (*fetch)();
   int (*exec)();
   int (*compile)();
   int (*open)();
   int (*commit)();
   int (*rollback)();
   int (*bind)();
   int connected; // Флаг коннекта - устанавливается при успешном db_connect, сбрасывается драйверами
   int mutex; // Предопределенное поле для многопоточного доступа, выделяется за пределами vdb
   t_blob buf; // Просто временный буфер, удаляется только при db_release()
   vdb_func *fun; // Указатель на блок функций БД (виртуал такой)
   int fetchable; // Флаг указывает открытый датасет.
   int transaction; // Флаг указывает есть открытая транзакция или нет
   int reserv[26];
   int typ_seq; // sequence type (2 - NextVal, other max(N)+1
   //char db_name[DB_MAX_NAME];
   } database;

int db_trace(int code,char *OK,char *ERR,database *db);
char *db_buf(db_cols *col,int len);
db_col *db_add_col_(db_cols *cols,char *name,int name_len,int type,int len);
db_col *db_add_col(db_cols *cols,char *name,int type,int len);
char   *db_prepare_blob(db_blob_handle *bh,int size);
#define db_blob_add(A,B,C) db_add_blob(A,B,C)
char   *db_add_blob(db_blob_handle *bh,void *data,int size);

int    db_int(db_col *column);
db_blob_handle db_blob(db_col *c);
double db_double(db_col *column);


database *db_new();

int db_init(database *db,char *module);
int db_connect(database *db,char *host,char *user,char *pass);
int db_connect_string(database *db,char *cs);
int db_disconnect(database *db);
int db_fetch(database *db);
int db_open(database *db);
int db_compile(database *db,char *sql);
int db_exec(database *db);
int db_execf(database *db, char *fmt, ...) ;
int db_commit(database *db);
int db_rollback(database *db);
int db_select(database *db,char *sql);
int db_selectf(database *db,char *fmt,...);
int db_bind(database *db,char *name,int type,int *index,void *data,int len);
int db_exec_once(database *db,char *sql);

int db_connect4(database *db,char *DLL,char *HOST,char *USER,char *PASS);
int db_connect_reg(database *db,char *name);
int db_connect_reg2(database *db,char *name);
int db_done(database *db);
int db_release(database *db);


char* db_prepare_blob(db_blob_handle *b,int size);
char* db_add_blob(db_blob_handle *b,void *buf,int size);
int   db_free_mem(char *p);
int   db_free_blob(db_blob_handle *b);
int   db_add_blob_(db_blob_handle *b,void *buf,int size);


char *db_text_buf(char *szbuf,db_col *c);
char *db_number_text(double d);
char *db_text(db_col *column);
char *db_text_out(database *db,int ncol);
int db_gettext(database *db,db_blob_handle *b,int heads);

char *next_line(char **s,int *nl);
char *next_word(char **s,int *nl);

int txt_is(char **text,char *p);
int dbx_insert(database *db,unsigned char *p,int (*callback)(),int handle);
char *dbx_xselect_text(database *db,char *sql);
int db_fetch_all(database *db,char *fmt,void **data,int size);

int dbx_select(database *db,db_blob_handle *tmp,char *sql);


int ora_connect(database *db,char *srvname,char *username,char *password);


#endif  // VDBH


