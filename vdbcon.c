#include "vos.h"

#include "vdb.h"
#include "stdio.h"

//#include "ora.c"

//#define TESTMODULE extern int dbo_connect(); printf("--->TEST DBO CONNECTOR\n"); vdb_static(0,"vodbc",dbo_connect);

database *db = 0;

int prn_help() {
fprintf(stderr,"vdbcon <database> [command] ...\n"
".help           Show this message\n"
".quit           Exit this program\n"
".reconnects     Start reconnect leack test\n"
".upload F T     Uploads file F to table T\n"
".stress         Stress test (reconnects & 10 select * from test)\n"
".compile        try compile this SQL\n"
".btest		 test bind for select * from dual where Dummy = :txt\n"
".sql ...    compile & exec sql"
"select ....     executes a select\n");
return 0;
}

int vdb_upload(database *db,char *filename, char *tablename);

char cs[1024]; // Last success reconnect


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
   printf(" .. stress_fetch: %d mem_used: %d KB  delta= %d Bytes time=%d fetched:%d \r",i,m2/1024, (m2-m1), (t2-t1),(p2-p1));
 if (skip) {
      m1=m2; t1 = t2; p1=p2; printf("\n");
      }
 }
fprintf(stderr,"+%d stress fetch done, leak: %d\n",i, (m2-m0));
return 0;
}

int do_reconnects(char *sql) {
int m0, m1, m2 = 0, i;
m0 =  m1 = os_mem_used();
//int do_smth=0;
//fprintf(stderr," start reconnects with mem %d  KB\n", m1/1024);
for(i=0;i<100*1000;i++) {
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
 printf(" .. reconnect: %d mem_used: %d KB  delta= %d Bytes  \r",i,m2/1024, (m2-m1));
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



int process(char *buf) {
unsigned char *p; int ok;
if (strncmp(buf,".connect",7)==0) {
    p=buf+7; while(*p && *p<=32) p++;
    fprintf(stderr," ...connecting to <%s>\n",p);
    ok = db_connect_string(db,p);
    if (ok) fprintf(stderr,"+connected\n");
      else fprintf(stderr,"-err: %s\n",db->error);
    return 1;
    }
if (strcmp(buf,".help")==0) { prn_help(); return 1;}
if (strcmp(buf,".reconnects")==0) { do_reconnects(0); return 1;}
if (strcmp(buf,".stressFetch")==0) { do_stress_fetch("select* from test"); return 1;}
if (strcmp(buf,".stress")==0) { do_reconnects("select * from test"); return 1;}
if (strcmp(buf,".btest")==0) { do_binds("select * from dual where dummy = :txt"); return 1;}
//if (strcmp(buf,".btest")==0) { do_binds("select * from email where sender = :txt"); return 1;}
if (strcmp(buf,".quit")==0) exit(1);
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
if (strncmp(buf,".upload",6)==0) {
     vdb_upload(db,trim(buf+6),0); //"mcc");
     return 1; // Anyway
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
    db_col *c; int i, row=0; char txtbuf[80];
    fprintf(stderr," ...selecting sql <%s>\n",buf);
    if (!db_select(db,buf)) { fprintf(stderr,"-err: %s\n",db->error); return 1;}
    c = db->out.cols;
    for(i=0;i<db->out.count;i++,c++) printf("%s%s",c->name,i+1<db->out.count?"\t":"\n");
    while(db_fetch(db)) {
       c = db->out.cols;
       for(i=0;i<db->out.count;i++,c++) printf("%s%s",db_text_buf(txtbuf,c),
         i+1<db->out.count?"\t":"\n");
       row++;
       }
    fprintf(stderr,"+%d rows selected\n",row);
    return 1;
    }
fprintf(stderr,"-command unknown '%s'\n",buf);
return 1;
}

int sq_connect(database *db , char *host, char *user, char *pass);

int vdbcon_main(int npar,char **par) {
//unsigned char *p; int ok;

vdb_static(0,"sqlite",sq_connect); // test for sqlite

char buf[1024]; int i;
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
while(1) {
 printf("vdb>");  buf[0]=0; gets(buf);
 if (!buf[0]) break;
 process(buf);
 }
return 0;
}
