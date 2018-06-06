#include "vos.h"

#include "vdb.h"
#include "stdio.h"

//#include "ora.c"

//#define TESTMODULE extern int dbo_connect(); printf("--->TEST DBO CONNECTOR\n"); vdb_static(0,"vodbc",dbo_connect);

database *db = 0;

uchar *get_word(uchar **str);
int lcmp(uchar **str,uchar *cmp);
uchar *get_col(uchar **row);

int vdb_upload(database *db,char *filename, char *tablename,char* (*next_col)(char**) ); // .import handler

int prn_help() {
fprintf(stderr,"vdbcon <database> [command] ...\n"
".help           Show this message\n"
".quit           Exit this program\n"
".import F T     Uploads file F to table T\n"
".mode csv|text  Set import/output format\n"
".output F       Output select results to a file\n"
".stress         Stress test (reconnects & 10 select * from test)\n"
".compile        try compile this SQL\n"
".btest		 test bind for select * from dual where Dummy = :txt\n"
".reconnects     Start reconnect leack test\n"
".sql ...    compile & exec sql"
"select ....     executes a select\n");
return 0;
}

//select name FROM my_db.sqlite_master WHERE type='table';
//select name FROM sqlite_master WHERE type='table';

int vdb_upload(database *db,char *filename, char *tablename,char* (*next_col)(char**) );
//char *get_col(char **);

uchar *get_till(uchar **data,uchar *del,int dl);

char *get_csv_col(char **row) {
char *r = get_till(row,",",1); // split it
int l = strlen(r);
if (l>=2 && r[0]=='\"' && r[l-1]=='\"') { r[l-1]=0; r++; }
return r;
}

char cs[1024]; // Last success reconnect
int mode = 0;  //  TEXT:0,CSV:1


time_t TimeNow;

int do_stress_fetch(char *sql) {
int m0, m1, m2 = 0, i,p1,p2;
time_t t1,t2;
m0 =  m1 = os_mem_used();
time(&TimeNow); t1=t2 = TimeNow;
p1=p2=0;
//int do_smth=0;
fprintf(stderr," start stress_fetch with mem %d  KB, SQL:%s\n", m1/1024,sql);
for(i=0;i<100*10000;i++) {
 int skip;
 time(&TimeNow);
 if (!db_select(db,sql)) {
   fprintf(stderr,"-select failed on step %d err=%s\n",i,db->error);
   return 0;
   }
 p2++;
 while(db_fetch(db)) ; //p1++; // Fetch all
 m2 = os_mem_used(); t2 = TimeNow;
 skip = t2!=t1; // New Second
 if (skip || (i%100 ==0))
   printf(" .. stress_fetch: %d mem_used: %d KB  delta= %d Bytes time=%d fetched:%d \r",i,m2/1024, (m2-m1), (int)(t2-t1),(p2-p1));
 if (skip) {
      m1=m2; t1 = t2; p1=p2; printf("\n");
      }
 }
fprintf(stderr,"+%d stress fetch done, leak: %d\n",i, (m2-m0));
return 0;
}

int do_reconnects(char *sql) {
int m0, m1, m2 = 0, i;

//sql="select table_name from all_tables";

m0 =  m1 = os_mem_used();
//int do_smth=0;
//fprintf(stderr," start reconnects with mem %d  KB\n", m1/1024);
for(i=1;i<100*1000;i++) {
 db_done(db); // clear prev
 if (!db_connect_string(db,cs)) {
   fprintf(stderr,"-reconnect failed on %d step err=%s\n",i,db->error);
   msleep(1000); continue;
   //return 0;
   }
if (sql) {
 if (!db_select(db,sql)) {
   fprintf(stderr,"-select failed on step %d err=%s\n",i,db->error);
   return 0;
   }
 //while(db_fetch(db)); // Fetch all
 }
 m2 = os_mem_used();
 printf(" .. reconnect: %d mem_used: %d KB  delta= %d Bytes          \r",i,m2/1024, (m2-m1));
 if (i%100==0) { m1=m2; printf("\n"); }
 }
fprintf(stderr,"+%d reconnects done, leak: %d\n",i, (m2-m0));
return 0;
}

int do_binds(char *sql) {
char *txt="X";
int ok;
ok = db_compile(db,sql) && db_bind(db,":txt",dbChar,0,txt,-1) && db_open(db) && db_exec(db);
if (!ok) {
  printf("compile failed: %s, SQL:%s\n",db->error,sql);
  }
ok  =  db_fetch(db);
printf("compiled ok, fetch = %d\n",ok);
if (!ok) return 0;
printf("text:'%s'\n",db_text(db->out.cols));
return 1;
}

int fcsv_text(FILE *f, char *t) { // change-> ", -> to spaces
fputc('"',f);
while(*t) {
 if (strchr("\",",*t)) fputc(' ',f);
      else fputc(*t,f);
 t++;
 }
fputc('"',f);
return 1;
}

int csv_cat(char *t,int (*cat)(),void *p) { // change-> ", -> to spaces
cat(p,"\"",1);
while(*t) {
 if (strchr("\",",*t)) cat(p," ",1);
      else cat(p,t,1);
 t++;
 }
cat(p,"\"",1);
return 1;
}

int dump_dataset_cb(database *db, int mode,int (*cat)(),void *p ) { // mode = 0=Text, 1 - csv
db_col *c;
int row=0,i; char txtbuf[80];
c = db->out.cols;
switch (mode) {
case 0: // TEXT
    for(i=0;i<db->out.count;i++,c++) { cat(p,c->name,-1); cat(p,(i+1<db->out.count?"\t":"\n"),1); }
    while(db_fetch(db)) {
       c = db->out.cols;
       for(i=0;i<db->out.count;i++,c++) {cat(p,db_text_buf(txtbuf,c),-1); cat(p,(i+1<db->out.count?"\t":"\n"),1);}
       //fprintf(f,"%s%s",db_text_buf(txtbuf,c),
       row++;
       }
       break;
case 1: // CSV
 for(i=0;i<db->out.count;i++,c++) {
        //fcsv_text(f,c->name); fprintf(f,"%s",i+1<db->out.count?",":"\n"); // ZZZU
        csv_cat(c->name,cat,p); cat(p,(i+1<db->out.count?",":"\n"),1);
        }
    while(db_fetch(db)) {
       c = db->out.cols;
       for(i=0;i<db->out.count;i++,c++) {
         //fcsv_text(f,db_text_buf(txtbuf,c)); fprintf(f,"%s",i+1<db->out.count?",":"\n");
         csv_cat(db_text_buf(txtbuf,c),cat,p); cat(p,(i+1<db->out.count?",":"\n"),1);
         }
       row++;
       }
       break;
}
return row;
}

int dump2file(FILE *f,char *data,int len) {
if (len<0) len=strlen(data);
return fwrite(data,len,1,f);
}

int _dump2str(char **s,char *data,int len) {
strCat(s,data,len);
return 1;
}

int dump_dataset2str(char **str,database *db,int mode) {
return dump_dataset_cb(db,mode,_dump2str,str);
}


int dump_dataset(FILE *f, database *db, int mode) { // mode = 0=Text, 1 - csv
db_col *c;
int row=0,i; char txtbuf[80];
c = db->out.cols;
if (!f) f=stdout;

return dump_dataset_cb(db,mode,dump2file,f);
/*


switch (mode) {
case 0: // TEXT
    for(i=0;i<db->out.count;i++,c++) fprintf(f,"%s%s",c->name,i+1<db->out.count?"\t":"\n");
    while(db_fetch(db)) {
       c = db->out.cols;
       for(i=0;i<db->out.count;i++,c++) fprintf(f,"%s%s",db_text_buf(txtbuf,c),
         i+1<db->out.count?"\t":"\n");
       row++;
       }
       break;
case 1: // CSV
 for(i=0;i<db->out.count;i++,c++) {
        fcsv_text(f,c->name); fprintf(f,"%s",i+1<db->out.count?",":"\n");
        }
    while(db_fetch(db)) {
       c = db->out.cols;
       for(i=0;i<db->out.count;i++,c++) {
         fcsv_text(f,db_text_buf(txtbuf,c)); fprintf(f,"%s",i+1<db->out.count?",":"\n");
         }
       row++;
       }
       break;
}
return row;
*/
}

FILE *output=0; // default for output

int process(char *buf) {
unsigned char *p=buf; int ok;
if (strncmp(buf,".connect",7)==0) {
    p=buf+7; while(*p && *p<=32) p++;
    fprintf(stderr," ...connecting to <%s>\n",p);
    ok = db_connect_string(db,p);
    if (ok) fprintf(stderr,"+connected\n");
      else fprintf(stderr,"-err: %s\n",db->error);
    return 1;
    }
if (lcmp(&p,".mode")) {
      unsigned char *m=get_word(&p);
      if ( lcmp(&m,"csv"))  { mode=1; fprintf(stderr,"+mode  csv now\n"); return 1;}
      if ( lcmp(&m,"text")) { mode=0; fprintf(stderr,"+mode  text now\n"); return 1;}
      fprintf(stderr,"ERR: mode %s unknown\n",m);
      return 2;
      }
if (lcmp(&p,".http")) { // start http server
      int code = vdb_http_start();
      fprintf(stderr,"+server started code=%d\n",code);
      return 1;
      //vdb_http_process();
      }
if (lcmp(&p,"url")) {
      char *u = get_word(&p); // rest is SQL
      http_addSQL(u,p);
      fprintf(stderr,"url %s added to map\n",u);
      return 1;
    }
if (strcmp(buf,".help")==0) { prn_help(); return 1;}
if (strcmp(buf,".reconnects")==0) { do_reconnects(0); return 1;}
if (strcmp(buf,".stressFetch")==0) { do_stress_fetch("select* from test"); return 1;}
if (strcmp(buf,".stress")==0) { do_reconnects("select * from test"); return 1;}
if (strcmp(buf,".btest")==0) { do_binds("select * from dual where dummy = :txt"); return 1;}
if (lcmp(&p,".output")) {
    output = fopen(p,"wt");
    if (!output) { fprintf(stderr,"cant open file %s",p);}
    return 1;
    }
//if (strcmp(buf,".btest")==0) { do_binds("select * from email where sender = :txt"); return 1;}
if (strcmp(buf,".quit")==0) exit(0);
if (strcmp(buf,".rollback")==0) {
     printf("rollback code = %d\n",db_rollback(db)); return 1;
    }
if (strcmp(buf,".commit")==0) {
     printf("commit code = %d\n",db_commit(db)); return 1;
    }
if (strncmp(buf,".compile",8)==0) {
    char *sql = buf+8;
    fprintf(stderr," ...compiling sql <%s>\n",sql);
    if (!db_compile(db,sql)) fprintf(stderr,"-err compile: %s\n",db->error);
      else fprintf(stderr,"+ok compiled\n");
    return 1;
    }
if (lcmp(&p,".import")) {
     char *file = get_word(&p);
     char *tbl  = get_word(&p);
     if (mode==0) return vdb_upload(db,file,tbl,get_col);
        else return vdb_upload(db,file,tbl,get_csv_col);

    }
if (strncmp(buf,".sql",4)==0) {
    char *sql = buf+4; int ok;
    fprintf(stderr," ...compile&exec sql <%s>\n",sql);
    ok = db_compile(db,sql) && db_exec(db);
    if (!ok) fprintf(stderr,"-err : %s\n",db->error);
      else fprintf(stderr,"+ok execed\n");
    return 1;
    }
if (strncmp(buf,".desc",5)==0) {
    char *sql = buf+5; int ok,i;
    fprintf(stderr," ...describe sql <%s>\n",sql);
    ok = db_compile(db,sql) && db_open(db);
    if (!ok) { fprintf(stderr,"-err : %s\n",db->error); return 1; }
    for(i=0;i<db->out.count;i++) {
        db_col *c = db->out.cols+i;
        printf(" %2d. NAME:%-20s TYP:%d LEN:%d DBTYPE:%d\n",i,c->name,c->type,c->len,c->dbtype);
        }
    fprintf(stderr,"+ok %d columns\n",i);
    return 1;
    }
if (strncmp(buf,"select",6)==0) {
    int row=0;
    fprintf(stderr," ...selecting sql <%s>\n",buf);
    if (!db_select(db,buf)) { fprintf(stderr,"-err: %s\n",db->error); return 1;}
    fprintf(stderr,"begin output=%p mode=%d\n",output,mode);
    row = dump_dataset( output, db, mode);
    if (output) { fclose(output); output=0; }
    fprintf(stderr,"+%d rows selected\n",row);
    return 1;
    }
fprintf(stderr,"-command unknown '%s'\n",buf);
return 1;
}

int sq_connect(database *db , char *host, char *user, char *pass);
int vdb_static(void* lib,char *name,int (*connect)());

#include "vos_linux_kbhit.c"

int vdbcon_main(int npar,char **par) {
//unsigned char *p; int ok;

//vdb_static(0,"sqlite",sq_connect); // test for sqlite
vdb_static(0,"ora",ora_connect); // test for oracle

char buf[4*1024],sbuf[200]; int i;
#ifdef TESTMODULE
TESTMODULE
#endif
if (npar<2) { prn_help(); return 1;}
db = db_new(); strcpy(cs,par[1]);
if (!db_connect_string(db,cs)) {
  fprintf(stderr,"-ErrOnConnect: %s\n",db->error);
  return 2;
  }
fprintf(stderr,"+vdb connected '%s'\n",cs);
for(i=2;i<npar;i++) {
 strcpy(buf,par[i]);
 if (!buf[0]) exit(0);
 process(buf);
 }
buf[0]=0;
while(1) {
 int l;
 if (buf[0]) fprintf(stderr,">"); else fprintf(stderr,"vdb>");
 sbuf[0]=0;
 while (!kbhit()) { if (vdb_http_process()<1) msleep(100); }
 if (!fgets(sbuf,sizeof(sbuf),stdin)) break; // EOF
 if (!sbuf[0]) break;
 l=strlen(sbuf);
 while(l>0 && (strchr("\r\n",sbuf[l-1]) )) l--;
 if (buf[0] ==0 && l==0) break; // empty line in a middle
 if ((sbuf[0]=='.') && (buf[0]==0)) { // one line command
    sbuf[l]=0;
    process(sbuf);
    }
 else {
    l = strlen(buf)+strlen(sbuf);
    if (l+1>=sizeof(buf)) {
       fprintf(stderr,"too long command max:%ld\n",sizeof(buf));
       exit(2);
       }
    strcat(buf,sbuf);
    while(l>0 && (strchr("\r\n",buf[l-1]) )) l--; // rtrim
    //printf("NEWBUF:%s\n",buf);
    if ( l > 0 && buf[l-1]==';') { // ok - last here
       buf[l-1]=0;
       process(buf);
       buf[0]=0;
       }
    }
 }
return 0;
}
