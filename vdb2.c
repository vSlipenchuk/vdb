#include "vdb.h"

int db_config_db(database *db) { // configure database by by a tables (nextN !!!)
if (db_select(db,"select TYP_SEQ from db where n = 0") && db_fetch(db)) { // Configure a db from db_info
    db_col *c = db->out.cols;
    db->typ_seq = db_int(c); // My Next N type
    }
return 0;
}


int db_nextN(database *db,char *table) {
if (db->typ_seq==2) {
     if (db_selectf(db,"select sq_%s.NextVal from dual",table) <0
          || db_fetch(db) <0) return -1;
     return db_int(db->out.cols);
     }
if (db_selectf(db,"select max(N) from %s",table)<0) return -1;
if (!db_fetch(db)) return 1; // NewOne
return db_int(db->out.cols)+1;
}

int db_compilef(database *db,uchar *fmt,...) {
uchar buf[1024];
BUF_FMT(buf,fmt);
//printf("compiling: %s\n",buf);
return db_compile(db,buf);
}
