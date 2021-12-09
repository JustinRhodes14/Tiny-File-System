CC=gcc
CFLAGS=-g -Wall -D_FILE_OFFSET_BITS=64
LDFLAGS=-lfuse

OBJ=tfs.o block.o

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

tfs: $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o tfs

.PHONY: clean
clean:
	rm -f *.o tfs

test:
	make clean; make; fusermount -u /tmp/jgr85/mountdir; rm DISKFILE; ./tfs -d /tmp/jgr85/mountdir;

debug: 
	make clean; make; fusermount -u /tmp/jgr85/mountdir; rm DISKFILE; gdb tfs;

cleaner:
	make clean; cd benchmark && make clean;cd ..; rm DISKFILE; fusermount -u /tmp/jgr85/mountdir;
