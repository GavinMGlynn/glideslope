# vcpkg triplets

How Cesium Native's dependencies are built on each platform (`cmake/Vcpkg.cmake`
chooses). The triplet files themselves carry no comments, because vcpkg hashes
a triplet's text into every package built with it, and a comment edited would
rebuild every dependency.

- **`x64-linux-glideslope`** — static libraries, release only, for the
  `x86-64-v2` baseline. Release only because nothing here debugs into a
  dependency, and on Linux a debug build links release libraries with no
  mismatch in the C++ runtime. The baseline is written down because GCC on
  RHEL 10 defaults to `x86-64-v3`, which enables AVX in every file, and
  basis_universal, inside KTX, refuses to build its SSE kernels with AVX
  enabled (`#error Please check your compiler options`); and a baseline written
  down is one a package can promise.
- **`arm64-osx-glideslope`** — static libraries, release only, for the same
  reason. libc++ has one ABI in both.
- **Windows uses vcpkg's own `x64-windows-static`**: static libraries against
  the static C runtime, which is what the project links (`CMakeLists.txt`), in
  both debug and release, because MSVC's debug runtime and iterator checks
  cannot be mixed with release libraries.
