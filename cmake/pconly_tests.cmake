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
# the gate script parses (it runs on Rocky against the base: tools/port/pconly_gate.sh)
find_program(PC_BASH bash)
if(PC_BASH)
    add_test(NAME pconly_gate_syntax COMMAND ${PC_BASH} -n ${PORT_TOOLS}/pconly_gate.sh)
endif()
