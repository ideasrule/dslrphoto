CC = g++

#no-write-strings to get rid of warnings in C code
FLAGS = -lgphoto2 -lgphoto2_port -lcfitsio -Wno-write-strings -g
FILES = config.c context.c rawtran.cpp

all: dcraw $(FILES)
	$(CC) $(FILES) cameracontroller.cpp $(FLAGS) -o capture
	$(CC) $(FILES) takeflats.cpp $(FLAGS) -o takeflats
	$(CC) -g IndiControl.cpp -I/usr/include/libindi -I/usr/include/libnova -lindi -lindiclient -lpthread -lz -o goto
	$(CC) -g MoveBack.cpp -I/usr/include/libindi -I/usr/include/libnova -lindi -lindiclient -lpthread -lz -o moveback


dcraw: dcraw.c
	gcc -o dcraw -O4 dcraw.c -lm -ljasper -ljpeg -llcms 

clean:
	rm -rf *.o capture

clean-all:
	rm -rf *.o capture dcraw