
#use this line for profiling
#PROFILEOPTION=-pg -g
#use this lines for no profiling
PROFILEOPTION=

#note:
#the -Werror can be removed to compile even if there are warnings,
#this is used to ensure that all released versions are free of warnings.

#normal compile
OPTIMIZATIONS= -O6 -fexpensive-optimizations
CFLAGS= -MD -Wall -Werror $(OPTIMIZATIONS) $(PROFILEOPTION)
#debug compile
#OPTIMIZATIONS=
#CFLAGS= -MD -Wall -Werror -ggdb $(OPTIMIZATIONS) $(PROFILEOPTION)

LDFLAGS= -lm $(PROFILEOPTION)

TARGETS= dpvanalysis1d dpvanalysis2d dpiencoder dpidecoder dpidecoderint dpidecoder2 dpiencoder3 dpidecoder3 dpidecoder4

all: $(TARGETS)

.c.o:
	gcc $(CFLAGS) -c $*.c

dpvanalysis1d: dpvanalysis1d.o file.o tgafile.o
	gcc -o $@ $^ $(LDFLAGS)

dpvanalysis2d: dpvanalysis2d.o file.o tgafile.o
	gcc -o $@ $^ $(LDFLAGS)

dpiencoder: dpiencoder.o file.o tgafile.o
	gcc -o $@ $^ $(LDFLAGS)

dpidecoder: dpidecoder.o file.o tgafile.o
	gcc -o $@ $^ $(LDFLAGS)

dpidecoderint: dpidecoderint.o file.o tgafile.o
	gcc -o $@ $^ $(LDFLAGS)

dpidecoder2: dpidecoder2.o file.o tgafile.o
	gcc -o $@ $^ $(LDFLAGS)

dpiencoder3: dpiencoder3.o file.o tgafile.o
	gcc -o $@ $^ $(LDFLAGS)

dpidecoder3: dpidecoder3.o file.o tgafile.o
	gcc -o $@ $^ $(LDFLAGS)

dpidecoder4: dpidecoder4.o file.o tgafile.o
	gcc -o $@ $^ $(LDFLAGS)



clean:
	-rm -f $(TARGETS) *.exe *.o *.d

.PHONY: clean

-include *.d
