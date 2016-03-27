RPM_SPEC_FILES.dom0 := $(if $(filter $(DIST_DOM0), $(DISTS_VM)),, rpm_spec/libvchan.spec)
RPM_SPEC_FILES.vm := rpm_spec/libvchan.spec
RPM_SPEC_FILES := $(RPM_SPEC_FILES.$(PACKAGE_SET))

ARCH_BUILD_DIRS := archlinux

ifeq ($(PACKAGE_SET),vm)
  ifneq ($(filter $(DISTRIBUTION), debian qubuntu),)
    DEBIAN_BUILD_DIRS := debian
    SOURCE_COPY_IN := source-debian-quilt-copy-in
  endif

  WIN_COMPILER = msbuild
  WIN_SOURCE_SUBDIRS = windows
  WIN_BUILD_DEPS = vmm-xen-windows-pvdrivers
  WIN_OUTPUT_LIBS = bin
  WIN_OUTPUT_HEADERS = include
  WIN_PREBUILD_CMD = set_version.bat && powershell -executionpolicy bypass set_version.ps1
endif

source-debian-quilt-copy-in: VERSION = $(shell cat $(ORIG_SRC)/version)
source-debian-quilt-copy-in: ORIG_FILE = "$(CHROOT_DIR)/$(DIST_SRC)/../libvchan-xen_$(VERSION).orig.tar.gz"
source-debian-quilt-copy-in:
	-$(shell $(ORIG_SRC)/debian-quilt $(ORIG_SRC)/series-debian-vm.conf $(CHROOT_DIR)/$(DIST_SRC)/debian/patches)
	tar cfz $(ORIG_FILE) --exclude-vcs --exclude=rpm --exclude=pkgs --exclude=deb --exclude=debian -C $(CHROOT_DIR)/$(DIST_SRC) .

# vim: filetype=make
