# Vcpkg.cmake - vcpkg, for Cesium Native's dependencies.
#
# **Cesium Native is built from source, and its dependencies through vcpkg**, as
# Cesium Native itself is (REQUIREMENTS.md, section 2). They are thirty
# libraries - Abseil, S2, OpenSSL, Draco, KTX and the rest - each with its own
# build, and vcpkg is how Cesium Native pins and builds them on every platform.
#
# **Included before project()**, because vcpkg's toolchain has to be in place
# when the first project() runs. It does not install anything there, though:
# the packages are built by glideslope_vcpkg_install(), called once the
# platform gate has accepted the compiler, so that a compiler the project
# refuses is refused at once, not after it has built thirty libraries.
#
# **vcpkg is pinned to a commit** - the one Cesium Native's release was built
# against - and kept outside the tree, since it is a tool and not a dependency:
# cloned at that commit, once, into the user's cache directory - on Windows,
# `C:\gs-vcpkg`, for MSVC's limit on the length of a path - or
# GLIDESLOPE_VCPKG_ROOT, and bootstrapped there. The ports and their versions
# are that checkout's. Built packages are kept in vcpkg's binary cache, so a
# second build directory, or a CI run with the cache restored, unpacks them
# rather than building them again.
#
# A build that already names a toolchain file - its own vcpkg - is left alone.

set(GLIDESLOPE_VCPKG_COMMIT "56bb2411609227288b70117ead2c47585ba07713")
set(GLIDESLOPE_VCPKG_REPOSITORY "https://github.com/microsoft/vcpkg.git")

if(NOT CMAKE_TOOLCHAIN_FILE)
    if(NOT "$ENV{GLIDESLOPE_VCPKG_ROOT}" STREQUAL "")
        set(_vcpkg_base "$ENV{GLIDESLOPE_VCPKG_ROOT}")
    elseif(CMAKE_HOST_WIN32)
        # Short, because MSVC cannot open a path longer than 260 characters,
        # and vcpkg builds each package's sources deep under its root: under
        # the user's AppData, Draco's object files were past it.
        set(_vcpkg_base "$ENV{SystemDrive}/gs-vcpkg")
    elseif(CMAKE_HOST_APPLE)
        set(_vcpkg_base "$ENV{HOME}/Library/Caches/glideslope/vcpkg")
    elseif(NOT "$ENV{XDG_CACHE_HOME}" STREQUAL "")
        set(_vcpkg_base "$ENV{XDG_CACHE_HOME}/glideslope/vcpkg")
    else()
        set(_vcpkg_base "$ENV{HOME}/.cache/glideslope/vcpkg")
    endif()
    # Named by the commit's first twelve characters, for the same reason.
    string(SUBSTRING "${GLIDESLOPE_VCPKG_COMMIT}" 0 12 _vcpkg_short)
    file(TO_CMAKE_PATH "${_vcpkg_base}/${_vcpkg_short}" _vcpkg)

    # Two configures at once - two presets in parallel - take turns.
    file(MAKE_DIRECTORY "${_vcpkg_base}")
    file(LOCK "${_vcpkg}.lock" GUARD FILE TIMEOUT 3600)
    if(NOT EXISTS "${_vcpkg}/.glideslope-bootstrapped")
        message(STATUS "vcpkg: fetching ${GLIDESLOPE_VCPKG_COMMIT} into ${_vcpkg}")
        find_program(GLIDESLOPE_GIT git REQUIRED)
        file(REMOVE_RECURSE "${_vcpkg}")
        file(MAKE_DIRECTORY "${_vcpkg}")
        string(TIMESTAMP _t0 "%s" UTC)
        execute_process(COMMAND "${GLIDESLOPE_GIT}" init --quiet
                        WORKING_DIRECTORY "${_vcpkg}" RESULT_VARIABLE _rc)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "vcpkg: git init failed (${_rc})")
        endif()
        # **A clone already on the machine is asked first.** GitHub's runners
        # carry vcpkg's whole history at VCPKG_INSTALLATION_ROOT; the pinned
        # commit is fetched from there when it has it - the same commit, so
        # the same tree, checked by git's hashes - and from GitHub otherwise.
        # Fetching from GitHub took 19 to 60 s of a Windows configure on CI
        # (PROJECT_STATUS, 2026-10-09).
        set(_from "${GLIDESLOPE_VCPKG_REPOSITORY}")
        set(_fetched FALSE)
        file(TO_CMAKE_PATH "$ENV{VCPKG_INSTALLATION_ROOT}" _local)
        if(NOT _local STREQUAL "" AND EXISTS "${_local}/.git")
            # file:// with --depth, since a plain path ignores it; a Windows
            # path, C:/vcpkg, takes a third slash: file:///C:/vcpkg.
            if(_local MATCHES "^/")
                set(_url "file://${_local}")
            else()
                set(_url "file:///${_local}")
            endif()
            execute_process(COMMAND "${GLIDESLOPE_GIT}" fetch --quiet --depth 1
                                    "${_url}" ${GLIDESLOPE_VCPKG_COMMIT}
                            WORKING_DIRECTORY "${_vcpkg}" RESULT_VARIABLE _rc
                            OUTPUT_QUIET ERROR_VARIABLE _err)
            if(_rc EQUAL 0)
                set(_from "${_local}")
                set(_fetched TRUE)
            else()
                string(STRIP "${_err}" _err)
                message(STATUS "vcpkg: ${_local} could not give the commit (${_rc}: ${_err}); "
                               "fetching from GitHub")
            endif()
        endif()
        if(NOT _fetched)
            execute_process(COMMAND "${GLIDESLOPE_GIT}" fetch --quiet --depth 1
                                    ${GLIDESLOPE_VCPKG_REPOSITORY} ${GLIDESLOPE_VCPKG_COMMIT}
                            WORKING_DIRECTORY "${_vcpkg}" RESULT_VARIABLE _rc)
            if(NOT _rc EQUAL 0)
                message(FATAL_ERROR "vcpkg: git fetch from ${GLIDESLOPE_VCPKG_REPOSITORY} failed (${_rc})")
            endif()
        endif()
        string(TIMESTAMP _t1 "%s" UTC)
        execute_process(COMMAND "${GLIDESLOPE_GIT}" -c advice.detachedHead=false
                                checkout --quiet FETCH_HEAD
                        WORKING_DIRECTORY "${_vcpkg}" RESULT_VARIABLE _rc)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "vcpkg: git checkout failed (${_rc})")
        endif()
        string(TIMESTAMP _t2 "%s" UTC)
        math(EXPR _fetch_s "${_t1} - ${_t0}")
        math(EXPR _checkout_s "${_t2} - ${_t1}")
        message(STATUS "vcpkg: fetched from ${_from} in ${_fetch_s} s, checked out in ${_checkout_s} s")
        if(CMAKE_HOST_WIN32)
            set(_bootstrap "${_vcpkg}/bootstrap-vcpkg.bat")
        else()
            set(_bootstrap "${_vcpkg}/bootstrap-vcpkg.sh")
        endif()
        execute_process(COMMAND "${_bootstrap}" -disableMetrics
                        WORKING_DIRECTORY "${_vcpkg}" RESULT_VARIABLE _rc)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "vcpkg: bootstrapping failed (${_rc})")
        endif()
        string(TIMESTAMP _t3 "%s" UTC)
        math(EXPR _bootstrap_s "${_t3} - ${_t2}")
        message(STATUS "vcpkg: bootstrapped in ${_bootstrap_s} s")
        file(TOUCH "${_vcpkg}/.glideslope-bootstrapped")
    endif()
    file(LOCK "${_vcpkg}.lock" RELEASE)
    set(CMAKE_TOOLCHAIN_FILE "${_vcpkg}/scripts/buildsystems/vcpkg.cmake"
        CACHE FILEPATH "The toolchain file: vcpkg's")
endif()

# The triplets are in cmake/triplets, whose README says why each is what it is.
if(NOT VCPKG_TARGET_TRIPLET)
    if(CMAKE_HOST_WIN32)
        set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
    elseif(CMAKE_HOST_APPLE)
        set(VCPKG_TARGET_TRIPLET "arm64-osx-glideslope" CACHE STRING "")
    else()
        set(VCPKG_TARGET_TRIPLET "x64-linux-glideslope" CACHE STRING "")
    endif()
endif()
set(VCPKG_MANIFEST_INSTALL OFF CACHE BOOL "glideslope_vcpkg_install() installs, after the platform gate" FORCE)
set(ENV{VCPKG_DISABLE_METRICS} 1)

# Installs what vcpkg.json lists, for the target triplet, into the build
# directory, as vcpkg's toolchain would have at project(). Build trees are
# removed as each package is built: they are large, and nothing reads them
# after. The whole of vcpkg's output is kept in vcpkg-install.log, and its
# summary - compiler, restores, builds, uploads - printed.
function(glideslope_vcpkg_install)
    if(NOT DEFINED Z_VCPKG_ROOT_DIR)
        message(FATAL_ERROR "vcpkg's toolchain was not loaded: CMAKE_TOOLCHAIN_FILE is "
                            "${CMAKE_TOOLCHAIN_FILE}")
    endif()
    if(CMAKE_HOST_WIN32)
        set(_exe "${Z_VCPKG_ROOT_DIR}/vcpkg.exe")
    else()
        set(_exe "${Z_VCPKG_ROOT_DIR}/vcpkg")
    endif()
    set(_host "")
    if(NOT "${VCPKG_HOST_TRIPLET}" STREQUAL "")
        set(_host "--host-triplet=${VCPKG_HOST_TRIPLET}")
    endif()
    # **Nothing to install is not worth fifty seconds.** vcpkg checks every
    # package on every configure, which took fifty seconds when nothing had
    # changed. What decides what is installed is the manifest, its
    # configuration, the triplets, the overlay ports and vcpkg's own commit;
    # when none of those has changed since the last install into this
    # directory, it is skipped. A new build directory - every CI job - has no
    # stamp, and installs.
    file(GLOB_RECURSE _inputs
         "${CMAKE_SOURCE_DIR}/cmake/triplets/*" "${CMAKE_SOURCE_DIR}/cmake/ports/*"
         "${CMAKE_SOURCE_DIR}/ext/cesium-native/extern/vcpkg/ports/*")
    list(SORT _inputs)
    set(_what "${GLIDESLOPE_VCPKG_COMMIT} ${VCPKG_TARGET_TRIPLET} ${VCPKG_HOST_TRIPLET}")
    foreach(_input IN ITEMS "${CMAKE_SOURCE_DIR}/vcpkg.json"
                            "${CMAKE_SOURCE_DIR}/vcpkg-configuration.json" ${_inputs})
        file(SHA256 "${_input}" _hash)
        string(APPEND _what " ${_input}=${_hash}")
    endforeach()
    string(SHA256 _stamp_wanted "${_what}")
    set(_stamp "${_VCPKG_INSTALLED_DIR}/.glideslope-installed")
    if(EXISTS "${_stamp}")
        file(READ "${_stamp}" _stamp_had)
        if(_stamp_had STREQUAL _stamp_wanted)
            message(STATUS "vcpkg: nothing has changed since the last install; not run")
            set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY
                         CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/vcpkg.json"
                         "${CMAKE_SOURCE_DIR}/vcpkg-configuration.json")
            return()
        endif()
    endif()
    message(STATUS "vcpkg: installing Cesium Native's dependencies for ${VCPKG_TARGET_TRIPLET}")
    execute_process(
        COMMAND "${_exe}" install
            --triplet "${VCPKG_TARGET_TRIPLET}" ${_host}
            --vcpkg-root "${Z_VCPKG_ROOT_DIR}" --x-wait-for-lock
            "--x-manifest-root=${CMAKE_SOURCE_DIR}" "--x-install-root=${_VCPKG_INSTALLED_DIR}"
            --clean-after-build
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
    file(WRITE "${CMAKE_BINARY_DIR}/vcpkg-install.log" "${_out}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "vcpkg install failed (${_rc}):\n${_out}")
    endif()
    # **What vcpkg did, not only that it finished**: the compiler whose hash
    # is in every package's ABI, what was restored and from where, and each
    # package built instead, with its time and its upload, and an upload
    # refused (NuGet's "failed", its status code). A configure that
    # took twenty minutes says why in its own output (a CI job's log), not
    # only in vcpkg-install.log.
    string(REPLACE "\r" "" _summary "${_out}")
    string(REPLACE ";" "," _summary "${_summary}")
    string(REGEX MATCHALL
           "(^|\n)((Detecting compiler|Compiler found|Restored|Building|Completed submission|Waiting for|Stored binaries|Uploading|Pushing|All requested|warning|error)[^\n]*|Elapsed time[^\n]*: [0-9.]+ (s|min|h)|[^\n]*([Ff]ailed|status code)[^\n]*)"
           _lines "${_summary}")
    foreach(_line IN LISTS _lines)
        string(STRIP "${_line}" _line)
        message(STATUS "vcpkg: ${_line}")
    endforeach()
    file(WRITE "${_stamp}" "${_stamp_wanted}")
    set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                 "${CMAKE_SOURCE_DIR}/vcpkg.json" "${CMAKE_SOURCE_DIR}/vcpkg-configuration.json")
endfunction()
