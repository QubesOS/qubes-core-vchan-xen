RPM_SPEC_FILES := rpm_spec/libvchan.spec
ARCH_BUILD_DIRS := archlinux

ifeq ($(PACKAGE_SET),vm)
  ifneq ($(filter $(DISTRIBUTION), debian qubuntu),)
    DEBIAN_BUILD_DIRS := debian
    SOURCE_COPY_IN := source-debian-quilt-copy-in
  endif

  WIN_SOURCE_SUBDIRS= vchan
  WIN_BUILD_DEPS = vmm-xen-windows-pvdrivers
  WIN_SIGN_CMD = true
  WIN_PACKAGE_CMD = true
  WIN_OUTPUT_LIBS = libs
  WIN_OUTPUT_HEADERS = .
endif

source-debian-quilt-copy-in: VERSION = $(shell cat $(ORIG_SRC)/version)
source-debian-quilt-copy-in: ORIG_FILE = "$(CHROOT_DIR)/$(DIST_SRC)/../libvchan-xen_$(VERSION).orig.tar.gz"
source-debian-quilt-copy-in:
	-$(shell $(ORIG_SRC)/debian-quilt $(ORIG_SRC)/series-debian-vm.conf $(CHROOT_DIR)/$(DIST_SRC)/debian/patches)
	tar cvfz $(ORIG_FILE) --exclude-vcs --exclude=debian -C $(CHROOT_DIR)/$(DIST_SRC) .

# vim: filetype=make
