CCOPTS= -Wall -ggdb -std=gnu99 -Wstrict-prototypes
LIBS= 
CC=gcc
AR=ar


BINS= inodefs_test\

OBJS = #add here your object files

HEADERS=inodefs_test_util.h\

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@  $<

.phony: clean all


all:	$(BINS) 

inodefs_test: inodefs_test.c $(OBJS) 
	$(CC) $(CCOPTS)  -o $@ $^ $(LIBS) 

clean:
	rm -rf *.o *~  $(BINS) inodefs_test.txt
