PREFIX ?= /usr
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include

help:
	@echo "make all                   -- build binaries"
	@echo "make clean                 -- cleanup"

all:
	$(MAKE) -C u2mfn
	$(MAKE) -C vchan -f Makefile.linux

install:
	install -D -m 0644 vchan/libvchan.h ${DESTDIR}$(INCLUDEDIR)/vchan-xen/libvchan.h
	install -D -m 0644 u2mfn/u2mfnlib.h ${DESTDIR}$(INCLUDEDIR)/u2mfnlib.h
	install -D -m 0644 u2mfn/u2mfn-kernel.h ${DESTDIR}$(INCLUDEDIR)/u2mfn-kernel.h
	install -D -m 0644 vchan/vchan-xen.pc ${DESTDIR}$(LIBDIR)/pkgconfig/vchan-xen.pc
	install -D u2mfn/libu2mfn.so ${DESTDIR}$(LIBDIR)/libu2mfn.so
	install -D vchan/libvchan-xen.so ${DESTDIR}$(LIBDIR)/libvchan-xen.so
	cd ${DESTDIR}$(LIBDIR)/pkgconfig && ln -s vchan-xen.pc vchan.pc

clean:
	make -C u2mfn clean
	make -C vchan -f Makefile.linux clean
