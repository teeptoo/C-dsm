ALL: default 

CC           = gcc
CLINKER      = $(CC)
OPTFLAGS     = -O0

SHELL = /bin/sh

CFLAGS  =   -DREENTRANT -Wunused -Wall -g 
CCFLAGS = $(CFLAGS)
LIBS =  -lpthread

EXECS = common.o dsmexec dsmwrap exemple exemple_init_finalize

default: $(EXECS)
all : $(EXECS)

dsmexec: dsmexec.o common.o
	$(CLINKER) $(OPTFLAGS) -o dsmexec dsmexec.o  common.o $(LIBS)
	mv dsmexec ./bin
	
dsmwrap: dsmwrap.o common.o
	$(CLINKER) $(OPTFLAGS) -o dsmwrap dsmwrap.o  common.o $(LIBS)
	mv dsmwrap ./bin
		
exemple: exemple.o common.o dsm.o 
	$(CLINKER) $(OPTFLAGS) -o exemple exemple.o common.o dsm.o  $(LIBS)
	mv exemple ./bin

exemple_init_finalize: exemple_init_finalize.o common.o dsm.o
	$(CLINKER) $(OPTFLAGS) -o exemple_init_finalize exemple_init_finalize.o common.o dsm.o  $(LIBS)
	mv exemple_init_finalize ./bin
	
clean:
	@-/bin/rm -f *.o *~ PI* $(EXECS) *.out core 
.c:
	$(CC) $(CFLAGS) -o $* $< $(LIBS)
.c.o:
	$(CC) $(CFLAGS) -c $<
.o:
	${CLINKER} $(OPTFLAGS) -o $* $*.o $(LIBS)
