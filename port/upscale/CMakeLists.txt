# port/upscale: 2x texture upscaling for the "upscaled textures" setting (docs/port/UPSCALE.md).
#
# Part of the top-level PC build (pcupscale is linked into rush2049pc), or on its own:
#   cmake -S port/upscale -B build/upscale && cmake --build build/upscale && ctest --test-dir build/upscale
#
#   upscale.golden    a fixed synthetic texture's bytes against a checksum (the same on every machine)
#   upscale.seams     wrapped edges against a tiled copy; clamped edges against what they mustn't take
#   upscale.alpha     binary alpha stays binary, cut-outs keep their colour, translucency stays in range
#   upscale.quality   beats bilinear (and Catmull-Rom) on a synthetic edge and ramp
#   upscale.args      what it refuses
# upscale_tool runs a batch file through it (tools/upscale/evaluate.py) and times it (upscale_tool bench).
cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(Rush2049PC_upscale C)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    endif()
endif()
enable_testing()
find_package(Threads REQUIRED)

add_library(pcupscale STATIC upscale.c)
target_include_directories(pcupscale PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pcupscale PUBLIC Threads::Threads)
set_target_properties(pcupscale PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # Optimized in every build type (a Debug build would take seconds over a game's textures), and each float
    # operation rounded on its own, so the output is the same bytes everywhere (upscale.golden).
    target_compile_options(pcupscale PRIVATE -O2 -ffp-contract=off -Wall -Wextra)
endif()

add_executable(test_upscale tests/test_upscale.c)
target_link_libraries(test_upscale PRIVATE pcupscale m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_upscale PRIVATE -ffp-contract=off)   # the quality image is the same everywhere
endif()
foreach(c golden seams alpha quality args)
    add_test(NAME upscale.${c} COMMAND test_upscale ${c})
endforeach()
add_executable(upscale_tool tests/upscale_tool.c)
target_link_libraries(upscale_tool PRIVATE pcupscale m)
# Test programs define main: keep them out of compile_commands.json (tools/port/census.py).
set_target_properties(test_upscale upscale_tool PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
