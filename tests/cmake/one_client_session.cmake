# one_client_session.cmake - both clients join, read, stall and say goodbye
# through one session (net::ClientSession), and every test of joining again,
# going back, being let go or saying goodbye runs one of them.
#
# The client's half of a session was written twice - `glideslope_cli
# connect`'s own and the shared one the window client uses - and a bug was
# found in one and not the other. This holds it written once:
#
# 1. **No client source has a session of its own.** Every first-party source
#    of the clients (src/frontend/cli, src/frontend/client, and the frontend
#    code they share; not src/frontend/server, which is the other half) is
#    read, comments left out, for the pieces a session is made of: the
#    handshake's `Initiator`, `Sealer` and `Unsealer`, the reliable layer,
#    `Rejoin`, the rules for believing a refusal (`lets_go`,
#    `refusal_from`), knocks (`sealed_knock`), reading an update
#    (`read_state`) and the server's goodbye (`is_leaving`). None may be
#    there: those are net::ClientSession's.
# 2. **Both clients join through it**: the window client by
#    `ClientSession::connect`, the command-line client by
#    `ClientSession::join`.
# 3. **Every test of joining again, going back, being let go or saying
#    goodbye runs one of the two** - by its name, among every test in
#    tests/CMakeLists.txt - so each passes through that one session. How many
#    there are is stated, and a new one fails this until it is counted. Left
#    out, by name: the tests of a flag refused before anything connects
#    (GLIDESLOPE_EXPECT_RUN), which run no session at all, and this one.
#
# -DSOURCE=the source tree's root.

if(NOT SOURCE)
    message(FATAL_ERROR "one_client_session.cmake needs -DSOURCE")
endif()

# --- 1. no client source has a session of its own ---------------------------
file(GLOB _sources
    ${SOURCE}/src/frontend/cli/*.cpp ${SOURCE}/src/frontend/cli/*.hpp
    ${SOURCE}/src/frontend/client/*.cpp ${SOURCE}/src/frontend/client/*.hpp
    ${SOURCE}/src/frontend/*.cpp ${SOURCE}/src/frontend/*.hpp)
list(LENGTH _sources _source_count)
# The clients' sources number more than this; fewer is a glob gone wrong.
if(_source_count LESS 20)
    message(FATAL_ERROR "only ${_source_count} client sources found under ${SOURCE}/src/frontend")
endif()
set(_pieces Initiator Sealer Unsealer Reliable Rejoin lets_go refusal_from sealed_knock
    read_state is_leaving)
list(LENGTH _pieces _piece_count)
set(_own "")
foreach(_file IN LISTS _sources)
    file(STRINGS ${_file} _lines)
    set(_code "")
    foreach(_line IN LISTS _lines)
        # Comments are not code: a comment may name the session's pieces.
        string(REGEX REPLACE "//.*$" "" _line "${_line}")
        string(APPEND _code " ${_line}")
    endforeach()
    foreach(_piece IN LISTS _pieces)
        if(_code MATCHES "[^A-Za-z0-9_]${_piece}[^A-Za-z0-9_]")
            file(RELATIVE_PATH _rel ${SOURCE} ${_file})
            list(APPEND _own "${_rel} uses ${_piece}")
        endif()
    endforeach()
endforeach()
if(_own)
    string(REPLACE ";" "\n  " _said "${_own}")
    message(FATAL_ERROR "a client has a session of its own - these are "
                        "net::ClientSession's:\n  ${_said}")
endif()
message(STATUS "${_source_count} client sources read for ${_piece_count} of a session's "
               "pieces: none has any")

# --- 2. both clients join through it ----------------------------------------
file(READ ${SOURCE}/src/frontend/client/main.cpp _window)
file(READ ${SOURCE}/src/frontend/cli/main.cpp _cli)
if(NOT _window MATCHES "ClientSession::connect\\(")
    message(FATAL_ERROR "the window client does not join by ClientSession::connect")
endif()
if(NOT _cli MATCHES "ClientSession::join\\(")
    message(FATAL_ERROR "the command-line client does not join by ClientSession::join")
endif()
message(STATUS "both clients join through net::ClientSession")

# --- 3. every such test runs one of the two ---------------------------------
set(_expected 16)
# This test is one by its name, and runs no client: it reads.
set(_this_test both_clients_join_read_stall_and_say_goodbye_through_one_session)
set(_names "joins_again|join_again|joining_again|goes_back_to_it|without_going_back|let_go|"
           "stalled_past|started_again|goodbye|is_leaving|_dropped")
string(REPLACE ";" "" _names "${_names}")
file(STRINGS ${SOURCE}/tests/CMakeLists.txt _lines)
set(_tests 0)
set(_current "")
set(_found "")
set(_wrong "")
set(_left_out "")
set(_all 0)
macro(_close_test)
    if(_current)
        if(_runner STREQUAL "expect_run" OR _current STREQUAL _this_test)
            list(APPEND _left_out "${_current}")
        elseif(_client STREQUAL "glideslope_cli" OR _client STREQUAL "glideslope")
            math(EXPR _tests "${_tests} + 1")
            list(APPEND _found "${_current} (${_client})")
        else()
            list(APPEND _wrong "${_current} runs '${_client}'")
        endif()
    endif()
    set(_current "")
endmacro()
foreach(_line IN LISTS _lines)
    if(_line MATCHES "^add_test\\(NAME ([a-z0-9_]+)")
        _close_test()
        math(EXPR _all "${_all} + 1")
        set(_name ${CMAKE_MATCH_1})
        if(_name MATCHES "${_names}")
            set(_current ${_name})
            set(_client "")
            set(_runner "")
        endif()
    elseif(_current)
        if(_line MATCHES "-DCLIENT=\\$<TARGET_FILE:([a-z_]+)>")
            set(_client ${CMAKE_MATCH_1})
        endif()
        if(_line MATCHES "GLIDESLOPE_EXPECT_RUN")
            set(_runner "expect_run")
        endif()
        if(_line MATCHES "^set_tests_properties|^file\\(|^$")
            _close_test()
        endif()
    endif()
endforeach()
_close_test()
if(_wrong)
    string(REPLACE ";" "\n  " _said "${_wrong}")
    message(FATAL_ERROR "a test of joining again, going back or goodbye runs neither client:"
                        "\n  ${_said}")
endif()
string(REPLACE ";" "\n  " _said "${_found}")
string(REPLACE ";" ", " _left "${_left_out}")
if(NOT _tests EQUAL _expected)
    message(FATAL_ERROR "${_tests} tests of joining again, going back, being let go or goodbye "
                        "among ${_all}, where ${_expected} are counted:\n  ${_said}\nCount a "
                        "new one here, or say why it is left out.")
endif()
message(STATUS "${_tests} of ${_expected} tests of joining again, going back, being let go or "
               "goodbye, among ${_all}, run a client on the one session:\n  ${_said}\n"
               "left out - this test, and a flag refused before anything connects: ${_left}")
