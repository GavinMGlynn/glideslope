### Shaders, reversed depth and the floating origin, 2026-09-17

**CI run 35229869171 said:** Metal on macOS and Direct3D 12 (WARP) on Windows
pass every frame test - sky, window, reversed depth, floating origin - in every
preset, and Rocky 9 passes. Ubuntu's sanitized build failed on the leak rule
(below), and Vulkan on Windows could not start: the runner has no Vulkan loader
or driver. CI now installs Mesa's lavapipe from mesa-dist-win 26.2.0 and the
Khronos loader from LunarG's 1.4.357.0 runtime components, each pinned by
SHA-256.

**What was missing first, before that run:** everything here was proved on
Vulkan on Linux alone. The meshes are coloured boxes and quads standing
in for mountains and an aircraft, drawn by one unlit pipeline. There is no
terrain, no texture and no culling.

**The shader toolchain.** Shaders are GLSL, compiled during the build by
`glideslope_shaderc` (`tools/shaderc/main.cpp`): glslang makes SPIR-V,
SPIRV-Cross makes MSL and HLSL, and on Windows D3DCompile makes DXBC. The
output is a C++ source per shader with each form and the resource counts
SDL_GPU asks for, read from the SPIR-V. The compiler checks every resource
against SDL_GPU's documented layout and fails the build naming the one out of
place. `ext/glslang` (16.6.0) and `ext/spirv-cross` (vulkan-sdk-1.4.357.0) are
new submodules; the program links neither. The decision is recorded in
`REQUIREMENTS.md`. The MSL and HLSL for a two-texture fragment shader were read
and put textures, samplers and uniform buffers where SDL_GPU's documentation
says: `[[texture(0)]]`, `[[sampler(0)]]`, `[[buffer(0)]]`; `t0`/`s0` in
`space2`, `b0` in `space3`.

**The renderer draws meshes.** `gfx::Renderer` now has a D32 float depth buffer
and a mesh pipeline, uploads meshes, and draws them from a camera. Positions are
double-precision ECEF until each mesh's transform relative to the camera is
worked out, in double, and narrowed to float (`gfx/scene.hpp`). The projection
is perspective with an infinite far plane and reversed depth: cleared to 0,
kept when greater. The client has `--scene sky|origin|depth` and
`--at LAT,LON,HEIGHT` / `--at-ecef X,Y,Z` to put a scene anywhere on the Earth.
`--size` is parsed without `sscanf`, which MSVC refuses as deprecated.

**The tests.**
- `a_distant_mountain_and_a_nearby_aircraft_draw_with_no_z_fighting_on_<driver>`:
  a face 40 km away with another 1 m in front, and a skin 1 m away with a decal
  1 mm in front, the nearer of each drawn first. Every pixel of both halves,
  shot at the Earth's centre and at Sydney airport, must be the nearer surface.
- `a_scene_draws_the_same_at_the_earths_centre_and_far_from_it_on_<driver>`:
  boxes from 7 cm to 2 m, shot at the Earth's centre, on the equator, at 45 N
  45 E 10 km up and at Sydney on frames 1 and 60 - every file identical, and at
  least a quarter of the pixels not sky.
- Six shader compiler tests: a shader keeping the layout compiles; a uniform
  buffer in the wrong set, a texture not bound from 0, a separate sampler, push
  constants and a GLSL error each fail with the reason.

**Leaks, judged rather than switched off.** Mesa's lavapipe leaks two small
allocations on a worker thread when it builds a pipeline, and Xlib, behind a
window, keeps its resource database and input method; both are unloaded before
exit. `tests/cmake/client.cmake` reads the leak report and judges each leak by
who allocated it - the first frame outside the sanitizer and the C and C++
runtimes: a leak allocated in glideslope or SDL fails the test, one allocated
inside a library loaded at run time does not, even when SDL called it. The
first CI run judged by any frame in SDL, and Ubuntu's Xlib, which keeps frame
pointers, failed it; the Ubuntu report was replayed through the new rule and
all 105 of its leaks were judged Xlib's, and a `new int[1000]` and a leaked
`std::string` in `main` still failed. The windowed test no longer runs with
leak detection off.

**Watched to fail:**
- positions narrowed to float before the camera offset was taken: the equator,
  45 N 45 E and Sydney frames all differed from the centre's;
- conventional depth (near 0.1 m, far 100 km, cleared to 1, kept when less):
  all 1,536 pixels of the far pair showed the face behind, at both places;
- the depth test off: both pairs showed the surface behind in every pixel;
- `new int[1000]` leaked in the client's `main`: the sky test failed, quoting
  the frame in `main.cpp`;
- each shader compiler case, as listed.

CI also said clang-cl compiled SDL's MSVC-only inline functions under this
project's warnings: SDL's include directory came from `SDL3_Headers`, which was
not marked as a system target. It is now.

**Verified locally:** `linux-debug` (sanitized) and `linux-release` pass 55 of
55.
