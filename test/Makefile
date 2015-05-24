
CC=gcc
CFLAGS=-g -Wall -Wextra -Werror -fPIC -O2 -I../vchan
all: node node-select

node:	node.o
	gcc -g -o node node.o -L../vchan -lvchan-xen
node-select:	node-select.o
	gcc -g -o node-select node-select.o -L../vchan -lvchan-xen
clean:
	rm -f *.o node node-select
	
