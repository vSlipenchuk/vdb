#include "vdb.h"



#include "logger.c"
#include "std_sock.c"

/* compile with all need sources --- */
#include "exe.c"
#include "sock.c"
#include "httpSrv.c"
//#include "vos.c"
//#include "vs0.c"
#include "vss.c"

//#include "coders.c"


void strClear(uchar **str) { objClear((void*)str); }

int port = 80;
int logLevel = 1;
int keepAlive = 1;
int runTill = 0;
char *rootDir = "./";
char *mimes=".htm,.html=text/html;charset=utf8&.js=text/javascript;charset=utf8&.css=text/css";
int i,Limit=1000000;
httpSrv *srv ; // my server

SocketMap **dbmap; // Link to list of url->sql values

void http_addSQL(uchar *Name,uchar *SQL) {
//SocketMap *httpSrvAddMap(httpSrv *srv, uchar *Name, void *proc, void *Data) { // Такой вот хендлер
SocketMap *sm;
sm = SocketMapNew();
sm->name = strNew(Name,-1);
sm->page = vssCreate(sm->name,-1); // just copy ?
//sm->data = objAddRef(data);
//sm->onRequest = proc;
sm->data = strNew(SQL,-1); // data = SQL
// now - push it
SocketMapPush(&dbmap,sm);
qsort(dbmap,arrLength(dbmap),sizeof(void*),(void*)SocketMapSortDo);
//SocketMapClear(&sm);
return sm; // Return Self
}

//SocketMap *SocketMapFind(SocketMap **map, vss *page);//

extern database *db; // defined in vcon - need output to sting?

int dump_dataset2str(char **str,database *db,int mode);

int db_sql_process(database *db, vss p, char *sql,char **out) {
if (!sql) sql="";
int ok = db_compile(db,sql);
   printf("Compile=%d SQL:%s\n",ok,sql);
if (!ok) return 0;
   while(p.len>0) {
     vss v=vssGetTillStr(&p,"&",1,1);
     vss n=vssGetTillStr(&v,"=",1,1);
     n.data[n.len]=0; v.data[v.len]=0;
     printf("ARG %s VAL=%s\n",n.data,v.data);
     //int db_bind(database *db,char *name,int type,int *index,void *data,int len);
     if (!db_bind(db,n.data,dbChar,0,v.data,v.len)) printf("bind %s error %s\n",n,db->error);
     }
   ok = db_open(db) && db_exec(db);
   if (!ok) { printf("DBERR:%s\n",db->error); return 1;}

   ok = ok && dump_dataset2str(&srv->buf,db,2); // get jso
return 1; // ok
}

int onHttpSql(Socket *sock, vssHttp *req, SocketMap *map) {
strSetLength(&srv->buf,0); // ClearResulted
char *sql=req->B.data;
strSetLength(&srv->buf,0); // ClearResulted
int  ok = db_sql_process(db,req->args,req->B.data,&srv->buf);
 SocketSendHttp(sock,req, srv->buf,strlen(srv->buf)) ;
return 1; // OK - generated
}

int onHttpDb(Socket *sock, vssHttp *req, SocketMap *map) {
char buf[1024];
httpSrv *srv = sock->pool;
strSetLength(&srv->buf,0); // ClearResulted
SocketMap *m = SocketMapFind(dbmap,&req->page);
if (!m) SocketPrintHttp(sock,req,"404 - Request: page:<%*.*s> args:<%*.*s>",VSS(req->page),VSS(req->args));
  else {
   int ok = db_sql_process(db,req->args,m->data,&srv->buf);
   SocketSendHttp(sock,req, srv->buf,strlen(srv->buf)) ;
   }
return 1; // OK - generated
}



int onHttpStat(Socket *sock, vssHttp *req, SocketMap *map) {
char buf[1024];
httpSrv *srv = sock->pool;
strSetLength(&srv->buf,0); // ClearResulted
sprintf(buf,"{clients:%d,connects:%d,requests:%d,mem:%d,serverTime:'%s',pps:%d}",arrLength(srv->srv.sock)-1,
  srv->srv.connects,
  srv->srv.requests,
  os_mem_used(), szTimeNow,
  (srv->readLimit.pValue+srv->readLimit.ppValue)/2);
SocketPrintHttp(sock,req,"%s",buf); // Flash Results as http
return 1; // OK - generated
}





int vdb_http_start() {
net_init();
TimeUpdate();
srv = httpSrvCreate(0); // New Instance, no ini
srv->log =  srv->srv.log = logOpen("microHttp.log"); // Create a logger
srv->logLevel = srv->srv.logLevel = logLevel;
srv->keepAlive=keepAlive;
srv->readLimit.Limit = Limit;
IFLOG(srv,0,"...starting microHttp {port:%d,logLevel:%d,rootDir:'%s',keepAlive:%d,Limit:%d},\n   mimes:'%s'\n",
  port,logLevel,rootDir,keepAlive,Limit,
  mimes);
//printf("...Creating a http server\n");
srv->defmime= vssCreate("text/plain;charset=windows-1251",-1);
httpSrvAddMimes(srv,mimes);
//httpMime *m = httpSrvGetMime(srv,vssCreate("1.HHtm",-1));printf("Mime here %*.*s\n",VSS(m->mime));
//httpSrvAddFS(srv,"/c/","c:/",0); // Adding some FS mappings
httpSrvAddFS(srv,"/",rootDir,0); // Adding some FS mappings
httpSrvAddMap(srv, strNew("/.stat",-1), onHttpStat, 0);
httpSrvAddMap(srv, strNew("/.sql",-1), onHttpSql, 0);
httpSrvAddMap(srv, strNew("/.db",-1), onHttpDb, 0);

 //http_addSQL("/all_tables","select * from all_tables"); // {

if (httpSrvListen(srv,port)<=0) { // Starts listen port
   Logf("-FAIL start listener on port %d\n",port); return 1;
   }
Logf(".. listener is ready, Ctrl+C to abort\n");
if (runTill) srv->runTill = TimeNow + runTill;
//httpSrvProcess(srv); // Run All messages till CtrlC...
TimeUpdate();
//IFLOG(srv,0,"...stop microHttp, done:{connects:%d,requests:%d,runtime:%d}\n",
  //   srv->srv.connects,srv->srv.requests,TimeNow - srv->created);
}

int vdb_http_process() {
if (!srv) return 0;
  TimeUpdate(); // TimeNow & szTimeNow
  int cnt = SocketPoolRun(&srv->srv);
  //printf("SockPoolRun=%d time:%s\n",cnt,szTimeNow); msleep(1000);
  RunSleep(cnt); // Empty socket circle -)))
  if (srv->runTill && TimeNow>=srv->runTill) return -1; // stop loop'break; // Done???
return cnt;
 //httpSrvProcess();
}
