# The PC-only work's tools (docs/port/PCONLY.md 3): the name maps' check, and rename.py and names_check.py on their
# fixtures (tools/port/tests/names: made-up names). CMakeLists.txt includes this after enable_testing(), where
# PC_PYTHON and PORT_TOOLS are set:
#
#   include(cmake/pconly_tests.cmake)
#
# names_check: every map in tools/port/names and its drop.tsv (none yet passes) against the tree: format,
# duplicates, collisions (where and how), addresses left, old names missing, kinds, labels to drop still used; a map
# the tree has had applied passes too (its rows are "applied already"). Seconds: it reads the C of src/, include/,
# port/ and nonmatching/ once, in worker processes.
add_test(NAME names_check COMMAND ${PC_PYTHON} ${PORT_TOOLS}/names_check.py --root ${CMAKE_SOURCE_DIR} --quiet)
set_tests_properties(names_check PROPERTIES TIMEOUT 300)
# rename.py on its fixture: every place a name lives, what is left, --check, idempotence, git mv, and 5,000 rows
add_test(NAME rename_tool COMMAND ${PC_PYTHON} ${PORT_TOOLS}/tests/test_rename.py)
# names_check.py on maps written for each error and warning
add_test(NAME names_check_tool COMMAND ${PC_PYTHON} ${PORT_TOOLS}/tests/test_names_check.py)
set_tests_properties(rename_tool names_check_tool PROPERTIES TIMEOUT 300)
# one_spelling.py: each constant's meaning one name, defined once in its header (tools/port/consts: the other
# spellings, the files' abbreviations of shared conversions, the numbers that stand for names); --check fails when a
# file still has a copy or another spelling the tables know (after a merge: one_spelling.py --apply). Seconds.
add_test(NAME one_spelling COMMAND ${PC_PYTHON} ${PORT_TOOLS}/one_spelling.py --root ${CMAKE_SOURCE_DIR} --check)
# one_spelling.py on a made-up tree: renames checked by value, copies dropped with their comments, a copy with another
# value reported, conversions inlined, numbers named, a second run changes nothing
add_test(NAME one_spelling_tool COMMAND ${PC_PYTHON} ${PORT_TOOLS}/tests/test_one_spelling.py)
set_tests_properties(one_spelling one_spelling_tool PROPERTIES TIMEOUT 300)
# objsame.py on objects compiled for the test: declarations, local names, comments and moved lines give `same`; code,
# data, strings, references, exports and __LINE__ don't (the tool compares two builds' game objects: PCONLY.md 2)
if(NOT WIN32)
    add_test(NAME objsame_tool COMMAND ${PC_PYTHON} ${PORT_TOOLS}/tests/test_objsame.py --cc ${CMAKE_C_COMPILER})
    # to_headers.py on a made-up tree: an m2c-style file becomes the include and its own views and types, compiles to
    # the same object, and a second run changes nothing (docs/code/README.md "The headers")
    add_test(NAME to_headers_tool COMMAND ${PC_PYTHON} ${PORT_TOOLS}/tests/test_to_headers.py --cc ${CMAKE_C_COMPILER})
endif()
# window_census.py (docs/port/NATIVE.md), wave 3's measure: its patterns on a made-up tree (each kind counts what it
# is for and not its look-alikes, every kind in its place, --baseline), then the census over this tree and build
# without the compiler pass (the tables, nm, the sources: seconds), which must run through.
add_test(NAME window_census_tool COMMAND ${PC_PYTHON} ${PORT_TOOLS}/tests/test_window_census.py)
if(NOT WIN32)
    add_test(NAME window_census COMMAND ${PC_PYTHON} ${PORT_TOOLS}/window_census.py --build ${CMAKE_BINARY_DIR}
        --no-compile --jobs 4)
endif()
# crossings.py (docs/port/NATIVE.md 9), wave 3's proof of each step: its report on made-up dumps (the kinds in their
# order, sites by name, the guard's instructions, the PC layer's sites named with addr2line), and its checks
# (--zero-kind, --zero-region, --zero-site, --no-faults, --baseline) failing as they should.
add_test(NAME crossings_tool COMMAND ${PC_PYTHON} ${PORT_TOOLS}/tests/test_crossings.py)
# In the counting build (cmake -DRUSH_COUNT_CROSSINGS=ON; ctest -L crossings): a few synthetic paths counted, then again
# in the arena guard's counting mode, each run ending as its script expects with its dump written, and every guarded
# run's trace its plain run's (the guard changes nothing). On today's layout the arenas are at their N64 addresses, so
# the guard faults on the game's every access by name (--expect-faults); NATIVE.md step 5 makes that --no-faults.
if(RUSH_COUNT_CROSSINGS)
    add_test(NAME crossings_paths COMMAND ${PC_PYTHON} ${PORT_TOOLS}/crossings.py --exe $<TARGET_FILE:rush2049pc>
        --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/crossings/test --guard --check --expect-faults -j 3
        menu race2p stunt)
    set_tests_properties(crossings_paths PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 600 LABELS crossings)
    # The guard's stopping mode (RUSH2049_WINDOW_GUARD=arenas): the first access to an arena by its N64 address stops
    # the program with a note naming the datum and the instruction, then the fault handler's report. Today the first
    # access by name does (--expect-faults); step 5 makes this --check --no-faults: every path runs through.
    add_test(NAME crossings_guard_stop COMMAND ${PC_PYTHON} ${PORT_TOOLS}/crossings.py --exe $<TARGET_FILE:rush2049pc>
        --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/crossings/test-stop --stop --expect-faults -j 2 menu race2p)
    set_tests_properties(crossings_guard_stop PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 600 LABELS crossings)
endif()
# the gate script parses (it runs on Rocky against the base: tools/port/pconly_gate.sh)
find_program(PC_BASH bash)
if(PC_BASH)
    add_test(NAME pconly_gate_syntax COMMAND ${PC_BASH} -n ${PORT_TOOLS}/pconly_gate.sh)
endif()
