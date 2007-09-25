
#normal compile
FLAGS= -lm -Wall -O2 -fexpensive-optimizations $(PROFILEOPTION)
#profile compile
#FLAGS= -lm -Wall -O2 -fexpensive-optimizations -pg -g
#debug compile
#FLAGS= -lm -Wall -ggdb $(PROFILEOPTION)

all:
ifdef windir
	$(MAKE) mingw
else
	$(MAKE) dpvencoder dpvdecoder dpvplayer dpvsimpleplayer
endif

mingw:
	$(MAKE) dpvencoder.exe dpvdecoder.exe dpvplayer.exe dpvsimpleplayer.exe

dpvencoder: dpvencoder.c dpvencode.c file.c tgafile.c
	$(CC) -o $@ $^ $(FLAGS)

dpvdecoder: dpvdecoder.c dpvdecode.c file.c tgafile.c
	$(CC) -o $@ $^ $(FLAGS)

dpvplayer: dpvplayer.c dpvdecode.c
	$(CC) -o $@ $^ `sdl-config --cflags` `sdl-config --libs` $(FLAGS)

dpvsimpleplayer: dpvsimpleplayer.c dpvsimpledecode.c
	$(CC) -o $@ $^ `sdl-config --cflags` `sdl-config --libs` $(FLAGS)

dpvencoder.exe: dpvencoder.c dpvencode.c file.c tgafile.c
	$(CC) -o $@ $^ $(FLAGS)

dpvdecoder.exe: dpvdecoder.c dpvdecode.c file.c tgafile.c
	$(CC) -o $@ $^ $(FLAGS)

dpvplayer.exe: dpvplayer.c dpvdecode.c
	$(CC) -o $@ $^ `sdl-config --cflags` `sdl-config --libs` $(FLAGS)

dpvsimpleplayer.exe: dpvsimpleplayer.c dpvsimpledecode.c
	$(CC) -o $@ $^ `sdl-config --cflags` `sdl-config --libs` $(FLAGS)



clean:
	-rm -f dpvencoder dpvdecoder dpvplayer dpvsimpleplayer *.exe *.o *.d

.PHONY: clean

-include *.d
