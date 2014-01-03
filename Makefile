objs := interp.o machine.o env.o memory.o native.o readparse.o secd.o

secd: $(objs)
	$(CC) $(objs) -o $@

%.o : %.c
	$(CC) -c $< -o $@ -Wall

.PHONY: clean
clean:
	rm secd *.o

interp.o : interp.c
machine.o : machine.c
env.o : env.c
native.o : native.c
memory.o : memory.c
readparse.o : readparse.c
secd.o : secd.c
