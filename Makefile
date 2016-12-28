CC=gcc
CFLAGS=-Wall -g

all: tcpclient tcpserver

tcpclient: tcpclient.o

tcpserver: tcpserver.o

clean:
	-rm tcpclient tcpserver
