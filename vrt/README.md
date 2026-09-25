# VRT

VRT is the native runtime targeted by VIRC's LLVM emitter. It owns the public C
ABI for program startup, generated function metadata, frames, values,
references, objects, arrays, regions, errors, and runtime services used by
native code.

The `libvrt` target is linked with generated native objects. Public ABI headers
are under `include/vrt/`; implementation headers and C++ types remain private
to this directory.

VRT is not yet a complete replacement for VBCI. Runtime functionality moves by
complete semantic subsystem, with independent VRT tests and LLVM-native
integration coverage. See the
[VBCI to VRT migration policy](../docs/architecture/vbci-vrt-migration.md).

Public ABI and implementation details are indexed in
[VRT Internals](docs/README.md).
