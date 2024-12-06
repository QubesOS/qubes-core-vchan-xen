# `libvchan` implementation for Windows/Xen.

## Local command-line build on Windows

### Prerequisites

- Microsoft EWDK iso mounted as a drive
- `qubes-builderv2`
- `powershell-yaml` PowerShell package (run `powershell -command Install-Package powershell-yaml` as admin)
  (TODO: provide offline installer for this)
- `vmm-xen-windows-pvdrivers` built with the same `output_dir` as below

### Build

- run `powershell qubes-builderv2\qubesbuilder\plugins\build_windows\scripts\local\build.ps1 src_dir output_dir Release|Debug`
