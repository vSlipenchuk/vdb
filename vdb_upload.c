#include <stdlib.h>
#include "vdb.h"


#include "../vos/vs0.c"
#include "../vos/strutil.c"
#include "../vos/coders.c"

int vdb_upload(database *db,char *filename, char *tablename,char* (*next_col)(char**) ) { // uploads tab delimited to tablename
char *d = strLoad(filename);
char *d2=0;
char *sql=0;
fprintf(stderr,"..importing file %s to table %s.\n",filename,tablename);
if (!d) {
    sprintf(db->error,"file load error");
    return 0;
    }
if (!tablename || !tablename[0]) {
  d2 = strNew(filename,-1);
  tablename=d2; char *p;
  while ( strchr(tablename,'/') ) {
     tablename=strchr(tablename,'/')+1;
     }
  p = strchr(tablename,'.'); if (p) *p=0;
  }
// ok - now need to load
char *n[200]; // column name
char *r = d;
char *c = get_row(&r); // get columns
int col_count = 0;
while(*c) {
    char *col_name=next_col(&c);
    col_name=trim(col_name);
    if (!col_name[0]) break;
    n[col_count]=col_name;
    col_count++;
    if (col_count==200) break; // max_column
    }
fprintf(stderr,"Found %d columns for a table %s\n",col_count,tablename);
int i;
strCat(&sql,"create table ",-1); strCat(&sql,tablename,-1); strCat(&sql,"(",-1);
for(i=0;i<col_count;i++) {
    strCat(&sql,n[i],-1);
    strCat(&sql," varchar(2000)",-1);
    strCat(&sql,(i==(col_count-1)?")":",") ,-1);
    }
printf("CreateSQL: %s\n",sql);
if (db_exec_once(db,sql) && db_commit(db)) {
   fprintf(stderr,"Table %s created OK\n",tablename);
   } else fprintf(stderr,"WARN: Table create %s failed %s\n",tablename,db->error);
// create insert SQL
strSetLength(&sql,0); strCat(&sql,"insert into ",-1); strCat(&sql,tablename,-1); strCat(&sql,"(",-1);
for(i=0;i<col_count;i++) { strCat(&sql,n[i],-1); strCat(&sql,(i==col_count-1?")values(":","),-1);}
for(i=0;i<col_count;i++) { strCat(&sql,":",-1); strCat(&sql,n[i],-1); strCat(&sql,(i==col_count-1?")":","),-1);}
fprintf(stderr,"insertSQL:%s\n",sql);
int cnt=0,err=0;
if (!db_compile(db,sql)) {
      fprintf(stderr,"CompileSQL failed: %s\n",db->error);
      return -1; // fail
     };
while(*r) { // show progress??? commit on some lines?
     char *row = get_row(&r);
     for(i=0;i<col_count;i++) {
        char *v = next_col(&row);
         db_bind(db,n[i],dbChar,0,v,strlen(v));
         //printf("V=%s\n",v);
        }
     if (db_exec(db)) cnt++; else err++;
     fprintf(stderr,"%d rows unloaded err=%d     \r",cnt,err);
     }
db_commit(db);
fprintf(stderr,"%d rows  imported , errors=%d   \n",cnt,err);
strClear(&d);
strClear(&d2);
strClear(&sql);
return 1; // ok
}
