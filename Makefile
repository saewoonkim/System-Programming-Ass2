objects = client.o protocol.o server.o memmanage.o
clientobjects = client.o protocol.o
serverobjects = protocol.o server.o memmanage.o

CC := gcc

CFLAGS = -Wall


.PHONY: clean all



all: server client
server: $(serverobjects)
	$(CC) $(CFLAGS) -o server $(serverobjects) -lpthread

client: $(clientobjects)
	$(CC) $(CFLAGS) -o client $(clientobjects) -lpthread


clean:
	rm $(objects)

