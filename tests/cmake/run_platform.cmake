# run_platform.cmake - run the platform gate against a toolchain described by -D
# flags. One case per process, because a refusal is a FATAL_ERROR and ends it.
include("${ROOT}/cmake/Platform.cmake")
glideslope_check_platform()
