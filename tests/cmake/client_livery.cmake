# client_livery.cmake - the aeroplane wears its livery, and its ailerons move
# with the stick.
#
#   cmake -DPROGRAM=<glideslope> -DCHECK=<glideslope_surfaces_check>
#         -DDRIVER=<driver> -DWORK=<dir> -DCACHE=<downloads dir>
#         -DMODELS=<data/models> -P client_livery.cmake
#
# Flies the Cessna headless and shoots it from behind three times at tick 2:
# with the stick held full left (--hold-stick -1,0,0), held full right, and
# with the aeroplane not drawn. Two ticks are a sixtieth of a second: her
# ailerons are over at once and she has not yet banked, so the two stick shots
# differ by her ailerons alone. glideslope_surfaces_check finds her outline
# from the shots with and without her, the livery's trim colour on it, and
# what the stick moved - out on both wings, not the fuselage.
#
# The trim's colour is read from data/models/c172p.livery, its cheatline's,
# so the test asks for what the data says she wears.
#
# The flight stands on the DEM and draws it, so it needs the tiles and the
# geoid, fetched into CACHE: without the network the test is skipped (exit 77)
# unless GLIDESLOPE_REQUIRE_NETWORK is set. No imagery: the three shots are
# three launches, and an imagery tile that streams in one and not another
# would differ where it lies (client_views.cmake says how that was seen).

cmake_minimum_required(VERSION 3.28)
include("${CMAKE_CURRENT_LIST_DIR}/client.cmake")

file(MAKE_DIRECTORY "${WORK}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
set(ENV{LSAN_OPTIONS} "exitcode=0")

set(_aircraft c172p)

function(shoot name)
    set(_shot "${WORK}/livery-${DRIVER}-${name}.bmp")
    file(REMOVE "${_shot}")
    execute_process(
        COMMAND "${PROGRAM}" --headless --gpu-driver "${DRIVER}" --size 320x240
                --screen flight --aircraft ${_aircraft} --view behind --imagery off
                ${ARGN} --shot-at 2 --shot "${_shot}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    glideslope_skip_when_not_downloaded("${_rc}" "${_err}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "glideslope ${ARGN} exited ${_rc}\n${_err}")
    endif()
    glideslope_judge_leaks("${_err}")
endfunction()

file(STRINGS "${MODELS}/${_aircraft}.livery" _cheatline REGEX "^coat cheatline ")
if(NOT _cheatline MATCHES "^coat cheatline +([0-9]+) +([0-9]+) +([0-9]+) ")
    message(FATAL_ERROR "no cheatline in ${MODELS}/${_aircraft}.livery")
endif()
set(_trim ${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3})

shoot(left --hold-stick -1,0,0)
shoot(right --hold-stick 1,0,0)
shoot(without --hold-stick -1,0,0 --draw-aircraft off)

execute_process(COMMAND "${CHECK}" "${WORK}/livery-${DRIVER}-left.bmp"
                        "${WORK}/livery-${DRIVER}-right.bmp"
                        "${WORK}/livery-${DRIVER}-without.bmp" ${_trim}
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "${_out}${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the Cessna drawn: ${_err}")
endif()
message(STATUS "the Cessna wears her livery, and her ailerons follow the stick")
