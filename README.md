# libvchan (Xen)

This is the Xen implementation of libvchan, the inter-VM communication
library used by [Qubes OS](https://www.qubes-os.org). It lets code in one
VM exchange a stream of data with code in another VM, with one side acting as
server and the other as client. It is the transport that
[qrexec](https://www.qubes-os.org/doc/qrexec/) and other higher-level Qubes
daemons are built on.

It is a wrapper around Xen's `libxenvchan` library. The wrapper exposes a
generic API, so programs can run without Xen by linking against another
implementation instead, such as the socket-based one in
[qubes-core-vchan-socket](https://github.com/QubesOS/qubes-core-vchan-socket).
For historical reasons, this API is quite similar to the `libxenvchan` one.

## Usage

The library installs the `libvchan.h` header and a `vchan-xen` pkg-config
module. Include `<libvchan.h>` in your program and build against the
`vchan-xen` module to pick up the right compiler and linker flags.
