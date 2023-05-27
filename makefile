CC			=  gcc
CFLAGS	   += -std=c99 -std=gnu99 -Wall -Werror -Wpedantic  -g -ggdb
INCDIR      = ./include
INCLUDES	= -I. -I $(INCDIR)
LDFLAGS 	= -L.
OPTFLAGS	= -O3 
LIBS        = -lpthread
TEST        = test.sh

TARGETS     = farm generafile

.PHONY: all clean cleanall
.SUFFIXES: .c .h

all  : $(TARGETS)

farm : farm.c worker_support.c master_worker.c threadpool.c collector.c tree.c
		$(CC) $^ $(CFLAGS) $(INCLUDES) -o $@  $(LDFLAGS) $(LIBS)

generafile: generafile.c
	$(CC) $(CFLAGS) -o $@ $^

test: farm generafile
	chmod +x $(TEST)
	./$(TEST)

clean		: 
	rm -f $(TARGETS)
cleanall	: clean
	\rm -f *.o *~ *.a *.dat
	-rm -r testdir
	-rm expected.txt
	-rm -f farm.sck
	