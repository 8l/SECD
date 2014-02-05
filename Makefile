objs := interp.o machine.o env.o memory.o native.o readparse.o

# posix:
objs += posix-io.o secd.o

CFLAGS := -O2 -m32 -Wno-shift-overflow -Wall -Wextra
VM := ./secd

$(VM): $(objs)
	$(CC) $(CFLAGS) $(objs) -o $@

sos: $(objs) sos.o repl.o
	$(CC) $(CFLAGS) $(objs) sos.o repl.o -o $@

repl.secd: repl.scm
	$(VM) scm2secd.secd < $< > $@

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o : %.secd
	$(LD) -r -b binary -o $@ $<

%.secd: %.scm $(VM)
	$(VM) scm2secd.secd < $< > tmp.secd && mv tmp.secd $@

libsecd: $(objs) repl.o
	ar -r libsecd.a $(objs) repl.o

.PHONY: clean
clean:
	rm secd *.o libsecd* || true

interp.o : interp.c memory.h secdops.h env.h
machine.o : machine.c memory.h secdops.h env.h
env.o : env.c memory.h env.h
native.o : native.c memory.h env.h
memory.o : memory.c memory.h
readparse.o : readparse.c memory.h secdops.h
secd.o : secd.c secd.h
posix-io.o: posix-io.c secd.h secd_io.h
sos.o: sos.c

repl.o: repl.secd
repl.secd: repl.scm

secd_io.h: conf.h secd.h
memory.h : secd.h
secd.h: conf.h
