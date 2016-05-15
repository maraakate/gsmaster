CFLAGS =-DNDEBUG -DLINUX
LDFLAGS =-S -lc -lz -ldl

ORIGDIR=Source

OBJS = master.o

q2master: $(OBJS)
	ld -o $@ $(OBJS) $(LDFLAGS)
	chmod 0755 $@
	ldd $@

clean:
	/bin/rm -f $(OBJS) q2master

$*.o: $*.c
	$(CC) $(CFLAGS) -c $*.c

$*.c: $(ORIGDIR)/$*.c
	tr -d '\015' < $(ORIGDIR)/$*.c > $*.c

$*.h: $(ORIGDIR)/$*.h
	tr -d '\015' < $(ORIGDIR)/$*.h > $*.h

# Dependencies

master.o: master.c
