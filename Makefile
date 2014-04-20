RPMS_DIR=rpm/

VERSION := $(shell cat version)

DIST_DOM0 ?= fc18

help:
	@echo "make rpms                  -- generate binary rpm packages"
	@echo "make rpms-vm               -- generate binary rpm packages for VM"
	@echo "make rpms-dom0             -- generate binary rpm packages for Dom0"
	@echo "make update-repo-current   -- copy newly generated rpms to qubes yum repo"
	@echo "make update-repo-current-testing  -- same, but to -current-testing repo"
	@echo "make update-repo-unstable  -- same, but to -testing repo"
	@echo "make update-repo-installer -- copy dom0 rpms to installer repo"
	@echo "make clean                 -- cleanup"

rpms: rpms-vm rpms-dom0

rpms-libs:
	rpmbuild --define "_rpmdir $(RPMS_DIR)" -bb rpm_spec/libvchan.spec
	rpm --addsign $(RPMS_DIR)/x86_64/qubes-libvchan-$(VERSION)*.rpm

rpms-vm: rpms-libs

rpms-dom0: rpms-libs

update-repo-current:
	ln -f $(RPMS_DIR)/x86_64/qubes-libvchan-$(VERSION)*$(DIST_DOM0)*.rpm ../yum/current-release/current/dom0/rpm/
	for vmrepo in ../yum/current-release/current/vm/* ; do \
		dist=$$(basename $$vmrepo) ;\
		ln -f $(RPMS_DIR)/x86_64/qubes-libvchan-$(VERSION)*$$dist*.rpm $$vmrepo/rpm/;\
	done

update-repo-current-testing:
	ln -f $(RPMS_DIR)/x86_64/qubes-libvchan-$(VERSION)*$(DIST_DOM0)*.rpm ../yum/current-release/current-testing/dom0/rpm/
	for vmrepo in ../yum/current-release/current-testing/vm/* ; do \
		dist=$$(basename $$vmrepo) ;\
		ln -f $(RPMS_DIR)/x86_64/qubes-libvchan-$(VERSION)*$$dist*.rpm $$vmrepo/rpm/;\
	done

update-repo-unstable:
	ln -f $(RPMS_DIR)/x86_64/qubes-libvchan-$(VERSION)*$(DIST_DOM0)*.rpm ../yum/current-release/unstable/dom0/rpm/
	for vmrepo in ../yum/current-release/unstable/vm/* ; do \
		dist=$$(basename $$vmrepo) ;\
		ln -f $(RPMS_DIR)/x86_64/qubes-libvchan-$(VERSION)*$$dist*.rpm $$vmrepo/rpm/;\
	done

update-repo-installer:
	ln -f $(RPMS_DIR)/x86_64/qubes-libvchan-$(VERSION)*fc18*.rpm ../installer/yum/qubes-dom0/rpm/

update-repo-template:
	for vmrepo in ../template-builder/yum_repo_qubes/* ; do \
		dist=$$(basename $$vmrepo) ;\
		ln -f $(RPMS_DIR)/x86_64/qubes-libvchan-$(VERSION)*$$dist*.rpm $$vmrepo/rpm/;\
	done

all:
	$(MAKE) -C u2mfn
	$(MAKE) -C vchan -f Makefile.linux

install:
	install -D -m 0644 vchan/libvchan.h ${DESTDIR}/usr/include/libvchan.h
	install -D -m 0644 u2mfn/u2mfnlib.h ${DESTDIR}/usr/include/u2mfnlib.h
	install -D -m 0644 u2mfn/u2mfn-kernel.h ${DESTDIR}/usr/include/u2mfn-kernel.h
	install -D -m 0644 u2mfn/libu2mfn.so ${DESTDIR}/usr/lib/libu2mfn.so
	install -D -m 0644 vchan/libvchan.so ${DESTDIR}/usr/lib/libvchan.so

clean:
	make -C u2mfn clean
	make -C vchan -f Makefile.linux clean
