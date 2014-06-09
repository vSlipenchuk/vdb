#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "vdb.h"
#include <ibase.h>
 
#define Version "3.0.2.0"
#define Description "Firebird 2.0 vdb Driver"
#define printf


typedef struct { // Специфические для ибазы свойства. Все храним в db->h
   void *dbh,*cur,*sql; // Коннектор к базе данных
   XSQLDA ISC_FAR *in,*out; // Список входящих и выходящих связанных переменных
   } ibase;


extern  char ib_trans[3];


int ib_connect();
int ib_disconnect();
int ib_compile();
int ib_open();
int ib_exec();
int ib_fetch();
int ib_blob_get();
int ib_blob_put();
int ib_bind();
int ib_commit();
int ib_rollback();

char *perfix();
int ib_init();


char *ib_params[]={
    "sysdate","sysdate()", // Текущая дата -)
    "all_tables"," name  from rdb$relations", // Все таблицы текущего пользователя
    "DATE_COL", "DATE",
    0,0}; 

vdb_func ib_fun = { // Статическая структура
   "FireBird", //   char *name; // Database Internal Name
   ib_connect, //int (*connect)() ;
   ib_disconnect, // int (*disconnect)();
   ib_fetch, //int (*fetch)();
   ib_exec, //int (*exec)();
   ib_compile, //int (*compile)();
   ib_open, //int (*open)();
   ib_commit, //int (*commit)();
   ib_rollback, //int (*rollback)();
   ib_bind, //int (*bind)();
   ib_params, //char **params; // Внешний буффер именованных параметров -)
   //int res[512]; // Для расширений -)
   } ;//vdb_func;





ib_init(database *db)
{
if (db) {
  db->fun = &ib_fun; // new one
  // copy old one -)))
  db->connect=ib_connect;
  db->disconnect=ib_disconnect;
  db->compile=ib_compile;
  db->open=ib_open;
  db->exec=ib_exec;
  db->fetch=ib_fetch;
  db->commit=ib_commit;
  db->rollback=ib_rollback;
  db->bind=ib_bind;
  
  }
  return 1;
}


char *prefix(){  return "ib_"; }

char ib_trans[3]=
{
  isc_tpb_version3,
  isc_tpb_read_committed,
  isc_tpb_no_rec_version
};


ib_error(database *db,ISC_STATUS *status_vector)
{
  long *pvector=status_vector; char *msg=db->error;
  isc_interprete(msg, &pvector);
  if(msg){ msg+=strlen(msg);  *msg=' '; msg++;}
  while(isc_interprete(msg,&pvector)){ msg+=strlen(msg); *msg=' '; msg++;}
  *msg=0;
  return 0;
}



_ib_connect(database *db,char *db_name,char *user_name,char *user_password)
{
  char dpb_buffer[256], *dpb, *p,*char_set="WIN1251";
  ISC_STATUS vector[20];
  short dpb_length;
  db->error[0]=0;
  dpb = dpb_buffer; *dpb++ = isc_dpb_version1;

  *dpb++ = isc_dpb_user_name;   *dpb++ = strlen(user_name);
  for (p = user_name; *p;)      *dpb++ = *p++;

  *dpb++ = isc_dpb_password;    *dpb++ = strlen(user_password);
  for (p = user_password; *p;)  *dpb++ = *p++;

  *dpb++ =isc_dpb_lc_ctype;     *dpb++ = strlen(char_set);
  for (p= char_set;*p;)         *dpb++ = *p++;

  dpb_length = dpb - dpb_buffer;

  if(isc_attach_database(vector,0, db_name, &db->h, dpb_length, dpb_buffer))
    return ib_error(db,vector);
  {
    XSQLDA ISC_FAR *d;
    d=malloc(XSQLDA_LENGTH(256));
    d->sqln=256;
    d->sqld=0;
    d->version=1;
    db->in.p=d;

    d=malloc(XSQLDA_LENGTH(256));
    d->sqln=256;
    d->sqld=0;
    d->version=1;
    db->out.p=d;
  }
  if(isc_dsql_allocate_statement(vector, &db->h, &db->s))
    return ib_error(db,vector);
  return 1;
}


int ib_connect(database *db,char *db_name,char *user_name,char *user_password)
{
int r;
r = _ib_connect(db,db_name,user_name,user_password);
if (r) ib_init(db); // Set functions...
return r;
}

ib_disconnect(database *db)
{
  ISC_STATUS vector[20];
  if (!db) return 0;
  if (db->c) isc_rollback_transaction(vector,&db->c);
  if(db->s) isc_dsql_free_statement(vector,&db->s,DSQL_close);
  Free(db->in.p);  
  Free(db->out.p); 
  Free(db->out.data);
  Free(db->out.blob);
  Free(db->out.cols);
  Free(db->in.data);
  Free(db->in.blob);
  Free(db->in.cols);
  if (db->h) isc_detach_database(vector,&db->h);
  db->s=0;
  db->c=0;
  db->h=0;
  return 1;
}

                      /*
ib_exec_once(TDatabase *db,char *sql)
{
ISC_STATUS vector[20]; XSQLDA *dd=db->o;
dd->sqld=0;
isc_dsql_free_statement(vector,&db->s,DSQL_close);
if(!db->c)
 if(isc_start_transaction(vector,&db->c,1,&db->h,sizeof(ib_trans),ib_trans))
       return ib_error(db,vector);
if(isc_dsql_execute_immediate(vector,&db->h,&db->c,0,sql,1,NULL))
 return ib_error(db,vector);
return 1;
}
                        */

_ib_commit(database *db)
{
  ISC_STATUS vector[20];
  if(isc_commit_transaction(vector,&db->c)) return ib_error(db,vector);
  if(isc_start_transaction(vector,&db->c,1,&db->h,sizeof(ib_trans),ib_trans))
       return ib_error(db,vector);
  return 1;
}

int ib_commit(database *db)
{
int r;
r = _ib_commit(db);
return r;
}


_ib_rollback(database *db)
{
  ISC_STATUS vector[20];
  if(isc_rollback_transaction(vector,&db->c)) return ib_error(db,vector);
  if(isc_start_transaction(vector,&db->c,1,&db->h,sizeof(ib_trans),ib_trans))
       return ib_error(db,vector);
  return ib_error(db,vector);
}

ib_rollback(database *db)
{
int r;
r = _ib_rollback(db);
return r;
}

/* ---------------------- OPEN AN IB CURSOR ------------------------*/


_ib_open(database *db)
{
  int i,nc;
  XSQLDA  ISC_FAR *out=db->out.p;
  XSQLVAR ISC_FAR *v;
  ISC_STATUS vector[20];
  nc=out->sqld; db->out.count=0; db->out.len=0;
  if(out->sqln<nc)
  {
    out->sqln=nc+DB_STEP_COL;
    db->out.p=out=realloc(out,XSQLDA_LENGTH(out->sqln));
  }
  if(isc_dsql_describe(vector,&db->s,1,out))  return ib_error(db,vector);
  for(i=0,v=out->sqlvar;i<nc;i++,v++)
  {
    int type,dbtype,scale;
    char szname[256];
    db_col *r;
    dbtype=v->sqltype&~1;
    scale=-v->sqlscale;
    switch(dbtype)
    {
      case SQL_LONG:    type=(scale)?dbDouble:dbInt;        break;
      case SQL_TEXT:
      case SQL_VARYING: type=dbChar;    break;
      case SQL_SHORT:   type=dbInt;     break;
      case SQL_FLOAT:
      case SQL_DOUBLE:  type=dbDouble;  break;
      case SQL_DATE:    type=dbDate; break;
      case SQL_BLOB:    type=dbBlob; break;
      default:
      {
        db->out.count=0;
        sprintf(db->error,"unknown interbase type #%d pos=%d",dbtype,i);
        return 0;
      }
    }
    //r=db_add_col(&db->out,v->sqlname,type,v->sqllen);
    {
    int len;
    len = v->sqlname_length;
    if (len)  {
       if (len>sizeof(szname)-1) len = sizeof(szname)-1;
       strncpy(szname,v->sqlname,len);
       szname[len]=0;
       } else { // try alias
       len = v->aliasname_length;
       if (len) {
         if (len>sizeof(szname)-1) len = sizeof(szname)-1;
         strncpy(szname,v->aliasname,len);
         szname[len]=0;
         } else {
         len = v->relname_length;
         if (len>sizeof(szname)-1) len = sizeof(szname)-1;
         strncpy(szname,v->relname,len);
         szname[len]=0;
         }
       }     
    len = v->sqllen;
    if (len<0) len = 64000; // Corrected for >32K
    r=db_add_col(&db->out,szname,type,len);
    }
    r->dbtype=dbtype;
    r->null=1;
    if(dbtype==SQL_VARYING) r->value+=2;
    if(type==dbInt || type==dbDouble) r->len=scale;
  }
  for(i=0,v=out->sqlvar; i<nc; i++,v++)
  {
    v->sqlind=(void*)&db->out.cols[i].null;
    v->sqldata=db->out.cols[i].dbvalue;
  }
  return 1;
}

int ib_open(database *db)
{
int r;
r = _ib_open(db);
return r;
}


int _ib_exec(database *db)
{
ISC_STATUS vector[20];XSQLDA  *p=NULL; db_col *c; int i; XSQLVAR ISC_FAR *v;
if(db->in.count>=0)
 {
 p=db->in.p;
 for(i=0,v=p->sqlvar;i<p->sqld;i++,v++)
  if(v->sqltype==SQL_TEXT+1)  v->sqllen=strlen(v->sqldata); // ZU - так у меня всегда c-строка?
 for(i=0,c=db->in.cols;i<db->in.count;i++,c++)
 switch(c->dbtype)
 {
 case SQL_DATE:
 case SQL_DATE+1:
  {
  int *l=(void *)c->dbvalue;
  double d=*(double*)c->value;
  if(d<678576) { c->null=0xFFFF;
     l[0]=l[1]=0;}
  else {l[0]=d-678576;l[1]=((d-(int)d)*86400*10000); c->null=0;
     }
  //printf("---ibDATE=%d,%d c->null=%d\n",l[0],l[1],c->null);
  break;
  }
 case SQL_BLOB:
  {
  db_blob_handle *b=(void*)c->value;
  if(!ib_blob_put(db,c->dbvalue,b->data,b->len)) return 0;
  }
  break;
 }}
if(isc_dsql_execute(vector,&db->c,&db->s,1,p)) return ib_error(db,vector);
return 1;
}

int ib_exec(database *db)
{
int r;
r = _ib_exec(db);
return r;
}



ib_blob_put(database *db,void *id,char *buf,int len)
{
isc_blob_handle   blob_handle=NULL;
ISC_STATUS        status[20];
int l;
if(isc_create_blob(status, &db->h, &db->c, &blob_handle, id))
           return ib_error(db,status);
while(len>0) {
 l=len; if(l>30*1024) l=30*1024; // No more than 30k
 if(isc_put_segment(status, &blob_handle,(unsigned short) l, buf))
                   return ib_error(db,status);
 buf+=l; len-=l;
 }
if(isc_close_blob(status, &blob_handle)) return ib_error(db,status);
return 1;
}

ib_blob_get(database *db,void *id,char **buf,int *size,int *len)
{
int blob_stat; int l=0;
unsigned short ISC_FAR blob_len;
char blob_segment[256];
ISC_STATUS status[20];
isc_blob_handle   blob_handle=NULL;
if(isc_open_blob(status, &db->h, &db->c, &blob_handle, id))
                            return ib_error(db,status);
while(1)
{
if(*len+DB_STEP_BUFFER>=*size)
     {
     *size=*len+DB_STEP_BUFFER;
     *buf=realloc(*buf,*size);
     }
blob_stat = isc_get_segment(status, &blob_handle,
                           (unsigned short ISC_FAR *) &blob_len,
                           DB_STEP_BUFFER-1, *buf+*len);
if(!(blob_stat == 0 || status[1] == isc_segment)) break;
*len+=blob_len;
}
isc_close_blob(status,&blob_handle);
(*buf)[*len]=0; *len=*len+1;
return 1;
}


_ib_fetch(database *db)
{
int i;
XSQLDA  ISC_FAR *out=db->out.p;
XSQLVAR ISC_FAR *v; db_col *r;
ISC_STATUS vector[20];
db->err_code=0; db->out.blen=0;
for(i=0,r=db->out.cols;i<db->out.count;i++,r++) r->null=0;
if(i=isc_dsql_fetch(vector,&db->s,1,out))
 {
 db->err_code=(i!=100L);
 if(db->err_code) ib_error(db,vector);
 //db->out.count=0;
 return 0;
 }
for(i=0,r=db->out.cols;i<db->out.count;i++,r++)
 {
/* r->null=*((short*)&r->null);*/
 if(!r->null) switch (r->dbtype)
  {
  case SQL_VARYING:
                r->value[*(unsigned short*)(r->value-2)]=0; break;
  case SQL_SHORT:
         *(int*)r->value=*(short*)r->value; break;
  case SQL_LONG:
        if(r->len)
         {
         int i,factor;double num;
         factor=1;
         for(i=1;i<r->len;i++) factor*=10;
         num=*(long*)r->value;
         *(double*)r->value=num/factor;
         }
         break;
  case SQL_FLOAT:
         *(double*)r->value=*(float*)r->value; break;
  case SQL_DATE:
        {
        int *l;
        l=(void*)r->value;
        *(double*)r->value=l[0]+678576+l[1]/(86400.*10000.);
        }
        break;
 case  SQL_BLOB:
       {
       char *p; int j,shift,plen;
       db_blob_handle *bh;
       p=db->out.blob; plen=db->out.blen;
       bh=(void*)r->value;
       if(!ib_blob_get(db,r->dbvalue,
             &db->out.blob,&db->out.bsize,&db->out.blen))
             {
             db->out.count=0;
             db->err_code=1;
             return 0;
             }
       if((shift=db->out.blob-p)!=0)
       for(j=0;j<i;j++) if(db->out.cols[j].type==dbBlob)
            ((db_blob_handle*)(db->out.cols[j].value))->data+=shift;
       bh->size=-1;
       bh->len=db->out.blen-plen-1;
       bh->data=db->out.blob+plen;
       break;
       }
 }}
return 1;
}

int ib_fetch(database *db)
{
int r;
r = _ib_fetch(db);
return r;
}



/* - COMPILING & PRECOMPILING ---------------*/

txt_cmp(char *name, int nl, char *patt)
{
  int i;
  if(!name) return 0;
  for(i=0; i<nl && patt[i]; i++)
    if(toupper(name[i])!=toupper(patt[i])) return 0;
  return patt[i]==0 && i==nl;
}

int txt_shift(char *p, char s)
{
  int l;
  p++; l=1;
  while(*p && *p!=s){ p++;l++;}
  if(*p==s){ p++;l++;}
  return l;
}

char *txt_next(char **s, int *len, char *del)
{
  unsigned char *p=*s; int l=0,ll,sl=0;
  *len=0; if(!p) return 0;
  while(*p && *p<=32) {p++;sl++;}
  if(!*p) return 0;
  while(*p)
  {
    int i;
    for(i=0,ll=1; del[i]; i++) if(*p==del[i]){ ll=0;break;};
    if(!ll) break;
    switch(*p)
    {
      case '"': case '\'': ll=txt_shift(p,*p); break;
      default:  ll=1;
    }
    if (!*p) break;
    l+=ll; p+=ll;
  }
  p=*s+sl;  *s=*s+l+sl;  if(ll==0) *s=*s+1;
  while(l>0 && p[l-1]<=32) l--;
  *len=l; return p;
}



_ib_compile(database *db,unsigned char *p)
{
  int no_precompile;
  ISC_STATUS vector[20];
  XSQLDA *dd=db->out.p;
  dd->sqld=0;  ((XSQLDA *)(db->in.p))->sqld=0;
  db->in.count=0; db->in.blen=0;
  no_precompile=(strnicmp(p,"CREATE",6)==0)||(strnicmp(p,"ALTER",5)==0);
  if(no_precompile) db_add_blob((void*)&db->in.bsize,p,-1);
  else
  {
    int l,lp; char *n;
    while(*p)
    {
      switch(*p)
      {
        case '"': case '\'': l=txt_shift(p,*p); break;
        case ':': l=lp=0; n=p+1;
           while(n[lp] && n[lp]>32 && !strchr(",)=><+-*/",n[lp])) lp++;
           break;
        default: l=1;
      }
      if(l>0){ db_add_blob((void*)&db->in.bsize,p,l); p+=l; }
      else
      {
        db_col *c=0; int i; XSQLDA  ISC_FAR *in=db->in.p;
        XSQLVAR ISC_FAR *v;
        db_add_blob((void*)&db->in.bsize,"?",1);
        for(i=0,c=db->in.cols; i<db->in.count; i++,c++)
          if(txt_cmp(n,lp,c->name)) break;
        if(i==db->in.count)  c=db_add_col_(&db->in,n,lp,0,16);
        if(in->sqld+1>=in->sqln)
        {
          in->sqln=in->sqld+DB_STEP_COL;
          db->in.p=in=realloc(in,XSQLDA_LENGTH(in->sqln));
        }
        v=in->sqlvar+in->sqld; (in->sqld)++;
        v->sqllen=-1;
        v->sqltype=-1;
        strncpy(v->sqlname,c->name,20); v->sqlname[20]=0;
        p+=lp+1;
      }
    }
  }
  isc_dsql_free_statement(vector,&db->s,DSQL_close);
  if(!db->c)
    if(isc_start_transaction(vector,&db->c,1,&db->h,sizeof(ib_trans),ib_trans))
      return ib_error(db,vector);
  //printf("ibSQL=<%s>\n",db->in.blob);
  if(isc_dsql_prepare(vector,&db->c,&db->s,0,db->in.blob,1,dd))
      return ib_error(db,vector);
  return 1;
}

int ib_compile(database *db,char *sql)
{
int r;
r = _ib_compile(db,sql);
return r;
}


/*
typedef struct { // Определитель "колонка"
     char *name;
     int null,len;
     int  type,dbtype;
     char *value,*dbvalue;
     } db_col;
*/

int always_null=0xFFFFFFFF;

_ib_bind(database *db, char *name, int type, int *ind, void *data, int len)
{
  db_col *c; int dbtype;
  XSQLDA  ISC_FAR *in=db->in.p;
  XSQLVAR ISC_FAR *v;
  ISC_STATUS vector[20];
  static short always_not_null=0;
  int i,j,l,found=0;

  

  l=strlen(name);  if(name[0]==':'){ name++; l--;};
  for(i=0,v=in->sqlvar; i<in->sqld; i++,v++)
  if(txt_cmp(name,l,v->sqlname))
  {
    found++;
    for(j=0,c=db->in.cols; j<db->in.count; j++,c++)
      if(txt_cmp(name,l,c->name)) break;
    if(j==db->in.count)
    {
      sprintf(db->error,"Bind internal cash error");
      return 0;
    }
    c->dbflag=type >> 16; type = type & 0xFF;
    switch (type)
    {
      case dbInt:    dbtype=SQL_LONG+1;   len=sizeof(int);    break;
      case dbDouble: dbtype=SQL_DOUBLE+1; len=8; break;
      case dbChar:   dbtype=SQL_TEXT+1;   
                     if (len<0) c->dbflag|=DB_STR_AUTOLEN;
                     len=0;break;
      case dbDate:   dbtype=SQL_DATE+1;   len=8;  break;
      case dbBlob:   dbtype=SQL_BLOB;     len=sizeof(ISC_QUAD);  break;
      default:
      {
        sprintf(db->error,"bind interbase type unknown #%d",type);
        return 0;
      }
    }
    c->value=data; c->dbtype=dbtype; c->type=type; c->len=len;
    v->sqldata=((type==dbBlob)||(type==dbDate))?c->dbvalue:c->value;
    v->sqltype=dbtype;
    v->sqllen=len;
    if (c->dbflag & 1) {
         v->sqlind = (void*)&always_null;
         } else {
         v->sqlind=(short*)&c->null; c->null=0; //(ind)?(void*)ind:&always_not_null;
         }
    v->sqlscale=0;
  }
  if(found==0)
  {
    sprintf(db->error,"ib: parametr %s undefined",name);
    return 0;
  }
  return 1;
}


int ib_bind(database *db,char *name, int type, int *ind, void *data, int len)
{
int r;
r = _ib_bind(db,name,type,ind,data,len);
return r;
}
