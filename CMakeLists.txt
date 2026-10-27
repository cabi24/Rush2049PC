# PC build of San Francisco Rush 2049 from the decompilation's C (docs/PORT_PLAN.md).
# The N64 matching build is the Makefile; nothing here is used by it.
#
#   cmake -S . -B build/pc -G Ninja
#   cmake --build build/pc
#   python3 tools/port/census.py --build build/pc      # docs/port/STATUS.md
cmake_minimum_required(VERSION 3.20)
project(Rush2049PC C)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # tools/port/census.py reads compile_commands.json
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
set(PORT_TOOLS ${CMAKE_SOURCE_DIR}/tools/port)

# ---------------------------------------------------------------- game code
# Every C file of the decompilation, compiled for the host with TARGET_PC.
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.c)
# libultra/libc units the PC layer replaces. They also use IDO-only syntax (__unaligned, IDO varargs).
set(GAME_REPLACED
    src/rom/lib_11640.c
    src/rom/lib_34a0.c)
foreach(f ${GAME_REPLACED})
    list(REMOVE_ITEM GAME_SOURCES ${CMAKE_SOURCE_DIR}/${f})
endforeach()

# Compile settings shared by the probe and the real build.
add_library(game_flags INTERFACE)
target_compile_definitions(game_flags INTERFACE TARGET_PC)
# The game defines its own main and these C library functions. Renamed, the game keeps calling its own N64
# versions, and the host C library (and SDL, GPU drivers) keeps its own.
foreach(name main memcpy memset memchr strlen strchr sinf cosf)
    target_compile_definitions(game_flags INTERFACE ${name}=n64_${name})
endforeach()
target_include_directories(game_flags INTERFACE include include/game include/PR)
# IDO semantics: gnu89, so `__inline` functions also get an out-of-line definition (C99 rules would drop it,
# and CMake's C_STANDARD adds no flag when the compiler's default is newer). Plain char is unsigned and signed
# overflow wraps. The code type-puns freely, and some files define the same tentative global (common symbols).
target_compile_options(game_flags INTERFACE -std=gnu89 -funsigned-char -fwrapv -fno-strict-aliasing -fcommon)
# Silence what the N64-era declarations trip over; the 64-bit pointer warnings (-Wpointer-to-int-cast,
# -Wint-to-pointer-cast) stay on, since they are the porting work list (docs/port/STATUS.md).
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(game_flags INTERFACE -Wno-builtin-declaration-mismatch
        -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types
        -Wno-discarded-qualifiers -Wno-overflow)
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(game_flags INTERFACE -Wno-incompatible-library-redeclaration
        -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types
        -Wno-incompatible-pointer-types-discards-qualifiers -Wno-constant-conversion)
endif()

# Many files define copies of functions another file owns (group context for IDO's optimizer, stand-ins).
# The probe compiles every file once (through a wrapper, so the real sources can carry their own options),
# tools/port/gen_weak.py reads what each file defines and writes build/pc/weak/<file>.h with `#pragma weak`
# for the copies, and the real build force-includes it: one strong definition per symbol.
set(PROBE_DIR ${CMAKE_BINARY_DIR}/probe)
set(WEAK_DIR ${CMAKE_BINARY_DIR}/weak)
set(PROBE_SOURCES "")
set(WEAK_HEADERS "")
foreach(src ${GAME_SOURCES})
    file(RELATIVE_PATH rel ${CMAKE_SOURCE_DIR} ${src})
    string(REGEX REPLACE "\\.c$" "" id ${rel})
    string(REPLACE "/" "__" id ${id})
    set(wrapper ${PROBE_DIR}/${id}.c)
    set(text "#include \"${src}\"\n")
    if(EXISTS ${wrapper})
        file(READ ${wrapper} old)
    else()
        set(old "")
    endif()
    if(NOT old STREQUAL text)
        file(WRITE ${wrapper} ${text})
    endif()
    list(APPEND PROBE_SOURCES ${wrapper})
    list(APPEND WEAK_HEADERS ${WEAK_DIR}/${id}.h)
    set_source_files_properties(${src} PROPERTIES
        COMPILE_OPTIONS "-include;${WEAK_DIR}/${id}.h"
        OBJECT_DEPENDS ${WEAK_DIR}/${id}.h)
endforeach()

add_library(game_probe OBJECT ${PROBE_SOURCES})
target_link_libraries(game_probe PRIVATE game_flags)
target_compile_options(game_probe PRIVATE -w)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/probe_objects.txt CONTENT "$<JOIN:$<TARGET_OBJECTS:game_probe>,\n>\n")
add_custom_command(
    OUTPUT ${WEAK_HEADERS}
    COMMAND ${Python3_EXECUTABLE} ${PORT_TOOLS}/gen_weak.py --objects-list ${CMAKE_BINARY_DIR}/probe_objects.txt
            --out-dir ${WEAK_DIR} --nm ${CMAKE_NM} --root ${CMAKE_SOURCE_DIR}
    DEPENDS $<TARGET_OBJECTS:game_probe> ${PORT_TOOLS}/gen_weak.py ${PORT_TOOLS}/n64syms.py
            ${CMAKE_SOURCE_DIR}/manifest/game.json ${CMAKE_SOURCE_DIR}/manifest/ovl_a.json
            ${CMAKE_SOURCE_DIR}/manifest/ovl_b.json ${CMAKE_SOURCE_DIR}/manifest/static.json
    COMMENT "Weak definitions for non-owned copies (tools/port/gen_weak.py)"
    VERBATIM)

add_library(game OBJECT ${GAME_SOURCES})
target_link_libraries(game PRIVATE game_flags)

# ---------------------------------------------------------------- generated data
# Storage for the data the game C declares but doesn't define (boot, game and both runtime images: initialized
# data and BSS), and the descriptors that fill it from the ROM at startup (docs/port/DATA.md). gen_data.py reads
# the game objects' DWARF (types), nm (what is undefined) and the symbol tables (addresses). data.c holds no ROM
# bytes; port/data/pc_data.c (pcdata_rt) is the loader, reached from data.c through weak references so the link
# test links without it.
set(PC_DATA_DIR ${CMAKE_BINARY_DIR}/pc-data)
if(NOT CMAKE_READELF)
    set(CMAKE_READELF readelf)
endif()
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/game_objects.txt CONTENT "$<JOIN:$<TARGET_OBJECTS:game>,\n>\n")
add_custom_command(
    OUTPUT ${PC_DATA_DIR}/data.c ${PC_DATA_DIR}/data_layout.txt ${PC_DATA_DIR}/data_layout.json
           ${PC_DATA_DIR}/data_symbols.txt
    COMMAND ${Python3_EXECUTABLE} ${PORT_TOOLS}/gen_data.py --objects-list ${CMAKE_BINARY_DIR}/game_objects.txt
            --nm ${CMAKE_NM} --readelf ${CMAKE_READELF} --root ${CMAKE_SOURCE_DIR} --out ${PC_DATA_DIR}/data.c
    DEPENDS $<TARGET_OBJECTS:game> ${PORT_TOOLS}/gen_data.py ${PORT_TOOLS}/n64syms.py
            ${CMAKE_SOURCE_DIR}/symbol_addrs.us.txt ${CMAKE_SOURCE_DIR}/undefined_syms_auto.us.txt
            ${CMAKE_SOURCE_DIR}/rom_symbol_aliases.ld ${CMAKE_SOURCE_DIR}/manifest/static.json
            ${CMAKE_SOURCE_DIR}/port/data/extents.txt
            ${CMAKE_SOURCE_DIR}/asm/us/blob/symbols.json ${CMAKE_SOURCE_DIR}/asm/us/ovl_a/symbols.json
            ${CMAKE_SOURCE_DIR}/asm/us/ovl_b/symbols.json ${CMAKE_SOURCE_DIR}/layout/game.json
            ${CMAKE_SOURCE_DIR}/layout/ovl_a.json ${CMAKE_SOURCE_DIR}/layout/ovl_b.json
    COMMENT "Data storage and descriptors (tools/port/gen_data.py)"
    VERBATIM)
add_library(pcdata OBJECT ${PC_DATA_DIR}/data.c)
target_include_directories(pcdata PRIVATE port/data)
target_compile_options(pcdata PRIVATE -fno-common)   # the .set aliases need the arenas in a section
find_package(ZLIB REQUIRED)
add_library(pcdata_rt OBJECT port/data/pc_data.c)
target_include_directories(pcdata_rt PUBLIC port/data)
target_compile_definitions(pcdata_rt PRIVATE PC_DATA_REPORT_DEFAULT="${CMAKE_BINARY_DIR}/data_report.txt")
target_link_libraries(pcdata_rt PUBLIC ZLIB::ZLIB)

# ---------------------------------------------------------------- link test
# Everything still undefined becomes an aborting stub or a zeroed placeholder (tools/port/gen_stubs.py), to
# prove the game, the generated data and the PC layer link. Not a playable program.
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/linktest_objects.txt
     CONTENT "$<JOIN:$<TARGET_OBJECTS:game>;$<TARGET_OBJECTS:pcdata>,\n>\n")
add_custom_command(
    OUTPUT ${PC_DATA_DIR}/stubs.c
    COMMAND ${Python3_EXECUTABLE} ${PORT_TOOLS}/gen_stubs.py --objects-list ${CMAKE_BINARY_DIR}/linktest_objects.txt
            --nm ${CMAKE_NM} --root ${CMAKE_SOURCE_DIR} --out ${PC_DATA_DIR}/stubs.c
    DEPENDS $<TARGET_OBJECTS:game> $<TARGET_OBJECTS:pcdata> ${PORT_TOOLS}/gen_stubs.py ${PORT_TOOLS}/n64syms.py
    COMMENT "Link-test stubs (tools/port/gen_stubs.py)"
    VERBATIM)
set_source_files_properties(${PC_DATA_DIR}/stubs.c PROPERTIES COMPILE_OPTIONS "-fno-builtin")
add_executable(rush2049pc_linktest port/main.c ${PC_DATA_DIR}/stubs.c
    $<TARGET_OBJECTS:game> $<TARGET_OBJECTS:pcdata>)
target_link_libraries(rush2049pc_linktest PRIVATE m)

# ---------------------------------------------------------------- generated data: checks
# pc_data_test: the loader on a synthetic ROM (ctest). rush2049pc_datacheck: the loader on your ROM with the
# game's tables (it says so and passes when there is no ROM); after linking it, gen_data.py --check verifies every
# placement and own-storage size in the executable against data_layout.json.
enable_testing()
add_executable(pc_data_test port/data/test_pc_data.c)
target_link_libraries(pc_data_test PRIVATE pcdata_rt)
add_test(NAME pc_data_test COMMAND pc_data_test)
add_executable(rush2049pc_datacheck port/data/datacheck.c ${PC_DATA_DIR}/stubs.c
    $<TARGET_OBJECTS:game> $<TARGET_OBJECTS:pcdata>)
target_link_libraries(rush2049pc_datacheck PRIVATE pcdata_rt m)
target_compile_definitions(rush2049pc_datacheck PRIVATE PC_DATA_ROM="${CMAKE_SOURCE_DIR}/baserom.us.z64")
add_custom_command(TARGET rush2049pc_datacheck POST_BUILD
    COMMAND ${Python3_EXECUTABLE} ${PORT_TOOLS}/gen_data.py --check $<TARGET_FILE:rush2049pc_datacheck>
            --layout ${PC_DATA_DIR}/data_layout.json --nm ${CMAKE_NM}
    VERBATIM)
add_test(NAME pc_data_rom COMMAND rush2049pc_datacheck)
add_test(NAME pc_data_synthetic COMMAND rush2049pc_datacheck --synthetic)
# Test programs define main: keep them out of compile_commands.json, which census.py reads as the game
# and the PC layer.
set_target_properties(pc_data_test rush2049pc_datacheck PROPERTIES EXPORT_COMPILE_COMMANDS OFF)

add_subdirectory(port/audio)
