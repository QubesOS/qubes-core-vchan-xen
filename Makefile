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
	ln -f $(RPMS_DIR)/x86_64/qubes-libvchan-$(VERSION)*$(DIST_DOM0)*.rpm ../yum/current-testing-release/current/dom0/rpm/
	for vmrepo in ../yum/current-testing-release/current/vm/* ; do \
		dist=$$(basename $$vmrepo) ;\
		ln -f $(RPMS_DIR)/x86_64/qubes-libvchan-$(VERSION)*$$dist*.rpm $$vmrepo/rpm/;\
	done

update-repo-unstable:
	ln -f $(RPMS_DIR)/x86_64/qubes-libvchan-$(VERSION)*$(DIST_DOM0)*.rpm ../yum/unstable-release/current/dom0/rpm/
	for vmrepo in ../yum/unstable-release/current/vm/* ; do \
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

clean:
	make -C u2mfn clean
	make -C vchan -f Makefile.linux clean
