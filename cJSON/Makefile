
CC=gcc
LIB=-lm

all: cJSON.o main.o
	$(CC) cJSON.o main.o -o main $(LIB)

cJSON.o:
	$(CC) -o cJSON.o -c cjson.c

main.o:
	$(CC) -o main.o -c main.c

.PHONY:
clean:
	rm -fr *.o main
