# San Francisco Rush 2049 for the PC, built from the game's C (docs/PORT_PLAN.md, docs/port/PCONLY.md).
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
# Every C file of the game.
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.c)
# The functions the N64 decompilation never byte-matched (the "matching ceiling") have their C in nonmatching/
# (nonmatching/README.md) where no file under src/ defines them. The runtime images' 8-byte stubs (the empty bodies
# IDO left of inlined helpers, which nothing calls) have no C: gen_data's function table has NULL at their addresses.
foreach(f updatePauseScreen TB_EllipseText createBG PrePlay PS_ReadPartialFile
          GhostEnd WorldCullObjects TB_DrawText animSpeedText StartWeaponIcon AnimateWeaponIcon video_animPanes)
    list(APPEND GAME_SOURCES ${CMAKE_SOURCE_DIR}/nonmatching/game/${f}.c)
endforeach()
# Image A's updateWarningScreen shares its address, 0x8038A400, and so its name in the loader's tables, with image
# B's findDamageSlot (docs/port/SYNTH.md).
list(APPEND GAME_SOURCES ${CMAKE_SOURCE_DIR}/nonmatching/ovl_a/carscr_animText.c
     ${CMAKE_SOURCE_DIR}/nonmatching/ovl_a/updateWarningScreen.c)
# src/rom is the boot code's libultra, libc and audio library less what the PC layer does itself (port/ultra,
# docs/port/ULTRA.md): thread dispatch, message queues, events, timers, interrupts, the VI/PI/SI/AI/SP managers and
# raw device access, cache and TLB, the inflate/LZSS readers (zlib and C in port/ultra/pi.c) and IDO's libc (the
# host's is used) have no C here.

# Compile settings shared by the game and the test programs that compile game files.
add_library(game_flags INTERFACE)
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
target_include_directories(game_flags INTERFACE include include/PR)
# First include of every game file: pointer-sized integer types and big-endian helpers for the 64-bit host
# (docs/port/POINTERS.md).
target_compile_options(game_flags INTERFACE -include ${CMAKE_SOURCE_DIR}/port/include/pc_prelude.h)
# A debug build that stops at the first store of a host address into a 32-bit N64 address field (PC_N64W in
# pc_prelude.h, docs/port/CASTS.md): for a run with the ROM, to check that only RDRAM-window addresses get there.
option(RUSH_CHECK_CASTS "Check the game's 32-bit N64 address stores for host addresses at run time" OFF)
if(RUSH_CHECK_CASTS)
    target_compile_definitions(game_flags INTERFACE RUSH_CHECK_CASTS)
endif()
# No fixed address (docs/port/NOFIXED.md, phase B): the RDRAM window is pc_window, a zero-fill section of the
# executable that data.c defines, and the game's names are relative to it (gen_data, gen_stubs, reserved.txt), so
# nothing is mapped at a fixed host address and no GOT slot is fixed up; the game C's boundaries between a 32-bit N64
# address and a host pointer translate (pc_prelude.h), with PC_N64W checked. Off by default: the fixed window stays
# the default until phase D. ELF only for now (Linux).
option(RUSH_NOFIXED "The RDRAM window anywhere: a section of the executable, the names relative to it" OFF)
set(PC_NOFIXED_ARGS "")
set(PC_DATA_NOFIXED_ARGS "")
if(RUSH_NOFIXED)
    if(WIN32 OR APPLE)
        message(FATAL_ERROR "RUSH_NOFIXED is ELF only so far (docs/port/NOFIXED.md 6, phase D)")
    endif()
    add_compile_definitions(RUSH_NOFIXED)   # the game C, the PC layer and the tests
    set(PC_NOFIXED_ARGS --nofixed)          # gen_stubs.py
    # gen_data.py also defines the names gen_stubs would make absolute, for which the PC layer's definitions count
    set(PC_DATA_NOFIXED_ARGS --nofixed --defined-list ${CMAKE_BINARY_DIR}/port_objects.txt)
endif()
# The crossing counter and the arena guard (docs/port/NATIVE.md 9, wave 3's measure): every place the game or the PC
# layer turns a 32-bit N64 address into a host pointer or back, counted by its site and written at exit
# (RUSH2049_CROSSINGS=FILE; tools/port/crossings.py runs the synthetic paths and reports), and RUSH2049_WINDOW_GUARD=
# arenas, the game's data arenas in the window without access. port/ultra/crossings.c is built only here, and the
# hooks elsewhere (port/include/pc_crossings.h) are nothing without the option: the default build doesn't change. A
# Debug build (crossings.py makes one), with no inlining in any case: the PC layer's sites are counted by their caller,
# which inlining would hide. Linux only (mprotect, a SIGSEGV handler), and the default layout only: the moved and
# cast-checking builds translate or check every crossing themselves.
option(RUSH_COUNT_CROSSINGS "Count the crossings between N64 addresses and host pointers; the arena guard" OFF)
if(RUSH_COUNT_CROSSINGS)
    if(WIN32 OR APPLE)
        message(FATAL_ERROR "RUSH_COUNT_CROSSINGS is Linux only so far (docs/port/NATIVE.md 9)")
    endif()
    if(RUSH_NOFIXED OR RUSH_CHECK_CASTS)
        message(FATAL_ERROR "RUSH_COUNT_CROSSINGS counts the default build's crossings: not with RUSH_NOFIXED or "
                "RUSH_CHECK_CASTS (docs/port/NATIVE.md 9)")
    endif()
    add_compile_definitions(RUSH_COUNT_CROSSINGS)   # the game C (pc_prelude.h), the PC layer and the tests
    # the game C's sites as paths in the tree (src/blob/forces1.c:71), wherever the checkout is; no inlining (above)
    add_compile_options(-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/= -fno-inline)
endif()
# IDO semantics: gnu89, so `__inline` functions also get an out-of-line definition (C99 rules would drop it,
# and CMake's C_STANDARD adds no flag when the compiler's default is newer). Plain char is unsigned and signed
# overflow wraps. The code type-puns freely, and some files define the same tentative global (common symbols).
target_compile_options(game_flags INTERFACE -std=gnu89 -funsigned-char -fwrapv -fno-strict-aliasing -fcommon)
# Each floating-point operation rounded on its own, as the N64's VR4300 does (it has no fused multiply-add): GCC's
# default for GNU C fuses a * b + c wherever the target has FMA (aarch64, x86-64-v3 such as RHEL 10's default) once it
# optimizes, which moved the cars by a bit a step from the Debug build's (docs/port/OPTIMIZED.md). -O0 never fuses.
target_compile_options(game_flags INTERFACE -ffp-contract=off)
# Online play's lockstep (docs/port/LOCKSTEP.md 5): the game C reads a few locals it never set; libultra's SI code copies
# such a struct whole into RDRAM (__osMakeMotorData's), so those bytes are whatever the host's stack held, other in
# every process and on every OS. With this on, every local the C leaves unset starts as zero (GCC 12, Clang 16), the
# same on every PC. Off by default, so the default game is compiled as before; the state hash leaves out the scratch
# buffers where such bytes were found.
option(RUSH_ZERO_LOCALS "Zero the game C's uninitialised locals, the same on every PC (online play)" OFF)
if(RUSH_ZERO_LOCALS)
    target_compile_options(game_flags INTERFACE -ftrivial-auto-var-init=zero)
endif()
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
include(cmake/sanitize.cmake)   # RUSH_SANITIZE (docs/port/SANITIZE.md)
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
        -Wsequence-point)   # unsequenced reads that only IDO's evaluation order defines (ResetCollisionObject)
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(game_flags INTERFACE -Wno-incompatible-library-redeclaration
        -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types
        -Wno-incompatible-pointer-types-discards-qualifiers -Wno-constant-conversion)
endif()

add_library(game OBJECT ${GAME_SOURCES})
target_link_libraries(game PRIVATE game_flags)

# The game's types for the generators and the DWARF checks (gen_data.py, view_check.py...): objects compiled at -O0
# with -g, so the generated tables don't depend on the build type (docs/port/OPTIMIZED.md). An optimized object's
# DWARF leaves out the declarations of variables whose uses the optimizer removed, and of types nothing left uses,
# so the data tables lost symbols at -O2 (start_water_sentinel, a word the loader then left in the ROM's byte order).
# A Debug build's game objects are -O0 already; any other build type compiles a second copy for its types, with the
# game's own flags. gen_data.py refuses optimized DWARF.
# A sanitizer build's game objects are instrumented, so it compiles the copy too, without (docs/port/SANITIZE.md).
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG}" MATCHES "(^| )-O[^0 ]"
   AND NOT PC_SANITIZE)
    set(PC_TYPES_TARGET game)
else()
    add_library(game_types OBJECT ${GAME_SOURCES})
    target_link_libraries(game_types PRIVATE game_flags)
    target_compile_options(game_types PRIVATE -O0 -g -UNDEBUG -w)
    set_target_properties(game_types PROPERTIES PC_NO_SANITIZE ON)
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
# The data's types: include/game's declarations, one per datum, compiled alone with every declaration in its DWARF
# (port/data/game_decls.c). gen_data lays each datum out by its declaration there, not by what the game's files
# declare (docs/port/DATA.md 1). Never linked.
add_library(game_decls OBJECT port/data/game_decls.c)
target_link_libraries(game_decls PRIVATE game_flags)
target_compile_options(game_decls PRIVATE -O0 -g -fno-eliminate-unused-debug-symbols
                       -fno-eliminate-unused-debug-types -UNDEBUG)
set_target_properties(game_decls PROPERTIES PC_NO_SANITIZE ON EXPORT_COMPILE_COMMANDS OFF)
# Types and symbols from the -O0 objects (types_objects.txt); the runtime images' C data, which --check finds in the
# executable, from the objects linked (game_objects.txt).
add_custom_command(
    OUTPUT ${PC_DATA_DIR}/data.c ${PC_DATA_DIR}/data_layout.txt ${PC_DATA_DIR}/data_layout.json
           ${PC_DATA_DIR}/data_symbols.txt ${PC_DATA_PE_OUTPUTS}
    COMMAND ${PC_PYTHON} ${PORT_TOOLS}/gen_data.py --objects-list ${CMAKE_BINARY_DIR}/types_objects.txt
            --link-objects-list ${CMAKE_BINARY_DIR}/game_objects.txt --decls $<TARGET_OBJECTS:game_decls>
            --nm ${CMAKE_NM} --readelf ${PC_DWARF_TOOL} --root ${CMAKE_SOURCE_DIR} --out ${PC_DATA_DIR}/data.c
            ${PC_DATA_PE_ARGS} ${PC_DATA_NOFIXED_ARGS}
    DEPENDS $<TARGET_OBJECTS:${PC_TYPES_TARGET}> $<TARGET_OBJECTS:game> $<TARGET_OBJECTS:game_decls>
            ${PORT_TOOLS}/gen_data.py
            ${PORT_TOOLS}/n64syms.py ${CMAKE_SOURCE_DIR}/port/include/pc_n64_map.h ${PORT_TOOLS}/gen_stubs.py
            ${CMAKE_SOURCE_DIR}/symbol_addrs.us.txt ${CMAKE_SOURCE_DIR}/undefined_syms_auto.us.txt
            ${CMAKE_SOURCE_DIR}/rom_symbol_aliases.ld ${CMAKE_SOURCE_DIR}/port/data/asm_functions.txt
            ${CMAKE_SOURCE_DIR}/port/data/extents.txt ${CMAKE_SOURCE_DIR}/port/data/reached.txt
            ${CMAKE_SOURCE_DIR}/port/data/decoupled.txt
            ${CMAKE_SOURCE_DIR}/port/data/reserved.txt
            ${CMAKE_SOURCE_DIR}/asm/us/blob/symbols.json ${CMAKE_SOURCE_DIR}/asm/us/ovl_a/symbols.json
            ${CMAKE_SOURCE_DIR}/asm/us/ovl_b/symbols.json ${CMAKE_SOURCE_DIR}/layout/game.json
            ${CMAKE_SOURCE_DIR}/layout/ovl_a.json ${CMAKE_SOURCE_DIR}/layout/ovl_b.json
    COMMENT "Data storage and descriptors (tools/port/gen_data.py)"
    VERBATIM)
add_library(pcdata OBJECT ${PC_DATA_DIR}/data.c)
target_include_directories(pcdata PRIVATE port/data)
target_compile_options(pcdata PRIVATE -fno-common)   # the .set aliases need the arenas in a section
set_target_properties(pcdata PROPERTIES PC_NO_SANITIZE ON)   # no redzones between own storages (SANITIZE.md)
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
target_include_directories(pcdata_rt PUBLIC port/data port/include)   # port/include: pc_n64_map.h, the N64 memory map
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
if(NOT RUSH_COUNT_CROSSINGS)   # the crossing counter (docs/port/NATIVE.md 9): only in its own build
    list(FILTER PC_ULTRA_SOURCES EXCLUDE REGEX "/crossings\\.c$")
endif()
add_library(pcultra OBJECT port/main.c ${PC_ULTRA_SOURCES})
# The state hash (docs/port/LOCKSTEP.md 5) normalises and hashes the 8-256 MiB RDRAM window: optimized in every build,
# or a Debug build's tests that hash it every 30 retraces take minutes.
set_source_files_properties(port/ultra/statehash.c PROPERTIES COMPILE_OPTIONS "-O2")
target_include_directories(pcultra PRIVATE port/include port/ultra port/config ${ZLIB_INCLUDE_DIRS})
target_compile_options(pcultra PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
if(PC_SANITIZE)
    target_compile_definitions(pcultra PRIVATE PC_SANITIZE)   # the fault handler chains to the runtime's
endif()
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
# netplay.c: an online session, the game over port/net (docs/port/NETPLAY.md), with port/input's session rules
target_include_directories(pcapp PRIVATE port/net port/input)
if(WIN32)
    # timeBeginPeriod (startup.c); the Open ROM dialog, IFileOpenDialog (pick.c): ole32, which SDL2 imports already
    target_link_libraries(pcapp PUBLIC winmm ole32)
endif()
target_compile_options(pcapp PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
target_compile_definitions(pcapp PRIVATE "PC_BUILD_TYPE=\"$<CONFIG>\"")   # --version, the log, the About page
if(RUSH_SDL2_TARGET)
    target_compile_definitions(pcapp PRIVATE PC_HAVE_SDL2)
    target_link_libraries(pcapp PUBLIC ${RUSH_SDL2_TARGET})
endif()
# main.c: port/app, and the settings overlay's About page (port/input's interface, port/audio's numbers)
target_include_directories(pcultra PRIVATE port/app port/input port/audio)
# port/assets: the game's files converted to the host's byte order as _ReadWave loads them (docs/port/ASSETS.md).
# One file per format; assets.c has the dispatcher and weak no-ops for formats not converted yet.
file(GLOB PC_ASSETS_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/port/assets/*.c)
add_library(pcassets OBJECT ${PC_ASSETS_SOURCES})
target_include_directories(pcassets PRIVATE port/include port/assets)
target_compile_options(pcassets PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
list(PREPEND PC_PORT_OBJECT_LIBS pcultra pcfrontend pcapp pcdata_rt pcassets)   # port/data's loader (docs/port/DATA.md)
# port/mods: the mods folder (docs/port/MODS.md): mods' files in place of the cartridge's, checked by port/assets/check.c.
file(GLOB PC_MODS_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/port/mods/*.c)
add_library(pcmods OBJECT ${PC_MODS_SOURCES})
target_include_directories(pcmods PRIVATE port/include port/ultra port/config port/assets port/mods ${ZLIB_INCLUDE_DIRS})
target_compile_options(pcmods PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
list(APPEND PC_PORT_OBJECT_LIBS pcmods)
target_include_directories(pcultra PRIVATE port/mods port/data)   # main.c: the mods, port/data's image table
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
if(RUSH_NOFIXED)   # gen_data.py --nofixed reads the PC layer's definitions (PC_DATA_NOFIXED_ARGS above)
    add_custom_command(OUTPUT ${PC_DATA_DIR}/data.c APPEND DEPENDS ${PC_PORT_OBJECTS})
endif()
add_custom_command(
    OUTPUT ${PC_DATA_DIR}/stubs.c ${PC_DATA_DIR}/aliases.ld
    COMMAND ${PC_PYTHON} ${PORT_TOOLS}/gen_stubs.py --objects-list ${CMAKE_BINARY_DIR}/game_link_objects.txt
            --defined-list ${CMAKE_BINARY_DIR}/port_objects.txt --nm ${CMAKE_NM} --root ${CMAKE_SOURCE_DIR}
            --out ${PC_DATA_DIR}/stubs.c --aliases ${PC_DATA_DIR}/aliases.ld ${PC_NOFIXED_ARGS}
    DEPENDS ${PC_GAME_OBJECTS} ${PC_PORT_OBJECTS} ${PORT_TOOLS}/gen_stubs.py ${PORT_TOOLS}/n64syms.py
            ${CMAKE_SOURCE_DIR}/port/include/pc_n64_map.h
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
# pcnet: online play's network library (port/net, docs/port/NETWORK.md), for port/app/netplay.c; Winsock 2 on Windows
target_link_libraries(rush2049pc PRIVATE ${PC_PORT_OBJECT_LIBS} pcnet ZLIB::ZLIB Threads::Threads m)
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
set_tests_properties(ultra_selftest PROPERTIES RUN_SERIAL TRUE)   # its timers on the host's clock: alone (ctest -j)
add_test(NAME ultra_selftest_fast COMMAND rush2049pc --selftest --fast)   # the same on the virtual clock
# and in paced mode, the virtual clock paced to the wall clock (port/ultra/pace.c): a 2 s wait takes 2 s. Alone: it
# measures time
add_test(NAME ultra_selftest_paced COMMAND rush2049pc --selftest --paced)
set_tests_properties(ultra_selftest_paced PROPERTIES RUN_SERIAL TRUE LABELS realtime)
# the RDRAM window at game.memory 64 and 256 (the most): its size through every view, the RCP's addresses past 16 MiB
# (docs/port/MEMORY.md)
add_test(NAME ultra_selftest_memory64 COMMAND rush2049pc --selftest --fast --set game.memory=64)
add_test(NAME ultra_selftest_memory256 COMMAND rush2049pc --selftest --fast --set game.memory=256)
# A sanitizer build's fault handler names the N64 thread, then hands the fault to AddressSanitizer's, which reports
# it with its own stack (docs/port/SANITIZE.md 1): the blank cartridge's boot faults in AllocateMemory. (CMake's "."
# takes a newline; an alternation such as (.|\n)* backtracks for minutes over the report.)
if(PC_SANITIZE_ADDRESS)
    add_test(NAME sanitize_fault_chain COMMAND rush2049pc --no-rom --headless --fast --seconds 10 --no-config
        --audio null --no-pak --no-log)
    set_tests_properties(sanitize_fault_chain PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=handle_segv=1:detect_leaks=0;UBSAN_OPTIONS=halt_on_error=0"
        PASS_REGULAR_EXPRESSION "Segmentation fault at address [^\n]* in N64 thread .*ERROR: AddressSanitizer: SEGV")
endif()
if(WIN32)
    # The subsystem, the image base and ASLR, the icon's sizes and the version information, from the file itself
    # (Python only: it runs on a Linux host that cross-compiles too, ctest -R win_pe)
    set(PE_CHECK_ARGS "")
    if(RUSH_WINDOWS_CONSOLE)
        set(PE_CHECK_ARGS --console)
    endif()
    if(RUSH_FETCH_SDL2)   # SDL2 linked statically, as in the release: only Windows' own DLLs imported
        list(APPEND PE_CHECK_ARGS --dlls)
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
# time (28 s). On Windows too (CI's native job). --exact: the logs have the first car's position and speed as exact
# floats and a status line each second, the trace CI's windows-same job compares with the Windows program's
# (docs/port/WINDOWS.md 12).
set(SYNTH_ROM ${CMAKE_BINARY_DIR}/synth/rush2049_synth.z64)
add_test(NAME synth_rom COMMAND ${PC_PYTHON} ${PORT_TOOLS}/synth_rom.py --out ${SYNTH_ROM}
    --data-symbols ${PC_DATA_DIR}/data_symbols.txt)
set_tests_properties(synth_rom PROPERTIES FIXTURES_SETUP synth_rom)
file(GLOB SYNTH_PATHS CONFIGURE_DEPENDS ${PORT_TOOLS}/synth/*.press)
set(SYNTH_BOOT_TESTS)
# A sanitizer build runs the paths many times slower: under ASan on CI's 4-core arm runner 17-37 times (battle2p 4 s
# to 135 s, drive about 375 s alone), its stack-use-after-return checks among the cost.
set(SYNTH_PATH_TIMEOUT 300)
if(PC_SANITIZE)
    set(SYNTH_PATH_TIMEOUT 1500)
endif()
foreach(script ${SYNTH_PATHS})
    get_filename_component(name ${script} NAME_WE)
    add_test(NAME synth_boot_${name} COMMAND ${PC_PYTHON} ${PORT_TOOLS}/synth_paths.py
        --exe $<TARGET_FILE:rush2049pc> --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/paths
        --timeout ${SYNTH_PATH_TIMEOUT} --exact ${name})
    set_tests_properties(synth_boot_${name} PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 150)
    list(APPEND SYNTH_BOOT_TESTS synth_boot_${name})
    # a path on the host's clock (`# realtime`: the race) runs alone under `ctest -j`, as the other real-time tests,
    # and is labelled realtime with them (CI gives those more than one try: .github/workflows/pc.yml)
    file(STRINGS ${script} realtime REGEX "^#[ \t]*realtime([ \t]*:[ \t]*only)?[ \t]*$")
    if(realtime)
        set_tests_properties(synth_boot_${name} PROPERTIES RUN_SERIAL TRUE LABELS realtime)
    endif()
endforeach()
# [audio] voices and [audio] engines (docs/port/AUDIO.md 10): the synthetic cartridge with a sound bank (synth_rom.py
# --sounds: an sfx for every sound index; on the default cartridge nothing plays), and races of 12 cars along the
# synthetic paths at each voices size (more voices mixed and fewer starts refused at each, the game's trace the same)
# and with each engines mode (no more drones' engines on than the mode allows), through tools/port/voices_check.py.
set(SYNTH_ROM_SOUNDS ${CMAKE_BINARY_DIR}/synth/rush2049_synth_snd.z64)
add_test(NAME synth_rom_sounds COMMAND ${PC_PYTHON} ${PORT_TOOLS}/synth_rom.py --sounds --out ${SYNTH_ROM_SOUNDS}
    --data-symbols ${PC_DATA_DIR}/data_symbols.txt)
set_tests_properties(synth_rom_sounds PROPERTIES FIXTURES_SETUP synth_rom_sounds)
set(SYNTH_VOICES_TIMEOUT 600)   # 1-4 runs; a sanitizer build runs each many times slower (above)
if(PC_SANITIZE)
    set(SYNTH_VOICES_TIMEOUT 3000)
endif()
set(SYNTH_VOICES_CHECK ${PC_PYTHON} ${PORT_TOOLS}/voices_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM_SOUNDS} --logs ${CMAKE_BINARY_DIR}/synth/voices)
add_test(NAME synth_voices_race12 COMMAND ${SYNTH_VOICES_CHECK} voices race12 --sizes original,48,64,96)
add_test(NAME synth_voices_race2p12 COMMAND ${SYNTH_VOICES_CHECK} voices race2p12 --sizes original,64)
add_test(NAME synth_engines_race12_original COMMAND ${SYNTH_VOICES_CHECK} engines race12 --mode original)
add_test(NAME synth_engines_race12_nearest4 COMMAND ${SYNTH_VOICES_CHECK} engines race12 --mode nearest4)
add_test(NAME synth_engines_race12_all COMMAND ${SYNTH_VOICES_CHECK} engines race12 --mode all)
add_test(NAME synth_engines_race2p12_nearest4 COMMAND ${SYNTH_VOICES_CHECK} engines race2p12 --mode nearest4
    --players 2 --drones 10)
foreach(t synth_voices_race12 synth_voices_race2p12 synth_engines_race12_original synth_engines_race12_nearest4
        synth_engines_race12_all synth_engines_race2p12_nearest4)
    set_tests_properties(${t} PROPERTIES FIXTURES_REQUIRED synth_rom_sounds TIMEOUT ${SYNTH_VOICES_TIMEOUT})
endforeach()
# game.memory (docs/port/MEMORY.md 6): the program at original against itself at 64 MiB with everything the game
# allocates above 17 MiB (RUSH2049_TEST_HEAP_FLOOR), along paths of each kind: the same trace in every run, so the game
# runs as it does in 8 MiB with all of its data where a physical address would read as a segmented one.
# One test per path, so ctest -j runs them side by side. An ASan or TSan build runs three of them (one player, the
# split screen, 12 cars): under ASan on a 4-core arm runner a run takes several minutes, and all 22 runs in one test
# went past the CI job's 90 minutes (run 41788707907); the other builds run all 11.
set(SYNTH_MEMORY_SAME_PATHS drive race2p race4p demo battle2p stunt circuit pause raceend race12 race2p12)
set(SYNTH_MEMORY_SAME_TIMEOUT 600)   # 2 runs; a sanitizer build runs each many times slower (above)
if(PC_SANITIZE)
    set(SYNTH_MEMORY_SAME_TIMEOUT 3000)
endif()
if(PC_SANITIZE_ADDRESS OR PC_SANITIZE_THREAD)
    set(SYNTH_MEMORY_SAME_PATHS drive race2p race12)
endif()
foreach(name ${SYNTH_MEMORY_SAME_PATHS})
    add_test(NAME synth_memory_same_${name} COMMAND ${PC_PYTHON} ${PORT_TOOLS}/synth_compare.py
        --exe $<TARGET_FILE:rush2049pc> --exe $<TARGET_FILE:rush2049pc> --set-b game.memory=64
        --env-b RUSH2049_TEST_HEAP_FLOOR=17 --rom ${SYNTH_ROM} --runs 1 --same
        --logs ${CMAKE_BINARY_DIR}/synth/memory-same/${name} ${name})
    set_tests_properties(synth_memory_same_${name} PROPERTIES FIXTURES_REQUIRED synth_rom
        TIMEOUT ${SYNTH_MEMORY_SAME_TIMEOUT})
endforeach()
# and the picture: drive's screenshot at game.memory 64, with the game's data above 17 MiB, and with the car types'
# files 3 MB larger, byte for byte the original's; race2p's split screen with the data above 17 MiB the same.
foreach(pair "drive;mem64" "drive;memhigh" "drive;membig" "race2p;memhigh2p")
    list(GET pair 0 a)
    list(GET pair 1 b)
    set_property(TEST synth_boot_${a} APPEND PROPERTY FIXTURES_SETUP synth_shot_${a})
    set_property(TEST synth_boot_${b} APPEND PROPERTY FIXTURES_SETUP synth_shot_${b})
    add_test(NAME synth_shot_same_${b} COMMAND ${CMAKE_COMMAND} -E compare_files
        ${CMAKE_BINARY_DIR}/synth/paths/${a}.png ${CMAKE_BINARY_DIR}/synth/paths/${b}.png)
    set_tests_properties(synth_shot_same_${b} PROPERTIES FIXTURES_REQUIRED "synth_shot_${a};synth_shot_${b}")
endforeach()
# The mods folder's paths (docs/port/MODS.md 8): their mods, made from the synthetic cartridge's own files by
# tools/port/mods_synth.py in the logs folder (where the paths run, so --mods-dir mods/... reaches them), and drive's
# picture, which the runs whose mods change nothing on screen are compared with.
add_test(NAME mods_synth COMMAND ${PC_PYTHON} ${PORT_TOOLS}/mods_synth.py --rom ${SYNTH_ROM}
    --out ${CMAKE_BINARY_DIR}/synth/paths/mods)
set_tests_properties(mods_synth PROPERTIES FIXTURES_REQUIRED synth_rom FIXTURES_SETUP mods_synth)
foreach(script ${SYNTH_PATHS})
    get_filename_component(name ${script} NAME_WE)
    if(name MATCHES "^mods_")
        set_tests_properties(synth_boot_${name} PROPERTIES FIXTURES_REQUIRED "synth_rom;mods_synth;synth_shot_drive")
    endif()
endforeach()
# the texture pack mod is made from mods_dump's texture dump
set_tests_properties(synth_boot_mods_dump PROPERTIES FIXTURES_SETUP mods_pack)
foreach(name mods_texture mods_texture_up)
    set_tests_properties(synth_boot_${name} PROPERTIES FIXTURES_REQUIRED "synth_rom;mods_synth;mods_pack")
endforeach()
# Input record, replay and delay, the pads gate and the state hash (docs/port/LOCKSTEP.md 8, tools/port/replay_check.py).
# replay_same_<path>: each path on the virtual clock recorded (--record-inputs), then replayed (--replay-inputs) by
# another process: the same trace (states, status lines, every retrace's physics and pads, exact floats) and the same
# state hash at every retrace, the window's, own storage's and the decoupled data's every 30, with the processes'
# images apart (ASLR). Its recording and replay log stay in synth/replay for CI's windows-replay. replay_delay: with
# --input-delay 2 the game's pads are the script's 2 retraces later and the run is the script's moved 2 retraces, and a
# delay schedule gives the delay line's pads and replays to its trace. replay_file: a file cut short, damaged or of
# other settings. gate_same: pads held back at random retraces for random real times (RUSH_SIM_GATE) leave no trace.
# hash_mutation: a word changed at retrace 1000 (RUSH_SIM_DESYNC) is caught from that retrace on. hash_aslr: two
# processes with the full hash every retrace agree. hash_decoupled_list: the full hash takes every decoupled item.
# render_values: no new render or per-PC value reaches the game C (tools/port/render_values_allowed.txt). The
# selftest checks xxHash64's known values and the delay line's rule.
set(REPLAY_CHECK ${PC_PYTHON} ${PORT_TOOLS}/replay_check.py --exe $<TARGET_FILE:rush2049pc> --rom ${SYNTH_ROM}
    --logs ${CMAKE_BINARY_DIR}/synth/replay --mods ${CMAKE_BINARY_DIR}/synth/paths/mods)
set(REPLAY_TIMEOUT 600)
if(PC_SANITIZE)
    set(REPLAY_TIMEOUT 3000)
endif()
# Under ASan or TSan a path runs 17-37 times slower (above): three paths of each kind, the file's checks (its reader on
# cut and damaged files), the gate and the two processes' hashes on race2p.
set(REPLAY_SANITIZER_PATHS drive race2p race12)
foreach(script ${SYNTH_PATHS})
    get_filename_component(name ${script} NAME_WE)
    file(STRINGS ${script} only REGEX "^#[ \t]*realtime[ \t]*:[ \t]*only[ \t]*$")
    if(only)
        continue()   # it can't run on the virtual clock
    endif()
    if((PC_SANITIZE_ADDRESS OR PC_SANITIZE_THREAD) AND NOT name IN_LIST REPLAY_SANITIZER_PATHS)
        continue()
    endif()
    add_test(NAME replay_same_${name} COMMAND ${REPLAY_CHECK} same ${name})
    set(fixtures synth_rom)
    if(name MATCHES "^mods_texture")
        set(fixtures "synth_rom;mods_synth;mods_pack")
    elseif(name MATCHES "^mods_")
        set(fixtures "synth_rom;mods_synth")
    endif()
    set_tests_properties(replay_same_${name} PROPERTIES FIXTURES_REQUIRED "${fixtures}" TIMEOUT ${REPLAY_TIMEOUT})
endforeach()
# replay_same60_<path>: the same at game.frame_rate = 60 (docs/port/NETPLAY.md 17), its recordings and replay logs in
# synth/replay60, which CI's windows-replay replays too (with --set game.frame_rate=60): the Windows program must take
# Linux's course at 60 as at original. Paths across the game's modes (each of the 60 paths that reach their end at 60
# records and replays the same on rocky: NETPLAY.md 17).
set(REPLAY60_PATHS race2p race2p12 battle2p circuit circuit12 drive steer stunt solo demo pause race12 memhigh2p ghost)
if(NOT PC_SANITIZE)
    foreach(name ${REPLAY60_PATHS})
        add_test(NAME replay_same60_${name} COMMAND ${PC_PYTHON} ${PORT_TOOLS}/replay_check.py
                 --exe $<TARGET_FILE:rush2049pc> --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/replay60
                 --mods ${CMAKE_BINARY_DIR}/synth/paths/mods --set game.frame_rate=60 same ${name})
        set_tests_properties(replay_same60_${name} PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT ${REPLAY_TIMEOUT})
    endforeach()
endif()
set(REPLAY_TESTS replay_file gate_same hash_aslr)
add_test(NAME replay_file COMMAND ${REPLAY_CHECK} file race2p)
if(PC_SANITIZE_ADDRESS OR PC_SANITIZE_THREAD)
    add_test(NAME gate_same COMMAND ${REPLAY_CHECK} gate race2p)
    add_test(NAME hash_aslr COMMAND ${REPLAY_CHECK} aslr race2p)
else()
    add_test(NAME gate_same COMMAND ${REPLAY_CHECK} gate race2p stunt)
    add_test(NAME hash_aslr COMMAND ${REPLAY_CHECK} aslr race2p battle2p drive --full 1)
    add_test(NAME replay_delay COMMAND ${REPLAY_CHECK} delay race2p)
    add_test(NAME hash_mutation COMMAND ${REPLAY_CHECK} mutation race2p)
    list(APPEND REPLAY_TESTS replay_delay hash_mutation)
endif()
foreach(t ${REPLAY_TESTS})
    set_tests_properties(${t} PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT ${REPLAY_TIMEOUT})
endforeach()
# Online sessions (docs/port/NETPLAY.md "Status: the session", tools/port/net_session_check.py): two rush2049pc
# processes over UDP on 127.0.0.1, a host and a guest, each playing its own player's presses of a path.
# net_session_<path>: both traces the same, line for line (every retrace's physics, pads and hash), and the trace of
# one process playing the whole script with --input-delay as the session's fixed --net-delay; the one-player paths
# with the guest idle. net_session_sim_<model>: the same under RUSH_SIM_NET's models (docs/port/NETWORK.md 8): the
# network changes when retraces happen, never what they do. net_session_auto: the adaptive delay under wan80, and
# the host's input log replays to the same trace. net_session_paced: the session's own clock, paced (ULTRA.md 10):
# at 4 times the pace one --fast process's trace, at the real pace under wan80 60 retraces a second of wall time.
# net_session_fuzz: seeded random presses on both players (menus,
# sticks, the overlay opened over the game), the hashes the same at every retrace. net_session_desync: a word
# changed on one PC ends both at its retrace, each with its report. net_session_mismatch: another ROM, build or mods
# refused, a setup the handshake can't see caught at retrace 0. net_session_adopt: the guest takes the host's
# settings, the paks in memory only. net_session_ends: a killed peer (timeout), BYE, nobody there (no-answer, and its
# message box). net_session_wait: the host's waiting screen drawn, saved and cancelled; nobody joining (its box).
# Rejoins (NETPLAY.md 15): net_session_rejoin: the guest's process killed in the race and started again: it replays
# the host's log and plays on, its trace the host's line for line; _host: the host's ended by SIGTERM (its game gone,
# as a system shutting down) and started again, the guest knocking; _paced: on the session's own
# clock. net_session_outage: the link down 20 s, the guest stopped 20 s: past the old 15 s timeout, nothing lost.
# net_session_recover: a desync the guest rebuilds from (its program again) and the session goes on; _repeat: the same
# desync in the rebuilt game ends both, said deterministic.
set(NET_SESSION_CHECK ${PC_PYTHON} ${PORT_TOOLS}/net_session_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/net --mods ${CMAKE_BINARY_DIR}/synth/paths/mods)
set(NET_SESSION_PATHS race2p battle2p battlequit memhigh2p race2p12 pause raceend menu)
if(PC_SANITIZE)
    set(NET_SESSION_PATHS race2p)   # 17-37 times slower there
endif()
foreach(name ${NET_SESSION_PATHS})
    add_test(NAME net_session_${name} COMMAND ${NET_SESSION_CHECK} path ${name})
    set_tests_properties(net_session_${name} PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT ${REPLAY_TIMEOUT})
endforeach()
if(NOT PC_SANITIZE)
    foreach(m lan20 wan80 bad150 shift pause burst)
        add_test(NAME net_session_sim_${m} COMMAND ${NET_SESSION_CHECK} sim --model ${m})
        set_tests_properties(net_session_sim_${m} PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 600 RUN_SERIAL TRUE
                             LABELS realtime)
    endforeach()
    add_test(NAME net_session_auto COMMAND ${NET_SESSION_CHECK} auto --model wan80)
    add_test(NAME net_session_paced COMMAND ${NET_SESSION_CHECK} paced --model wan80)
    add_test(NAME net_session_fuzz COMMAND ${NET_SESSION_CHECK} fuzz --seeds 3 --seconds 120)
    add_test(NAME net_session_ends COMMAND ${NET_SESSION_CHECK} ends)
    set_tests_properties(net_session_auto net_session_paced net_session_fuzz net_session_ends PROPERTIES
                         FIXTURES_REQUIRED synth_rom TIMEOUT 900 RUN_SERIAL TRUE LABELS realtime)
endif()
add_test(NAME net_session_rejoin COMMAND ${NET_SESSION_CHECK} rejoin)
add_test(NAME net_session_recover COMMAND ${NET_SESSION_CHECK} recover)
set_tests_properties(net_session_rejoin net_session_recover PROPERTIES FIXTURES_REQUIRED synth_rom
                     TIMEOUT ${REPLAY_TIMEOUT})
if(NOT PC_SANITIZE)
    add_test(NAME net_session_rejoin_host COMMAND ${NET_SESSION_CHECK} rejoin --host-side --term)
    add_test(NAME net_session_recover_repeat COMMAND ${NET_SESSION_CHECK} recover --repeat)
    # _page: a guest the F1 Online page started (--net-return) rebuilds too, rather than going back to the game
    add_test(NAME net_session_recover_page COMMAND ${NET_SESSION_CHECK} recover --page)
    set_tests_properties(net_session_rejoin_host net_session_recover_repeat net_session_recover_page PROPERTIES
                         FIXTURES_REQUIRED synth_rom TIMEOUT ${REPLAY_TIMEOUT})
    add_test(NAME net_session_rejoin_paced COMMAND ${NET_SESSION_CHECK} rejoin --paced)
    add_test(NAME net_session_outage COMMAND ${NET_SESSION_CHECK} outage)
    set_tests_properties(net_session_rejoin_paced net_session_outage PROPERTIES FIXTURES_REQUIRED synth_rom
                         TIMEOUT 900 RUN_SERIAL TRUE LABELS realtime)
endif()
# At game.frame_rate = 60 (docs/port/NETPLAY.md 17): the host's 60, which the guest adopts. net_session_fps60: race2p
# and battle2p, host = guest = one process at 60, line for line; net_session_paced60: the session's own clock at 60,
# at 4 times the pace one --fast process's trace, at the real pace under wan80 60 retraces a second of wall time.
# Not under the sanitizers (17-37 times slower there).
if(NOT PC_SANITIZE)
    add_test(NAME net_session_fps60 COMMAND ${NET_SESSION_CHECK} path race2p battle2p
             --logs ${CMAKE_BINARY_DIR}/synth/net60 --host-set game.frame_rate=60)
    set_tests_properties(net_session_fps60 PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT ${REPLAY_TIMEOUT})
    add_test(NAME net_session_paced60 COMMAND ${NET_SESSION_CHECK} paced --model wan80
             --logs ${CMAKE_BINARY_DIR}/synth/net60 --host-set game.frame_rate=60)
    set_tests_properties(net_session_paced60 PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 900 RUN_SERIAL TRUE
                         LABELS realtime)
endif()
add_test(NAME net_session_desync COMMAND ${NET_SESSION_CHECK} desync)
add_test(NAME net_session_adopt COMMAND ${NET_SESSION_CHECK} adopt)
set_tests_properties(net_session_desync net_session_adopt PROPERTIES FIXTURES_REQUIRED synth_rom
                     TIMEOUT ${REPLAY_TIMEOUT})
add_test(NAME net_session_wait COMMAND ${NET_SESSION_CHECK} wait)
set_tests_properties(net_session_wait PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 300)
# net_session_router: net.upnp (docs/port/NETPLAY.md 16) against tools/port/fake_router.py on 127.0.0.1, never the
# real router: off by default (nothing asked), on (the address said, the mapping renewed and removed, the traces
# unchanged), a killed host and a host's game gone left to the lease, what the waiting screen says when it can't work
if(NOT PC_SANITIZE)
    add_test(NAME net_session_router COMMAND ${NET_SESSION_CHECK} router)
    set_tests_properties(net_session_router PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 600 RUN_SERIAL TRUE
                         LABELS realtime)
endif()
# net_session_view: own view (net.view = own) on one PC, split on the other, each way: one game, hash for hash, and
# each PC's screenshot what it shows (docs/port/NETPLAY.md 14)
add_test(NAME net_session_view COMMAND ${NET_SESSION_CHECK} view)
set_tests_properties(net_session_view PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT ${REPLAY_TIMEOUT})
# net_session_paknames: an imported ghost on the session's pak has the game's names, as offline (NETPLAY.md 17)
add_test(NAME net_session_paknames COMMAND ${NET_SESSION_CHECK} paknames)
set_tests_properties(net_session_paknames PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT ${REPLAY_TIMEOUT})
# net_session_rumble: net.rumble on one PC, each way round: both games have that player's rumble pak, one game, and its
# motor rumbles that player's controller alone (docs/port/NETPLAY.md 17); skipped without SDL's virtual controllers,
# and not under the sanitizers (eight runs, 17-37 times slower there)
if(RUSH_SDL2_TARGET AND NOT PC_SANITIZE)
    add_test(NAME net_session_rumble COMMAND ${NET_SESSION_CHECK} rumble)
    set_tests_properties(net_session_rumble PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT ${REPLAY_TIMEOUT}
                         SKIP_RETURN_CODE 77)
endif()
add_test(NAME net_session_mismatch COMMAND ${NET_SESSION_CHECK} mismatch)
set_tests_properties(net_session_mismatch PROPERTIES FIXTURES_REQUIRED "synth_rom;mods_synth"
                     TIMEOUT ${REPLAY_TIMEOUT})
# The F1 Online page (docs/port/NETPLAY.md 13, tools/port/net_menu_check.py): net_menu_restart, a process that hosts from
# the page (F1, Online, Host now) restarts into the host and a guest joins; one that joins from the page (a recent
# host, Join now) restarts into the guest; the hashes the same on both; the guest started from the page goes back to
# the game when the host leaves; a host that chooses Leave the session in its session restarts offline, its guest
# told. net_menu_replay: a session's input log replays offline as it is (--replay-inputs
# alone) and watched from the page, to the session's hashes. Their presses drive the overlay in real time.
set(NET_MENU_CHECK ${PC_PYTHON} ${PORT_TOOLS}/net_menu_check.py --exe $<TARGET_FILE:rush2049pc> --rom ${SYNTH_ROM}
    --logs ${CMAKE_BINARY_DIR}/synth/net-menu)
if(NOT PC_SANITIZE)
    add_test(NAME net_menu_restart COMMAND ${NET_MENU_CHECK} restart)
    add_test(NAME net_menu_replay COMMAND ${NET_MENU_CHECK} replay)
    set_tests_properties(net_menu_restart net_menu_replay PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 600
                         RUN_SERIAL TRUE LABELS realtime)
endif()
add_test(NAME hash_decoupled_list COMMAND ${PC_PYTHON} ${PORT_TOOLS}/hash_compare.py --check-decoupled)
add_test(NAME render_values COMMAND ${PC_PYTHON} ${PORT_TOOLS}/render_values_check.py)
# synth_compare.py's verdicts on made-up runs: a run that doesn't end as its script's `# expect:` line says fails,
# whatever the builds' agreement (docs/port/QA.md, "Integration QA"; the retail battle scripts in tools/port/retail).
add_test(NAME synth_compare_expect COMMAND ${PC_PYTHON} ${PORT_TOOLS}/test_synth_compare.py)
# The PC-only work's tools (docs/port/PCONLY.md 3): names_check over the name maps, rename.py and names_check on
# fixtures, the gate script's syntax.
include(cmake/pconly_tests.cmake)
# The first run (docs/port/RELEASE.md): what romcheck.c says a file is, on images the test makes; the folders and the
# ROM lookup order in temporary folders; and the program's own messages for each kind of missing or wrong ROM, and
# the lookup order, through tools/port/firstrun_check.py with copies of the synthetic cartridge.
add_executable(test_romcheck port/ultra/tests/test_romcheck.c port/ultra/romcheck.c)
target_include_directories(test_romcheck PRIVATE port/ultra port/include)
target_compile_options(test_romcheck PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
add_test(NAME ultra_romcheck COMMAND test_romcheck)
if(NOT WIN32)
    add_executable(test_absfix port/ultra/tests/test_absfix.c port/ultra/absfix.c)
    target_include_directories(test_absfix PRIVATE port/include)   # pc_n64_map.h
    target_compile_options(test_absfix PRIVATE -std=gnu11 -Wall -Wextra)
    set_target_properties(test_absfix PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
    add_test(NAME ultra_absfix COMMAND test_absfix)
endif()
add_executable(test_app port/app/tests/test_app.c port/app/paths.c)
target_include_directories(test_app PRIVATE port/app port/include)
target_compile_options(test_app PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
add_test(NAME app_paths COMMAND test_app ${CMAKE_BINARY_DIR}/test-scratch)
# The saves (docs/port/RELEASE.md "Saves"): what a pak image is, the atomic write, the backups and their rotation,
# a damaged pak set aside and the newest good backup put in its place, in a temporary folder.
add_executable(test_saves port/app/tests/test_saves.c port/app/saves.c port/app/paths.c port/ultra/pakfile.c)
target_include_directories(test_saves PRIVATE port/app port/ultra port/include)
target_compile_options(test_saves PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
add_test(NAME app_saves COMMAND test_saves ${CMAKE_BINARY_DIR}/test-scratch-saves)
# netplay/ kept to the newest online sessions (net.keep, docs/port/NETPLAY.md 17): what goes, what stays.
add_executable(test_netkeep port/app/tests/test_netkeep.c port/app/netkeep.c port/app/paths.c)
target_include_directories(test_netkeep PRIVATE port/app port/include)
target_compile_options(test_netkeep PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
set_target_properties(test_netkeep PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
add_test(NAME app_netkeep COMMAND test_netkeep ${CMAKE_BINARY_DIR}/test-scratch-netkeep)
# The paks' writer thread (port/ultra/pakwrite.c, docs/port/ULTRA.md "Controller pak") with a slow, held or failing
# fake write: submits never wait for a write, newer images replace queued ones, the order, one write at a time, a
# failure kept and tried again, the wait at the end, the fault path, and the real atomic write.
add_executable(test_pakwrite port/ultra/tests/test_pakwrite.c port/ultra/pakwrite.c port/ultra/pakfile.c)
target_include_directories(test_pakwrite PRIVATE port/ultra port/include)
target_compile_options(test_pakwrite PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(test_pakwrite PRIVATE Threads::Threads)
add_test(NAME ultra_pakwrite COMMAND test_pakwrite ${CMAKE_BINARY_DIR}/test-scratch-pakwrite)
set_target_properties(test_pakwrite PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
# game.saves = files (docs/port/SAVES.md): a pak's notes as files (port/ultra/pakfs.c, pakstore.c): images and notes,
# the save files, the ghost check, the saves folder (the .mpk imported once, the game's writes split into files, the
# choice of what is on the pak, imports, exports, a damaged file, an unwritable folder) and a fuzz of the readers.
add_executable(test_pakstore port/ultra/tests/test_pakstore.c port/ultra/pakfs.c port/ultra/pakstore.c
    port/ultra/pakfile.c)
target_include_directories(test_pakstore PRIVATE port/ultra port/include ${ZLIB_INCLUDE_DIRS})
target_compile_options(test_pakstore PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(test_pakstore PRIVATE ZLIB::ZLIB Threads::Threads m)
add_test(NAME ultra_pakstore COMMAND test_pakstore ${CMAKE_BINARY_DIR}/test-scratch-pakstore)
set_target_properties(test_pakstore PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
set_tests_properties(ultra_pakstore PROPERTIES FIXTURES_SETUP pakstore_files)
# tools/port/ghost.py, the saves' tool, against the C: the files ultra_pakstore leaves, and the C's own verdicts,
# images and imports (tools/port/test_ghost_c.c) on everything the tool makes.
add_executable(test_ghost_c tools/port/test_ghost_c.c port/ultra/pakfs.c port/ultra/pakstore.c port/ultra/pakfile.c)
target_include_directories(test_ghost_c PRIVATE port/ultra port/include ${ZLIB_INCLUDE_DIRS})
target_compile_options(test_ghost_c PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(test_ghost_c PRIVATE ZLIB::ZLIB Threads::Threads m)
set_target_properties(test_ghost_c PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
add_test(NAME app_firstrun COMMAND ${PC_PYTHON} ${PORT_TOOLS}/firstrun_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM})
set_tests_properties(app_firstrun PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 300)
set_target_properties(test_romcheck test_app test_saves PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
# A high frame rate leaves the game's timing alone (docs/port/GFX.md "High frame rate"): three paths on the virtual
# clock with video.frame_rate original and 120, their state and status lines compared line for line
# (tools/port/hfr_check.py), and frames drawn between the game's in the second run.
# Under AddressSanitizer, drive alone: the three paths took over 1,200 s on CI's 4-core arm runner.
set(HFR_TIMING_PATHS race2p drive demo)
if(PC_SANITIZE_ADDRESS)
    set(HFR_TIMING_PATHS drive)
endif()
add_test(NAME hfr_timing COMMAND ${PC_PYTHON} ${PORT_TOOLS}/hfr_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/hfr --require-drawn ${HFR_TIMING_PATHS})
# Six runs one after another, the longest test (about a minute alone on a CI runner): alone under ctest -j, where
# four other game processes made it two to three times as long, toward its timeout.
set_tests_properties(hfr_timing PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 300 RUN_SERIAL TRUE)
# Live input drives the game (docs/port/FRONTEND.md "Testing live input"): the drive path through SDL virtual
# controllers, a virtual joystick with the generic mapping, key events and a controller plugged in late, each the same
# run as the scripted one, with the car's steering input following the stick; race2p with two controllers
# (tools/port/input_check.py). Headless: live input pumps SDL's events itself.
if(RUSH_SDL2_TARGET)
    add_test(NAME input_live_drive COMMAND ${PC_PYTHON} ${PORT_TOOLS}/input_check.py --exe $<TARGET_FILE:rush2049pc>
        --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/input)
    set_tests_properties(input_live_drive PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 600 SKIP_RETURN_CODE 77
        LABELS realtime)
endif()
# The output in real time under a model of a Windows desktop (docs/port/AUDIO.md 7): the synthetic race with SDL's
# disk driver, a 59.94 Hz vsync and 15.6 ms timed waits; the retraces and the audio counters at exit. Alone, since
# it measures time.
add_test(NAME audio_realtime COMMAND ${PC_PYTHON} ${PORT_TOOLS}/realtime_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/realtime --seconds 30)
set_tests_properties(audio_realtime PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 200 RUN_SERIAL TRUE LABELS realtime)
# The same race at 120 frames a second with each swap held 40 ms, then until the vsync (a host always behind): the
# window must still show the game's frames (docs/port/QA.md "Integration soak").
add_test(NAME hfr_realtime COMMAND ${PC_PYTHON} ${PORT_TOOLS}/realtime_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/hfr_realtime --seconds 30 --frame-rate 120 --present-ms 40)
set_tests_properties(hfr_realtime PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 200 RUN_SERIAL TRUE LABELS realtime)
# The same race under a 59.94 Hz vsync swap at frame rates display, 60 and 120 (docs/port/FRAMERATE.md 2): ticks paced
# by the display's vblanks, at most 10% of them lost to skipped ticks (1.7-5.6% measured), frames drawn between (at
# least half of what the same host draws with vsync off: a GPU's count, not the pacing's), the retraces at 60 a
# second and the audio's bounds. The loop's waits precise, as Windows' high-resolution timer has them. The vsync
# model is Linux's: on Windows the hfr_paced tests are skipped (exit 77). In every pacing test at most 30 retraces
# one after another a whole retrace late (--max-late-run): the display's swaps once held the 60 Hz retraces a retrace
# behind for good, every retrace from the 1,000th caught up (FRAMERATE.md 2, "The retraces keep their 60 Hz").
add_test(NAME hfr_paced COMMAND ${PC_PYTHON} ${PORT_TOOLS}/realtime_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/hfr_paced --seconds 30 --pacing display,60,120
    --timer-step-us 0 --max-late-run 30)
set_tests_properties(hfr_paced PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 500 RUN_SERIAL TRUE SKIP_RETURN_CODE 77
    LABELS realtime)
# A driver that queues frames and never blocks (WGL on NVIDIA and Intel in steady state) on a 59.94 Hz display that SDL
# calls 59: no blocking swap corrects the vblank grid, so it runs at the display's own refresh, from the platform (its
# exact rate as XRandR gives it, and its vblanks as Windows' compositor does). At most 5 vblanks in 30 s show a frame
# twice with nothing decided for them (1 measured; a grid at SDL's 59: about 22).
add_test(NAME hfr_paced_queue COMMAND ${PC_PYTHON} ${PORT_TOOLS}/realtime_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/hfr_paced_queue --seconds 30 --pacing display
    --timer-step-us 0 --vsync-queue never --display-hz 59 --display-timing rate,vblank --max-doubled 5
    --max-late-run 30)
set_tests_properties(hfr_paced_queue PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 400 RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77 LABELS realtime)
# An exact 60.000 Hz display, its vblanks 0.05 of a refresh after the retraces (the worst phase: 1114 of 1200 ticks
# lost, none drawn between): the retraces are moved once to sit halfway between two vblanks (FRAMERATE.md 2), and
# at most 10% of the vblanks are lost to skipped ticks (2 measured), frames drawn between as hfr_paced's.
add_test(NAME hfr_paced_60 COMMAND ${PC_PYTHON} ${PORT_TOOLS}/realtime_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/hfr_paced_60 --seconds 30 --pacing display
    --timer-step-us 0 --vsync-hz 60 --vsync-phase 0.05 --display-timing vblank --max-late-run 30)
set_tests_properties(hfr_paced_60 PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 300 RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77 LABELS realtime)
# game.frame_rate 60 (docs/port/FRAMERATE.md 7). The game at 60 against original along the synthetic paths, on the
# virtual clock (tools/port/fps_compare.py): the same states in the same order, none more than 10 retraces apart (the
# game decides at a frame, 60 a second against 30), and in the races nothing on
# the game thread steers (solo: one player, the gas and the steering held; stunt; battle2p) the same physics, every
# car's position and velocity, model tick by model tick, to the bit. The others' physics first difference is
# reported (drones, resurrections: the game thread decides them once a frame).
# Under AddressSanitizer or ThreadSanitizer solo only (the 16 runs took over 600 s on CI's 4-core arm runner, run
# 41810017777, and solo and race 1,507 s, run 41821298325): the game at 60 under the sanitizer, with its physics checked.
set(FPS_PHYSICS_ARGS --physics solo,stunt,battle2p -j 2 solo stunt battle2p race demo menu screens raceend)
set(FPS_PHYSICS_TIMEOUT 600)
if(PC_SANITIZE_ADDRESS OR PC_SANITIZE_THREAD)
    set(FPS_PHYSICS_ARGS --physics solo -j 2 solo)
    set(FPS_PHYSICS_TIMEOUT 2400)
endif()
add_test(NAME fps_physics COMMAND ${PC_PYTHON} ${PORT_TOOLS}/fps_compare.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/fps --max-shift 10 ${FPS_PHYSICS_ARGS})
set_tests_properties(fps_physics PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT ${FPS_PHYSICS_TIMEOUT})
# A host that can't keep 60 (docs/port/FRAMERATE.md 7.7), on the virtual clock: with the RDP held 20 ms a graphics
# task (RUSH_SIM_GFX_MS) every frame at 60 takes two retraces; the log says once that the host can't keep 60, the
# game keeps its speed (the solo race's physics the same as at 60 unslowed), and at original nothing is said
# (tools/port/fps_slow.py).
# Not under AddressSanitizer or ThreadSanitizer (fps_physics's solo runs the game at 60 there; the sanitizer jobs'
# time is spent: .github/workflows/pc.yml).
if(NOT (PC_SANITIZE_ADDRESS OR PC_SANITIZE_THREAD))
    add_test(NAME fps_slow_host COMMAND ${PC_PYTHON} ${PORT_TOOLS}/fps_slow.py --exe $<TARGET_FILE:rush2049pc>
        --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/fps-slow)
    set_tests_properties(fps_slow_host PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 600)
endif()
# The high frame rate on top of the game at 60: hfr_timing's paths with the game at 60, video.frame_rate original
# against 120 (the same state and status lines, frames drawn between); hfr_paced's pacing under the 59.94 Hz vsync
# model with the game at 60 (a frame between the game's one retrace apart, at 120 and display), and the game's own
# frames at about one a retrace; audio_realtime's bounds with the game at 60, video.frame_rate original under the
# vsync swap, the waits precise as Windows' high-resolution timer has them: with the 15.6 ms waits of a Windows
# without one the game at 60 keeps about 0.8 of its frames (docs/port/FRAMERATE.md 7.7).
# Not under AddressSanitizer: hfr_timing's one path there takes about 760 s on CI's arm runner, and this one at 60
# went past 300 s (run 41810017777); fps_physics runs the game at 60 under it.
if(NOT PC_SANITIZE_ADDRESS)
    add_test(NAME hfr_timing_game60 COMMAND ${PC_PYTHON} ${PORT_TOOLS}/hfr_check.py --exe $<TARGET_FILE:rush2049pc>
        --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/hfr60 --require-drawn --game-fps 60 ${HFR_TIMING_PATHS})
    set_tests_properties(hfr_timing_game60 PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 300 RUN_SERIAL TRUE)
endif()
add_test(NAME hfr_paced_game60 COMMAND ${PC_PYTHON} ${PORT_TOOLS}/realtime_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/hfr_paced_game60 --seconds 30 --pacing display,60,120
    --timer-step-us 0 --game-fps 60 --max-late-run 30)
set_tests_properties(hfr_paced_game60 PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 500 RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77 LABELS realtime)
# The same on a busy host, whose timed waits are precise but now and then late (RUSH_SIM_WAKE_LATE: every 50th that
# times out returns 2.5 ms late; on rocky held to two cores with llvmpipe, 1-8% of them came 2 ms late or more), at
# display and 60 (FRAMERATE.md 2, "The retraces keep their 60 Hz"): the loop stays off the coarse waits' way (one late
# wait once put it there for good: the game at 60 kept 0.6 of its frames, and CI's runners saw 805 of 1,800 retraces
# caught up), the game keeps 0.9 of its frames and no retrace is held behind. The ticks are planned earlier, and a late
# wake costs its tick, so up to 25% of the vblanks may be lost to skipped ticks (11% measured).
add_test(NAME hfr_paced_busy COMMAND ${PC_PYTHON} ${PORT_TOOLS}/realtime_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/hfr_paced_busy --seconds 30 --pacing display,60
    --timer-step-us 0 --game-fps 60 --wake-late 50:2500 --max-late-run 30 --max-lost 0.25)
set_tests_properties(hfr_paced_busy PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 400 RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77 LABELS realtime)
add_test(NAME audio_realtime_game60 COMMAND ${PC_PYTHON} ${PORT_TOOLS}/realtime_check.py
    --exe $<TARGET_FILE:rush2049pc> --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/realtime60 --seconds 30
    --game-fps 60 --timer-step-us 0 --max-late-run 30)
set_tests_properties(audio_realtime_game60 PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 200 RUN_SERIAL TRUE
    LABELS realtime)
# The game at 60 with video.frame_rate original on an exact 60.000 Hz display whose vblanks come just after the
# retraces (0.02 of a refresh), its timing given as Windows' compositor gives it: each frame presented at its retrace
# with a vsync swap waited a whole refresh there, and the game kept 0.73 of its frames, 0.5 at worse phases (30 a
# second); paced by the vblanks (pc_gfx_set_own_paced) it keeps 0.97 and each vblank shows its next frame
# (docs/port/FRAMERATE.md 7.6). At most 0.1 of the vblanks without a new frame: 0.01 on rocky, 0.045-0.053 on CI's
# x86-64 runner (run 41908916509), 0.264 before the pacing.
add_test(NAME realtime_game60_v60 COMMAND ${PC_PYTHON} ${PORT_TOOLS}/realtime_check.py
    --exe $<TARGET_FILE:rush2049pc> --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/realtime60_v60 --seconds 30
    --game-fps 60 --timer-step-us 0 --vsync-hz 60 --vsync-phase 0.02 --display-timing vblank --max-stale 0.1
    --max-late-run 30)
set_tests_properties(realtime_game60_v60 PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 200 RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77 LABELS realtime)
# video.frame_rate = auto with the game at 60 under the vsync model at 60 and 120 Hz (the platform giving the rate):
# the shown moment steps off the vblank's period by at most 7 and 14 ms rms over the race (4.7 and 8.3-11.3 on rocky
# alone; 8.33 is an even hold of two vblanks; tools/port/vsync_even.py, docs/port/FRAMERATE.md 7.9).
add_test(NAME realtime_auto_even COMMAND ${PC_PYTHON} ${PORT_TOOLS}/vsync_even.py
    --exe $<TARGET_FILE:rush2049pc> --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/realtime_auto --hz 60,120
    --game 60 --rate auto -j 1 --check 60:60:auto:7 --check 120:60:auto:14)
set_tests_properties(realtime_auto_even PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 200 RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77 LABELS realtime)
# Paced mode (docs/port/ULTRA.md 10, NETPLAY.md 5 phase 0.1-0.2): online play's clock, --fast's virtual clock with
# each of its moves made at its wall time (port/ultra/pace.c), the presentation in real time, the audio device fed from
# a tap. paced_same_<path>: every path on the virtual clock takes --fast's course to the bit in paced mode
# (tools/port/synth_compare.py, the program against itself with RUSH_PACED=1). At RUSH_PACE_SPEED=4, N64 time four
# times the wall's: the pacing's code, a quarter of each path's game time; the whole set at the real pace is a run by
# hand (ULTRA.md 10). Not alone: what else ctest runs loads the host, which mustn't matter either. A sanitizer build
# runs three of them (one player, the split screen, 12 cars), as synth_memory_same.
set(PACED_SPEED 4)
set(PACED_SAME_TIMEOUT 600)
if(PC_SANITIZE)
    set(PACED_SAME_TIMEOUT 3000)
endif()
foreach(script ${SYNTH_PATHS})
    get_filename_component(name ${script} NAME_WE)
    file(STRINGS ${script} realtime_only REGEX "^#[ \t]*realtime[ \t]*:[ \t]*only[ \t]*$")
    if(realtime_only)   # pcsetup: the host's clock only
        continue()
    endif()
    if((PC_SANITIZE_ADDRESS OR PC_SANITIZE_THREAD) AND NOT name MATCHES "^(drive|race2p|race12)$")
        continue()
    endif()
    set(link)
    if(name MATCHES "^mods_" AND NOT WIN32)   # their mods, as synth_boot_mods_* have them (a symbolic link to them)
        set(link --link mods=${CMAKE_BINARY_DIR}/synth/paths/mods)
    endif()
    add_test(NAME paced_same_${name} COMMAND ${PC_PYTHON} ${PORT_TOOLS}/synth_compare.py
        --exe $<TARGET_FILE:rush2049pc> --exe $<TARGET_FILE:rush2049pc> --env-b RUSH_PACED=1
        --env-b RUSH_PACE_SPEED=${PACED_SPEED} --rom ${SYNTH_ROM} --runs 1 --same ${link}
        --logs ${CMAKE_BINARY_DIR}/synth/paced-same/${name} ${name})
    set_tests_properties(paced_same_${name} PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT ${PACED_SAME_TIMEOUT})
    if(link)
        set_tests_properties(paced_same_${name} PROPERTIES FIXTURES_REQUIRED "synth_rom;mods_synth;mods_pack")
    endif()
endforeach()
# The same under the timing models, which change real time's course (docs/port/AUDIO.md 7, FRAMERATE.md 4), for both
# runs: each present held 40 ms of wall time (a host too slow for the pace: the game slows in wall time, re-based
# again and again), the timed waits in Windows' coarse 15.6 ms steps, a 59.94 Hz vsync swap with frames drawn between
# at 120 (paced by the display's vblanks, on the wall clock), and the RDP held 40 ms of N64 time a graphics task, which
# changes --fast's course too (docs/port/NETPLAY.md E4): paced mode takes that one.
foreach(model "present;RUSH_SIM_PRESENT_MS=40" "timerstep;RUSH_SIM_TIMER_STEP_US=15625"
              "vsync;RUSH_SIM_VSYNC_HZ=59.94" "gfx;RUSH_SIM_GFX_MS=40")
    list(GET model 0 tag)
    list(GET model 1 env)
    add_test(NAME paced_model_${tag} COMMAND ${PC_PYTHON} ${PORT_TOOLS}/synth_compare.py
        --exe $<TARGET_FILE:rush2049pc> --exe $<TARGET_FILE:rush2049pc> --env-b RUSH_PACED=1
        --env-b RUSH_PACE_SPEED=${PACED_SPEED} --set-b video.frame_rate=120 --rom ${SYNTH_ROM} --runs 1 --same
        --logs ${CMAKE_BINARY_DIR}/synth/paced-model/${tag} drive race2p)
    set_tests_properties(paced_model_${tag} PROPERTIES FIXTURES_REQUIRED synth_rom ENVIRONMENT ${env}
        TIMEOUT ${PACED_SAME_TIMEOUT})
endforeach()
# In real time (tools/port/paced_check.py): the race paced at 60 retraces a second of wall time (59.95-60.05, at most
# two lags re-based, besides those of the host's own work: a software renderer's first draw with a shader), its trace
# and the sound it pushed --fast's (paced_realtime); the same with SDL's disk driver fed from the tap, the AI's queue
# the null output's on the N64 clock (paced_audio: no dry FIFO after the first second but after the host's own lags,
# its mean level within 15 ms of 50); and with each present held 40 ms of wall time, a host too slow for the pace: the
# game slows in wall time and takes the same course (paced_slow_host); and beside four busy processes (paced_load, at
# RUSH_PACE_SPEED 4: a loaded host only delays the moves). Alone: they measure time, or load the host.
add_test(NAME paced_realtime COMMAND ${PC_PYTHON} ${PORT_TOOLS}/paced_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --path race --logs ${CMAKE_BINARY_DIR}/synth/paced-realtime)
add_test(NAME paced_audio COMMAND ${PC_PYTHON} ${PORT_TOOLS}/paced_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --path race --audio sdl --logs ${CMAKE_BINARY_DIR}/synth/paced-audio)
add_test(NAME paced_slow_host COMMAND ${PC_PYTHON} ${PORT_TOOLS}/paced_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --path race --model RUSH_SIM_PRESENT_MS=40 --slowed
    --logs ${CMAKE_BINARY_DIR}/synth/paced-slow)
add_test(NAME paced_load COMMAND ${PC_PYTHON} ${PORT_TOOLS}/paced_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --path drive --speed ${PACED_SPEED} --load 4 --slowed
    --logs ${CMAKE_BINARY_DIR}/synth/paced-load)
# Holds: the game held 3 s at retrace 600 as the pause key holds it (RUSH_SIM_HOLD), and the process stopped whole for
# 2 s at 12 s of wall time (SIGSTOP, a debugger's stop; POSIX): the pacing moves on by the time held (hold()) or
# re-bases the lag, the rest at 60 a second, no burst of retraces after it, and the course --fast's without either.
add_test(NAME paced_hold COMMAND ${PC_PYTHON} ${PORT_TOOLS}/paced_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --path race --env RUSH_SIM_HOLD=600:3000 --held 3 --logs ${CMAKE_BINARY_DIR}/synth/paced-hold)
set(PACED_REALTIME_TESTS paced_realtime paced_audio paced_slow_host paced_load paced_hold paced_coarse)
# Windows' coarse waits (RUSH_SIM_TIMER_STEP_US=15625, a Windows without the high-resolution timer): the pace still 60
# a second, each retrace caught up from its step (paced_coarse); and the presentation's models in real time
# (tools/port/realtime_check.py --paced): the Windows desktop's (a 59.94 Hz vsync swap and those waits, the game's own
# frames; paced_desktop), and frames drawn between paced by a 59.94 Hz display's vblanks at display and 120 with
# precise waits, as hfr_paced checks real time's (paced_vsync: at most 10% of the vblanks lost, the retraces 60 a
# second). Paced mode never takes real time's coarse way of pacing the ticks (the swap as the clock), which would
# starve the N64 side: with coarse waits and a vsync swap its frames between mostly miss their vblank, the game's
# own frames are shown, and the game keeps its pace.
add_test(NAME paced_coarse COMMAND ${PC_PYTHON} ${PORT_TOOLS}/paced_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --path race --model RUSH_SIM_TIMER_STEP_US=15625 --logs ${CMAKE_BINARY_DIR}/synth/paced-coarse)
add_test(NAME paced_desktop COMMAND ${PC_PYTHON} ${PORT_TOOLS}/realtime_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/paced-desktop --seconds 30 --paced)
add_test(NAME paced_vsync COMMAND ${PC_PYTHON} ${PORT_TOOLS}/realtime_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/paced-vsync --seconds 30 --pacing display,120 --timer-step-us 0
    --paced)
list(APPEND PACED_REALTIME_TESTS paced_desktop paced_vsync)
# The pads gate (pc_lockstep.h) holding retraces at random for up to 40 ms of real time (RUSH_SIM_GATE, the paced run
# only): the course and the state hash every retrace --fast's without it, the pads that came over half a retrace late
# re-based by the network library's rule (paced_gate). And audio.latency 100 with SDL's disk driver fed from the tap,
# against --fast's null output at 0: the AI's queue is the null output's whatever the device and the latency, so the
# state hash every retrace (the RDRAM window every 60) is the same (paced_latency; with --fast the latency changes the
# window from retrace 0, docs/port/LOCKSTEP.md).
add_test(NAME paced_gate COMMAND ${PC_PYTHON} ${PORT_TOOLS}/paced_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --path race2p --env RUSH_SIM_GATE=7,0.05,40 --hash 1 --slowed
    --logs ${CMAKE_BINARY_DIR}/synth/paced-gate)
add_test(NAME paced_latency COMMAND ${PC_PYTHON} ${PORT_TOOLS}/paced_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --path race --audio sdl --set audio.latency=100 --hash 1,60
    --logs ${CMAKE_BINARY_DIR}/synth/paced-latency)
list(APPEND PACED_REALTIME_TESTS paced_gate paced_latency)
if(NOT WIN32)
    add_test(NAME paced_stop COMMAND ${PC_PYTHON} ${PORT_TOOLS}/paced_check.py --exe $<TARGET_FILE:rush2049pc>
        --rom ${SYNTH_ROM} --path race --stop 2@12 --held 2 --logs ${CMAKE_BINARY_DIR}/synth/paced-stop)
    list(APPEND PACED_REALTIME_TESTS paced_stop)
endif()
foreach(t ${PACED_REALTIME_TESTS})
    set_tests_properties(${t} PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 600 RUN_SERIAL TRUE SKIP_RETURN_CODE 77
        LABELS realtime)
endforeach()
# Every frame at 60 sees one model tick, at original two, and the camera's eye and view move as evenly at 60 as at
# original along the synthetic drive, on the virtual clock (tools/port/fps_even.py, --log-frames).
# Not under AddressSanitizer or ThreadSanitizer: its two runs of drive took past 300 s on CI's arm ASan runner (run
# 41865036645); fps_physics's solo runs the game at 60 there.
if(NOT (PC_SANITIZE_ADDRESS OR PC_SANITIZE_THREAD))
    add_test(NAME fps_even COMMAND ${PC_PYTHON} ${PORT_TOOLS}/fps_even.py --exe $<TARGET_FILE:rush2049pc>
        --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/fps-even drive)
    set_tests_properties(fps_even PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 300)
endif()
# A slow disk (each pak write held 200 ms, one a second through the race) costs the main loop no retraces: the writes
# are on the writer's thread (port/ultra/pakwrite.c, docs/port/ULTRA.md "Controller pak"); SIGTERM during a held
# write waits for it; and the same writes on the main loop, as before, drop retraces (tools/port/pakwrite_check.py).
# POSIX: it sends SIGTERM.
if(NOT WIN32)
    add_test(NAME pak_realtime COMMAND ${PC_PYTHON} ${PORT_TOOLS}/pakwrite_check.py --exe $<TARGET_FILE:rush2049pc>
        --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/pakwrite)
    set_tests_properties(pak_realtime PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 400 RUN_SERIAL TRUE
        LABELS realtime)
endif()
# game.saves = files on the synthetic cartridge (tools/port/saves_check.py, docs/port/SAVES.md 10): synthetic ghosts
# and hostile files imported (the good ones raced on, each bad one refused with its reason), an .mpk imported and
# made again byte for byte, the game's own save code making, reading back, changing and deleting a profile and saving
# and racing a ghost, each run's files checked on disk; an imported ghost raced. The swap is a real-time run (the
# F1 overlay's script): alone.
add_test(NAME ghost_tool COMMAND ${PC_PYTHON} ${PORT_TOOLS}/test_ghost.py ${CMAKE_BINARY_DIR}/test-scratch-pakstore
    --c-check $<TARGET_FILE:test_ghost_c> --work ${CMAKE_BINARY_DIR}/test-scratch-ghost)
set_tests_properties(ghost_tool PROPERTIES FIXTURES_REQUIRED pakstore_files TIMEOUT 300)
add_test(NAME saves_files COMMAND ${PC_PYTHON} ${PORT_TOOLS}/saves_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/saves import migrate game race)
set_tests_properties(saves_files PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 1500)
if(PC_SANITIZE)   # under ASan on CI's 4 arm cores it took 1,150 s before the ghost prompt's runs, and passed 1,500 after
    set_tests_properties(saves_files PROPERTIES TIMEOUT 3000)
endif()
add_test(NAME saves_files_swap COMMAND ${PC_PYTHON} ${PORT_TOOLS}/saves_check.py --exe $<TARGET_FILE:rush2049pc>
    --rom ${SYNTH_ROM} --logs ${CMAKE_BINARY_DIR}/synth/saves-swap swap)
set_tests_properties(saves_files_swap PROPERTIES FIXTURES_REQUIRED synth_rom TIMEOUT 600 RUN_SERIAL TRUE
    LABELS realtime)
# The GSM kernels (port/ultra/gsm.c) against their own MIPS code (interpreted from kernels.s) and a GSM 06.10
# reference, alone and under the game's frame decoder (gsm.c, built with the game's flags).
add_library(test_gsm_game OBJECT port/ultra/tests/gsm_game.c)
target_link_libraries(test_gsm_game PRIVATE game_flags)
target_compile_options(test_gsm_game PRIVATE -w)
add_executable(test_gsm port/ultra/tests/test_gsm.c port/ultra/gsm.c port/ultra/n64mem.c
    $<TARGET_OBJECTS:test_gsm_game>)
target_include_directories(test_gsm PRIVATE port/ultra port/include)
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
        port/ultra/rcpaddr.c $<TARGET_OBJECTS:pcassets>)
    target_include_directories(test_assets_${n} PRIVATE port/include port/assets port/assets/tests)
    target_compile_options(test_assets_${n} PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
    target_link_libraries(test_assets_${n} PRIVATE m)
    set_target_properties(test_assets_${n} PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
    add_test(NAME assets_${n} COMMAND test_assets_${n})
    # A test that runs the game's own readers on its converted files names their sources on ` * game: ...` lines
    # of its leading comment: they are compiled as the game is (game_flags) and linked in, with the test
    # standing in for everything else they reference. The lines are read when configuring, so the test file is a
    # configure dependency (an edited list once left a build linking without the new sources).
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${t})
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
# The mods folder's test (docs/port/MODS.md): port/mods and the INI reader with the converters; its `names` and
# `verdicts` modes are compared with tools/mods/rushmods.py's (the same names, the same rules).
target_sources(test_assets_mods PRIVATE ${PC_MODS_SOURCES} port/config/config.c)
target_include_directories(test_assets_mods PRIVATE port/mods port/config port/ultra)
target_link_libraries(test_assets_mods PRIVATE ZLIB::ZLIB)
set_tests_properties(assets_mods PROPERTIES SKIP_RETURN_CODE 77)
add_test(NAME mods_tool COMMAND ${PC_PYTHON} ${CMAKE_SOURCE_DIR}/tools/mods/rushmods.py selftest
    --program $<TARGET_FILE:test_assets_mods> --work ${CMAKE_BINARY_DIR}/mods-selftest)
# The race roster's test declares more slots than the game walks: 16 where the game build has 13
# (port/include/pc_game_types.h, docs/port/DECOUPLE.md).
target_compile_definitions(test_assets_roster PRIVATE PC_MAX_LINKS=16)
target_compile_definitions(test_assets_roster_game PRIVATE PC_MAX_LINKS=16)
# So does the model records' test: 16 where the game build has 12 and the N64 6.
target_compile_definitions(test_assets_models PRIVATE PC_MAX_LINKS=16)
target_compile_definitions(test_assets_models_game PRIVATE PC_MAX_LINKS=16)
# And the car records' test.
target_compile_definitions(test_assets_carrecs PRIVATE PC_MAX_LINKS=16)
target_compile_definitions(test_assets_carrecs_game PRIVATE PC_MAX_LINKS=16)
# And the effects' test: 16 slots where a race of 12 cars uses 12 (docs/port/DECOUPLE.md 6).
target_compile_definitions(test_assets_fxcars PRIVATE PC_MAX_LINKS=16)
target_compile_definitions(test_assets_fxcars_game PRIVATE PC_MAX_LINKS=16)

# And the test of the cars' sounds, checkpoint state and finish cameras (12 cars in the race).
target_compile_definitions(test_assets_percar PRIVATE PC_MAX_LINKS=16)
target_compile_definitions(test_assets_percar_game PRIVATE PC_MAX_LINKS=16)
# The pools' test shrinks them, so the checks are what it reaches, and runs the frame's through port/ultra/pools.c
# (docs/port/MORECARS.md 8).
set(PC_POOLS_TEST_SIZES PC_MATRIX_SPILL=4 PC_FRAME_MATRICES=8 PC_GLIST_LEN=32)
target_compile_definitions(test_assets_pools PRIVATE ${PC_POOLS_TEST_SIZES})
target_compile_definitions(test_assets_pools_game PRIVATE ${PC_POOLS_TEST_SIZES})
target_sources(test_assets_pools PRIVATE port/ultra/pools.c)
target_include_directories(test_assets_pools PRIVATE port/ultra)
# The heap's test runs port/ultra/heap.c on the game's own heap (docs/port/MEMORY.md 4), in a 32 MiB window.
target_sources(test_assets_heap PRIVATE port/ultra/heap.c)
target_include_directories(test_assets_heap PRIVATE port/ultra)
target_compile_definitions(test_assets_heap PRIVATE TEST_HEAP_C)   # support.c leaves the heap's reports to heap.c

# game.frame_rate's follow camera (docs/port/FRAMERATE.md 7): the game's frame steps, as the game has them.
target_sources(test_assets_fps PRIVATE port/config/game_fps.c)
target_include_directories(test_assets_fps PRIVATE port/config)
target_compile_definitions(test_assets_fps PRIVATE PC_TEST_GAME_FPS)   # tests/support.c leaves them to it
# Its N64 expression must round as the game's does (-ffp-contract=off, game_flags): a compiler that targets FMA by
# default (x86-64-v3, Rocky 10's) fused the test's own and missed it by a bit in an optimized build.
target_compile_options(test_assets_fps PRIVATE -ffp-contract=off)
# And the drones' (the lists, maxpath's controls, the schedule; docs/port/DECOUPLE.md 8).
target_compile_definitions(test_assets_drones PRIVATE PC_MAX_LINKS=16)
target_compile_definitions(test_assets_drones_game PRIVATE PC_MAX_LINKS=16)
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
add_library(test_wad_game OBJECT src/blob/groups/waves/waves.c src/blob/MBSetupWad.c
    src/blob/MBGetFromWad.c src/blob/MBFindInWad.c src/blob/MBOffsetWad.c src/blob/relocate_texture_loads.c
    src/blob/relocate_model_dl.c src/blob/mirror_vertex_texcoords.c src/blob/groups/slot_sound/MirrorWaveInstance.c
    src/blob/display_list_traverse.c src/blob/flip_triangle_winding.c src/blob/ModelActive.c src/blob/MBOX_FindTexture_WORK.c
    src/blob/MBOX_FindPalette_WORK.c src/blob/InitTextureAnimation.c src/blob/TextureAnimation.c src/blob/displaylist_get_texture_image.c
    src/blob/displaylist_set_texture_image.c src/blob/displaylist_set_tile_size.c src/blob/texcmp.c src/blob/game_strcmp.c)
target_link_libraries(test_wad_game PRIVATE game_flags)
target_compile_options(test_wad_game PRIVATE -w)
add_executable(test_assets_wad_game port/assets/tests/wad_game.c port/assets/tests/support.c port/ultra/n64mem.c
    port/ultra/rcpaddr.c $<TARGET_OBJECTS:pcassets> $<TARGET_OBJECTS:test_wad_game>)
target_include_directories(test_assets_wad_game PRIVATE port/include port/assets port/assets/tests)
target_compile_definitions(test_assets_wad_game PRIVATE TEST_OWN_WINDOW)   # its own 64 MiB window's functions
target_compile_options(test_assets_wad_game PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
set_target_properties(test_assets_wad_game test_wad_game PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
# rush2049pc_assetcheck: every wave of your ROM through the converters, checked as the game reads it
# (port/assets/tests/assetcheck.c, docs/port/ASSETS.md 8). Skipped (77) without a ROM.
add_executable(rush2049pc_assetcheck port/assets/tests/assetcheck.c port/assets/tests/support.c port/ultra/n64mem.c
    port/ultra/absfix.c $<TARGET_OBJECTS:pcassets>)
target_include_directories(rush2049pc_assetcheck PRIVATE port/include port/assets port/assets/tests)
target_compile_options(rush2049pc_assetcheck PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
target_compile_definitions(rush2049pc_assetcheck PRIVATE PC_DATA_ROM="${CMAKE_SOURCE_DIR}/baserom.us.z64"
    PC_SYMBOL_FILE="${CMAKE_SOURCE_DIR}/symbol_addrs.us.txt")   # the images' and the file table's addresses
target_link_libraries(rush2049pc_assetcheck PRIVATE ZLIB::ZLIB m)
set_target_properties(rush2049pc_assetcheck PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
add_test(NAME assets_rom COMMAND rush2049pc_assetcheck)
set_tests_properties(assets_rom PROPERTIES SKIP_RETURN_CODE 77)
add_test(NAME assets_wad_game COMMAND test_assets_wad_game)
set_tests_properties(assets_wad_game PROPERTIES SKIP_RETURN_CODE 77)
# A test program (main) and a second copy of gsm.c: kept out of compile_commands.json (census.py).
set_target_properties(test_gsm test_gsm_game PROPERTIES EXPORT_COMPILE_COMMANDS OFF)

# The same stubs without the PC layer, for test programs that link the game alone (rush2049pc_datacheck): every
# libultra name is stubbed, and the stubs abort by themselves.
add_custom_command(
    OUTPUT ${PC_DATA_DIR}/stubs_standalone.c
    COMMAND ${PC_PYTHON} ${PORT_TOOLS}/gen_stubs.py --objects-list ${CMAKE_BINARY_DIR}/game_link_objects.txt
            --nm ${CMAKE_NM} --root ${CMAKE_SOURCE_DIR} --out ${PC_DATA_DIR}/stubs_standalone.c --standalone
    DEPENDS ${PC_GAME_OBJECTS} ${PORT_TOOLS}/gen_stubs.py ${PORT_TOOLS}/n64syms.py
            ${CMAKE_SOURCE_DIR}/port/include/pc_n64_map.h
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
            ${CMAKE_SOURCE_DIR}/port/include/pc_n64_map.h
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
target_compile_definitions(rush2049pc_datacheck PRIVATE PC_DATA_ROM="${CMAKE_SOURCE_DIR}/baserom.us.z64"
    PC_SYMBOL_FILE="${CMAKE_SOURCE_DIR}/symbol_addrs.us.txt")   # the inflate tables' addresses, by name
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
        src/blob/groups/save/AudioInit.c src/blob/groups/audio_emitters/group.c
        src/blob/groups/audiomgr/ProcessSequences.c src/blob/groups/audiomgr/audiomgr_thread.c
        src/blob/groups/aud_stop_effect/group.c src/blob/groups/audiomgr_emitters/group.c src/blob/groups/transform/group.c
        src/blob/groups/audiomgr/ProcessEmitters.c src/blob/groups/update_fx/updatefx.c
        src/blob/groups/interrupt_fx_tag/InterruptFxTag.c src/blob/groups/weighted_priority/group.c src/blob/AudEffectPlaying.c)
    set(SOUND_SEQ_VIEWS
        src/blob/groups/audiomgr/audiomgr_thread.c src/blob/groups/audiomgr/ProcessSequences.c
        src/blob/groups/save/AudioInit.c src/blob/AudInit3D.c src/blob/AudPlaySequence.c
        src/blob/NewSequence.c src/blob/stop_sequence.c)
    set(SOUND_EMITTER_VIEWS
        src/blob/groups/audio_emitters/group.c src/blob/groups/audiomgr/ProcessSequences.c
        src/blob/AudInit3D.c src/blob/groups/audiomgr_emitters/group.c src/blob/groups/aud_stop_effect/group.c
        src/blob/groups/audiomgr/ProcessEmitters.c)
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
    # The commands (N64 audiomgr_gCmds) are PC data now (pc_snd_msgs, docs/port/DECOUPLE.md 9), which each reader casts
    # to its own view, so the views are the live readers' record types (the three the symbol's declarations were).
    add_test(NAME views_sound_commands COMMAND ${VIEW_CHECK} --n64-size 0x20 --tag ShutdownMsg,Slot24,Slot18
        --in src/blob/groups/heap_release/AudioShutdown.c,src/blob/groups/save/AudioInit.c,src/blob/groups/abort_game/GetCmd.c)
    # The road collision's views of the car's model record (pc_models): the tires' road nodes at MAT3's +0x3A0 and the
    # moving surface at +0x648 are host pointers (docs/port/CARS.md 4).
    add_test(NAME views_model_road COMMAND ${VIEW_CHECK} --n64-size 0x808 --tag VCar,EUCar,tModelData
        --in src/blob/groups/stree_collide/CollideCarBody.c,src/blob/groups/stree_tiresurf/TireSurf.c,src/blob/groups/update_game_data/update_game_data.c)
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
# Test programs define main: keep them out of compile_commands.json, which census.py reads as the game
# and the PC layer.
set_target_properties(pc_data_test pc_reach_test pc_data_test_decls rush2049pc_datacheck test_overlay_data test_overlay_data_game
    PROPERTIES EXPORT_COMPILE_COMMANDS OFF)

# Before port/gfx, whose renderer links pcupscale (and makes it itself when nothing has).
add_subdirectory(port/upscale)   # 2x texture upscaling and its tests (docs/port/UPSCALE.md)
add_subdirectory(port/audio)
add_subdirectory(port/gfx)
add_subdirectory(port/input)   # the frontend's tests (docs/port/FRONTEND.md)
add_subdirectory(port/net)     # online play's network library and its tests (docs/port/NETWORK.md); netplay.c uses it
# A sanitizer build runs the programs several times slower: the tests that start the game many times get more time,
# and so do the synthetic paths (SYNTH_PATH_TIMEOUT; docs/port/SANITIZE.md 5).
if(PC_SANITIZE)
    set_tests_properties(app_firstrun hfr_timing PROPERTIES TIMEOUT 1200)
    if(TEST hfr_timing_game60)
        set_tests_properties(hfr_timing_game60 PROPERTIES TIMEOUT 1200)
    endif()
    set_tests_properties(${SYNTH_BOOT_TESTS} PROPERTIES TIMEOUT 1800)
endif()

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
option(RUSH_INSTALL_PORTABLE "Install as one folder: the program, README.txt, the online launchers and LICENSES/"
       ${RUSH_INSTALL_PORTABLE_DEFAULT})
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
    # The mods folder (docs/port/MODS.md), in the data folder, which is the program's own when portable: empty but
    # for a README saying what goes in it.
    install(FILES dist/mods-README.txt DESTINATION ${RUSH_INSTALL_BIN}/mods RENAME README.txt)
    # The online race's launchers (README.txt, "Playing online"): host, and join (it asks for the address); the
    # options given to them are passed on.
    if(WIN32)
        install(FILES "dist/Host online race.bat" "dist/Join online race.bat" DESTINATION ${RUSH_INSTALL_BIN})
    else()
        install(PROGRAMS dist/host-online.sh dist/join-online.sh DESTINATION ${RUSH_INSTALL_BIN})
    endif()
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
