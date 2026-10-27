# Cross-compiling the PC build for Windows x86-64 with mingw-w64 (docs/port/WINDOWS.md):
#
#   cmake -S . -B build/win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake
#   cmake --build build/win
#
# The POSIX-threads flavour of the compiler (x86_64-w64-mingw32-gcc-posix, winpthreads) is preferred. Libraries
# (zlib, SDL2) are looked up under the mingw-w64 sysroot and under RUSH_MINGW_ROOTS, a list of extra prefixes
# (for example a zlib built for Windows in some directory: -DRUSH_MINGW_ROOTS=/path/to/prefix).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(RUSH_MINGW_TRIPLE x86_64-w64-mingw32 CACHE STRING "mingw-w64 target triple (tool prefix)")
set(RUSH_MINGW_ROOTS "" CACHE STRING "Extra prefixes with Windows libraries (zlib, SDL2)")
find_program(CMAKE_C_COMPILER NAMES ${RUSH_MINGW_TRIPLE}-gcc-posix ${RUSH_MINGW_TRIPLE}-gcc REQUIRED)
find_program(CMAKE_CXX_COMPILER NAMES ${RUSH_MINGW_TRIPLE}-g++-posix ${RUSH_MINGW_TRIPLE}-g++)   # SDL2's project
find_program(CMAKE_RC_COMPILER NAMES ${RUSH_MINGW_TRIPLE}-windres)
# The build's generators read objects with these (tools/port): nm for symbols, objdump for DWARF (readelf only
# reads ELF).
find_program(CMAKE_NM NAMES ${RUSH_MINGW_TRIPLE}-nm REQUIRED)
find_program(CMAKE_OBJDUMP NAMES ${RUSH_MINGW_TRIPLE}-objdump REQUIRED)
find_program(CMAKE_AR NAMES ${RUSH_MINGW_TRIPLE}-ar REQUIRED)
find_program(CMAKE_RANLIB NAMES ${RUSH_MINGW_TRIPLE}-ranlib REQUIRED)
find_program(RUSH_ADDR2LINE NAMES ${RUSH_MINGW_TRIPLE}-addr2line)

set(CMAKE_FIND_ROOT_PATH /usr/${RUSH_MINGW_TRIPLE} ${RUSH_MINGW_ROOTS})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
