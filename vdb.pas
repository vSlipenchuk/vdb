{
 vdb interface (v.1.0)
}
unit vdb;

interface

type db_blob = record
     size,len:integer;
     data:pchar;
     handle:integer
     end;

type db_col = record
     name:pchar;
     null,len:integer;
     typ,dbtype:integer; // Расширяем под ибазу
     value,dbvalue:pchar; // данные в приложении и данные в БД буфере
     end;

type pdb_col = ^ db_col;
type adb_col = array[0..256] of db_col;

type db_cols = record
    count,capacity:integer;
    p:pointer;
    cols:^adb_col;
    size,len:integer;
    data:pchar;
    bsize,blen:integer;
    blob:pchar;
    end;

const DB_MAX_ERROR  = 512;

type db = packed record
   lib,h,c,s,p:pointer;
   in_,out_:db_cols;
   err_code:integer;
   error:array[0..DB_MAX_ERROR-1] of char;
   connect,disconnect,fetch,exec,compile,open,commit,rollback,bind:pointer;
   connected:integer; // Флаг коннекта - устанавливается при успешном db_connect, сбрасывается драйверами
   mutex:integer; // Предопределенное поле для многопоточного доступа, выделяется за пределами vdb
   buf:db_blob; // Просто временный буфер, удаляется только при db_release()
   fun:pointer; // Указатель на блок функций БД (виртуал такой)
   fetchable:integer; // Флаг указывает открытый датасет.
   transaction:integer; // Флаг указывает есть открытая транзакция или нет
   reserv:array[0..26-1] of integer;
   //char db_name[DB_MAX_NAME];
   end;

type pdb = ^db;

function db_new():pdb; cdecl;
function db_open(db:pdb):integer; cdecl;
function db_connect_string(db:pdb;cs:pchar):integer; cdecl;
function db_compile(db:pdb;sql:pchar):integer; cdecl;
function db_fetch(db:pdb):integer; cdecl;
function db_text_buf(buf:pchar;col:pdb_col):pchar; cdecl;

type dbconnect = class
   db:pdb;
   function connect(ConnectString:string):boolean;
   function fetch():boolean;
   function compile(SQL:string):boolean;
   function select(SQL:string):boolean;
   function open():boolean;
   function colCount:integer;
   function colText(col:integer):string;
   function error:string;
   end;

implementation

const dll = 'vdb.dll';

function dbconnect.select(SQL:string):boolean;
begin
Result:=compile(SQL) and open();
end;

function dbconnect.connect(ConnectString:string):boolean;
begin
if (db=nil) then db := db_new();
Result:=db_connect_string(db,PCHAR(ConnectString))>0;
end;

function dbconnect.fetch():boolean;
begin
Result:=(db<>nil)and(db_fetch(db)>0);
end;

function dbconnect.compile(SQL:string):boolean;
begin
Result:=(db<>nil)and(db_compile(db,PCHAR(SQL))>0);
end;

function dbconnect.open():boolean;
begin
Result:=(db<>nil)and(db_open(db)>0);
end;

function dbconnect.colCount:integer;
begin
if (db=nil) then Result:=0
 else Result:=db.out_.count;
end;

function dbconnect.colText(col:integer):string;
var buf:array[0..256] of char;
    res:pchar;
begin
if ((col<0) or (col>=colCount)) then begin
 Result:='[error]'; exit;
 end;
res := db_text_buf(buf,@db.out_.cols[col]);
Result:=res; // Assigned???
end;

function dbconnect.error:string;
begin
if (db=nil) then begin Result:='no db connect'; exit; end;
Result:=db.error;
end;


function db_new():pdb; cdecl; external dll;
function db_connect_string(db:pdb;cs:pchar):integer; cdecl; external dll;
function db_compile(db:pdb;sql:pchar):integer; cdecl; external dll;
function db_open(db:pdb):integer; cdecl; external dll;
function db_fetch(db:pdb):integer; cdecl; external dll;
function db_text(buf:pchar;col:pdb_col):pchar; cdecl; external dll;
function db_text_buf(buf:pchar;col:pdb_col):pchar; cdecl; external dll;

end.
