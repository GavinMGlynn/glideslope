### A window, and Metal's video driver, 2026-09-17

CI run 35226940913: macOS passed both presets, Metal headless and in a window;
Ubuntu and Rocky 9 passed with the windowed test on Xvfb. Windows did not build
(below, fixed in the next entry).

**What is missing first:** Direct3D 12 and Metal have still not drawn a frame.
The first CI run of the frame tests failed on both before a device was asked
for, for reasons that had nothing to do with the GPU (below); this commit is
their second attempt. Windows Vulkan needs a Vulkan driver the runner may not
have.

**What CI said about the last commit** (run 35224985264): Ubuntu and Rocky 9
passed, Vulkan on lavapipe. Windows did not compile the client — MSVC's
`<string>` does not bring in `std::runtime_error`, which GCC's does; the client
now includes `<stdexcept>`. macOS failed
`the_client_renders_the_sky_headless_on_metal` in both presets with SDL's
"SDL_HINT_GPU_DRIVER metal unsupported!". SDL's `METAL_PrepareDriver` accepts
only a video driver that can create a Metal view, and headless set SDL's
offscreen video driver, which cannot. On macOS headless now keeps Cocoa's video
driver and simply opens no window; everywhere else it is still `offscreen`.

**A window, tested.** `gfx::Renderer::presented()` counts the frames that
reached the window's swapchain, and the client prints it after a windowed
`--shot`. `the_client_renders_the_sky_in_a_window_on_<driver>` opens a window,
renders ten frames, requires at least one to have been presented, and reads the
frame back as the headless test does. On Linux with no `DISPLAY` or
`WAYLAND_DISPLAY` it reports itself skipped; CI sets
`GLIDESLOPE_REQUIRE_WINDOW`, which makes that a failure, and runs the Linux
tests on Xvfb. The sanitized build runs the windowed test with leak detection
off: the X11 and driver libraries loaded for a window leak allocations whose
stacks hold no frame of glideslope or SDL. The headless test keeps it on.

**Watched to fail:** with the present count never incremented the test failed
with "no frame reached the window's swapchain"; with no display and
`GLIDESLOPE_REQUIRE_WINDOW` set it failed, and without it it skipped.

**Verified locally,** under WSLg: both frame tests pass in `linux-debug`,
presenting 10 of 10 frames to the window.
