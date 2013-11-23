CC = g++

#no-write-strings to get rid of warnings in C code
FLAGS = -lgphoto2 -lcfitsio -Wno-write-strings -g
FILES = cameracontroller.cpp config.c context.c rawtran.cpp

all: dcraw $(FILES)
	$(CC) $(FILES) $(FLAGS) -o capture

dcraw: dcraw.c
	gcc -o dcraw -O4 dcraw.c -lm -ljasper -ljpeg -llcms 

clean:
	rm -rf *.o capture

clean-all:
	rm -rf *.o capture dcraw