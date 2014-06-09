MY=c:\MySQLClient
MY5=.
I=d:\prj\i

all: vmy.dll
clean: 
    erase vmy.dll vmy.exp vmy.res vmy.obj vmy.lib


mtest.exe: mtest.c
	$(CC) -I$(MY)\include mtest.c $(MY)\lib\opt\libmySQL.lib ../vdb.lib


myTest.exe: myTest.c vmy.dll
	$(CC) -I$(MY)\include myTest.c /link $(MY)\lib\opt\libmySQL.lib ./vdb.lib

vmy.dll: vmy.c makefile  
	$(CC) -DMSWIN -DWIN32 /MD -D_DLL -D_MT -I$(MY)\include -I$(I) vmy.c $(I)/vdb.c  /link /dll  /nodefaultlib:libc \
        msvcrt.lib advapi32.lib $(MY)/lib/libMySQL.lib   /export:prefix  /export:m_connect /out:vmy.dll
