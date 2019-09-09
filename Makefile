help:
	@echo "make all                   -- build binaries"
	@echo "make clean                 -- cleanup"

all:
	$(MAKE) -C u2mfn
	$(MAKE) -C vchan -f Makefile.linux

install:
	install -D -m 0644 vchan/libvchan.h ${DESTDIR}/usr/include/vchan-xen/libvchan.h
	install -D -m 0644 u2mfn/u2mfnlib.h ${DESTDIR}/usr/include/u2mfnlib.h
	install -D -m 0644 u2mfn/u2mfn-kernel.h ${DESTDIR}/usr/include/u2mfn-kernel.h
	install -D -m 0644 vchan/vchan-xen.pc ${DESTDIR}/usr/lib/pkgconfig/vchan-xen.pc
	install -D -m 0644 u2mfn/libu2mfn.so ${DESTDIR}/usr/lib/libu2mfn.so
	install -D -m 0644 vchan/libvchan-xen.so ${DESTDIR}/usr/lib/libvchan-xen.so

clean:
	make -C u2mfn clean
	make -C vchan -f Makefile.linux clean
