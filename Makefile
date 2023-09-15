LDFLAGS=-L/usr/local/lib -lsqlite3 -lcurl -lpthread

objects = work.o db.o buffer.o torrent.o http.o bncode.o main.o mem.o buffer_pool.o td_pool.o
CC=cc
cflags = -I/usr/local/include -g  -O0 -Wextra

bncode.o: bncode.c bncode.h
	cc $(cflags) -c bncode.c
buffer_pool.o: buffer_pool.c buffer_pool.h
	cc $(cflags) -c buffer_pool.c
td_pool.o: td_pool.c td_pool.h
	cc $(cflags) -c td_pool.c
mem.o: mem.c mem.h
	cc $(cflags) -c mem.c
work.o: work.c work.h buffer.h torrent.h http.h
	cc $(cflags) -c work.c
db.o: db.c db.h
	cc $(cflags) -c db.c
torrent.o: torrent.c torrent.h buffer.h torrent_ctx.h
	cc $(cflags) -c torrent.c
buffer.o: buffer.c buffer.h
	cc $(cflags) -c buffer.c
http.o: http.c http.h torrent.h server.h
	cc $(cflags) -c http.c
main.o: main.c server.h work.h torrent.h buffer.h http.h
	cc $(cflags) -c main.c
all: $(objects)
	$(CC) $(objects) -o ttracker $(LDFLAGS)
remake: clean all
clean:
	rm -rf *.o
	rm ttracker
