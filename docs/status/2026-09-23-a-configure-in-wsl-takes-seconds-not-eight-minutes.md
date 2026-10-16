### A configure in WSL takes seconds, not eight minutes, 2026-09-23

**A configure of the Linux debug build took 8 minutes 21 seconds; it takes 2
to 3 now.** Profiled with CMake's own `--profiling-output`: almost all of it
was `find_path` and `find_file` searching the Windows directories WSL appends
to PATH, through WSL's slow share of the Windows drive - 224 seconds looking
for ICU's data files, about 150 in SDL's OpenGL and X11 checks looking for
GL/xmesa.h, 35 for liburing.h - searches that fail and so are made again at
every configure. `CMakeLists.txt` takes `/mnt/*` off PATH before anything else,
only when WSL's own variables say it is WSL. SDL still finds X11, Wayland,
OpenGL and Vulkan. **vcpkg's install is skipped when nothing that decides it
has changed**: a stamp in the install directory holds a hash of the manifest,
its configuration, the triplets, the overlay ports and vcpkg's commit. It was
fifty seconds of checking at every configure; a new directory, which is every
CI job's, has no stamp and installs.

This is the cost that made every new lesson file, which a configure-time glob
picks up, a ten-minute wait.

**A Windows crash is dumped by Windows too.** Two tests have crashed on the
way out on Windows debug with nothing printed by the program's own handlers.
The Windows test jobs now keep Windows Error Reporting's local dumps and read
each one's stack with cdb after a failed shard, and upload the dumps.
