CC = gcc

all:
	$(CC) -pthread -o netfileserver netfileserver.c
	./netfileserver

.PHONY: clean

clean:
	rm -f netfileserver netfileserver.o
