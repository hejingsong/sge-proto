INC_DIR=../../build/include
LIB_DIR=../../build/lib
LIB_NAME=sgeproto

main: main.o
	gcc main.o -L$(LIB_DIR) -l$(LIB_NAME) -lpthread -g -o main

main.o: main.c
	gcc -c main.c -I$(INC_DIR) -g -o main.o

clean:
	rm -rf main main.o
