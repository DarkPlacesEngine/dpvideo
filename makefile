
#use this line for profiling
#PROFILEOPTION=-pg -g
#use this lines for no profiling
PROFILEOPTION=

#note:
#the -Werror can be removed to compile even if there are warnings,
#this is used to ensure that all released versions are free of warnings.

#normal compile
OPTIMIZATIONS= -O2 -fexpensive-optimizations
CFLAGS= `sdl-config --cflags` -MD -Wall $(OPTIMIZATIONS) $(PROFILEOPTION)
#debug compile
#OPTIMIZATIONS=
#CFLAGS= `sdl-config --cflags` -MD -Wall -ggdb $(OPTIMIZATIONS) $(PROFILEOPTION)

LDFLAGS= -lm $(PROFILEOPTION)

TARGETS= dpvencoder dpvdecoder dpvplayer dpvsimpleplayer

all: $(TARGETS)

.c.o:
	gcc $(CFLAGS) -c $*.c

dpvencoder: dpvencoder.o dpvencode.o file.o tgafile.o
	gcc -o $@ $^ $(LDFLAGS)

dpvdecoder: dpvdecoder.o dpvdecode.o file.o tgafile.o
	gcc -o $@ $^ $(LDFLAGS)

dpvplayer: dpvplayer.o dpvdecode.o
	gcc -o $@ $^ $(LDFLAGS) -lSDL -lSDLmain -lpthread

dpvsimpleplayer: dpvsimpleplayer.o dpvsimpledecode.o
	gcc -o $@ $^ $(LDFLAGS) -lSDL -lSDLmain -lpthread



clean:
	-rm -f $(TARGETS) *.exe *.o *.d

.PHONY: clean

-include *.d
