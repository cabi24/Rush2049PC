# port/gfx: F3DEX2 display-list interpreter and OpenGL renderer (docs/port/GFX.md).
#
# Part of the top-level PC build, or on its own for quick iteration:
#   cmake -S port/gfx -B build/gfx && cmake --build build/gfx -j3 && ctest --test-dir build/gfx
# Options: RUSH_GFX_SDL (window backend when SDL2 is found, ON), RUSH_FETCH_SDL2 (download and build SDL2, OFF).
cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(Rush2049PC_gfx C)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    endif()
endif()
enable_testing()

find_package(PkgConfig REQUIRED)
pkg_check_modules(PCGFX_EGL REQUIRED IMPORTED_TARGET egl)
find_package(ZLIB REQUIRED)
# GL entry points are loaded at run time (eglGetProcAddress / SDL_GL_GetProcAddress): only the headers are used.
find_path(PCGFX_GLES3_INCLUDE GLES3/gl3.h REQUIRED)

option(RUSH_GFX_SDL "Build the SDL2 window backend when SDL2 is found" ON)
option(RUSH_FETCH_SDL2 "Download and build SDL2 (pinned 2.30.x) for the window backend" OFF)

add_library(pcgfx STATIC
    gfx_api.c
    gfx_rsp.c
    gfx_rdp.c
    gfx_combine.c
    gfx_gl.c
    gfx_egl.c
    gfx_png.c)
set_target_properties(pcgfx PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(pcgfx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include
                                 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PCGFX_GLES3_INCLUDE})
target_link_libraries(pcgfx PUBLIC PkgConfig::PCGFX_EGL ZLIB::ZLIB m)
# compile_commands.json is tools/port/census.py's list of game files: keep the renderer out of it.
set_target_properties(pcgfx PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pcgfx PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------- SDL2 window backend
set(PCGFX_SDL_TARGET "")
if(RUSH_GFX_SDL)
    if(RUSH_FETCH_SDL2)
        include(FetchContent)
        FetchContent_Declare(SDL2
            URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz
            URL_HASH SHA256=24b574f71c87a763f50704bbb630cbe38298d544a1f890f099a4696b1d6beba4)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(SDL2)
        set(PCGFX_SDL_TARGET SDL2::SDL2-static)
    else()
        find_package(SDL2 CONFIG QUIET)
        if(TARGET SDL2::SDL2)
            set(PCGFX_SDL_TARGET SDL2::SDL2)
        else()
            pkg_check_modules(PCGFX_SDL2 QUIET IMPORTED_TARGET sdl2)
            if(PCGFX_SDL2_FOUND)
                set(PCGFX_SDL_TARGET PkgConfig::PCGFX_SDL2)
            endif()
        endif()
    endif()
endif()
if(PCGFX_SDL_TARGET)
    target_sources(pcgfx PRIVATE gfx_sdl.c)
    target_compile_definitions(pcgfx PRIVATE PC_GFX_HAVE_SDL)
    target_link_libraries(pcgfx PUBLIC ${PCGFX_SDL_TARGET})
    message(STATUS "port/gfx: SDL2 window backend (${PCGFX_SDL_TARGET})")
else()
    message(STATUS "port/gfx: no SDL2, headless only")
endif()

# ---------------------------------------------------------------- tests
add_executable(gfx_test tests/gfx_test.c)
set_target_properties(gfx_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(gfx_test PRIVATE pcgfx)
set_target_properties(gfx_test PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
set(PCGFX_TEST_OUT ${CMAKE_CURRENT_BINARY_DIR}/test-png)
file(MAKE_DIRECTORY ${PCGFX_TEST_OUT})
foreach(c fill shade zbuf textures texwrap texrect lit fog twocycle dualtile belist matrix cull branchz nearclip
          xlu screenshot)
    add_test(NAME gfx.${c} COMMAND gfx_test ${c} --png ${PCGFX_TEST_OUT})
endforeach()
# The same cases on desktop GL and on the software rasterizer (llvmpipe), when those exist.
add_test(NAME gfx.all.desktop-gl COMMAND gfx_test all)
set_tests_properties(gfx.all.desktop-gl PROPERTIES ENVIRONMENT "RUSH_GFX_API=gl")
add_test(NAME gfx.all.scale2 COMMAND gfx_test all)
set_tests_properties(gfx.all.scale2 PROPERTIES ENVIRONMENT "RUSH_GFX_SCALE=2")
add_test(NAME gfx.all.software COMMAND gfx_test all)
set_tests_properties(gfx.all.software PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1")
if(PCGFX_SDL_TARGET)   # the window path, on SDL's offscreen driver (no display needed)
    add_test(NAME gfx.all.sdl-window COMMAND gfx_test all --window)
    set_tests_properties(gfx.all.sdl-window PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen")
endif()
