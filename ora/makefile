#OOCI=c:/c/oci
#OCI=d:\oracle\product\10.2.0.\db_1\oci
#Cyber=d:\prj\CyberPlat
#SSL=d:\prj\ssl\openssl-0.9.ok
#IB_PATH= c:\ibase
#SQ_PATH= d:\prj\sqlite
#INCLUDE=$(SSL)\include;$(INCLUDE);..\..\i;

CFILE=v8.c  vos.c vsClass.c vsDB.c coders.c vdb.c vdb2.c db_dbf.c vss.c vsGear.c vsLog.c vsMem.c vsOS.c vsRun.c vsSSL.c vsSocket.c vsString.c vsWIN.c vsc.c

all:  vora12c.so

clean:
		rm vora12c.so

vora12c.so: ora.c
		$(CC) -fPIC  -D _DLL -shared -I  include -I ..  -I ../../vos/ ora.c -L .  	-locci -lclntsh    -o vora12c.so 


test:	
		../vdb vovka/mypass@x201#./vora12c.so