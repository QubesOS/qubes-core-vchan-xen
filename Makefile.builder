RPM_SPEC_FILES := rpm_spec/libvchan.spec
ARCH_BUILD_DIRS := archlinux
WIN_SOURCE_SUBDIRS= vchan
WIN_BUILD_DEPS = vmm-xen-windows-pvdrivers
WIN_SIGN_CMD = true
WIN_PACKAGE_CMD = true
WIN_OUTPUT_LIBS = libs
WIN_OUTPUT_HEADERS = .

