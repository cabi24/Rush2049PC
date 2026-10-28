# PC build of San Francisco Rush 2049 from the decompilation's C (docs/PORT_PLAN.md).
# The N64 matching build is the Makefile; nothing here is used by it.
#
#   cmake -S . -B build/pc -G Ninja
#   cmake --build build/pc
#   python3 tools/port/census.py --build build/pc      # docs/port/STATUS.md
cmake_minimum_required(VERSION 3.20)
if(CMAKE_VERSION VERSION_LESS 3.24)   # $<LINK_LIBRARY:WHOLE_ARCHIVE,...> below
    message(FATAL_ERROR "The PC build needs CMake 3.24 or later; this is ${CMAKE_VERSION} (pip install cmake gives a newer one)")
endif()
project(Rush2049PC C)
if(WIN32)
    enable_language(RC)   # windres: the executable's icon and version (port/app/rush2049pc.rc)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # tools/port/census.py reads compile_commands.json
# Executables are PIE, so host code and data load above 4 GB, clear of the RDRAM window, and a pointer below
# 4 GB is an N64 address (docs/port/DATA.md). Debian and Ubuntu compilers link PIE by default, Red Hat's don't.
include(CheckPIESupported)
check_pie_supported()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
set(PORT_TOOLS ${CMAKE_SOURCE_DIR}/tools/port)
# The generators read and write UTF-8 (sources, nm and objdump output); Windows' Python defaults to the ANSI code page.
set(PC_PYTHON ${Python3_EXECUTABLE})
if(CMAKE_HOST_WIN32)
    set(PC_PYTHON ${Python3_EXECUTABLE} -X utf8)
endif()

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
# standalone drafts of those no other file defines; the rest already come in as group context copies. Drafts that
# need the PC layouts of shared records have them under TARGET_PC. The runtime images' 8-byte stubs (deleted bodies
# of inlined helpers) are empty functions in nonmatching/ovl_<x>/stubs.c, PC only.
foreach(f RaceStateMachine_Update TB_EllipseText createBG display_list_flush drone_set_catchup
          ghost_race_setup physics_float_calc state_utility animSpeedText StartWeaponIcon func_8010D680 func_8010E8B4)
    list(APPEND GAME_SOURCES ${CMAKE_SOURCE_DIR}/nonmatching/game/${f}.c)
endforeach()
# Image A's func_8038A400 (updateWarningScreen) shares its address, and so its name, with image B's: it is
# compiled as updateWarningScreen (docs/port/SYNTH.md).
list(APPEND GAME_SOURCES ${CMAKE_SOURCE_DIR}/nonmatching/ovl_a/func_803A44FC.c
     ${CMAKE_SOURCE_DIR}/nonmatching/ovl_a/func_8038A400.c
     ${CMAKE_SOURCE_DIR}/nonmatching/ovl_a/stubs.c ${CMAKE_SOURCE_DIR}/nonmatching/ovl_b/stubs.c)
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
# libultra's bzero, bcopy and bcmp take a 32-bit length, which the game's own prototypes declare as s32, int or
# u32. The host's take a size_t, so a call through one of those prototypes would leave the length's upper half
# undefined: the game calls port/ultra's 32-bit versions instead (libc.c).
foreach(name bzero bcopy bcmp)
    target_compile_definitions(game_flags INTERFACE ${name}=n64_${name})
endforeach()
# The game's deflate code has its own zcfree. Windows links zlib statically, and a static zlib's zcfree is global
# (a shared one keeps it hidden), so the game's is renamed there.
if(WIN32)
    target_compile_definitions(game_flags INTERFACE zcfree=n64_zcfree)
endif()
target_include_directories(game_flags INTERFACE include include/game include/PR)
# First include of every game file: pointer-sized integer types and big-endian helpers for the 64-bit host
# (docs/port/POINTERS.md).
target_compile_options(game_flags INTERFACE -include ${CMAKE_SOURCE_DIR}/port/include/pc_prelude.h)
# A debug build that stops at the first store of a host address into a 32-bit N64 address field (PC_N64W in
# pc_prelude.h, docs/port/CASTS.md): for a run with the ROM, to check that only RDRAM-window addresses get there.
option(RUSH_CHECK_CASTS "Check the game's 32-bit N64 address stores for host addresses at run time" OFF)
if(RUSH_CHECK_CASTS)
    target_compile_definitions(game_flags INTERFACE RUSH_CHECK_CASTS)
endif()
# IDO semantics: gnu89, so `__inline` functions also get an out-of-line definition (C99 rules would drop it,
# and CMake's C_STANDARD adds no flag when the compiler's default is newer). Plain char is unsigned and signed
# overflow wraps. The code type-puns freely, and some files define the same tentative global (common symbols).
target_compile_options(game_flags INTERFACE -std=gnu89 -funsigned-char -fwrapv -fno-strict-aliasing -fcommon)
# Each floating-point operation rounded on its own, as the N64's VR4300 does (it has no fused multiply-add): GCC's
# default for GNU C fuses a * b + c wherever the target has FMA (aarch64, x86-64-v3 such as RHEL 10's default) once it
# optimizes, which moved the cars by a bit a step from the Debug build's (docs/port/OPTIMIZED.md). -O0 never fuses.
target_compile_options(game_flags INTERFACE -ffp-contract=off)
# No loop bounds from declared array sizes: the game C's declarations are views, often shorter than the data they
# index (an open-ended table declared with one element, a struct member used as an array past its end; -O2's
# -Warray-bounds sees 260 such accesses), so the rule the optimizer would derive bounds from doesn't hold here, as
# -fwrapv and -fno-strict-aliasing say of theirs. Optimized, it changed the loops of 65 game objects; -O0 doesn't use
# it (docs/port/OPTIMIZED.md).
target_compile_options(game_flags INTERFACE -fno-aggressive-loop-optimizations)
# Symbols at absolute N64 addresses (aliases.ld) are out of PC-relative reach of the executable's code, so code
# that names them (the game, and parts of port/) must load every extern's address from the GOT. aarch64 PIE
# code does that already. x86-64 PIE code reaches externs PC-relatively, so everything is compiled as PIC there,
# without GOT relaxation (port/ultra/memory.c fixes the slots at startup). On Windows, GCC's default code model
# loads every extern's address from a .refptr slot, which the linker fills with the absolute value
# (docs/port/WINDOWS.md).
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64" AND NOT WIN32)
    add_compile_options(-fPIC -fno-semantic-interposition -Wa,-mrelax-relocations=no)
    add_link_options(-Wl,--no-relax)   # ld would still turn GOT loads of absolute symbols into PC32
endif()
# ---------------------------------------------------------------- Windows (mingw-w64, docs/port/WINDOWS.md)
# The image links above 4 GiB (0x140000000) and moves with high-entropy ASLR, which keeps the heap and the thread
# stacks above 4 GiB too: the renderer and the audio interpreter read values below 4 GiB as N64 addresses.
# Absolute symbols get no base relocations, so their .refptr slots keep the N64 addresses wherever the image
# lands. Executables are static (no MinGW runtime DLLs next to them). port/include/win holds the part of
# <sys/mman.h> the PC layer's tests use.
if(WIN32)
    include_directories(BEFORE ${CMAKE_SOURCE_DIR}/port/include/win)
    add_link_options(-static -Wl,--dynamicbase,--high-entropy-va,--nxcompat)
    set(ZLIB_USE_STATIC_LIBS ON)   # libz.a, not zlib1.dll
    add_compile_definitions(SDL_MAIN_HANDLED)   # test programs have their own main and include SDL.h
endif()
# Silence what the N64-era declarations trip over; the 64-bit pointer warnings (-Wpointer-to-int-cast,
# -Wint-to-pointer-cast) stay on, since they are the porting work list (docs/port/STATUS.md).
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(game_flags INTERFACE -Wno-builtin-declaration-mismatch
        -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types
        -Wno-discarded-qualifiers -Wno-overflow
        -Wsequence-point)   # unsequenced reads that only IDO's evaluation order defines (listener_position_set)
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
set(PC_WEAK_PE_ARGS "")
if(WIN32)
    set(PC_WEAK_PE_ARGS --pe)   # each header also defines a global of its file's own (tools/port/gen_weak.py)
endif()
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
# Only its global definitions are read, which are the same at every optimization level: -O0 is the quickest.
target_compile_options(game_probe PRIVATE -w -O0)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/probe_objects.txt CONTENT "$<JOIN:$<TARGET_OBJECTS:game_probe>,\n>\n")
add_custom_command(
    OUTPUT ${WEAK_HEADERS}
    COMMAND ${PC_PYTHON} ${PORT_TOOLS}/gen_weak.py --objects-list ${CMAKE_BINARY_DIR}/probe_objects.txt
            --out-dir ${WEAK_DIR} --nm ${CMAKE_NM} --root ${CMAKE_SOURCE_DIR} ${PC_WEAK_PE_ARGS}
    DEPENDS $<TARGET_OBJECTS:game_probe> ${PORT_TOOLS}/gen_weak.py ${PORT_TOOLS}/n64syms.py
            ${CMAKE_SOURCE_DIR}/manifest/game.json ${CMAKE_SOURCE_DIR}/manifest/ovl_a.json
            ${CMAKE_SOURCE_DIR}/manifest/ovl_b.json ${CMAKE_SOURCE_DIR}/manifest/static.json
    COMMENT "Weak definitions for non-owned copies (tools/port/gen_weak.py)"
    VERBATIM)

add_library(game OBJECT ${GAME_SOURCES})
target_link_libraries(game PRIVATE game_flags)

# The game's types for the generators and the DWARF checks (gen_data.py, view_check.py...): objects compiled at -O0
# with -g, so the generated tables don't depend on the build type (docs/port/OPTIMIZED.md). An optimized object's
# DWARF leaves out the declarations of variables whose uses the optimizer removed, and of types nothing left uses,
# so the data tables lost symbols at -O2 (D_80117498, a word the loader then left in the ROM's byte order).
# A Debug build's game objects are -O0 already; any other build type compiles a second copy for its types, with the
# game's own flags and weak headers (set on the sources). gen_data.py refuses optimized DWARF.
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG}" MATCHES "(^| )-O[^0 ]")
    set(PC_TYPES_TARGET game)
else()
    add_library(game_types OBJECT ${GAME_SOURCES})
    target_link_libraries(game_types PRIVATE game_flags)
    target_compile_options(game_types PRIVATE -O0 -g -UNDEBUG -w)
    set_target_properties(game_types PROPERTIES EXPORT_COMPILE_COMMANDS OFF)   # census.py, endian_check.py
    set(PC_TYPES_TARGET game_types)
endif()
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/types_objects.txt
    CONTENT "$<JOIN:$<TARGET_OBJECTS:${PC_TYPES_TARGET}>,\n>\n")

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
set(PC_DWARF_TOOL ${CMAKE_READELF})
set(PC_DATA_PE_ARGS "")
set(PC_DATA_PE_OUTPUTS "")
if(WIN32)
    # COFF objects: objdump prints their DWARF (readelf reads only ELF), and data.c's weak references resolve
    # through weak.ld (tools/port/gen_data.py).
    set(PC_DWARF_TOOL ${CMAKE_OBJDUMP})
    set(PC_DATA_PE_ARGS --weak-script ${PC_DATA_DIR}/weak.ld)
    set(PC_DATA_PE_OUTPUTS ${PC_DATA_DIR}/weak.ld)
endif()
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/game_objects.txt CONTENT "$<JOIN:$<TARGET_OBJECTS:game>,\n>\n")
# Types and symbols from the -O0 objects (types_objects.txt); the runtime images' C data, which --check finds in the
# executable, from the objects linked (game_objects.txt).
add_custom_command(
    OUTPUT ${PC_DATA_DIR}/data.c ${PC_DATA_DIR}/data_layout.txt ${PC_DATA_DIR}/data_layout.json
           ${PC_DATA_DIR}/data_symbols.txt ${PC_DATA_PE_OUTPUTS}
    COMMAND ${PC_PYTHON} ${PORT_TOOLS}/gen_data.py --objects-list ${CMAKE_BINARY_DIR}/types_objects.txt
            --link-objects-list ${CMAKE_BINARY_DIR}/game_objects.txt
            --nm ${CMAKE_NM} --readelf ${PC_DWARF_TOOL} --root ${CMAKE_SOURCE_DIR} --out ${PC_DATA_DIR}/data.c
            ${PC_DATA_PE_ARGS}
    DEPENDS $<TARGET_OBJECTS:${PC_TYPES_TARGET}> $<TARGET_OBJECTS:game> ${PORT_TOOLS}/gen_data.py
            ${PORT_TOOLS}/n64syms.py
            ${CMAKE_SOURCE_DIR}/symbol_addrs.us.txt ${CMAKE_SOURCE_DIR}/undefined_syms_auto.us.txt
            ${CMAKE_SOURCE_DIR}/rom_symbol_aliases.ld ${CMAKE_SOURCE_DIR}/manifest/static.json
            ${CMAKE_SOURCE_DIR}/port/data/extents.txt ${CMAKE_SOURCE_DIR}/port/data/reached.txt
            ${CMAKE_SOURCE_DIR}/asm/us/blob/symbols.json ${CMAKE_SOURCE_DIR}/asm/us/ovl_a/symbols.json
            ${CMAKE_SOURCE_DIR}/asm/us/ovl_b/symbols.json ${CMAKE_SOURCE_DIR}/layout/game.json
            ${CMAKE_SOURCE_DIR}/layout/ovl_a.json ${CMAKE_SOURCE_DIR}/layout/ovl_b.json
    COMMENT "Data storage and descriptors (tools/port/gen_data.py)"
    VERBATIM)
add_library(pcdata OBJECT ${PC_DATA_DIR}/data.c)
target_include_directories(pcdata PRIVATE port/data)
target_compile_options(pcdata PRIVATE -fno-common)   # the .set aliases need the arenas in a section
# Own storage in the order data.c defines it, which is N64 address order: GCC reorders top-level definitions from -O1
# on (at -O2 it put them in reverse), and game C that walks from one symbol to the next relies on the N64's order
# (docs/port/OPTIMIZED.md). -O0 keeps the order anyway; gen_data.py --check verifies it in the executable.
include(CheckCCompilerFlag)
check_c_compiler_flag(-fno-toplevel-reorder PC_HAVE_NO_TOPLEVEL_REORDER)
if(PC_HAVE_NO_TOPLEVEL_REORDER)
    target_compile_options(pcdata PRIVATE -fno-toplevel-reorder)
endif()
find_package(ZLIB REQUIRED)
add_library(pcdata_rt OBJECT port/data/pc_data.c)
target_include_directories(pcdata_rt PUBLIC port/data)
# The loader's report on the first load (docs/port/DATA.md), for bring-up: in the build directory by default; empty
# for none (the release builds, docs/port/RELEASE.md). PC_DATA_REPORT overrides it at run time.
set(RUSH_DATA_REPORT "${CMAKE_BINARY_DIR}/data_report.txt" CACHE STRING "Where the data loader writes its report; empty for none")
target_compile_definitions(pcdata_rt PRIVATE PC_DATA_REPORT_DEFAULT="${RUSH_DATA_REPORT}")
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
if(NOT WIN32)
    list(FILTER PC_ULTRA_SOURCES EXCLUDE REGEX "/win32\\.c$")   # Windows' fault handler (docs/port/WINDOWS.md)
endif()
add_library(pcultra OBJECT port/main.c ${PC_ULTRA_SOURCES})
target_include_directories(pcultra PRIVATE port/include port/ultra port/config ${ZLIB_INCLUDE_DIRS})
target_compile_options(pcultra PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
if(WIN32)
    target_link_libraries(pcultra PUBLIC winmm)   # timeBeginPeriod (loop.c)
endif()
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
# port/app: the program around the game (docs/port/RELEASE.md): its folders, the ROM lookup and what is wrong with a
# ROM, message boxes. Only the executable links it (it calls port/ultra).
file(GLOB PC_APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/port/app/*.c)
# The version (git describe), the commit and the build (a GitHub Actions run or a local build), written at every
# build when they change (cmake/version.cmake): --version and the log's first lines.
set(PC_VERSION_C ${CMAKE_BINARY_DIR}/version/pc_version.c)
set(PC_VERSION_RC_H ${CMAKE_BINARY_DIR}/version/pc_version_rc.h)   # the same for the Windows resources
add_custom_target(pc_version
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DOUT=${PC_VERSION_C} -DOUT_RC=${PC_VERSION_RC_H}
            -P ${CMAKE_SOURCE_DIR}/cmake/version.cmake
    BYPRODUCTS ${PC_VERSION_C} ${PC_VERSION_RC_H}
    COMMENT "Version (cmake/version.cmake)"
    VERBATIM)
add_library(pcapp OBJECT ${PC_APP_SOURCES} ${PC_VERSION_C})
add_dependencies(pcapp pc_version)
target_include_directories(pcapp PRIVATE port/include port/ultra port/config port/app ${ZLIB_INCLUDE_DIRS})
if(WIN32)
    target_link_libraries(pcapp PUBLIC winmm)   # timeBeginPeriod (startup.c)
endif()
target_compile_options(pcapp PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
if(RUSH_SDL2_TARGET)
    target_compile_definitions(pcapp PRIVATE PC_HAVE_SDL2)
    target_link_libraries(pcapp PUBLIC ${RUSH_SDL2_TARGET})
endif()
target_include_directories(pcultra PRIVATE port/app)   # main.c
# port/assets: the game's files converted to the host's byte order as _ReadWave loads them (docs/port/ASSETS.md).
# One file per format; assets.c has the dispatcher and weak no-ops for formats not converted yet.
file(GLOB PC_ASSETS_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/port/assets/*.c)
add_library(pcassets OBJECT ${PC_ASSETS_SOURCES})
target_include_directories(pcassets PRIVATE port/include port/assets)
target_compile_options(pcassets PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
list(PREPEND PC_PORT_OBJECT_LIBS pcultra pcfrontend pcapp pcdata_rt pcassets)   # port/data's loader (docs/port/DATA.md)
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
    COMMAND ${PC_PYTHON} ${PORT_TOOLS}/gen_stubs.py --objects-list ${CMAKE_BINARY_DIR}/game_link_objects.txt
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
# Each runtime image's C data (its objects' .data and .bss, statics included) in one range, which the loader puts
# back on every load of the image as the N64's load does (port/data/overlays.ld, docs/port/DATA.md 5). data.c names
# the ranges, so every program that links it links this too.
set(PC_OVERLAYS_LD ${CMAKE_SOURCE_DIR}/port/data/overlays.ld)
# A program's linker scripts: the given implicit ones (symbol assignments) and overlays.ld with -T. On Windows they
# are one -T script: binutils 2.44+ tells a -T script from the others by stat, and Windows' stat gives every file
# inode 0, so MSYS2's ld takes any second script for the -T one given twice. overlays.ld goes first there, since
# its INSERT moves the statements before it.
function(pc_link_scripts target)
    if(WIN32)
        set(out ${CMAKE_BINARY_DIR}/link/${target}.ld)
        add_custom_command(OUTPUT ${out}
            COMMAND ${CMAKE_COMMAND} -DOUT=${out} "-DIN=$<JOIN:${PC_OVERLAYS_LD};${ARGN},|>"
                    -P ${CMAKE_SOURCE_DIR}/cmake/concat.cmake
            DEPENDS ${PC_OVERLAYS_LD} ${ARGN} ${CMAKE_SOURCE_DIR}/cmake/concat.cmake VERBATIM)
        add_custom_target(${target}_ld DEPENDS ${out})
        add_dependencies(${target} ${target}_ld)
        target_link_options(${target} PRIVATE "LINKER:-T,${out}")
        set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${out})
    else()
        target_link_options(${target} PRIVATE ${ARGN} "LINKER:-T,${PC_OVERLAYS_LD}")
        set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${ARGN} ${PC_OVERLAYS_LD})
    endif()
endfunction()
pc_link_scripts(rush2049pc ${PC_DATA_DIR}/aliases.ld ${PC_DATA_PE_OUTPUTS})   # PE: data.c's weak references
# Windows (docs/port/WINDOWS.md, "The console"; "Icon and version"): a GUI program, so Windows opens no console
# window with it (port/app/startup.c attaches to the parent's console instead, or keeps the output for a message box
# when there is none), with an icon and version information. RUSH_WINDOWS_CONSOLE builds a console program, which
# gets a console window of its own when double-clicked (and waits for a key after an error, port/app/log.c).
if(WIN32)
    option(RUSH_WINDOWS_CONSOLE "Build rush2049pc.exe as a console program (a console window opens with it)" OFF)
    target_sources(rush2049pc PRIVATE port/app/rush2049pc.rc)
    target_include_directories(rush2049pc PRIVATE $<$<COMPILE_LANGUAGE:RC>:${CMAKE_BINARY_DIR}/version>
                               $<$<COMPILE_LANGUAGE:RC>:${CMAKE_SOURCE_DIR}/dist>)
    set_source_files_properties(port/app/rush2049pc.rc PROPERTIES
        OBJECT_DEPENDS "${PC_VERSION_RC_H};${CMAKE_SOURCE_DIR}/dist/rush2049pc.ico")
    add_dependencies(rush2049pc pc_version)
    if(NOT RUSH_WINDOWS_CONSOLE)
        set_target_properties(rush2049pc PROPERTIES WIN32_EXECUTABLE ON)   # -mwindows
    endif()
endif()
enable_testing()
add_test(NAME ultra_selftest COMMAND rush2049pc --selftest)   # threads, queues, events, timers, SI (no ROM)
add_test(NAME ultra_selftest_fast COMMAND rush2049pc --selftest --fast)   # the same on the virtual clock
if(WIN32)
    # The subsystem, the image base and ASLR, the icon's sizes and the version information, from the file itself
    # (Python only: it runs on a Linux host that cross-compiles too, ctest -R win_pe)
    set(PE_CHECK_ARGS "")
    if(RUSH_WINDOWS_CONSOLE)
        set(PE_CHECK_ARGS --console)
    endif()
    add_test(NAME win_pe COMMAND ${PC_PYTHON} ${PORT_TOOLS}/pe_check.py --exe $<TARGET_FILE:rush2049pc>
        --version-file ${PC_VERSION_RC_H} ${PE_CHECK_ARGS})
endif()
# The synthetic test cartridge (tools/port/synth_rom.py, docs/port/SYNTH.md): original, minimal data, generated into
# the build directory from the data report. synth_boot_<path> drives the game along one of its paths with the press
# script tools/port/synth/<path>.press (the races for 1-4 players, circuit, stunt, battle, the attract demo, the pause
# screen, the end of a race...), through tools/port/synth_paths.py: the script's leading comments say which state to
# reach, for how many frames, what the state log must say there (players, windows, cars, mode, track), and which
# screenshot checks to make. Most run on the virtual clock (--fast, about a second each); synth_boot_race runs in real
# time (28 s). On Windows too (CI's native job).
set(SYNTH_ROM ${CMAKE_BINARY_DIR}/synth/rush2049_synth.z64)
add_test(NAME synth_rom COMMAND ${PC_PYTHON} ${PORT_TOOLS}/synth_rom.py --out ${SYNTH_ROM}
    --data-symbols ${PC_DATA_DIR}/data_symbols.txt)
set_tests_properties(synth_rom PROPERTIES FIXTURES_SETUP synth_rom)
file(GLOB SYNTH_PATHS CONFIGURE_DEPENDS ${PORT_TOOLS}/synth/*.press)
foreach(script ${SYNTH_PATHS})
    get_filename_component(name ${script} NAME_WE)
    add_test(NAME synth_boot_${name} COMMAND ${PC_PYTHON} ${PORT_TOOLS}/synth_paths.py
        --exe $<TARGET_FILE:rush2049pc> --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/paths ${name})
    set_tests_properties(synth_boot_${name} PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 150)
endforeach()
# The first run (docs/port/RELEASE.md): what romcheck.c says a file is, on images the test makes; the folders and the
# ROM lookup order in temporary folders; and the program's own messages for each kind of missing or wrong ROM, and
# the lookup order, through tools/port/firstrun_check.py with copies of the synthetic cartridge.
add_executable(test_romcheck port/ultra/tests/test_romcheck.c port/ultra/romcheck.c)
target_include_directories(test_romcheck PRIVATE port/ultra port/include)
target_compile_options(test_romcheck PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
add_test(NAME ultra_romcheck COMMAND test_romcheck)
add_executable(test_app port/app/tests/test_app.c port/app/paths.c)
target_include_directories(test_app PRIVATE port/app port/include)
target_compile_options(test_app PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
add_test(NAME app_paths COMMAND test_app ${CMAKE_BINARY_DIR}/test-scratch)
add_test(NAME app_firstrun COMMAND ${PC_PYTHON} ${PORT_TOOLS}/firstrun_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM})
set_tests_properties(app_firstrun PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 300)
set_target_properties(test_romcheck test_app PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
# A high frame rate leaves the game's timing alone (docs/port/GFX.md "High frame rate"): three paths on the virtual
# clock with video.frame_rate original and 120, their state and status lines compared line for line
# (tools/port/hfr_check.py), and frames drawn between the game's in the second run.
add_test(NAME hfr_timing COMMAND ${PC_PYTHON} ${PORT_TOOLS}/hfr_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/hfr --require-drawn race2p drive demo)
set_tests_properties(hfr_timing PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 300)
# Live input drives the game (docs/port/FRONTEND.md "Testing live input"): the drive path through SDL virtual
# controllers, a virtual joystick with the generic mapping, key events and a controller plugged in late, each the same
# run as the scripted one, with the car's steering input following the stick; race2p with two controllers
# (tools/port/input_check.py). Headless: live input pumps SDL's events itself.
if(RUSH_SDL2_TARGET)
    add_test(NAME input_live_drive COMMAND ${PC_PYTHON} ${PORT_TOOLS}/input_check.py --exe $<TARGET_FILE:rush2049pc>
        --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/input)
    set_tests_properties(input_live_drive PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 600 SKIP_RETURN_CODE 77)
endif()
# The GSM kernels (port/ultra/gsm.c) against their own MIPS code (interpreted from kernels.s) and a GSM 06.10
# reference, alone and under the game's frame decoder (lib_26f60.c, built with the game's flags).
add_library(test_gsm_game OBJECT port/ultra/tests/gsm_game.c)
target_link_libraries(test_gsm_game PRIVATE game_flags)
target_compile_options(test_gsm_game PRIVATE -w)
add_executable(test_gsm port/ultra/tests/test_gsm.c port/ultra/gsm.c port/ultra/n64mem.c
    $<TARGET_OBJECTS:test_gsm_game>)
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
    add_executable(test_assets_${n} ${t} port/assets/tests/support.c port/ultra/n64mem.c port/ultra/absfix.c
        $<TARGET_OBJECTS:pcassets>)
    target_include_directories(test_assets_${n} PRIVATE port/include port/assets port/assets/tests)
    target_compile_options(test_assets_${n} PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
    target_link_libraries(test_assets_${n} PRIVATE m)
    set_target_properties(test_assets_${n} PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
    add_test(NAME assets_${n} COMMAND test_assets_${n})
    # A test that runs the game's own readers on its converted files names their sources on ` * game: ...` lines
    # of its leading comment: they are compiled as the game is (game_flags) and linked in, with the test
    # standing in for everything else they reference.
    file(STRINGS ${t} game_lines REGEX "^ \\* game: ")
    set(game_srcs "")
    foreach(l ${game_lines})
        string(REGEX REPLACE "^ \\* game: " "" l "${l}")
        separate_arguments(l)
        list(APPEND game_srcs ${l})
    endforeach()
    if(game_srcs)
        add_library(test_assets_${n}_game OBJECT ${game_srcs})
        target_link_libraries(test_assets_${n}_game PRIVATE game_flags)
        target_compile_options(test_assets_${n}_game PRIVATE -w)
        set_target_properties(test_assets_${n}_game PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
        target_sources(test_assets_${n} PRIVATE $<TARGET_OBJECTS:test_assets_${n}_game>)
    endif()
endforeach()
# test_assets_musyx also runs the game's own MusyX readers on the converted files: the boot library's units that
# read them, built with the game's flags (one wrapper per unit, like test_gsm_game).
file(GLOB PC_ASSETS_MUSYX_GAME CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/port/assets/tests/musyx_lib_*.c)
add_library(test_musyx_game OBJECT ${PC_ASSETS_MUSYX_GAME})
target_link_libraries(test_musyx_game PRIVATE game_flags)
target_compile_options(test_musyx_game PRIVATE -w)
target_sources(test_assets_musyx PRIVATE $<TARGET_OBJECTS:test_musyx_game>)
set_target_properties(test_musyx_game PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
# The game's own WAD loader reading a converted synthetic WAD (port/assets/tests/wad_game.c): _ReadWave,
# SetupWaveModel, the TOC functions, the display-list walkers, the texture lookups and the texture animation,
# built with the game's flags, in an RDRAM window the test maps itself (exit 77, skipped, if it can't).
add_library(test_wad_game OBJECT src/blob/groups/audio_frame_sync/waves.c src/blob/MBSetupWad.c
    src/blob/MBGetFromWad.c src/blob/func_80096B00.c src/blob/func_80096BBC.c src/blob/entity_cull_check.c
    src/blob/entity_lod_select.c src/blob/func_800966D8.c src/blob/groups/slot_sound/entity_render_mode.c
    src/blob/display_list_traverse.c src/blob/func_8008AD6C.c src/blob/func_80092D80.c src/blob/func_800B24EC.c
    src/blob/MBOX_FindPalette_WORK.c src/blob/camera_shake_start.c src/blob/race_setup_1.c src/blob/func_800BDA24.c
    src/blob/func_800BD080.c src/blob/camera_smooth_follow.c)
target_link_libraries(test_wad_game PRIVATE game_flags)
target_compile_options(test_wad_game PRIVATE -w)
add_executable(test_assets_wad_game port/assets/tests/wad_game.c port/assets/tests/support.c port/ultra/n64mem.c
    $<TARGET_OBJECTS:pcassets> $<TARGET_OBJECTS:test_wad_game>)
target_include_directories(test_assets_wad_game PRIVATE port/include port/assets port/assets/tests)
target_compile_options(test_assets_wad_game PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
set_target_properties(test_assets_wad_game test_wad_game PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
add_test(NAME assets_wad_game COMMAND test_assets_wad_game)
set_tests_properties(assets_wad_game PROPERTIES SKIP_RETURN_CODE 77)
# A test program (main) and a second copy of lib_26f60.c: kept out of compile_commands.json (census.py).
set_target_properties(test_gsm test_gsm_game PROPERTIES EXPORT_COMPILE_COMMANDS OFF)

# The same stubs without the PC layer, for test programs that link the game alone (rush2049pc_datacheck): every
# libultra name is stubbed, and the stubs abort by themselves.
add_custom_command(
    OUTPUT ${PC_DATA_DIR}/stubs_standalone.c
    COMMAND ${PC_PYTHON} ${PORT_TOOLS}/gen_stubs.py --objects-list ${CMAKE_BINARY_DIR}/game_link_objects.txt
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
# Part of pc_data_test's tables comes from gen_data.py's layout rules (unions, overlapping declarations) applied
# to the declarations in test_pc_data_decls.c, which is compiled for its DWARF only.
add_library(pc_data_test_decls OBJECT port/data/test_pc_data_decls.c)
target_compile_options(pc_data_test_decls PRIVATE -O0 -g)   # unoptimized DWARF, as for the game (types_objects.txt)
add_custom_command(
    OUTPUT ${PC_DATA_DIR}/test_pc_data_gen.h
    COMMAND ${Python3_EXECUTABLE} ${PORT_TOOLS}/gen_data.py --test-tables $<TARGET_OBJECTS:pc_data_test_decls>
            --nm ${CMAKE_NM} --readelf ${PC_DWARF_TOOL} --out ${PC_DATA_DIR}/test_pc_data_gen.h
    DEPENDS $<TARGET_OBJECTS:pc_data_test_decls> ${PORT_TOOLS}/gen_data.py ${PORT_TOOLS}/n64syms.py
    COMMENT "pc_data_test's tables from test_pc_data_decls.c (tools/port/gen_data.py --test-tables)"
    VERBATIM)
add_executable(pc_data_test port/data/test_pc_data.c ${PC_DATA_DIR}/test_pc_data_gen.h)
target_include_directories(pc_data_test PRIVATE ${PC_DATA_DIR})
target_link_libraries(pc_data_test PRIVATE pcdata_rt)
add_test(NAME pc_data_test COMMAND pc_data_test)
add_executable(pc_reach_test port/data/test_reach.c)
target_link_libraries(pc_reach_test PRIVATE pcdata_rt)
add_test(NAME pc_reach_test COMMAND pc_reach_test)
add_executable(rush2049pc_datacheck port/data/datacheck.c ${PC_DATA_DIR}/stubs_standalone.c
    $<TARGET_OBJECTS:game> $<TARGET_OBJECTS:pcdata>)
target_link_libraries(rush2049pc_datacheck PRIVATE pcdata_rt m)
target_compile_definitions(rush2049pc_datacheck PRIVATE PC_DATA_ROM="${CMAKE_SOURCE_DIR}/baserom.us.z64")
set(PC_DATA_CHECK_PE "")
if(WIN32)
    set(PC_DATA_CHECK_PE --pe)   # COFF symbols have no sizes; the image links high and moves
endif()
pc_link_scripts(rush2049pc_datacheck ${PC_DATA_PE_OUTPUTS})
add_custom_command(TARGET rush2049pc_datacheck POST_BUILD
    COMMAND ${PC_PYTHON} ${PORT_TOOLS}/gen_data.py --check $<TARGET_FILE:rush2049pc_datacheck>
            --layout ${PC_DATA_DIR}/data_layout.json --nm ${CMAKE_NM} ${PC_DATA_CHECK_PE}
    VERBATIM)
add_test(NAME pc_data_rom COMMAND rush2049pc_datacheck)
add_test(NAME pc_data_synthetic COMMAND rush2049pc_datacheck --synthetic)
# pc_data_overlay: runtime image B's weapons.c built as the game is (its BSS state and its global table) and a
# stand-in for image A's C with statics, linked through overlays.ld, written to, then reloaded with the loader
# (port/data/test_overlay_data.c). The stand-in's path has src/ovl_a/ in it, which is how the script tells image A's.
add_library(test_overlay_data_game OBJECT src/ovl_b/groups/weapons/weapons.c
    port/data/test_overlay/src/ovl_a/statics.c)
target_link_libraries(test_overlay_data_game PRIVATE game_flags)
target_compile_options(test_overlay_data_game PRIVATE -w)
add_executable(test_overlay_data port/data/test_overlay_data.c $<TARGET_OBJECTS:test_overlay_data_game>)
target_link_libraries(test_overlay_data PRIVATE pcdata_rt)
pc_link_scripts(test_overlay_data)
add_test(NAME pc_data_overlay COMMAND test_overlay_data)
# tools/port/endian_check.py (docs/port/ENDIAN.md): the detector on its synthetic cases, and the loader's element
# widths where game declarations were made to agree with how the game reads the data.
# The detector's own cases need its reading of gcc's GIMPLE dumps, which finds none of them under MSYS2's gcc 16:
# Windows runs the loader check only.
if(WIN32)
    set(ENDIAN_CHECK_ARGS --check-loader)
else()
    set(ENDIAN_CHECK_ARGS --selftest --check-loader)
endif()
add_test(NAME endian_check COMMAND ${Python3_EXECUTABLE} ${PORT_TOOLS}/endian_check.py --build ${CMAKE_BINARY_DIR}
    ${ENDIAN_CHECK_ARGS})
# The audio manager's records (docs/port/SYNTH.md): every live view of an fx tag (N64 0x44 bytes), a sequence
# (0x3C), an emitter (0x2C) and the command table has the host layout of audiomgr_thread.c's views, read from the
# DWARF of the game's objects (tools/port/view_check.py; ELF objects only).
if(NOT WIN32)
    set(SOUND_FX_VIEWS
        src/blob/groups/menu_save_options/hud_setup.c src/blob/groups/camera_target_track/group.c
        src/blob/groups/engine_sound_sync/audio_effect_setup_b132.c src/blob/groups/engine_sound_sync/audiomgr_thread.c
        src/blob/groups/entity_lookup/group.c src/blob/groups/entity_helpers/group.c src/blob/groups/transform/group.c
        src/blob/groups/engine_sound_sync/engine_sound_sync.c src/blob/groups/updatefx/updatefx.c
        src/blob/groups/audio_pitch_adjust/ap.c src/blob/groups/record_score/group.c src/blob/results_time_display.c)
    set(SOUND_SEQ_VIEWS
        src/blob/groups/engine_sound_sync/audiomgr_thread.c src/blob/groups/engine_sound_sync/audio_effect_setup_b132.c
        src/blob/groups/menu_save_options/hud_setup.c src/blob/hud_speed_display.c src/blob/car_stats_display.c
        src/blob/func_800D18D8.c src/blob/game_timer_pause.c)
    set(SOUND_EMITTER_VIEWS
        src/blob/groups/camera_target_track/group.c src/blob/groups/engine_sound_sync/audio_effect_setup_b132.c
        src/blob/hud_speed_display.c src/blob/groups/entity_helpers/group.c src/blob/groups/entity_lookup/group.c
        src/blob/groups/engine_sound_sync/engine_sound_sync.c)
    string(REPLACE ";" "," SOUND_FX_VIEWS "${SOUND_FX_VIEWS}")
    string(REPLACE ";" "," SOUND_SEQ_VIEWS "${SOUND_SEQ_VIEWS}")
    string(REPLACE ";" "," SOUND_EMITTER_VIEWS "${SOUND_EMITTER_VIEWS}")
    set(VIEW_CHECK ${Python3_EXECUTABLE} ${PORT_TOOLS}/view_check.py --build ${CMAKE_BINARY_DIR})
    add_test(NAME views_sound_fxtag COMMAND ${VIEW_CHECK} --n64-size 0x44 --in ${SOUND_FX_VIEWS}
        --tag tFxSequence,Node68,Voice,Entity,Effect,FxTag,Node,Record,ResEntry)
    add_test(NAME views_sound_sequence COMMAND ${VIEW_CHECK} --n64-size 0x3C --in ${SOUND_SEQ_VIEWS}
        --tag anon_ll_id_3c,Sequence,Chan,RecA,Event,Node16)
    add_test(NAME views_sound_emitter COMMAND ${VIEW_CHECK} --n64-size 0x2C --in ${SOUND_EMITTER_VIEWS}
        --tag Cam,CamSlot,Owner,RecB,Source)
    add_test(NAME views_sound_commands COMMAND ${VIEW_CHECK} --sym D_80142DD8)
    # The road collision's views of the car's model record (D_8014A250, N64 layout): the tires' road nodes at
    # +0x3A0 are u32 window addresses (docs/port/CARS.md 2.2).
    add_test(NAME views_model_road COMMAND ${VIEW_CHECK} --n64-size 0x808 --tag VCar,EUCar,tModelData
        --in src/blob/groups/func_800AD4C8/camera_victory.c,src/blob/groups/entity_update/entity_update.c,src/blob/groups/update_game_data/update_game_data.c)
endif()
# tools/port/layout_check.py (docs/port/LAYOUT.md): the layout check on its synthetic cases (it reads the same GIMPLE,
# so not on Windows either).
if(NOT WIN32)
    add_test(NAME layout_check COMMAND ${Python3_EXECUTABLE} ${PORT_TOOLS}/layout_check.py --build ${CMAKE_BINARY_DIR}
        --selftest)
    # tools/port/offset_check.py (docs/port/LAYOUT.md 9): raw N64 offsets into records whose PC layout moved the
    # field, on its synthetic cases (CI runs it over the game).
    add_test(NAME offset_check COMMAND ${Python3_EXECUTABLE} ${PORT_TOOLS}/offset_check.py --build ${CMAKE_BINARY_DIR}
        --selftest)
endif()
# tools/port/index_check.py: initialized data the code indexes past a scalar declaration must have the table's extent
# in the data report, or its other elements keep the ROM's byte order (docs/port/SYNTH.md 3).
add_test(NAME index_check COMMAND ${PC_PYTHON} ${PORT_TOOLS}/index_check.py --build ${CMAKE_BINARY_DIR})
# tools/port/weak_copies_check.py: a function several files define with no manifest owner must be the same in each,
# since gen_weak keeps one copy for every caller (docs/port/QA.md 2: the start screen's InitEgg).
add_test(NAME weak_copies_check COMMAND ${PC_PYTHON} ${PORT_TOOLS}/weak_copies_check.py --build ${CMAKE_BINARY_DIR}
    --root ${CMAKE_SOURCE_DIR})
# Test programs define main: keep them out of compile_commands.json, which census.py reads as the game
# and the PC layer.
set_target_properties(pc_data_test pc_reach_test pc_data_test_decls rush2049pc_datacheck test_overlay_data test_overlay_data_game
    PROPERTIES EXPORT_COMPILE_COMMANDS OFF)

# Before port/gfx, whose renderer links pcupscale (and makes it itself when nothing has).
add_subdirectory(port/upscale)   # 2x texture upscaling and its tests (docs/port/UPSCALE.md)
add_subdirectory(port/audio)
add_subdirectory(port/gfx)
add_subdirectory(port/input)   # the frontend's tests (docs/port/FRONTEND.md)

# ---------------------------------------------------------------- install (docs/port/RELEASE.md)
# `cmake --install build/pc --prefix DIR`. Portable (the default on Windows, and the release archives): one folder
# with the program, README.txt and LICENSES/. Otherwise the usual layout under the prefix: bin/, the desktop entry
# and icon under share/, README and licences under share/doc/rush2049pc. The licences are those of what is linked
# in: zlib (its text from the zlib.h compiled against), SDL2 when it is built here (RUSH_FETCH_SDL2), and any
# RUSH_EXTRA_LICENSES, a list of NAME=FILE pairs (the release workflow passes the mingw-w64 runtime's and GCC's).
# No ROM and nothing from one is installed.
include(GNUInstallDirs)
if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)   # DESTINATION . is the prefix itself
endif()
if(WIN32)
    set(RUSH_INSTALL_PORTABLE_DEFAULT ON)
else()
    set(RUSH_INSTALL_PORTABLE_DEFAULT OFF)
endif()
option(RUSH_INSTALL_PORTABLE "Install as one folder: the program, README.txt and LICENSES/" ${RUSH_INSTALL_PORTABLE_DEFAULT})
set(RUSH_EXTRA_LICENSES "" CACHE STRING "More licence texts to install, as NAME=FILE pairs")
if(RUSH_INSTALL_PORTABLE)
    set(RUSH_INSTALL_BIN .)
    set(RUSH_INSTALL_DOC .)
else()
    set(RUSH_INSTALL_BIN ${CMAKE_INSTALL_BINDIR})
    set(RUSH_INSTALL_DOC ${CMAKE_INSTALL_DATADIR}/doc/rush2049pc)
endif()
install(TARGETS rush2049pc RUNTIME DESTINATION ${RUSH_INSTALL_BIN})
if(WIN32)
    set(RUSH_README_TXT ${CMAKE_SOURCE_DIR}/dist/README-windows.txt)
else()
    set(RUSH_README_TXT ${CMAKE_SOURCE_DIR}/dist/README-linux.txt)
endif()
install(FILES ${RUSH_README_TXT} DESTINATION ${RUSH_INSTALL_DOC} RENAME README.txt)
if(RUSH_INSTALL_PORTABLE)
    # An empty rush2049pc.ini next to the program makes the folder portable: settings, saves, screenshots and the
    # log stay in it. The first run fills the file with the defaults (port/main.c).
    file(WRITE ${CMAKE_BINARY_DIR}/dist/rush2049pc.ini "")
    install(FILES ${CMAKE_BINARY_DIR}/dist/rush2049pc.ini DESTINATION ${RUSH_INSTALL_BIN})
else()
    install(FILES README.md docs/port/FRONTEND.md docs/port/RELEASE.md DESTINATION ${RUSH_INSTALL_DOC})
    if(NOT WIN32)   # the desktop entry and its icon (a plain road: nothing of the game's)
        install(FILES dist/rush2049pc.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
        install(FILES dist/rush2049pc.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    endif()
endif()
set(RUSH_LICENSE_DIR ${CMAKE_BINARY_DIR}/licenses)
file(MAKE_DIRECTORY ${RUSH_LICENSE_DIR})
foreach(dir ${ZLIB_INCLUDE_DIRS})   # zlib's licence is the first comment of zlib.h
    if(EXISTS ${dir}/zlib.h)
        file(READ ${dir}/zlib.h text)
        string(FIND "${text}" "/*" start)
        string(SUBSTRING "${text}" ${start} -1 text)
        string(FIND "${text}" "*/" end)
        string(SUBSTRING "${text}" 2 ${end} text)
        string(REGEX REPLACE "\\*/$" "" text "${text}")
        string(REGEX REPLACE "^ +" "" text "${text}")
        file(WRITE ${RUSH_LICENSE_DIR}/zlib.txt "${text}\n")
        install(FILES ${RUSH_LICENSE_DIR}/zlib.txt DESTINATION ${RUSH_INSTALL_DOC}/LICENSES)
        break()
    endif()
endforeach()
if(RUSH_FETCH_SDL2 AND EXISTS ${sdl2_SOURCE_DIR}/LICENSE.txt)
    install(FILES ${sdl2_SOURCE_DIR}/LICENSE.txt DESTINATION ${RUSH_INSTALL_DOC}/LICENSES RENAME SDL2.txt)
elseif(WIN32 AND RUSH_SDL2_TARGET AND NOT CMAKE_VERSION VERSION_LESS 3.21)
    install(FILES $<TARGET_RUNTIME_DLLS:rush2049pc> DESTINATION ${RUSH_INSTALL_BIN})   # MSYS2's SDL2.dll
endif()
foreach(pair ${RUSH_EXTRA_LICENSES})
    string(REGEX MATCH "^([^=]+)=(.+)$" ok "${pair}")
    if(NOT ok)
        message(FATAL_ERROR "RUSH_EXTRA_LICENSES: '${pair}' is not NAME=FILE")
    endif()
    install(FILES ${CMAKE_MATCH_2} DESTINATION ${RUSH_INSTALL_DOC}/LICENSES RENAME ${CMAKE_MATCH_1})
endforeach()
