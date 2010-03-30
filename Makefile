all: src/proto

src/proto: src/proto.o
	gcc -ggdb `pkg-config --cflags libusb-1.0` `pkg-config --libs libusb-1.0` -o src/proto src/proto.o

src/proto.o: src/proto.c src/*.h
	gcc -ggdb `pkg-config --cflags libusb-1.0` `pkg-config --libs libusb-1.0` -o src/proto.o -c src/proto.c

clean: 
	rm src/proto src/proto.o
