all: proto

proto: proto.o
	gcc -ggdb `pkg-config --cflags libusb-1.0` `pkg-config --libs libusb-1.0` -obin/proto bin/proto.o

proto.o: src/proto.c
	gcc -ggdb `pkg-config --cflags libusb-1.0` `pkg-config --libs libusb-1.0` -obin/proto.o -c src/proto.c

clean: 
	rm bin/proto bin/proto.o
