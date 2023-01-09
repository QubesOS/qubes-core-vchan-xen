PREFIX ?= /usr
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include

help:
	@echo "make all                   -- build binaries"
	@echo "make clean                 -- cleanup"

all:
	$(MAKE) -C vchan -f Makefile.linux

install:
	$(MAKE) -C vchan -f Makefile.linux install

clean:
	make -C vchan -f Makefile.linux clean
