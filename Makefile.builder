RPM_SPEC_FILES := rpm_spec/libvchan.spec
ARCH_BUILD_DIRS := archlinux

ifeq ($(PACKAGE_SET),vm)
  ifneq ($(filter $(DISTRIBUTION), debian qubuntu),)
    DEBIAN_BUILD_DIRS := debian
  endif

  WIN_COMPILER = msbuild
  WIN_SOURCE_SUBDIRS = windows
  WIN_BUILD_DEPS = vmm-xen-windows-pvdrivers
  WIN_OUTPUT_LIBS = bin
  WIN_OUTPUT_HEADERS = include
  WIN_OUTPUT_BIN = bin
  WIN_PREBUILD_CMD = set_version.bat && powershell -executionpolicy bypass -File set_version.ps1 < nul
  WIN_SLN_DIR = vs2017
endif

# vim: filetype=make
