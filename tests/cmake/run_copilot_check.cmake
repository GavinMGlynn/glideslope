# run_copilot_check.cmake - run the copilot's names check over the tree at
# ROOT, in script mode. One tree per process, because a violation is a
# FATAL_ERROR and ends it.
cmake_minimum_required(VERSION 3.28)
include("${GLIDESLOPE_ROOT}/cmake/Copilot.cmake")
glideslope_check_copilot("${ROOT}")
