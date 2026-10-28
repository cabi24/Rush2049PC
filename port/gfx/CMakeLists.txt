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
    # PIE, as in the top-level build: test data below 4 GB would be taken for N64 addresses.
    include(CheckPIESupported)
    check_pie_supported()
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
enable_testing()

find_package(ZLIB REQUIRED)
# GL entry points are loaded at run time (eglGetProcAddress / wglGetProcAddress / SDL_GL_GetProcAddress): only
# the headers are used. Windows has no EGL: the headless context is WGL's on a hidden window (gfx_wgl.c), and the
# GL declarations come from mingw-w64's GL/glcorearb.h, which has the same names as GLES3/gl3.h.
if(WIN32)
    find_package(PkgConfig QUIET)
    find_path(PCGFX_GLES3_INCLUDE GL/glcorearb.h REQUIRED)
    set(PCGFX_HEADLESS gfx_wgl.c)
    set(PCGFX_HEADLESS_LIBS opengl32 gdi32 user32)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PCGFX_EGL REQUIRED IMPORTED_TARGET egl)
    find_path(PCGFX_GLES3_INCLUDE GLES3/gl3.h REQUIRED)
    set(PCGFX_HEADLESS gfx_egl.c)
    set(PCGFX_HEADLESS_LIBS PkgConfig::PCGFX_EGL)
endif()

option(RUSH_GFX_SDL "Build the SDL2 window backend when SDL2 is found" ON)
# In the top-level build, SDL2 is looked up (or fetched) once there, for every part (RUSH_SDL2_TARGET). Built on
# its own, this directory finds or fetches its own.
option(RUSH_FETCH_SDL2 "Download and build SDL2 (pinned 2.30.x) for the window backend" OFF)

add_library(pcgfx STATIC
    gfx_api.c
    gfx_rsp.c
    gfx_rdp.c
    gfx_combine.c
    gfx_gl.c
    gfx_interp.c
    gfx_ui.c
    gfx_font.c
    gfx_tex.c
    ${PCGFX_HEADLESS}
    gfx_png.c)
set_target_properties(pcgfx PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(pcgfx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include
                                 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PCGFX_GLES3_INCLUDE})
target_link_libraries(pcgfx PUBLIC ${PCGFX_HEADLESS_LIBS} ZLIB::ZLIB m)

# Texture replacement (gfx_tex.c, docs/port/TEXTURES.md): a worker thread, and the 2x upscaler in port/upscale. That
# directory's own CMakeLists.txt makes the target pcupscale when it has one; else its upscale.c is the library.
find_package(Threads REQUIRED)
if(NOT TARGET pcupscale)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../upscale/CMakeLists.txt)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../upscale ${CMAKE_CURRENT_BINARY_DIR}/upscale)
    else()
        add_library(pcupscale STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../upscale/upscale.c)
        target_include_directories(pcupscale PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../upscale)
        set_target_properties(pcupscale PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    endif()
endif()
target_link_libraries(pcgfx PUBLIC pcupscale Threads::Threads)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pcgfx PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------- SDL2 window backend
set(PCGFX_SDL_TARGET "")
if(RUSH_GFX_SDL)
    if(RUSH_SDL2_SHARED)
        set(PCGFX_SDL_TARGET "${RUSH_SDL2_TARGET}")
    elseif(RUSH_FETCH_SDL2)
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
        elseif(PkgConfig_FOUND)
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
add_executable(gfx_test tests/gfx_test.c tests/clip_check.c)
set_target_properties(gfx_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(gfx_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})   # clip_check.c: gfx_internal.h
target_link_libraries(gfx_test PRIVATE pcgfx)
# A test program defines main: keep it out of compile_commands.json (census.py reads the game and the PC layer).
set_target_properties(gfx_test PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
set(PCGFX_TEST_OUT ${CMAKE_CURRENT_BINARY_DIR}/test-png)
file(MAKE_DIRECTORY ${PCGFX_TEST_OUT})
foreach(c fill shade zbuf textures texwrap texrect lit fog twocycle lod dualtile belist matrix cull branchz nearclip
          eyeplane clip xlu widescreen autosize screenshot)
    add_test(NAME gfx.${c} COMMAND gfx_test ${c} --png ${PCGFX_TEST_OUT})
endforeach()
# The same cases on desktop GL, at 2x, on the software rasterizer (llvmpipe) and in widescreen (the 4:3 frame's
# pixels must come out the same, in the middle of the wider target), when those exist. Each run has its own
# directory for the screenshot case, since they run in parallel.
function(pcgfx_all_variant name env)
    file(MAKE_DIRECTORY ${PCGFX_TEST_OUT}/${name})
    add_test(NAME gfx.all.${name} COMMAND gfx_test all ${ARGN})
    set_tests_properties(gfx.all.${name} PROPERTIES ENVIRONMENT "${env};TMPDIR=${PCGFX_TEST_OUT}/${name}")
endfunction()
# Both APIs explicitly, each also on llvmpipe and at 2x in 16:9: the default (video.api auto) is desktop GL first
# on Windows, GLES first elsewhere (pcgfx_want_gles).
pcgfx_all_variant(gles "RUSH_GFX_API=gles")
pcgfx_all_variant(desktop-gl "RUSH_GFX_API=gl")
pcgfx_all_variant(desktop-gl-software "RUSH_GFX_API=gl;LIBGL_ALWAYS_SOFTWARE=1")
pcgfx_all_variant(desktop-gl-wide-scale2 "RUSH_GFX_API=gl;RUSH_GFX_WIDESCREEN=16:9;RUSH_GFX_SCALE=2")
pcgfx_all_variant(scale2 "RUSH_GFX_SCALE=2")
pcgfx_all_variant(software "RUSH_GFX_API=gles;LIBGL_ALWAYS_SOFTWARE=1")
pcgfx_all_variant(wide "RUSH_GFX_WIDESCREEN=16:9")
pcgfx_all_variant(wide21-scale3 "RUSH_GFX_WIDESCREEN=21:9;RUSH_GFX_SCALE=3")
pcgfx_all_variant(scale-auto "RUSH_GFX_SCALE=auto")   # 2 in the 640 x 480 headless size
# Frame interpolation (gfx_interp.c, a high frame rate): pairs of frames built as the game builds them, and the
# frames shown between against frames drawn directly at those moments. Headless; again at 2x, in widescreen, on
# desktop GL and on llvmpipe.
add_executable(gfx_interp_test tests/interp_test.c)
set_target_properties(gfx_interp_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
    EXPORT_COMPILE_COMMANDS OFF)
target_include_directories(gfx_interp_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gfx_interp_test PRIVATE pcgfx)
foreach(c exact move camera cut teleport split hud appear instances children spin eyerel timing parse)
    add_test(NAME gfx.interp.${c} COMMAND gfx_interp_test ${c})
endforeach()
foreach(v "scale2;RUSH_GFX_SCALE=2" "wide;RUSH_GFX_WIDESCREEN=16:9" "gles;RUSH_GFX_API=gles"
          "desktop-gl;RUSH_GFX_API=gl" "software;RUSH_GFX_API=gles,LIBGL_ALWAYS_SOFTWARE=1"
          "desktop-gl-software;RUSH_GFX_API=gl,LIBGL_ALWAYS_SOFTWARE=1")
    list(GET v 0 name)
    list(GET v 1 env)
    string(REPLACE "," ";" env "${env}")
    add_test(NAME gfx.interp.all.${name} COMMAND gfx_interp_test all)
    set_tests_properties(gfx.interp.all.${name} PROPERTIES ENVIRONMENT "${env}")
endforeach()
# The overlay's drawing (gfx_ui.c) and font (gfx_font.c), headless: the window's offscreen image is read back.
add_executable(gfx_ui_test tests/ui_test.c)
set_target_properties(gfx_ui_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
    EXPORT_COMPILE_COMMANDS OFF)
target_link_libraries(gfx_ui_test PRIVATE pcgfx)
foreach(c font panel closed scaled)
    add_test(NAME gfx.ui.${c} COMMAND gfx_ui_test ${c} ${PCGFX_TEST_OUT})
endforeach()
foreach(v "gles;RUSH_GFX_API=gles" "desktop-gl;RUSH_GFX_API=gl" "software;RUSH_GFX_API=gles,LIBGL_ALWAYS_SOFTWARE=1"
          "desktop-gl-software;RUSH_GFX_API=gl,LIBGL_ALWAYS_SOFTWARE=1")
    list(GET v 0 name)
    list(GET v 1 env)
    string(REPLACE "," ";" env "${env}")
    file(MAKE_DIRECTORY ${PCGFX_TEST_OUT}/ui-${name})
    add_test(NAME gfx.ui.all.${name} COMMAND gfx_ui_test all ${PCGFX_TEST_OUT}/ui-${name})
    set_tests_properties(gfx.ui.all.${name} PROPERTIES ENVIRONMENT "${env}")
endforeach()
# Texture replacement (gfx_tex.c, docs/port/TEXTURES.md), headless, on synthetic textures: the PNG reader, texture
# identities, a pack's round trip, wrap and mirror seams at 2x, mip levels, cut-outs, the worker and the disk cache.
# Each case has its own folders under test-png/tex/<case>.
add_executable(gfx_tex_test tests/tex_test.c)
set_target_properties(gfx_tex_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
    EXPORT_COMPILE_COMMANDS OFF)
target_compile_definitions(gfx_tex_test PRIVATE _POSIX_C_SOURCE=200809L)
target_include_directories(gfx_tex_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gfx_tex_test PRIVATE pcgfx)
foreach(c png identity roundtrip seams lod alpha async)
    add_test(NAME gfx.tex.${c} COMMAND gfx_tex_test ${c} ${PCGFX_TEST_OUT}/tex)
endforeach()
# The pack helper (tools/texpack), on a synthetic dump with its built-in nearest upscaler.
find_program(PCGFX_PYTHON NAMES python3 python)
if(PCGFX_PYTHON AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME gfx.tex.texpack COMMAND ${PCGFX_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/texpack/texpack.py
             --self-test)
endif()
if(PCGFX_SDL_TARGET)   # the window path, on SDL's offscreen driver (no display needed)
    # Windows: SDL's offscreen driver makes its GL contexts through EGL, which Windows lacks, so a real window
    # through WGL, where the back buffer doesn't survive the swap for the window comparison (docs/port/WINDOWS.md)
    if(WIN32)
        pcgfx_all_variant(sdl-window "SDL_VIDEODRIVER=windows" --window --no-window-readback)
    else()
        pcgfx_all_variant(sdl-window "SDL_VIDEODRIVER=offscreen" --window)
    endif()
endif()
