PREFIX ?= /usr
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include

help:
	@echo "make all                   -- build binaries"
	@echo "make clean                 -- cleanup"

all:
	$(MAKE) -C vchan -f Makefile.linux

install:
	install -D -m 0644 vchan/libvchan.h ${DESTDIR}$(INCLUDEDIR)/vchan-xen/libvchan.h
	install -D -m 0644 vchan/vchan-xen.pc ${DESTDIR}$(LIBDIR)/pkgconfig/vchan-xen.pc
	install -D vchan/libvchan-xen.so ${DESTDIR}$(LIBDIR)/libvchan-xen.so
	cd ${DESTDIR}$(LIBDIR)/pkgconfig && ln -s vchan-xen.pc vchan.pc

clean:
	make -C vchan -f Makefile.linux clean
