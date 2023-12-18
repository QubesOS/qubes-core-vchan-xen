# `libvchan` implementation for Windows/Xen.

- TODO: installer for win10 QWT
- TODO: integrate with Qubes builder

### Environment variables

- `QUBES_INCLUDES` must contain paths containing `libxenvchan` and `xeniface` includes. Normally it's `<src>/qubes-vmm-xen-windows-pvdrivers/include;<src>/qubes-vmm-xen-windows-pvdrivers/xeniface/include`.
- `QUBES_LIBS` must contain paths containing `libxenvchan` and `xeniface` libraries. Normally it's `<src>/qubes-vmm-xen-windows-pvdrivers/bin;<src>/qubes-vmm-xen-windows-pvdrivers/bin/xeniface`.

## Command-line build

`EWDK_PATH` env variable must be set to the root of MS Enterprise WDK for Windows 10/Visual Studio 2022. 

`build.cmd` script builds the solution from command line using the EWDK (no need for external VS installation).

Usage: `build.cmd Release|Debug`
