# run_copilot_sources.cmake - run the copilot's source rules over the tree at
# ROOT, in script mode. One tree per process: a violation ends it.
cmake_minimum_required(VERSION 3.28)
include("${GLIDESLOPE_ROOT}/cmake/Copilot.cmake")
glideslope_check_copilot_sources("${ROOT}")
