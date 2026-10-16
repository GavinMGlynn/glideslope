### A headless client drawing ten thousand frames keeps its memory level, 2026-09-26 — tail done

**Cause: nothing waited for the GPU when there was no window.** With a window,
`SDL_WaitAndAcquireGPUSwapchainTexture` waits until the GPU has caught up to
within the swapchain's frames in flight. Headless there is no swapchain, and
nothing else waited: SDL_GPU keeps every command buffer submitted - its
uniform buffers, its fence, the overlay's transfer buffer - until its fence
has signalled, and cleans up only those already done at each submit (SDL's
Vulkan backend, `VULKAN_Submit`). Mesa's lavapipe draws on the CPU, slower
than the client submits, so the queue of frames not yet drawn grew without
end. It was not a leak: every resource was released, just never reached.
Measured in WSL on lavapipe, release build, 320x240, the flight screen: 125
MiB at frame 1, 558 at 500, 991 at 1,000, 1,601 at 1,750, and a segmentation
fault inside the driver before frame 2,000 - 0.8 MiB a frame, with 18 GB of
the machine still free.

**A hidden or minimised window had the same queue.** SDL's Vulkan acquire
returns at once, with no image and no error, when the window is hidden or its
swapchain cannot yet be made again (`SDL_gpu_vulkan.c`, the
`SDL_WINDOW_HIDDEN` and `VULKAN_INTERNAL_TRY_AGAIN` cases), so a minimised
client queued frames exactly as a headless one did.

**Now** the renderer keeps the fence of every frame, with a window or
without, and each frame begins by waiting for the one two before it - before
the overlay's upload and before its command buffer is acquired, so a failed
wait leaves nothing half made (`Renderer::in_flight_`, `gfx/renderer.hpp`).
It is what a swapchain of two does - two is SDL's default frames in flight,
and the ring must follow it if that is ever raised. With a visible window the
renderer's own wait now comes first, at the top of the frame; the swapchain's
wait comes later and finds the same frame already drawn, so what a visible
window does is unchanged. The fences are waited on and released with the
renderer.

**New test flags**, for this test: `--shot-frame N` shoots frame N with every
frame two ticks however many frames that is (`--shot-at` lengthens frames to
keep a long flight to 300), and `--memory-every N` prints the memory the
process holds every N frames - `platform::memory_held_bytes()`, the resident
set on Linux, private bytes on Windows (the working set is trimmed at the
system's whim), the physical footprint on macOS (`TASK_VM_INFO`'s
`phys_footprint`, Apple's own figure; the resident size misses what the
system has compressed). The macOS branch is compiled first by CI: there is no
Mac here. The client's comment on why an
online shot draws only its own frame now gives the reason that remains: the
machine's time, not its memory.

**Verified** by `a_headless_client_drawing_ten_thousand_frames_keeps_its_memory_level_on_<driver>`
(`tests/cmake/client_memory.cmake`).
- **The run.** The terrain screen, headless, 160x120 - Sydney from 1,500 m
  over Botany Bay, looking up the harbour, the imagery and its credits drawn -
  for ten thousand frames, counted in frames, sampled at frame 1 and every
  500th: 21 samples, each of which must be read. Then frame 10,001 is shot.
- **Why the terrain screen, and small.** It was the flight screen at 320x240
  first, and took 283 s in the sanitized build, near CI's thirty-minute limit
  on a shard once a slower runner is allowed for. Sampling the sanitized
  client's stack with gdb put most of each frame in JSBSim's steps and the
  HUD's text, none of it the GPU's, and 64x48 was barely quicker than
  320x240. Every frame of every screen goes through the renderer's queue the
  same way - a command buffer, its uniforms, the overlay's upload - so the
  terrain screen tests it as well, in a fifth of the time. Its timeout is
  900 s, half the shard's limit, so a slow run fails the test, not the shard.
- **Why the shot is the frame after.** The frame shot waits for every terrain
  tile its view needs, which by itself takes the client up hundreds of MiB,
  whether it is the second frame or the ten-thousand-and-first. That is the
  shot, not the frames.
- **The bound.** From frame 1,000, when the terrain, Cesium Native's caches
  and the driver's pools have settled, to frame 10,000, the highest sample
  may be at most 64 MiB above the lowest - the lowest, not the one at frame
  1,000, so a sample that caught a peak cannot hide growth after it. 64 MiB
  is eighty frames of the old growth.
- **The figures**, frame 1 / 1,000 / 10,000, the range from frame 1,000, and
  the test's time:
  - WSL, lavapipe, linux-release: 80 / 108 / 110 MiB, ranged 2 MiB (17 s).
  - WSL, lavapipe, linux-debug (sanitized): 298 / 532 / 543 MiB, ranged
    17 MiB, 529 to 546, with the sanitizer's quarantine (69 s alone, 117 s
    beside three windowed tests at `-j4`).
  - Windows, windows-debug (`tools/windows_build.sh`): Direct3D 12 103 / 103 /
    107 MiB, ranged 4 MiB (94 s, from 174 s); Vulkan (lavapipe) 208 / 208 /
    209 MiB, ranged 1 MiB (176 s, from 240 s). The client's sky in a window
    passed there on both.
  - The flight screen at 320x240, before, for comparison: 124 / 157 / 160 MiB
    in the release build, 453 / 609 / 601 MiB sanitized; on Windows Direct3D
    12 133 / 137 / 142 MiB and lavapipe 229 / 231 / 234 MiB.
- **Offline, it is skipped, not failed.** The terrain screen drapes EOX's
  imagery, and with no imagery the shot refuses its tiles ("11 terrain tiles
  were drawn without their imagery"), so the test skips on that as on a DEM it
  could not download, as `frame_terrain.cmake` does. Built on purpose: the
  test run under `unshare -rn` (no network) against a copy of the DEM and
  geoid with an empty Cesium cache exited 77; with
  `GLIDESLOPE_REQUIRE_NETWORK` set it failed instead.
- **The windowed tests still pass** with the fences on the window's path:
  the client rendering the sky in a window, the client with the window
  joining a server, flying its aircraft and riding along, and the server's
  window, sanitized, in WSLg.
- **Seen to fail.** With the frames submitted without their fences, as
  before, the test failed: `glideslope exited Segmentation fault`, its
  samples printed as they were taken - 77, 565, 993 and 1,361 MiB at frames
  1, 500, 1,000 and 1,500 (the flight screen's, the first time: 125, 558, 955
  and 1,321). The bound's own branch was seen to fail too, with the bound
  made 5 MiB: "from frame 1000 to frame 10000 the memory held ranged 17 MiB,
  from 532 to 549: more than 5 MiB". Both reverted.
