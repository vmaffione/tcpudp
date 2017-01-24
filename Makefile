CC=gcc
CFLAGS=-Wall -g
PROGRAMS=tcpclient tcpserver udpsend

all: $(PROGRAMS)

tcpclient: tcpclient.o

tcpserver: tcpserver.o

udpsend: udpsend.o

clean:
	-rm $(PROGRAMS)
