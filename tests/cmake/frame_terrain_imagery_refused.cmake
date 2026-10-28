# frame_terrain_imagery_refused.cmake - with every imagery fetch failing, the
# window client still draws its frame, on the DEM's colours, and says why.
#
#   cmake -DPROGRAM=<glideslope> -DCHECK=<glideslope_terrain_check> -DDRIVER=<driver>
#         -DWORK=<dir> -DCACHE=<downloads dir> -DDATA=<data dir> -DPORT=<a port>
#         [-DWEDGE=ON [-DOUT_OF_ORDER=ON]] -P frame_terrain_imagery_refused.cmake
#
# frame_terrain.cmake with REFUSE_IMAGERY, its imagery asked of 127.0.0.1 at
# PORT, a port from the tests' block that no test listens on: every fetch is
# refused. With WEDGE, the client holds the imagery's finer fetches until the
# root tile's has failed, which builds Cesium Native's wedge every run, and
# the test requires the client to give the wedged imagery up; without, the
# refusals land in whatever order they do, and only the frame is held. See
# frame_terrain.cmake for what is held.

cmake_minimum_required(VERSION 3.28)

set(REFUSE_IMAGERY ON)
string(CONCAT REFUSED_AT "http://127.0.0.1:" "${PORT}" "/")
include("${CMAKE_CURRENT_LIST_DIR}/frame_terrain.cmake")
