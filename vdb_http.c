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
if (strncmp(sql,"select",6)!=0) { // return as static text
   strCat(out,sql,-1); // just flash as static text
   return 1; // OK
   }
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

int SocketSendHttpCode(Socket *sock, vssHttp *req, char *code, uchar *data, int len) {
char buf[1024];
vss reqID = {0,0};
if (req && req->reqID.len>0) reqID=req->reqID;
if (len<0) len = strlen(data);
sprintf(buf,"HTTP/1.1 %s\r\nConnection: %s\r\n%s: %*.*s\r\nContent-Length: %d\r\n\r\n",code,sock->dieOnSend?"close":"Keep-Alive",
    X_REQUEST_ID,VSS(reqID),len);
strCat(&sock->out,buf,-1); // Add a header
strCat(&sock->out,data,len); // Push it & Forget???
//printf("TOSEND:%s\n",sock->out);
sock->state = sockSend;
// Wait???
return 1;
}

char *Auth; //userpass="vovka:password;master:key";

int getUserId(char *UserPass) {
//printf("UP=%s Auth=%s\n",UserPass,Auth);
if (strLength(Auth)==0) return 1; // no auth - ok
vss A=vssCreate(Auth,-1);
vss UP=vssCreate(UserPass,-1);
while (A.len>0) {
  vss U = vssGetTillStr(&A,";",1,1);
  if (U.len==UP.len && memcmp(U.data,UP.data,U.len)==0) return 1; // found
  }
//printf("Not found\n");
return 0; // not found
}


int onHttpSql(Socket *sock, vssHttp *req, SocketMap *map) {
strSetLength(&srv->buf,0); // ClearResulted
char *sql=req->B.data;
strSetLength(&srv->buf,0); // ClearResulted
int  ok = db_sql_process(db,req->args,req->B.data,&srv->buf);
 SocketSendHttp(sock,req, srv->buf,strlen(srv->buf)) ;
return 1; // OK - generated
}
char *fileSQL = 0; // tmp for load SQL from a file

int onHttpDb(Socket *sock, vssHttp *req, SocketMap *map) {
char buf[1024];
httpSrv *srv = sock->pool;
strSetLength(&srv->buf,0); // ClearResulted
SocketMap *m = SocketMapFind(dbmap,&req->page);
//if (!checkAuth(sock,req)) return 1; // Done, no auth
if (!m) { // try find in local dir
   sprintf(buf,".db/%*.*s",VSS(req->page));
   printf("Find Local File: %s...\n",buf);
   strClear(&fileSQL);
   if (!strCatFile(&fileSQL,buf)) { // no local file
      SocketPrintHttp(sock,req,"404 - Request: page:<%*.*s> args:<%*.*s>",VSS(req->page),VSS(req->args));
      return 1;
      }
   printf("SQL loaded: %s\n",fileSQL);
   int ok = db_sql_process(db,req->args,fileSQL,&srv->buf);
   SocketSendHttp(sock,req, srv->buf,strlen(srv->buf)) ;
   }
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

int vdb_http_auth_set(char *auth) {
strClear(&Auth);
strCat(&Auth,auth,-1);
return 1;
}



int vdb_http_start() {
net_init();
TimeUpdate();
srv = httpSrvCreate(0); // New Instance, no ini
srv->log =  srv->srv.log = logOpen("microHttp.log"); // Create a logger
srv->logLevel = srv->srv.logLevel = logLevel;
srv->keepAlive=keepAlive;
srv->readLimit.Limit = Limit;
#ifdef HTTPSRV_AUTH
srv->realm = "my realm";
srv->auth = getUserId;
#endif // HTTPSRV_AUTH

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
httpSrvAddMap(srv, strNew("/.sql",-1), onHttpSql, 0); // debug SQL messages
httpSrvAddMap(srv, strNew("/.db",-1), onHttpDb, 0);   // map to local handlers

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
