CFLAGS=-I../vos

all:  sq3u.so

clean:
	rm *.so

sq3u.so:  vsqlite.c vdb.c
	$(CC) $(CFLAGS) -shared -o sq3u.so vsqlite.c vdb.c sqlite3/sqlite3.c
	
test:
	./vdb u/p@test2.db#./sq3u.so
