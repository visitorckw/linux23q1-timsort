CC = gcc
CFLAGS = -O2

all: main

mainOBJS := main.o list_sort.o shiverssort.o \
        timsort.o list_sort_old.o inplace_timsort.o \
		inplace_shiverssort.o
measureOBJS := measure.o list_sort.o shiverssort.o \
		timsort.o list_sort_old.o inplace_timsort.o \
		inplace_shiverssort.o

deps := $(OBJS:%.o=.%.o.d)

main: $(mainOBJS)
	$(CC) -o $@ $(LDFLAGS) $^

%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c -MMD -MF .$@.d $<

test: main
	@./main

measure: $(measureOBJS)
	$(MAKE) main
	$(CC) -o $@ $(CFLAGS) $^

debug: .force
.force:
	gcc -g -o debug inplace_timsort.c inplace_shiverssort.c list_sort.c list_sort_old.c main.c shiverssort.c timsort.c

clean:
	rm -f $(mainOBJS) $(deps) *~ main
	rm -f $(measureOBJS) $(deps) *~ main
	rm -rf *.dSYM

-include $(deps)
