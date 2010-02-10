# Makefile for proto project
main: proto img
proto: proto.o
	gcc -ggdb `pkg-config --cflags libusb-1.0` `pkg-config --libs libusb-1.0` -obin/proto bin/proto.o
proto.o: src/proto.c
	gcc -ggdb `pkg-config --cflags libusb-1.0` `pkg-config --libs libusb-1.0` -obin/proto.o -c src/proto.c
img: img.o
	gcc -ggdb `pkg-config --cflags gtk+-2.0` `pkg-config --libs gtk+-2.0` -obin/img bin/img.o
img.o: src/img.c
	gcc -ggdb `pkg-config --cflags gtk+-2.0` `pkg-config --libs gtk+-2.0` -obin/img.o -c src/img.c
clean: 
	rm bin/proto bin/proto.o bin/img bin/img.o
