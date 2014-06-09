// Раппер на Oracle Open Call Interface vdb.1.0

// Включен дебаг-мод!!!
#include <windows.h>
#include <stdlib.h> // Диспетчер памяти...
#include <oci.h> // Тут весь оракл лежит... (должна быть в INCLUDE-PATH)
#include <ocidem.h> // Симолические константы описаний типов...

//#include "vdb.h" // Ссылка на структуры vdb (должна быть в INCLUDE-PATH)
#include "vdb.c"


#define debugf printf
//#define debugf /*IF NO DEBUG */



#define Version "3.0.11.3"
#define Description "Oracle 10g vdb driver"

#define csid_csfrm ,0,0 /* Need for LobRead till 10i*/
char szVersion[] = Version;
#define MAX_CLOB 300*1024 /* Max Alloc for select clob fields*/

/*
 Глюкало -  проблемы с БЛОБ write ПОСЛЕ блоб read....
 "Подглючки" - не вытаскивается реальная длина VARCHAR, считаю что 4к.

 ToDo:
  Перетащить под Оракл&Linux ...
  Протестить под Oracle работу в многих потоках ...
  Оттестировать работу с "флажками" is_null - вход-выход...

 Версии:
 3.0.11.1 - ORA-03114 - теперь переконнечивается -)))
 3.0.7.3 - blob - в ora_bind заменена переинициализация CreateTemp...
 3.0.7.2 - cblob -> varchar 100*1024
 3.0.7.1 - при compile "сбрасывается" blob_handle
 3.0.7.0 - переконнект ora92, починка его ликаджа ...
 3.0.6.4 - очищение флага connected при 3113 (eof on channel) и 12560 (TNS adapter error)
 3.0.6.2 - вытаскивание DATA при ora_exec
 3.0.6.0 - вытаскивание блоба при out-параметрах
 3.0.5.0 - mutex on ORA_CONNECT
 3.0.4.0 - Server Attach
 3.0.3.0 - trim BLOB
 3.0.2.0 - тоже при фетче (OCI_SUCCESS_WIDTH_INFO)
 3.0.1.0 - OCI_SUCCESS+OCI_SUCCES_WITH_INFO -> ora_ok()
 3.0.0.7 - trim(db->error) Зачем Оракл туда вписывал \r\n?
 DONE - 10.00 28.11.2002 - 3.0.0.6 - blob in (select)
 DONE??? 3.0.0.5 - blob out (insert update) 20-44 CreateTempBLOB !!! NoLeak?
 DONE 3.0.0.4 - bind - Типа все? тест перекодировки данных... 18-16
 DONE - 3.0.0.3 - open + fetch (datatype?) - начало - 11-19 - DONE 17-40.
   Проблемы с длиной varchar2()? - заглушка = 4000
   DONE - 14-30. перекодировка дат? после фетча...
   DONE Проблемы с агргатными функциями count(*) - Oracle фетчит первую запись!
 3.0.0.2 -  (еще час) - compile&exec&rollback&commit - TEST PASSED
 27 nov 3.0.0.1 ( 2 часа) - connect&disconnect&ora_error - tested NO LEAK.

 Тестирование:
 1. Leackage connect&select(blob)&disconnect - много раз...
 2. Leack со связ переменными (bind) & exec

*/



typedef struct  {
  OCISession *authp; // Нужна для установки "авторизации" на сессию
  OCIServer  *srvhp;
  OCISvcCtx *svchp;  // Собственно курсор, на котором идет работа...
  OCIStmt   *stmt;   // Скомпилированное предложение SQL
  OCIEnv *envhp;     // Сессионный обьект - набор параметров
  OCIError *errhp;   // Обьект типа "ошибка"
  OCIBind *defs[256];   // Дефайнеры для SELECT колонок(подвязывание)
  OCIDefine *defs2[256]; // Начиная с 10
  OCIDescribe *dschp;// Обьектик для получения SELECT-DEFINE
  OCIParam     *mypard; // Обьектик для получения информациипо отселекченой колонке
  int rownum;           // Номер строки, сбрасывается при open, увеличивается fetch
  int execnum;          // Сбрасывается compile, устанавливается exec
  OCILobLocator *blob;  // Для блобов нужен
  void *p; // Memory for enviroment...
  //OCILobLocator *blob_read; // Еще один - для чтения отдельно...
  } t_ora;

#define blob_read blob

// Первая функция (инициализация либы), котороя вызовется при подьеме модуля


int   ora_mutex = 0; OCIEnv *env=0; // Глобальные - на все случаи жизни ...

#ifdef _DLL
int mutex_create()
{
  CRITICAL_SECTION *cs;
  cs=malloc(sizeof(CRITICAL_SECTION));
  if(!cs) return 0;
  memset(cs,0,sizeof(CRITICAL_SECTION));
  InitializeCriticalSection(cs);
  return (int)cs;
}


int mutex_lock(u_int mutex){
  if(!mutex) return 0;
  EnterCriticalSection((void*)mutex);
  return 1;
}


int mutex_unlock(u_int mutex) {
  if(!mutex) return 0;
  LeaveCriticalSection((void*)mutex);
  return 1;
}

int mutex_destroy(int mutex) {
  CRITICAL_SECTION *cs=(void*)mutex;
  if (!cs) return 0;
  DeleteCriticalSection(cs);
  free(cs);
  return 0;
}

char *prefix(int version)  {
if (!ora_mutex)
 { int cnt=0; int mode = /*OCI_DEFAULT*/OCI_THREADED;
 //return 0;
 ora_mutex = mutex_create();
 //printf("mutex created!\n");
   // Без этой штуки Oracle не хочет работать в потоках!
 if(1) (void) OCIInitialize((ub4)mode, (dvoid *)0,
                       (dvoid * (*)(dvoid *, size_t)) 0,
                      (dvoid * (*)(dvoid *, dvoid *, size_t))0,
                     (void (*)(dvoid *, dvoid *)) 0 );
                     //OCI_UTF16ID
 //OCIEnvNlsCreate(&env, mode , 0 , 0,0,0,0,0,OCI_UTF16ID,OCI_UTF16ID);  //my_malloc,my_realloc,	my_free,0,0); //&o->p);
 OCIEnvCreate(&env, mode , 0 , 0,0,0,0,0); //,OCI_UTF16ID,OCI_UTF16ID);  //my_malloc,my_realloc,	my_free,0,0); //&o->p);
 debugf(" created OCI:{env:0x%p,mode:%d,prefix:'_ora'\n",env,mode);
 }
return "ora_";
}

BOOL WINAPI DllMain(HINSTANCE hinstDll, DWORD fdwReason, PVOID fImpLoad) {
switch (fdwReason)
{
case DLL_PROCESS_ATTACH: // DLL проецируется на адресное пространство процесса
	prefix(0); //inits a process ...
break;
case DLL_THREAD_ATTACH: // создается поток
break;
case DLL_THREAD_DETACH: // поток корректно завершается
break;
case DLL_PROCESS_DETACH: // DLL отключается от адресного пространства процесса
	if (ora_mutex) {
		mutex_destroy(ora_mutex);
                //OCIHandleFree((dvoid *)o->envhp, OCI_HTYPE_ENV);
		//printf("Clean up...\n");
		//OCITerminate(OCI_DEFAULT);
		}
 break;
}
return(TRUE);
// используется только для DLL_PROCESS_ATTACH
}

#endif

#define getch()


// Заполняет текст последней ошибки в db->error
int ora_error(database *db)
{
t_ora *o = db->h;
int len, i , ecode;
unsigned char *err;
debugf("ORA_ERROR on database... try extract text");
//return 0;
//printf("ORA ERROR!!"); getch();
if (!o)  sprintf(db->error,"vora - no oracle connection");
 else OCIErrorGet((dvoid *)o->errhp, (ub4) 1, (text *) NULL, &db->err_code,
         db->error, (ub4) sizeof(db->error), OCI_HTYPE_ERROR );
//printf("ORA ERROR CODE=%d!!",db->err_code); getch();
if (db->err_code == 3113 || db->err_code == 12560 || db->err_code==12571 || db->err_code==3114) { // Clear connection flags
  db->connected = 0;
  }
err = db->error; len = strlen(err); err+=len;
//printf("Ora error length=%d\n",len);
for ( i=2; i<5 ; i++) { // Не работает - ничего кроме первого не вытаскивается !!!
	     int l;
	     OCIErrorGet((dvoid *)o->errhp, (ub4) i, (text *) NULL, &db->err_code,
	     err, (ub4) (sizeof(db->error) - len-1), OCI_HTYPE_ERROR );
             l = strlen(err);
	     //printf("Ora error length=%d\n",l);
	     len+= l; err+=l;
             }
*err=0; // Terminate it !!!
//printf("ORA ERROR TEXT=%s!!",db->error); getch();
return 0;
}

int ora_ok(int code)
{
return code==OCI_SUCCESS || code==OCI_SUCCESS_WITH_INFO;
}

#define ORA_NULL 0xFFFFFFF
// Перекодировка is_null, in bind vars, закачка блобов и потом ora_exec
int ora_exec(database *db) {
t_ora *o = db->h;
db_col *c;
int i;
if (!o) return ora_error(db);
if (!o->execnum)
 {
 o->execnum = 1;
 if (!ora_check_bind(db)) return 0; // Если первый раз - подвязать переменные...
 }
debugf(" ..ora10 - check bind ok!\n");
o->execnum++; // Номер запуска exec...
for(i=0,c=db->in.cols;i<db->in.count;i++,c++) if ((c->dbflag & 2)==0) { // Все ин-параметры
   if(c->type==dbDate) // Перекодируем дату
   {
    unsigned char *D=(void*)c->dbvalue; double uval = *(double*)c->value;
    int Year,Month,Day,Hour,Min,Sec;
    if (!uval) c->null=ORA_NULL;
    else {
    c->null=0;
    dt_decode(uval,&Year,&Month,&Day,&Hour,&Min,&Sec);
    D[0]=100+Year/100; D[1]=Year%100+100;
    D[2]=Month; D[3]=Day; D[4]=Hour+1; D[5]=Min+1; D[6]=Sec+1;
    }
    //printf("ORA uval=%lf\n",uval); getch();
  }
  else if (c->type == dbBlob && (!(c->dbflag & 2))) { // Закачиваем блобы...
  db_blob_handle *b = (void*) c->value; // Странно, но я храню блоб тута...
  int cnt_write = 0, offset = 0, code;
  cnt_write = b->len; // Сколько записывать...
  code = OCILobTrim(o->svchp, o->errhp, o->blob,0);
  //printf("2LobTrimmed data=%d len=%d olob=%d code=%d!\n",b->data,b->len,o->blob,code);
  c->null = ORA_NULL;
  if (b->data && b->len>0)
   {
   c->null = 0;
   if ( !ora_ok(db->err_code = OCILobWrite(o->svchp, o->errhp, o->blob,
                  &cnt_write, 1,
                 (dvoid *) b->data, (ub4) b->len, OCI_ONE_PIECE,
                   0, 0 csid_csfrm) )) {
             debugf("Fuck! cant write blob err_code=%d ND=%d!\n",db->err_code,OCI_NEED_DATA);
            //if (db->err_code!=-2) // ZU?
             return ora_error(db);
           }
   debugf("vora- ok write %d bytes of lob\n",b->len);
   }
  }
  }
db->err_code = OCIStmtExecute(o->svchp, o->stmt, o->errhp, (ub4) 1, (ub4) 0,
               (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL,OCI_DEFAULT);
debugf("ORARES on exec = %d\n",db->err_code);
if (db->err_code == OCI_NO_DATA) return 1; // ExecOK, данные не сфетчились...
if (!ora_ok(db->err_code)) return ora_error(db); // А это уже ошибка...
debugf("ORA - decode out\n");
//return 1; // Далее - идет обратная декодировка. Пока не отлаживаем
for(i = 0, c = db->in.cols;i<db->in.count; i++, c++) if (c->dbflag & 2) { // Смотрим на аут-параметры
    int is_null;
     debugf("----ORA --- chek OUT HERE=%s\n",c->name);
    //printf("NULL=%d on %s\n",*(short*)c->null,c->name);
    //is_null = c->null && ((*(short*)(c->null)) );
    //if (c->null) *(int*)(c->null) = is_null;
    //printf("Check in var=%s is_null = %d val=%d\n",c->name,is_null,*(int*)c->value);

    //if (is_null) continue;
    if ( c->type==dbDate) {
      unsigned char *D=(void*)c->dbvalue;
      if (c->null) *(double*)c->value=0;
          else *(double*)c->value=dt_encode((D[0]-100)*100+(D[1]-100),
                                    D[2],D[3],D[4]-1,D[5]-1,D[6]-1);
    }
  else if (c->type==dbBlob) { // Вытягиваем блобу...
    db_blob_handle *b=(void*)c->value; // Это указатель на "вытащенный блоб", его нужно установить на...
    int cnt_read = 0, offset = 0, blob_length = 0;
    debugf("ORA --- chek out blob null==%d\n",c->null);
    if (c->null) {b->len=0; continue;}
	  /// OCILobLock ???
    //db->err_code = OCILobOpen ( o->svchp, o->errhp, o->blob_read, OCI_LOB_READONLY );
    //printf("Lob opened error = %d\n", db->err_code);
    //if (db->out.size<1) {
	//    db->out.data = realloc(db->out.data,1);
	   // db->out.size = 1;
	   // }
    //OCILobRead(o->svchp,o->errhp,o->blob_read,  // Интересно, такое бывает?
      //&cnt_read, 1, db->out.blob, 1 , 0, 0); // Just for FUN ...
       // printf("Read = %d bytes\n", cnt_read );

    if (!ora_ok(db->err_code=OCILobGetLength (o->svchp,o->errhp,o->blob_read, &blob_length)))
    {      debugf("Fail get blob length code=%d result=%d!\n",db->err_code,blob_length);
	    blob_length = 100* 1024 ;
	    // return 1; // ZUZUKA ora_error(db);
    }
    //printf("Getting out blob len = %d..\n",blob_length);
    // - Сюда сливаются БЛОБЫ от SELCETA --  db->out.blob,&db->out.bsize,&db->out.blen
    if (blob_length +1 >= db->out.bsize)
      {
      // Еще место нужно... Realloc...
      db->out.bsize = blob_length+1024; // Новая длина
      db->out.blob = realloc(db->out.blob,db->out.bsize); // Думаем что памяти достаточно
      }
   if (!ora_ok(db->err_code=OCILobRead(o->svchp,o->errhp,o->blob_read,  // Интересно, такое бывает?
      &cnt_read, 1, db->out.blob, db->out.bsize, 0, 0  csid_csfrm)))
        {
	//printf("Fail read blob err=%d!\n");
        return ora_error(db);
	}
   //OCILobTrim(o->svchp, o->errhp, o->blob_read,0);
   db->out.blen = cnt_read+1; // Zero termanited на всякий случай...
   //printf("Read blob len = %d bh=%d\n",cnt_read,b);
   // Корректируем указатель, т.к. блоб мог реаллокнуться..
   b->data = db->out.blob; // Все заточено под один БЛОБ
   b->len  = cnt_read;
   b->data[b->len] = 0; // Терминирование нулем - вещь полезная...
    }
  }
return 1;
}

// Это просто коммит...
int ora_commit(database *db) {
t_ora *o = db->h;
if (!o) return ora_error(db);
if (!ora_ok(OCITransCommit(o->svchp,o->errhp, 0))) return ora_error(db);
return 1;
}

// Это просто роллбак...
int ora_rollback(database *db) {
t_ora *o = db->h;
if (!o) return ora_error(db);
if (!ora_ok(OCITransRollback(o->svchp,o->errhp, 0))) return ora_error(db);
return 1;
}

in_command(unsigned char *cmd,char *rep) {
while(*cmd && *cmd<=32) cmd++; // ltrim;
if (strcmp(cmd,"version")==0) strcpy(rep,szVersion);
 else if (strcmp(cmd,"tns")==0) { // edit tnsnames.ora
	 HKEY K1,K2; int t,l; char tns[512],buf[1024];
         if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,"SOFTWARE",0,KEY_READ,&K1)!=0) return 0;
         t=RegOpenKeyEx(K1,"Oracle",0,KEY_READ,&K2);
	 RegCloseKey(K1); if(t) return 0;
         l=79; t=RegQueryValueEx(K2,"ORACLE_HOME",0,&t,buf,&l);
         RegCloseKey(K2);
         sprintf(tns,"notepad.exe %s\\network\\admin\\tnsnames.ora",buf);
	 system(tns);
	 strcpy(rep,buf);
	 }
 else strcpy(rep,"unknown command");
return 0;
}

// "Подготовка" (compile, parsing, preparing) SQL для выполнения по exec...
int ora_compile(database *db, char *sql) {
t_ora *o = db->h;
if (!o) return ora_error(db);
db->err_code = 0; o->execnum = 0; db->in.count = 0; // Все почистили...
if (strncmp(sql,"vovka",5)==0)
  {
  in_command(sql+5,db->error);
  return 0;
  }
//OCILobTrim(o->svchp, o->errhp, o->blob,0);
if (!ora_ok(OCIStmtPrepare(o->stmt, o->errhp, (text *) sql,
			 (ub4) strlen(sql),
			 (ub4) OCI_NTV_SYNTAX, (ub4) OCI_DEFAULT))) return ora_error(db);
return 1;
}

// Все делается перед первым exec. Этот bind только запоминает имена переменных
int ora_bind(database *db, char *name, int type, int *ind, void *val, int len) {
  db_col *c; int ptype = type;
  t_ora *o = db->h;
  type = ptype&0xFF; // Игнорируем флаги
  if (!o) return ora_error(db); // Если еще нет оракла...
  c=db_add_col(&db->in,name,type,len);
  if (!c) return 0; // No memory ???
  c->dbflag = ptype>>16;
  //printf("col=%s db_flag=%d\n",c->name,c->dbflag);
  //c->null=(int)ind;
  c->value=val;
  c->null = 0; // Не нулевое значение указателя
  db->in.p=(void*)1;
  return 1;
}


// Вызывается перед первым эхеком...
int ora_check_bind(database *db) {
  int typ,len,i;
  db_col *c; void *val;
  t_ora *o = db->h;
  if(db->in.count==0) return 1; // Нету переменных, OK.
  for(i=0,c=db->in.cols; i<db->in.count; i++,c++)   {
    val=(void*)c->value; c->dbvalue = c->value; // by default -)))
    switch (c->type)
    {
      case dbInt:     typ=INT_TYPE;    len=sizeof(int); break;
      case dbDouble:  typ=SQLT_FLT;  len=sizeof(double); break;
      case dbChar:    typ=STRING_TYPE; len=c->len+1; break;
      case dbDate:    typ=DATE_TYPE; // Придется декодировать...
                      len=7; val=c->dbvalue; //c->value=(void*)c->dbtype;
                      break;
      case dbBlob:    val = c->name[0]!=':'?&o->blob_read:&o->blob;
                      if (1) { int code; ub4 lobEmpty = 0;
//                             if (o->blob) { OCIDescriptorFree(o->blob, (ub4) OCI_DTYPE_LOB); o->blob=0;}
                             if (o->blob) { OCIDescriptorFree(o->blob, (ub4) OCI_DTYPE_LOB); o->blob=0;}
                      OCIDescriptorAlloc((dvoid *) o->envhp, (dvoid **) &o->blob,
                           (ub4) OCI_DTYPE_LOB, (size_t) 0, (dvoid **) 0);
                      OCILobCreateTemporary(o->svchp,o->errhp,o->blob,
                        OCI_DEFAULT,OCI_DEFAULT,OCI_TEMP_BLOB,FALSE,OCI_DURATION_SESSION);
                            //if( code=OCILobCreateTemporary(o->svchp, o->errhp, o->blob,
                           //(ub2)0, SQLCS_IMPLICIT,
                           //OCI_TEMP_BLOB, OCI_ATTR_NOCACHE,
                           //OCI_DURATION_SESSION) )
  //{
    // (void) printf("FAILED: CreateTemporary() code=%d \n",code);
    // return 0;
  //}
                           //   code=OCIDescriptorAlloc((dvoid *) o->envhp, (dvoid **) &o->blob,
                                    //     (ub4) OCI_DTYPE_LOB, (size_t) 0, (dvoid **) 0);
                            //  printf("Created with code=%d\n",code);
                             // code=OCIAttrSet((dvoid *) o->blob, (ub4) OCI_DTYPE_LOB,
                              //    &lobEmpty, sizeof(lobEmpty),
                               //  (ub4) OCI_ATTR_LOBEMPTY, o->errhp);
                              //if (o->blob)
                              len = 0; typ = SQLT_BLOB;
                              //printf("init oblob=%d code=%d\n",o->blob,code);
                              code = OCILobTrim(o->svchp, o->errhp, o->blob,0);
  //printf("LobTrimmed %d  code=%d!\n",o->blob,code);

                      }
                       len = 0; typ = SQLT_BLOB;
                      break;
      default:        sprintf(db->error,"VORA-bind: unknown type %d pos=%d",c->type,i);
                      return 0;
    }
  //rintf("Start bind var %s\n",c->name);
  if (!ora_ok(db->err_code = OCIBindByName(o->stmt,&o->defs[db->out.count+i],
         o->errhp, (text *) c->name, -1,
              val, len, typ,  (short*)&c->null,
             (ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT)))
  {
	  ora_error(db);
	  strcat(db->error," on bind ");
	  strcat(db->error,c->name);
	  return 0;
   }
  //rintf("End bind %s\n",c->name);
  }
  return 1;
}


// После фетча - перекодируем date и  выкачиваем blob.s
int ora_fetch(database *db) {
 t_ora *o = db->h; int i; db_col *c;
//printf("Here fetch!"); getch();
if (!o) return ora_error(db);
//printf("Try real  fetch!, OCI_NO_DATA=%d",OCI_NO_DATA); getch();
if (++o->rownum>1) i = OCIStmtFetch( o->stmt, o->errhp, (ub4) 1,
                 (ub4) OCI_FETCH_NEXT, (ub4) OCI_DEFAULT);
                   else i=db->err_code; // Первый фетч делает сам Оракл...
//printf("Returns code=%d... Try real  fetch!",i); getch();
db->err_code = 0; // Clear last error code...
if (i == OCI_NO_DATA )
  { ora_error(db); db->err_code = 0; return 0; }
if (!ora_ok(i)) return ora_error(db);
debugf(" ..ora10 start fetch ...\n");
for(i = db->out.count-1, c = db->out.cols;i>=0; i--, c++)  {
    c->null=(((short)c->null)!=0); // Перекодировка флажков is_null..
    //if(!c->null)
    if(c->type==dbDate)
    {
       unsigned char *D=(void*)c->dbvalue;
       if (c->null) *(double*)c->value=0;
            else    *(double*)c->value=dt_encode((D[0]-100)*100+(D[1]-100),
                                    D[2],D[3],D[4]-1,D[5]-1,D[6]-1);
    }  else if (c->type==dbBlob) // Вытягиваем блобу...
    {
    db_blob_handle *b=(void*)c->value; // Это указатель на "вытащенный блоб", его нужно установить на...
     int cnt_read = 0, offset = 0, blob_length = 0;
    debugf(" ..ora10 download blob to handle=%d\n",b);
    if (c->null) { b->len=0; continue;}
    if (OCI_SUCCESS!=OCILobGetLength (o->svchp,o->errhp,o->blob_read, &blob_length)) return ora_error(db);
    // - Сюда сливаются БЛОБЫ от SELCETA --  db->out.blob,&db->out.bsize,&db->out.blen
    if (blob_length +1 >= db->out.bsize)
      {
	int sz = blob_length+1024; // Новая длина
	void *p;
      // Еще место нужно... Realloc...
      debugf(" ..ora10 blob size=%d\n",sz);
      db->out.bsize = sz;
      p = realloc(db->out.blob,db->out.bsize); // Думаем что памяти достаточно
	if (!p) { sprintf(db->error,"vdb - no memory"); db->err_code = -2; return 0; }
      db->out.blob = p ;

      }
   debugf("  .. ora Ok - begin read\n");
   if (OCI_SUCCESS!=OCILobRead(o->svchp,o->errhp,o->blob_read,  // Интересно, такое бывает?
      &cnt_read, 1, db->out.blob, db->out.bsize, 0, 0 csid_csfrm)) return ora_error(db);
   debugf(" .. ora ReadDone, no error cnt_read=%d\n",cnt_read);
   db->out.blen = cnt_read+1; // Zero termanited на всякий случай...
   // Корректируем указатель, т.к. блоб мог реаллокнуться..
   debugf(" .. ora Setb->data\n");
   b->data = db->out.blob; // Все заточено под один БЛОБ
   b->len  = cnt_read;
   b->data[b->len] = 0; // Терминирование нулем - вещь полезная...
   debugf("  ora OK flashed to bh=%d- do it again\n",b);
    }
  }
debugf(" ..ora - FetchDone OK\n");
return 1;
}

// Связывает переменные для выгрузки select во внутренние буфера...
int ora_open(database *db) {
t_ora *o = db->h;
int nc, i , len; db_col *c;
char szname[256]; ub4 name_len; // Буфер для имени колонки
if (!o) return ora_error(db);
o->rownum = 0;
db->out.count=0; db->out.len=0; // Очищаем "внутренние" колонки
// Теперь нужно еще и так сделать перез запросом колонок...
if (!ora_ok(OCIStmtExecute(o->svchp, o->stmt, o->errhp, (ub4) 1, (ub4) 0,
              (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, OCI_DESCRIBE_ONLY)))
               return ora_error(db);
for(nc=0;;nc++)
  {
    int dbtype=0,type; sword scale=0, ok;
    ub4 dblen;
    char *name;
    name_len=sizeof(name)-1;
    ok = (OCI_SUCCESS==OCIParamGet(o->stmt, OCI_HTYPE_STMT,o->errhp, &o->mypard,(ub4)nc+1))
     &&  (OCI_SUCCESS==OCIAttrGet(o->mypard, (ub4) OCI_DTYPE_PARAM,(dvoid*) &dbtype,(ub4 *) 0, (ub4) OCI_ATTR_DATA_TYPE,(OCIError *)o->errhp))
     &&  (OCI_SUCCESS==OCIAttrGet(o->mypard, (ub4) OCI_DTYPE_PARAM,(dvoid**)&name,(ub4 *) &name_len, (ub4) OCI_ATTR_NAME,(OCIError *)o->errhp ))
     &&  (OCI_SUCCESS==OCIAttrGet(o->mypard, (ub4) OCI_DTYPE_PARAM,(dvoid**)&dblen,&dblen, (ub4) OCI_ATTR_DISP_SIZE,(OCIError *)o->errhp ))
     &&  (OCI_SUCCESS==OCIAttrGet(o->mypard, (ub4) OCI_DTYPE_PARAM,(dvoid**)&scale,(ub4 *) 0, (ub4) OCI_ATTR_SCALE,(OCIError *)o->errhp ));
    if (!ok) break;
    if(name_len>255) name_len=255; strncpy(szname,name,name_len); szname[name_len]=0;
    //printf("Col:%d name='%*.*s' dbtype=%d dblen=%d scale=%d\n",nc,name_len,name_len,name,  dbtype,dblen,scale);
    switch (dbtype)
    {
      case INT_TYPE:      type=dbInt;    break;
      case FLOAT_TYPE:
      case NUMBER_TYPE:   type=dbDouble; break;
      case ROWID_TYPE:
      case 96:
      case 8:
      case 23:
      case STRING_TYPE:
      case VARCHAR2_TYPE: type=dbChar;
           dblen = 4*1024;
           break;
      case 112: // CLOB
           dblen = MAX_CLOB; type=dbChar;
           printf("Clob?\n");
           break;
      case DATE_TYPE:     type=dbDate;    break;
      //case 112:     type=dbBlob;    break; // Char LOB
      case 113:     type=dbBlob;    break; // Bin LOB
      default:
      {
        sprintf(db->error,"VORA - open: unkown oracle type #%d pos=%d",dbtype,nc+1);
        db->out.count=0; return 0;
      }
    }
    c=db_add_col(&db->out,szname,type,dblen); c->dbtype=dbtype;
    if (!c) {
	      sprintf(db->error,"vora - failed alloc memory");
	      return 0;
              }
    if((c->type==dbInt)||(c->type==dbDouble)) c->len=scale;
  }
// А Это уже подвязка к локальным переменным
#define ora_def(DATA,TYPE,SIZE) nl = OCIDefineByPos(o->stmt, &o->defs[i] , o->errhp, \
  (ub4) i+1 , (dvoid *) DATA, \
  (sb4) SIZE, TYPE, &c->null, (ub2 *) 0, \
  (ub2 *) 0, OCI_DEFAULT)


#define ora_def2(DATA,TYPE,SIZE) nl = OCIDefineByPos(o->stmt, &o->defs2[i] , o->errhp, \
  (ub4) i+1 , (dvoid *) DATA, \
  (sb4) SIZE, TYPE, &c->null, (ub2 *) 0, \
  (ub2 *) 0, OCI_DEFAULT)

 for(i=0,c=db->out.cols; i<nc; i++,c++)
  {
  int nl ; // Код возврата операции повязывания...
    switch (c->type)
    {
      case dbInt:    ora_def2(c->value,INT_TYPE,4); break; // Сразу в буфер!
      case dbDouble: ora_def2(c->value,SQLT_FLT,8); break; // Сразу в буфер!
      case dbDate:   ora_def2(c->dbvalue,DATE_TYPE,8); break;
      case dbChar:   ora_def2(c->dbvalue,STRING_TYPE,c->len+1); break;
      case dbBlob:   {
            ora_def2(&o->blob_read,SQLT_BLOB,0);   break;
            }
    }
    if(nl!=OCI_SUCCESS) { db->out.count=0;  return ora_error(db); }
  }
return 1;
}

int asize = 0;

// Очистка соединения
int ora_disconnect(database *db)
{
t_ora *o = db->h;
if (o)
 {
 int i;	 //ZUZUKA
  // OCISessionEnd(svchp, errhp, usrhp, (ub4)OCI_DEFAULT);
  if (o->blob) OCIDescriptorFree(o->blob, (ub4) OCI_DTYPE_LOB);
  if (o->dschp) OCIHandleFree(o->dschp,(ub4) OCI_HTYPE_DESCRIBE);
  if (o->stmt) OCIHandleFree( (dvoid *)o->stmt,OCI_HTYPE_STMT);
  if (o->authp) OCIHandleFree((dvoid *) o->authp, OCI_HTYPE_SESSION);
  if (o->srvhp) // Оракл сам не килит "соединение"
  {
   OCISessionEnd (o->svchp,o->errhp,0,OCI_DEFAULT);
   OCIServerDetach(o->srvhp,o->errhp, (ub4) OCI_DEFAULT);
  //	OCIServerDetach(o->srvhp,o->errhp, (ub4) 0 );
   OCIHandleFree( (dvoid *)o->srvhp, OCI_HTYPE_SERVER);
	//  printf("SrvHandle freed?\n");
  }
  if (o->svchp) {
	  OCIHandleFree( (dvoid *)o->svchp, OCI_HTYPE_SVCCTX);
           }
   if (o->errhp) {
	 //printf("Err handle=%d\n",o->errhp);
	 OCIHandleFree((dvoid *)o->errhp, OCI_HTYPE_ERROR);
	// printf("ErrHandle freed?\n");
    }

  //OCIHandleFree((dvoid *)o->envhp, OCI_HTYPE_ENV);
    //printf("EnvHandle freed?\n");

 //if (o->blob_read) OCIDescriptorFree(o->blob_read, (ub4) OCI_DTYPE_LOB);



 //if (o->mypard) OCIHandleFree(o->mypard, OCI_HTYPE_
 //for(i=0;i<256;i++) if (o->defs[i]) OCIHandleFree(o->defs[i],OCI_HTYPE_);   // Дефайнеры для SELECT колонок(подвязывание)
 //   OCIParam     *mypard; // Обьектик для получения информациипо отселекченой колонке

 if (o->p) free(o->p);
  //printf("asize = %d\n",asize);
 //if (o->envhp) OCIHandleFree((dvoid *) o->envhp, OCI_HTYPE_ENV);

 free(o);


 }
db->h = 0;
Free(db->s);
 // Так как свои диспетчеры памяти, нужно вылить...
 Free(db->out.data);
 Free(db->out.blob); // Так как юзаем блобы - это вещь необходимая...
 Free(db->out.cols);
 Free(db->in.data);
 Free(db->in.blob);
 Free(db->in.cols);
db->disconnect = 0;
return 1;
}

//void *my_malloc(database *db,int size) { return malloc(size);}
//void *my_realloc(database *db,void *p,int size) { return realloc(p,size);}
//void  my_free(database *db,void *p)  { free(p);}

// Инициализация @database + установление соединения с Сервером


int _ora_connect(database *db,char *srvname,char *username,char *password) {
t_ora * o; void *p;
//printf("Connecting???\n");
// Создаем симпле-ора, ошибки памяти не проверяем...
o = malloc( sizeof(t_ora) );
if (!o) { sprintf(db->error,"vora - no memory"); return 0;}
memset(o,0,sizeof(t_ora)); db->h = o;
db->disconnect = ora_disconnect;
db->compile = ora_compile;
db->exec = ora_exec;
db->commit = ora_commit;
db->rollback = ora_rollback;
db->open = ora_open;
db->fetch = ora_fetch;
db->bind = ora_bind;
o->envhp=env; // Copy It
// Инициализиреум envhp - "сессионный набор" (для дальнейшего ЮЗА)
//OCIEnvInit( (OCIEnv **) &o->envhp, OCI_DEFAULT, (size_t) 0,(dvoid **) 0 );
debugf(" ..try connect to oracle {user:'%s',pass:'%s',server:'%s',env:0x%p}\n",username,password,srvname,o->envhp);
// Инитим обьект типа ошибка Оракл
OCIHandleAlloc( (dvoid *) o->envhp, (dvoid **) &o->errhp, OCI_HTYPE_ERROR,(size_t) 0, (dvoid **) 0);
debugf(".. OCIHandleAlloc errhp:0x%p\n", o->errhp);
//printf("ErrHP=%d\n",o->errhp);
// Инитим "серверОракл"
OCIHandleAlloc( (dvoid *) o->envhp, (dvoid **) &o->srvhp, OCI_HTYPE_SERVER,(size_t) 0, (dvoid **) 0);
debugf(".. OCUHandleAlloc srvhp:0x%p, attaching\n", o->srvhp);
// Следующие две команды совершенно непонятны...
if (OCI_SUCCESS!=OCIServerAttach( o->srvhp, o->errhp, (text *)srvname, strlen(srvname), OCI_DEFAULT)) {
 ora_error(db);
 ora_disconnect(db);
 return 0;
 }

debugf(".. Server Attached... srvhp:0x%p\n", o->srvhp);

//return ora_disconnect(db);

OCIHandleAlloc( (dvoid *) o->envhp, (dvoid **) &o->svchp, OCI_HTYPE_SVCCTX,(size_t) 0, (dvoid **) 0);


// Забираем с сервера настройки для нашей сессии
OCIAttrSet( (dvoid *) o->svchp, OCI_HTYPE_SVCCTX, (dvoid *)o->srvhp,(ub4) 0, OCI_ATTR_SERVER, (OCIError *) o->errhp);
// Создаем обьет "авторизованность"
OCIHandleAlloc((dvoid *) o->envhp, (dvoid **)&o->authp,(ub4) OCI_HTYPE_SESSION, (size_t) 0, (dvoid **) 0);
// Записыавем имя пользователя
OCIAttrSet((dvoid *) o->authp, (ub4) OCI_HTYPE_SESSION,(dvoid *) username, (ub4) strlen((char *)username),
                 (ub4) OCI_ATTR_USERNAME, o->errhp);
// Записываем пароль
OCIAttrSet((dvoid *) o->authp, (ub4) OCI_HTYPE_SESSION,
       (dvoid *) password, (ub4) strlen((char *)password), (ub4) OCI_ATTR_PASSWORD, o->errhp);

// Compile statement...
OCIHandleAlloc( (dvoid *) o->envhp, (dvoid **) &o->stmt, OCI_HTYPE_STMT, (size_t) 0, (dvoid **) 0);

// Это для получения "describe" от SELECT
OCIHandleAlloc((dvoid *) o->envhp, (dvoid **) &o->dschp,
                        (ub4) OCI_HTYPE_DESCRIBE,(size_t) 0, (dvoid **) 0);

// Для блобчика готовим...
OCIDescriptorAlloc((dvoid *) o->envhp, (dvoid **) &o->blob,
                           (ub4) OCI_DTYPE_LOB, (size_t) 0, (dvoid **) 0);

OCIAttrSet((dvoid *) o->blob, (ub4) OCI_DTYPE_LOB,
       0,0, (ub4) OCI_ATTR_LOBEMPTY, o->errhp);

//OCIDescriptorAlloc((dvoid *) o->envhp, (dvoid **) &o->blob_read,
  //                         (ub4) OCI_DTYPE_LOB, (size_t) 0, (dvoid **) 0);

//OCIAttrSet((dvoid *) o->blob_read, (ub4) OCI_HTYPE_SESSION,
   //    0,0, (ub4) 0 /*OCI_ATTR_LOBEMPTY*/, o->errhp);


debugf(" initing OCISessionBegin {svchp:%p,errhp:%p,authp:%p}\n",o->svchp,o->errhp,o->authp);
if (OCI_SUCCESS!=OCISessionBegin (o->svchp,o->errhp,o->authp, OCI_CRED_RDBMS,(ub4) OCI_DEFAULT)) {
 ora_error(db);
 ora_disconnect(db);
 return 0;
 }
debugf(" ora_session inited, set some attributes\n");

//return 1;
// Его (blob) еще нужно создавать... интересно, что это за действо?
//OCILobCreateTemporary(o->svchp,o->errhp,o->blob,
   //    OCI_DEFAULT,OCI_DEFAULT,OCI_TEMP_BLOB,FALSE,OCI_DURATION_SESSION);

//OCILobCreateTemporary(o->svchp,o->errhp,o->blob_read,
   //    OCI_DEFAULT,OCI_DEFAULT,OCI_TEMP_BLOB,FALSE,OCI_DURATION_SESSION);



// Нужное какое-то подсоединение...
OCIAttrSet((dvoid *) o->svchp, (ub4) OCI_HTYPE_SVCCTX,
                   (dvoid *) o->authp, (ub4) 0,
                   (ub4) OCI_ATTR_SESSION, o->errhp);
if (0) { // Как установить аттрибут???
ub1 yes = 1;
int res;
res = OCIAttrSet((dvoid *) o->stmt, (ub4) OCI_PTYPE_DATABASE,
                   (dvoid *) &yes, (ub4) 1,
                   (ub4) OCI_ATTR_AUTOCOMMIT_DDL, o->errhp);
printf("\n ------------- Autocommit set result=%d, ok=%d\n",res, OCI_SUCCESS);
if (res) {
   {
 ora_error(db);
 ora_disconnect(db);
 return 0;
 }
  }
}


debugf(" ok, oracle sever connected\n");
return 1; // Connected OK!
}

char *ora_params[]={
    "sysdate","sysdate", // Текущая дата -)
    "all_tables"," table_name  from all_tables where owner=user order by 1", // Все таблицы текущего пользователя
    "DATE_COL", "DATE",
    0,0};

vdb_func ora_fun = { // Статическая структура
   "Oracle", //   char *name; // Database Internal Name
   ora_connect, //int (*connect)() ;
   ora_disconnect, // int (*disconnect)();
   ora_fetch, //int (*fetch)();
   ora_exec, //int (*exec)();
   ora_compile, //int (*compile)();
   ora_open, //int (*open)();
   ora_commit, //int (*commit)();
   ora_rollback, //int (*rollback)();
   ora_bind, //int (*bind)();
   ora_params, //char **params; // Внешний буффер именованных параметров -)
   //int res[512]; // Для расширений -)
   } ;//vdb_func;



int ora_connect(database *db,char *srvname,char *username,char *password) {
int r;
//printf("???");
//mutex_lock(ora_mutex);
r = _ora_connect(db,srvname,username,password);
if (r) db->fun = &ora_fun;
//mutex_unlock(ora_mutex);
return r;
}
