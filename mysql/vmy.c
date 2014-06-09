#include        <windows.h>
#include	<stdio.h>
#include	<string.h>
#include  <mysql.h>
  
#include "../vdb.h"

// +1 - уровни изол€ции - сейчас все сразу коммититс€....
// +2 (сами ушли без автокоммита...) 2 - подавление ворнингов (при компил€ции) ???
// 3 -- работа с блобами???
// 4 - похоже, про бинд придетс€ забыть??? - EC“№ ¬Ќ”“–≈ЌЌ»≈ ѕ≈–≈ћ≈ЌЌџ≈!!!


int m_error(database *db)
{
unsigned const char *e=0;
e =  mysql_error(db->h); 
if (!e) e = "MYSQL: unreported error";
strncpy(db->error,e,sizeof(db->error)-1); 
db->error[sizeof(db->error)-1]=0;
return 0;
}

int m_compile(database *db,char *sql)
{
db->out.count = 0;
if (db->c) mysql_free_result(db->c); db->c=0; // Clears a prev query...
if (!mysql_query(db->h,sql))	return 1; // Ok, No error
return m_error(db);
}
 
int m_open(database *db) // Opens a compiled result...
{
MYSQL_FIELD *fd;
db_col *c;
if (!db->h) { sprintf(db->error,"Not connected"); return 0;}
db->c = mysql_use_result(db->h); // One by one fetch...
if (!db->c) return m_error(db);
db->out.count = 0; db->out.len = 0;
for(;fd = mysql_fetch_field(db->c);)
{  //db_add_col_(db_cols *cols,char *name,int name_len,int type,int len);
	if (fd->type == FIELD_TYPE_TINY||fd->type == FIELD_TYPE_SHORT|| 
               fd->type == FIELD_TYPE_LONG || fd->type== FIELD_TYPE_LONGLONG 
	       || fd->type == FIELD_TYPE_INT24 ) db_add_col(&db->out,fd->name,dbInt,16);
                else if (fd->type == FIELD_TYPE_DATETIME || fd->type == FIELD_TYPE_DATE ) db_add_col(&db->out,fd->name,dbDate,8);
	    else if ( fd->type == FIELD_TYPE_FLOAT || fd->type==FIELD_TYPE_DOUBLE || fd->type==FIELD_TYPE_DECIMAL ) 
	    {
		    c=db_add_col(&db->out,fd->name,dbDouble,8);
		    c->len = fd->decimals; // Auto precision...
	    }
		 else 
		 {
			 c=db_add_col(&db->out, fd->name,dbChar,fd->length);
			 c->len = fd->length;
			 }
	//printf("TYP=%d datetyp=%d\n",fd->type,FIELD_TYPE_DATE);
} 
return 1; 
}
 
int m_fetch(database *db)
{
db_col *c; int i;
MYSQL_ROW row; 
//printf("Start fetching...\n");
if (!db->c) 
{
sprintf(db->error,"No open cursor for a fetch");
return 0;	
}
row = mysql_fetch_row(db->c);
if (!row) { 
	 if (db->c) mysql_free_result(db->c); db->c=0; 
	 db->err_code = mysql_errno(db->h); // sets a flag...
	 if (db->err_code) m_error(db); // Reports a error
	 return 0;
	 } // No more data
//printf("Ok, decode results..\n");
for(i=0, c=db->out.cols; i<db->out.count; i++,c++)
{ 
char *v = row[i]; int year,mon,day,h,m,s;
//printf("Here type=%d, col = %d <%s>\n",c->type,i,v);
switch (c->type) {
case dbInt:
	 c->null = !v || sscanf(v,"%d", c->value )==0;  break;
case dbDouble:
	c->null = !v || sscanf(v,"%lf",c->value)==0;  break;
case dbDate:
	year=mon=day=h=m=s=0;
        c->null = !v || sscanf(v,"%d-%d-%d %d:%d:%d",&year,&mon,&day,&h,&m,&s)<3 || year<=0;
	if (!c->null)  *(double*)(c->value) = dt_encode(year,mon,day,h,m,s);
  	break;
default: 
	c->null = !v;
	if (v) { memset(c->value,0,c->len); strncpy(c->value,v,c->len); }
      		
}
}
//printf("Returning...\n");
return 1; // OK, fetch a next...
}

int m_exec(database *db) {return 1; } // Always OK?

int m_disconnect(database *db)
{
if (db && db->h)
{
if (db->c) mysql_free_result(db->c); db->c=0;
if (db->h) mysql_close(db->h);
Free(db->out.cols);
Free(db->out.data);
db->h=0;
}
return 1;	
}

int m_connect(database *db,char *host,char *user,char *pass)
{
	MYSQL *myData;
	char dbname[200],*dbh,*dbn;
	strncpy(dbname,host,199); dbname[199]=0;
	dbn = strchr(dbname,'/'); 
        if (dbn) { *dbn=0; dbn++; dbh=dbname;}
            else { dbh=0; dbh=dbname;} // localhost
                                  
	  if ( (myData = mysql_init((MYSQL*) 0)) && 
               mysql_real_connect( myData, dbh , user, pass, 0 , MYSQL_PORT,
			   NULL, 0) 
	       &&   (mysql_select_db( myData, dbn ) >= 0) 
	     ) { 
	   db->h = myData; // Save a connection ...
           mysql_query(db->h,"set autocommit = 0"); // Turns off autocommit...		     
	   db->compile=m_compile;
	   db->open = m_open;
	   db->exec = m_exec;
	   db->fetch = m_fetch;
	   return 1; // OK !!!
	   }
          //printf("failed load dbname='%s'\n",dbname);
	  sprintf(db->error,mysql_error(myData)); 
	  mysql_close(myData);
	  return 0;
}


#ifdef _DLL
char *prefix() { return "m_";}
#else 

database db;
database *pdb = &db;

test_select(char *sql)
{
int ok = 0,i;
printf("Selecting...\n");
if (!db_select(pdb,sql)) 
{ printf("Error:%s\n",pdb->error); return 0;}
printf("Selected %d columns fetch=%d?%d!\n",pdb->out.count,pdb->fetch,m_fetch);
while(db_fetch(pdb))
{ 
db_col *c; char szbuf[200]; 
printf("Row%d ",ok);
for(i=0,c=pdb->out.cols;i<pdb->out.count;i++,c++)
  printf(" %s='%s' ",c->name,db_text_buf(szbuf,c));
printf("\n"); ok++;
}
printf("Done....\n");
return ok;
}

main()
{
int i = 0, ok = 0;
pdb = db_new();
if (!db_connect_string(pdb,"root/selectfrom@test#vmy"))
{
	printf("Error connect %s!\n",db.error);
	return 0;
}	
printf("Connected OK!\n");
test_select("select sysdate() from dual");
test_select("select * from dual");
test_select("select * from customer");
printf("ALL OK!\n");
m_disconnect(pdb);
return 0;
}

#endif
