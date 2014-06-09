

#include "vdb.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef MSWIN
#include "windows.h"


int dbregkey(char *name,char *buf,int sz) {
  HKEY K1,K2; int t;
  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,"SOFTWARE",0,KEY_READ,&K1)!=0) return 0;
  t=RegOpenKeyEx(K1,"VDB",0,KEY_READ,&K2);
  RegCloseKey(K1); if(t) return 0;
  t=RegQueryValueEx(K2,name,0,(void*)&t,buf,(void*)&sz);
  RegCloseKey(K2);
  if(t) return 0; // some error
  return 1; // ok
}

int db_connect_reg2(database *db, char *name) {
  char buf[80];
  db_done(db);
  if (!dbregkey(name,buf,sizeof(buf))) {
        sprintf(db->error,"registry2 database %s not found",name);
        return 0;
        };
return db_connect_string(db,buf);
}

int db_connect_reg(database *db, char *name) { return db_connect_reg2(db,name);}

#endif


#include <dlfcn.h>
#include <link.h>


#define LoadLibrary(A) dlopen(A,RTLD_NOW)
#define GetProcAddress(A,N) dlsym(A,N)


void* db_getproc();

database *db_new() { // Creates new database instance ...
  database *r=malloc(sizeof(database));
  if(!r) return 0;
  memset(r,0,sizeof(database));
  return r;
}

char *db_sysdate(database *db) {
char **n;// = (void*)db->fun;
if (db->fun) for(n=db->fun->params ;n[0];n+=2) if (strcmp(n[0],"sysdate")==0) return n[1];
return "sysdate"; // def
}


typedef struct {
	char name[20];
	void *lib;
	int (*connect)();
	} vdb_dll;

vdb_dll vdbdll[10]; // MaxVdbDllCash ...
int      vdbdll_count; // Count of used dll

int vdb_static(void* lib,char *name,int (*connect)()) { // Copy static prog ...
vdb_dll *d;
d=vdbdll+vdbdll_count;
if (vdbdll_count>=10) return 0; // No more
d->lib = lib;
d->connect=connect;
strcpy(d->name,name);
vdbdll_count++;
return 1;
}

int db_init(database *db,char *module) { // Inits &
void *(*p)(),*prefix,*lib; vdb_dll *d;
char szconnect[100];
int i , cashed = 1;
if (!module) { // Clear cash
     for(i=0,d=vdbdll;i<vdbdll_count;i++,d++) if (d->lib) dlclose(d->lib);
     vdbdll_count=0;
     return 0;
     }
if (module[0]=='!') { module++; cashed=0;}
if (cashed) for(i=0,d=vdbdll;i<vdbdll_count;i++,d++) if (strcmp(d->name,module)==0) {
     //printf("Set proc for lib %s to %d ora_connect=%d\n",module,d->connect,ora_connect);
     db->connect = d->connect; // Copy connect procedure ...
     return 1; // Ok, inited ...
     }
//printf("Try load lib %s\n",module);
lib=dlopen(module,RTLD_NOW);
if (!lib) { sprintf(db->error,"vdb load library %s failed",module); return 0;}
p =(void*)GetProcAddress(lib,"prefix");
if (!p) {  // No prefix function!!!
	    dlclose(lib);
            sprintf(db->error,"vdb %s invalid module",module);
            return 0;
           }
prefix=p(2);// Get prefix by VDB function
sprintf(szconnect,"%sconnect",(char*)prefix);
p = (void*)GetProcAddress(lib,szconnect);
if (!p) {  // No connect function!!!
	    dlclose(lib);
            sprintf(db->error,"vdb %s inconsistent module",module);
            return 0;
           }
if (cashed) vdb_static(lib,module,(void*)p); // Copy static prog ...
	  else db->lib =lib; // No cashed - owns a lib...
db->connect = (void*) p; // Remember in a database handle
return 1;
}



int db_done(database *db) {
  t_blob b = db->buf; // Сохраняем карман ...
  if(db->h)   db_disconnect(db);
  if(db->lib) dlclose(db->lib);
  Free(db->out.data);
  Free(db->out.blob);
  Free(db->out.cols);
  Free(db->in.data);
  Free(db->in.blob);
  Free(db->in.cols);
  memset(db,0,sizeof(database));
  db->buf = b; // Восстанавливаем карман
  return 1;
}


int db_release(database *db) { //
if (!db) return 0;
db_done(db);
if (db->buf.data) free(db->buf.data); // Освобождаем "карман"
free(db);
return 0;
}

db_blob_handle db_blob(db_col *c) {
db_blob_handle bh;
switch (c->type) {
case dbBlob: bh = *((db_blob_handle*)c->value); break;
case dbChar: bh.data=db_text(c); bh.len=strlen(bh.data); break;
default: bh.data=0; bh.len=0;
}
return bh;
}


int db_int(db_col *r) { // Отдать целове значение ...
  int ret=0; t_blob *e;
  if(r->null) return 0;
  switch(r->type)
  {
    case dbInt:  return *(int*)r->value;
    case dbDouble:
    case dbDate: return (int)(*(double*)(r->value));
    case dbChar: sscanf(r->value,"%d",&ret);   return ret;
    case dbBlob:
	    e = ((db_blob_handle*)(r->value));
            if (!e || e->len==0) return 0;
	    sscanf(e->data,"%d",&ret);
	    return ret;
  }
  return 0;
}


double db_double(db_col *r) {
  double ret=0;
  if(r->null) return 0;
  switch (r->type)
  {
    case dbInt:  return *(int*)r->value;
    case dbDouble:
    case dbDate: return *(double*)(r->value);
    case dbChar: sscanf(r->value,"%lf",&ret);   return ret;
  }
  return 0;
}

// db1.c


int db_nofunc(database *db, void *f, char *name) {
  if(!f){  sprintf(db->error,"vdb %s - function undefined",name);  return 1; }
  return 0;
}


int db_connect(database *db, char *host, char *user, char *pass) {
  if(db_nofunc(db,db->connect,"connect")) return 0;
  return db->connect(db,host,user,pass);
}


int db_disconnect(database *db) {
  if(db_nofunc(db,db->disconnect,"disconnect")) return 0;
  return db->disconnect(db);
}


int db_open(database *db) {
  if(db_nofunc(db,db->open,"open")) return 0;
  return db->open(db);
}


int db_compile(database *db,char *sql) {
  db->out.count=db->in.count=0; // Clears out buffer
  db->out.len=db->in.len=0;  // Used size in a buffers
  if(db_nofunc(db,db->compile,"compile")) return 0;
  return db->compile(db,sql);
}


int db_exec(database *db) {
  if(db_nofunc(db,db->exec,"exec")) return 0;
  return db->exec(db);
}


int db_exec_once(database *db, char *sql) {
  return db_compile(db,sql) && db_exec(db);
}


int db_fetch(database *db) {
  int ok;
  if(db_nofunc(db,db->fetch,"fetch")) return 0;
  ok = db->fetch(db);
  if (!ok && !db->error[0]) sprintf(db->error,"NO DATA FOUND");
  return ok;
}


int db_select(database *db, char *sql) {
  if(!db_compile(db,sql)) return 0;
  if(!db_open(db)) return 0;
  if (!db_exec(db)) return 0;
  return 1;
}


int db_selectf(database *db, char *fmt, ...) {
  char B[1024];
  va_list va;
  va_start(va,fmt);
  vsnprintf(B,sizeof(B)-1,fmt,va); B[sizeof(B)-1]=0; // !!!UAU!!!
  va_end(va);
//  printf("SELECTF:<%s>\n",B);
  return db_select(db,B);
}


int db_execf(database *db, char *fmt, ...) {
  char B[1000];
  int r,auto_commit;
  va_list va;
  va_start(va,fmt);
  vsnprintf(B,sizeof(B)-1,fmt,va);
   va_end(va);
  fmt=B;
  if (*fmt=='!') {auto_commit=1; fmt++;} else auto_commit=0;
  r=db_exec_once(db,fmt);
  if (!r) return 0;
  if (auto_commit) db_commit(db);
  return r;
}

int db_commit(database *db) {
  if(db_nofunc(db,db->commit,"commit")) return 0;
  return db->commit(db);
}


int db_rollback(database *db) {
  if(db_nofunc(db,db->rollback,"rollback")) return 0;
  return db->rollback(db);
}


int db_bind(database *db, char *name, int type, int *index, void *data, int len) {
  if(db_nofunc(db,db->bind,"bind")) return 0;
  return db->bind(db,name,type,index,data,len);
}


// --  Additional connect functions...


int db_connect4(database *db, char *dll, char *server, char *user, char *pass) {
  if(!db_init(db,dll)) return 0; return db_connect(db,server,user,pass);
}


int db_connect_string(database *db, char *str) {
  char *p,*s,*d,u[80];
	//printf("Connecting %s\n",str);
#ifdef MSWIN
  if (str[0]=='.') return db_connect_reg2(db,str+1);
#endif
  strncpy(u,str,79); u[79]=0;
  db_done(db);
  if(!(s=strchr(u,'@'))){ sprintf(db->error,"- server undefined");   return 0;}
  *s=0; s++;
  d=strchr(s,' '); if (!d) d=strchr(s,'#');
  if(!d) { if (vdbdll_count>0) d=vdbdll[0].name; // Default - first loaded
                 else {  sprintf(db->error,"- dll undefined");      return 0;}}
    else {   *d=0; d++; }
  if(!(p=strchr(u,'/'))){ sprintf(db->error,"- password undefined"); return 0;}
  *p=0; p++;
  return db_connect4(db,d,s,u,p);
}

// Date&Time functions ...


// Blob functions ...

char *db_prepare_blob(db_blob_handle *b,int size) {
  if(b->len+size+1>=b->size)
  { int sz=b->len+size+DB_STEP_BUFFER; void *p;
    p =realloc(b->data,sz);
    if (!p) return 0;
    b->size = sz; b->data=p;
  }
  return b->data+b->len;
}


char *db_add_blob(db_blob_handle *b,void *buf,int size) {
  char *p;
  if (size<0) {if (!buf) size=0; else size=strlen(buf);}
  p=db_prepare_blob(b,size);
  if (!p) return 0;
  if(buf) memcpy(p,buf,size); else  memset(p,0,size);
  b->len+=size; b->data[b->len]=0;
  return p;
}


int   db_free_mem(char *p) {
if (p) free(p);
return 0;
}

int db_free_blob(db_blob_handle *b) {
  if(b->data) free(b->data);
  b->size=0;
  b->len =0;
  b->data=0;
  return  0;
}


int  db_add_blob_(db_blob_handle *b,void *buf,int size) {
  char *r;
  r=db_add_blob(b,buf,size);
  return (char*)r-(char*)b->data;
}

// Column blob functions ...

#define ALIGN 16

char *db_buf(db_cols *c, int len)
{
  char *p;
  if(c->count==0) c->len=0;
  if (len%ALIGN ) len+=ALIGN-( len % ALIGN ) ;
  if(c->len+len>=c->size)
  {
    db_col *r;  int i,shift;
    p=c->data;
    c->size=c->len+len+DB_STEP_BUFFER;
    c->data=realloc(p,c->size);
    if (!c->data) printf("DB: no memory...\n");
    shift=c->data-p;
    for(i=0,r=c->cols; i<c->count; i++,r++)
    {
      r->name=r->name+shift;
      r->value=r->value+shift;
      r->dbvalue=r->dbvalue+shift;
    }
  }
  memset(c->data+c->len,0,len);
  p=c->data+c->len;
  c->len+=len;
  return p;
}


db_col *db_add_col(db_cols *c, char *name, int type, int len) {
  return db_add_col_(c,name,-1,type,len);
}


db_col *db_add_col_(db_cols *c, char *name, int nl, int type, int len) {
  db_col *r;
  if(c->count+1 >= c->capacity) { // Проверка колонок ...
     int size = c->count+DB_STEP_COL; void *p;
     p=realloc(c->cols, sizeof(db_col)*size);  //## нет контроля
     if (!p) return 0;
     c->cols=p; c->capacity= size;
     memset(c->cols+c->count, 0, sizeof(db_col)*DB_STEP_COL);
  }
  r=c->cols+c->count; c->count++;
  r->len=len;
  r->type=type;
  if(len<16) len=16;
  if(type==dbChar) len+=16;
  else if(type==dbBlob) len+=sizeof(db_blob_handle);
  if(nl<0) nl=strlen(name);
  r->name=db_buf(c,nl+1);
  memcpy(r->name,name,nl);  r->name[nl]=0;
  //printf("SetValue=%s\n",r->name);
  r->value=r->dbvalue=db_buf(c,len);
  if(type==dbBlob) r->dbvalue+=sizeof(db_blob_handle);
  return r;
}


//! Text functions ...
char *db_number_text_buf(char *sz_buf, double d) {
  char *p=sz_buf; int i,p1=0,lz;
  sprintf(sz_buf,"%.10lf",d);

  for(i=0,lz=0; p[i]; i++)
  {
    if(p[i]=='.'){ p1=i; lz=1; }
    if(lz && p[i]!='0') p1=i;
  }
  if(lz)  // . was found
  {
    if(p[p1]!='.') p1++;
    p[p1]=0;
  }
  p=sz_buf;
  while(*p==' ') p++;
  return p;
}


char *db_number_text(double d) {
  static char szbuf[100];
  return db_number_text_buf(szbuf,d);
}


char *db_text_out(database *db, int ncol) {
  if(ncol<0 || ncol>=db->out.count)  return "";
  return db_text(db->out.cols+ncol);
}


char *db_text_buf(char *sz_text, db_col *r) { // Возвращает текст в буфер...
double d;
  sz_text[0]=0;
  if(r->null) return "";
  switch(r->type)   {
    case dbInt:    sprintf(sz_text,"%d",*(int*)(r->value));  break;
    case dbDouble:
      d=*(double*)r->value;
      if(r->len>0 && r->len<=10)
         sprintf(sz_text,"%.*lf",r->len,d);
      else return db_number_text_buf(sz_text,d);
      break;
    case dbDate:
    {
      int Year,Month,Day,Hour=0,Min=0,Sec=0;
      dt_decode(*(double*)r->value,&Year,&Month,&Day,&Hour,&Min,&Sec);
      sprintf(sz_text,"%02d.%02d.%04d %02d:%02d:%02d",
                        Day,Month,Year,Hour,Min,Sec);
      if((!Hour)&&(!Min)&&(!Sec)) sz_text[10]=0;
      break;
    }
    case dbBlob:  return ((db_blob_handle*)(r->value))->data;
    case dbChar:     {
      int l=r->len;  unsigned char *p=r->value+r->len;
      while(l>0 && p[-1]<=32){ p--; l--; };  // єфрыхэшх яЁюсхыют ёяЁртр
      *p=0;
      return r->value;
    }
  }
  return sz_text;
}


char *db_text_buf_n(char *sz_text, int size, db_col *r) {
  sz_text[0]=0;
  if(r->null) return "";
  switch(r->type)
  {
    case dbInt:    sprintf(sz_text,"%d",*(int*)(r->value)); break;

    case dbDouble:
      if(r->len>0 && r->len<=10)
        sprintf(sz_text,"%.*lf",r->len,*(double*)r->value);
      else return db_number_text(*(double*)(r->value));
      break;

    case dbDate:
    {
      int Year,Month,Day,Hour=0,Min=0,Sec=0;
      dt_decode(*(double*)r->value,&Year,&Month,&Day,&Hour,&Min,&Sec);
      sprintf(sz_text,"%02d.%02d.%04d %02d:%02d:%02d",
                        Day,Month,Year,Hour,Min,Sec);
      if((!Hour)&&(!Min)&&(!Sec)) sz_text[10]=0;
      break;
    }
    case dbBlob: return ((db_blob_handle*)(r->value))->data;

    case dbChar:
    {
      int l=r->len;  unsigned char *p=r->value+r->len;
      // ZUZUKA - трим неверно работает в статических буферах !!!
      while(l>0 && p[-1]<=32){ p--; l--; };  // єфрыхэшх яЁюсхыют ёяЁртр
      *p=0;
      return r->value;
    }
  }
  return sz_text;
}

char *db_text(db_col *r) { // Returns text using statical buffer ...
  static char sz_text[200];
  return db_text_buf(sz_text,r);
}


typedef struct { char *fmt; int Y[6]; } t_scan_fmt;

t_scan_fmt scan_fmts[]={
   {"%d.%d.%d %d:%d:%d", {2,1,0,3,4,5} },
   {"%04d%02d%02d%02d%02d%02d",{0,1,2,3,4,5}},
   {"%d-%d-%d %d:%d:%d", {0,1,2,3,4,5}},
   {"%d-%d-%d-%d:%d:%d", {0,1,2,3,4,5}},
   {"%d-%d-%d.%d.%d.%d", {0,1,2,3,4,5}},
   {0}};


date_time dt_scanf(char *B,int len) {
int S[6];
t_scan_fmt *fmt;
for(fmt=scan_fmts;fmt->fmt;fmt++) {
     memset(S,0,sizeof(int)*6);
     if (sscanf(B,fmt->fmt,S+fmt->Y[0],S+fmt->Y[1],S+fmt->Y[2],S+fmt->Y[3],S+fmt->Y[4],S+fmt->Y[5])<3) continue;
     if (S[0]>0 && S[0]<3000 && S[1]>=1 && S[1]<=12 && S[2]>=1 && S[2]<=31 &&
         S[3]>=0 && S[3]<=24 && S[4]>=0 && S[4]<=60 && S[5]>=0 && S[5]<=60) {
         double d = dt_encode(dt_year4(S[0]),S[1],S[2],S[3],S[4],S[5]);
         //printf("fired fmt=%s year=%d\n",fmt->fmt,S[0]);
         if (d) return d;
         }
     }
return 0;
}









