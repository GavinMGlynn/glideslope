# run_layering.cmake - run the include check over the tree at ROOT, in script
# mode. One tree per process, because a violation is a FATAL_ERROR and ends it.
include("${GLIDESLOPE_ROOT}/cmake/Layering.cmake")
glideslope_check_layering("${ROOT}")
