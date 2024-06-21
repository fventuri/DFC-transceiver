CC=gcc
CFLAGS=-O -Wall -Werror
LDLIBS=-lusb-1.0 -lm

all: streaming-client

streaming-client: streaming-client.o dfc.o usb.o clock.o stream.o

straming-client.o: straming-client.c dfc.h usb.h clock.h stream.h

dfc.o: dfc.c usb.h clock.h

usb.o: usb.c usb.h

clock.o: clock.c clock.h usb.h

stream.o: stream.c stream.h usb.h


clean:
	rm -f *.o streaming-client
