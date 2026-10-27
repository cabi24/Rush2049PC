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
# libultra and libc units the PC layer replaces (port/ultra, docs/port/ULTRA.md): thread dispatch, message
# queues, events, timers, interrupts, the VI/PI/SI/AI/SP managers and raw device access, cache and TLB, and the
# inflate/LZSS readers (zlib and C in port/ultra/pi.c). lib_34a0 is the IDO libc (sprintf, fcvt, modff): the
# host's is used. Everything else under src/rom (gu*, libm, the scheduler, Pfs and controller packing, the
# audio library, the boot logic) compiles natively.
set(GAME_REPLACED
    src/rom/lib_10760.c src/rom/lib_107a0.c src/rom/lib_10970.c src/rom/lib_109a0.c src/rom/lib_10a00.c
    src/rom/lib_10b60.c src/rom/lib_10cc0.c src/rom/lib_10d10.c src/rom/lib_34a0.c src/rom/lib_5610.c
    src/rom/lib_7600.c src/rom/lib_7630.c src/rom/lib_7940.c src/rom/lib_79a0.c src/rom/lib_7a10.c
    src/rom/lib_7ad0.c src/rom/lib_7b30.c src/rom/lib_7c80.c src/rom/lib_7fb0.c src/rom/lib_8040.c
    src/rom/lib_8040_msg.c src/rom/lib_81e0.c src/rom/lib_8330.c src/rom/lib_83d0.c src/rom/lib_8440.c
    src/rom/lib_8700.c src/rom/lib_8920.c src/rom/lib_8a80.c src/rom/lib_8dd0.c src/rom/lib_8e10.c
    src/rom/lib_8e10_pi.c src/rom/lib_9230.c src/rom/lib_a5b0.c src/rom/lib_a800.c src/rom/lib_c990.c
    src/rom/lib_ca70.c src/rom/lib_cc50.c src/rom/lib_d140.c src/rom/lib_d260.c src/rom/lib_d270.c
    src/rom/lib_d580.c src/rom/lib_e1c0.c src/rom/lib_e240.c src/rom/lib_e250.c src/rom/lib_e280.c
    src/rom/lib_e310.c src/rom/lib_e340.c src/rom/lib_e390.c src/rom/lib_e3f0.c src/rom/lib_e7c0.c
    src/rom/lib_e8d0.c src/rom/lib_e9a0.c src/rom/lib_efd0.c src/rom/lib_f0b0.c)
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
# Data the game C declares but doesn't define. bss.c: zeroed storage at the N64 offsets of the boot and
# game BSS (tools/port/gen_bss.py), from the game objects' undefined symbols.
set(PC_DATA_DIR ${CMAKE_BINARY_DIR}/pc-data)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/game_objects.txt CONTENT "$<JOIN:$<TARGET_OBJECTS:game>,\n>\n")
add_custom_command(
    OUTPUT ${PC_DATA_DIR}/bss.c
    COMMAND ${Python3_EXECUTABLE} ${PORT_TOOLS}/gen_bss.py --objects-list ${CMAKE_BINARY_DIR}/game_objects.txt
            --nm ${CMAKE_NM} --root ${CMAKE_SOURCE_DIR} --out ${PC_DATA_DIR}/bss.c
    DEPENDS $<TARGET_OBJECTS:game> ${PORT_TOOLS}/gen_bss.py ${PORT_TOOLS}/n64syms.py
            ${CMAKE_SOURCE_DIR}/symbol_addrs.us.txt
    COMMENT "BSS definitions (tools/port/gen_bss.py)"
    VERBATIM)
add_library(pcdata OBJECT ${PC_DATA_DIR}/bss.c)

# ---------------------------------------------------------------- the PC layer and the executable
# port/ultra: libultra, ROM, memory, threads and the main loop (docs/port/ULTRA.md); port/input: pads. The
# other parts (port/gfx, port/audio, port/data) append their object libraries to PC_PORT_LIBS before this
# section; until they do, port/ultra's weak no-op defaults (port/ultra/defaults.c) stand in for them.
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
file(GLOB PC_ULTRA_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/port/ultra/*.c ${CMAKE_SOURCE_DIR}/port/input/*.c)
add_library(pcultra OBJECT port/main.c ${PC_ULTRA_SOURCES})
target_include_directories(pcultra PRIVATE port/include port/ultra ${ZLIB_INCLUDE_DIRS})
target_compile_options(pcultra PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
list(PREPEND PC_PORT_LIBS pcultra)

# Everything the game and the generated data still leave undefined becomes a stub that prints its name and a
# backtrace, or a zeroed placeholder (tools/port/gen_stubs.py); names that differ from a definition only by the
# case of their address digits become linker aliases (aliases.ld). The PC layer's own definitions count, its
# references (the C library, zlib, pthreads) do not.
set(PC_GAME_OBJECTS $<TARGET_OBJECTS:game> $<TARGET_OBJECTS:pcdata>)
set(PC_PORT_OBJECTS "")
foreach(lib ${PC_PORT_LIBS})
    list(APPEND PC_PORT_OBJECTS $<TARGET_OBJECTS:${lib}>)
endforeach()
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/game_link_objects.txt CONTENT "$<JOIN:${PC_GAME_OBJECTS},\n>\n")
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/port_objects.txt CONTENT "$<JOIN:${PC_PORT_OBJECTS},\n>\n")
add_custom_command(
    OUTPUT ${PC_DATA_DIR}/stubs.c ${PC_DATA_DIR}/aliases.ld
    COMMAND ${Python3_EXECUTABLE} ${PORT_TOOLS}/gen_stubs.py --objects-list ${CMAKE_BINARY_DIR}/game_link_objects.txt
            --defined-list ${CMAKE_BINARY_DIR}/port_objects.txt --nm ${CMAKE_NM} --root ${CMAKE_SOURCE_DIR}
            --out ${PC_DATA_DIR}/stubs.c --aliases ${PC_DATA_DIR}/aliases.ld
    DEPENDS ${PC_GAME_OBJECTS} ${PC_PORT_OBJECTS} ${PORT_TOOLS}/gen_stubs.py ${PORT_TOOLS}/n64syms.py
    COMMENT "Stubs for what is still missing (tools/port/gen_stubs.py)"
    VERBATIM)
set_source_files_properties(${PC_DATA_DIR}/stubs.c PROPERTIES COMPILE_OPTIONS "-fno-builtin")

# rush2049pc: the game on the PC layer. It stays position-independent, so every host address of its code, data,
# heap and stacks is above 4 GiB and none can be mistaken for an N64 address (physical, segmented, or KSEG0/1,
# where port/ultra maps the RDRAM window). Note that binutils 2.40 on aarch64 relocates GOT entries of absolute
# symbols in a PIE, so no symbol may be defined at an absolute N64 address.
add_executable(rush2049pc ${PC_DATA_DIR}/stubs.c ${PC_GAME_OBJECTS} ${PC_PORT_OBJECTS})
target_link_libraries(rush2049pc PRIVATE ZLIB::ZLIB Threads::Threads m)
target_link_options(rush2049pc PRIVATE ${PC_DATA_DIR}/aliases.ld)
set_target_properties(rush2049pc PROPERTIES LINK_DEPENDS ${PC_DATA_DIR}/aliases.ld)
