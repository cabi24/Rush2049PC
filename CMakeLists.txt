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

# ---------------------------------------------------------------- SDL2 (optional)
# One SDL2 for the window (port/gfx), the keyboard and controllers (port/input) and audio output (port/audio).
# An installed one is used when CMake finds it (config package, then pkg-config); RUSH_FETCH_SDL2 downloads the
# pinned release once and builds it statically instead (about 50 s on the Pi 5 with -j3). Without SDL2 the build
# is headless EGL, null/WAV/ALSA audio and scripted input only (docs/port/FRONTEND.md).
option(RUSH_SDL2 "Use SDL2 when it is found: window, keyboard and controllers, audio" ON)
option(RUSH_FETCH_SDL2 "Download and build SDL2 2.30.9 (SHA-256 pinned) instead of an installed one" OFF)
set(RUSH_SDL2_TARGET "")
if(RUSH_FETCH_SDL2)
    include(FetchContent)
    FetchContent_Declare(SDL2
        URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz
        URL_HASH SHA256=24b574f71c87a763f50704bbb630cbe38298d544a1f890f099a4696b1d6beba4)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_STATIC_PIC ON CACHE BOOL "" FORCE)   # linked into the PIE executable
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL2)
    set(RUSH_SDL2_TARGET SDL2::SDL2-static)
elseif(RUSH_SDL2)
    find_package(SDL2 CONFIG QUIET)
    if(TARGET SDL2::SDL2)
        set(RUSH_SDL2_TARGET SDL2::SDL2)
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(RUSH_SDL2_PC QUIET IMPORTED_TARGET sdl2)
            if(RUSH_SDL2_PC_FOUND)
                set(RUSH_SDL2_TARGET PkgConfig::RUSH_SDL2_PC)
            endif()
        endif()
    endif()
endif()
set(RUSH_SDL2_SHARED ON)   # port/gfx and port/audio use RUSH_SDL2_TARGET instead of their own lookup
if(RUSH_SDL2_TARGET)
    message(STATUS "SDL2: ${RUSH_SDL2_TARGET}")
else()
    message(STATUS "SDL2: none (headless, scripted input; -DRUSH_FETCH_SDL2=ON downloads it)")
endif()

# ---------------------------------------------------------------- game code
# Every C file of the decompilation, compiled for the host with TARGET_PC.
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.c)
# Functions at the matching ceiling: the ROM takes them from their retail assembly, but their C drafts
# (nonmatching/README.md) differ only in register allocation and scheduling. The PC build compiles the
# standalone drafts of those no other file defines; the rest already come in as group context copies.
foreach(f RaceStateMachine_Update TB_EllipseText createBG display_list_flush drone_set_catchup
          ghost_race_setup physics_float_calc state_utility)
    list(APPEND GAME_SOURCES ${CMAKE_SOURCE_DIR}/nonmatching/game/${f}.c)
endforeach()
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
# First include of every game file: pointer-sized integer types and big-endian helpers for the 64-bit host
# (docs/port/POINTERS.md).
target_compile_options(game_flags INTERFACE -include ${CMAKE_SOURCE_DIR}/port/include/pc_prelude.h)
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

# ---------------------------------------------------------------- the PC layer and the executable
# port/ultra: libultra, ROM, memory, threads and the main loop (docs/port/ULTRA.md). pcfrontend: port/input (pads,
# keyboard, controllers, window keys) and port/config (rush2049pc.ini), docs/port/FRONTEND.md. The other
# parts link in as object libraries (PC_PORT_OBJECT_LIBS) or as static libraries linked whole
# (PC_PORT_ARCHIVES), so their strong definitions replace port/ultra's weak no-op defaults
# (port/ultra/defaults.c); a part not built yet is simply left out of the lists. Targets may be defined later in
# this file: they are only named here.
find_package(Threads REQUIRED)
file(GLOB PC_ULTRA_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/port/ultra/*.c)
add_library(pcultra OBJECT port/main.c ${PC_ULTRA_SOURCES})
target_include_directories(pcultra PRIVATE port/include port/ultra port/config ${ZLIB_INCLUDE_DIRS})
target_compile_options(pcultra PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
file(GLOB PC_FRONTEND_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/port/input/*.c
     ${CMAKE_SOURCE_DIR}/port/config/*.c)
add_library(pcfrontend OBJECT ${PC_FRONTEND_SOURCES})
target_include_directories(pcfrontend PRIVATE port/include port/ultra port/config port/input)
target_compile_options(pcfrontend PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(pcfrontend PUBLIC m)
if(RUSH_SDL2_TARGET)
    target_compile_definitions(pcfrontend PRIVATE PC_HAVE_SDL2)
    target_link_libraries(pcfrontend PUBLIC ${RUSH_SDL2_TARGET})
endif()
# port/assets: the game's files converted to the host's byte order as _ReadWave loads them (docs/port/ASSETS.md).
# One file per format; assets.c has the dispatcher and weak no-ops for formats not converted yet.
file(GLOB PC_ASSETS_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/port/assets/*.c)
add_library(pcassets OBJECT ${PC_ASSETS_SOURCES})
target_include_directories(pcassets PRIVATE port/include port/assets)
target_compile_options(pcassets PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
list(PREPEND PC_PORT_OBJECT_LIBS pcultra pcfrontend pcdata_rt pcassets)   # port/data's loader (docs/port/DATA.md)
list(APPEND PC_PORT_ARCHIVES pcaudio pcgfx)            # port/audio, port/gfx (docs/port/AUDIO.md, GFX.md)

# Everything the game and the generated data still leave undefined becomes a stub that prints its name and a
# backtrace, or a zeroed placeholder (tools/port/gen_stubs.py); names that differ from a definition only by the
# case of their address digits become linker aliases (aliases.ld). The PC layer's own definitions count, its
# references (the C library, zlib, pthreads) do not.
set(PC_GAME_OBJECTS $<TARGET_OBJECTS:game> $<TARGET_OBJECTS:pcdata>)
set(PC_PORT_OBJECTS "")
foreach(lib ${PC_PORT_OBJECT_LIBS})
    list(APPEND PC_PORT_OBJECTS $<TARGET_OBJECTS:${lib}>)
endforeach()
foreach(lib ${PC_PORT_ARCHIVES})
    list(APPEND PC_PORT_OBJECTS $<TARGET_FILE:${lib}>)
endforeach()
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/game_link_objects.txt CONTENT "$<JOIN:${PC_GAME_OBJECTS},\n>\n")
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/port_objects.txt CONTENT "$<JOIN:${PC_PORT_OBJECTS},\n>\n")
add_custom_command(
    OUTPUT ${PC_DATA_DIR}/stubs.c ${PC_DATA_DIR}/aliases.ld
    COMMAND ${Python3_EXECUTABLE} ${PORT_TOOLS}/gen_stubs.py --objects-list ${CMAKE_BINARY_DIR}/game_link_objects.txt
            --defined-list ${CMAKE_BINARY_DIR}/port_objects.txt --nm ${CMAKE_NM} --root ${CMAKE_SOURCE_DIR}
            --out ${PC_DATA_DIR}/stubs.c --aliases ${PC_DATA_DIR}/aliases.ld
    DEPENDS ${PC_GAME_OBJECTS} ${PC_PORT_OBJECTS} ${PORT_TOOLS}/gen_stubs.py ${PORT_TOOLS}/n64syms.py
            ${CMAKE_SOURCE_DIR}/symbol_addrs.us.txt ${CMAKE_SOURCE_DIR}/layout/game.json
            ${CMAKE_SOURCE_DIR}/layout/ovl_a.json ${CMAKE_SOURCE_DIR}/layout/ovl_b.json
    COMMENT "Stubs for what is still missing (tools/port/gen_stubs.py)"
    VERBATIM)
set_source_files_properties(${PC_DATA_DIR}/stubs.c PROPERTIES COMPILE_OPTIONS "-fno-builtin")

# rush2049pc: the game on the PC layer. It stays position-independent, so every host address of its code, data,
# heap and stacks is above 4 GiB and none can be mistaken for an N64 address (physical, segmented, or KSEG0/1,
# where port/ultra maps the RDRAM window). Symbols at absolute N64 addresses (aliases.ld) work through a startup
# fix-up of their GOT slots (port/ultra/memory.c): binutils 2.40 on aarch64 relocates them by the load address.
add_executable(rush2049pc ${PC_DATA_DIR}/stubs.c ${PC_GAME_OBJECTS})
target_link_libraries(rush2049pc PRIVATE ${PC_PORT_OBJECT_LIBS} ZLIB::ZLIB Threads::Threads m)
foreach(lib ${PC_PORT_ARCHIVES})
    target_link_libraries(rush2049pc PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,${lib}>")
endforeach()
target_link_options(rush2049pc PRIVATE ${PC_DATA_DIR}/aliases.ld)
set_target_properties(rush2049pc PROPERTIES LINK_DEPENDS ${PC_DATA_DIR}/aliases.ld)
enable_testing()
add_test(NAME ultra_selftest COMMAND rush2049pc --selftest)   # threads, queues, events, timers, SI (no ROM)
# The GSM kernels (port/ultra/gsm.c) against their own MIPS code (interpreted from kernels.s) and a GSM 06.10
# reference, alone and under the game's frame decoder (lib_26f60.c, built with the game's flags).
add_library(test_gsm_game OBJECT port/ultra/tests/gsm_game.c)
target_link_libraries(test_gsm_game PRIVATE game_flags)
target_compile_options(test_gsm_game PRIVATE -w)
add_executable(test_gsm port/ultra/tests/test_gsm.c port/ultra/gsm.c $<TARGET_OBJECTS:test_gsm_game>)
target_include_directories(test_gsm PRIVATE port/ultra)
target_compile_options(test_gsm PRIVATE -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter)   # -O2: 2 s, not 7
target_compile_definitions(test_gsm PRIVATE GSM_KERNELS_S="${CMAKE_SOURCE_DIR}/asm/us/boot_data/kernels.s")
add_test(NAME ultra_gsm COMMAND test_gsm)
# port/assets: each tests/test_<name>.c is a program run by ctest as assets_<name>, linked with the converters
# and tests/support.c (a big-endian range registry in place of port/ultra's).
file(GLOB PC_ASSETS_TESTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/port/assets/tests/test_*.c)
foreach(t ${PC_ASSETS_TESTS})
    get_filename_component(n ${t} NAME_WE)
    string(REGEX REPLACE "^test_" "" n ${n})
    add_executable(test_assets_${n} ${t} port/assets/tests/support.c $<TARGET_OBJECTS:pcassets>)
    target_include_directories(test_assets_${n} PRIVATE port/include port/assets port/assets/tests)
    target_compile_options(test_assets_${n} PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
    set_target_properties(test_assets_${n} PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
    add_test(NAME assets_${n} COMMAND test_assets_${n})
endforeach()
# A test program (main) and a second copy of lib_26f60.c: kept out of compile_commands.json (census.py).
set_target_properties(test_gsm test_gsm_game PROPERTIES EXPORT_COMPILE_COMMANDS OFF)

# The same stubs without the PC layer, for test programs that link the game alone (rush2049pc_datacheck): every
# libultra name is stubbed, and the stubs abort by themselves.
add_custom_command(
    OUTPUT ${PC_DATA_DIR}/stubs_standalone.c
    COMMAND ${Python3_EXECUTABLE} ${PORT_TOOLS}/gen_stubs.py --objects-list ${CMAKE_BINARY_DIR}/game_link_objects.txt
            --nm ${CMAKE_NM} --root ${CMAKE_SOURCE_DIR} --out ${PC_DATA_DIR}/stubs_standalone.c --standalone
    DEPENDS ${PC_GAME_OBJECTS} ${PORT_TOOLS}/gen_stubs.py ${PORT_TOOLS}/n64syms.py
    COMMENT "Standalone stubs (tools/port/gen_stubs.py)"
    VERBATIM)
set_source_files_properties(${PC_DATA_DIR}/stubs_standalone.c PROPERTIES COMPILE_OPTIONS "-fno-builtin")

# ---------------------------------------------------------------- generated data: checks
# pc_data_test: the loader on a synthetic ROM (ctest). rush2049pc_datacheck: the loader on your ROM with the
# game's tables (it says so and passes when there is no ROM); after linking it, gen_data.py --check verifies every
# placement and own-storage size in the executable against data_layout.json.
enable_testing()
add_executable(pc_data_test port/data/test_pc_data.c)
target_link_libraries(pc_data_test PRIVATE pcdata_rt)
add_test(NAME pc_data_test COMMAND pc_data_test)
add_executable(rush2049pc_datacheck port/data/datacheck.c ${PC_DATA_DIR}/stubs_standalone.c
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
add_subdirectory(port/gfx)
add_subdirectory(port/input)   # the frontend's tests (docs/port/FRONTEND.md)
