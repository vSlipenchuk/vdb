CFLAGS=-I../vos -fPIC

all:  sq3u.so

clean:
	rm *.so vdb 

sq3u.so:  vsqlite.c vdb.c
	$(CC) $(CFLAGS) -shared -o sq3u.so vsqlite.c vdb.c sqlite3/sqlite3.c
	
test:
	./vdb u/p@test2.db#./sq3u.so
	
	
vdbcon:	
	$(CC) -I../vos vdbcon.c vdb.c vdb_upload.c ../vos/vos.c ../vos/vs0.c \
	-ldl -lpthread
