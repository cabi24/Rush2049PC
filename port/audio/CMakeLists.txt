# port/audio: the audio task interpreter (MusyX v1) and output (docs/port/AUDIO.md).
#
# Part of the top-level PC build (add_subdirectory), or on its own:
#   cmake -S port/audio -B build/audio && cmake --build build/audio -j3 && ctest --test-dir build/audio
# In the top-level build the tests are under build/pc/port/audio:
#   cmake --build build/pc --target pcaudio_tests && ctest --test-dir build/pc/port/audio
cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(Rush2049PC_audio C)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif()
endif()

set(PC_AUDIO_TASK_FORM NATIVE CACHE STRING "Form of the audio task records pc_audio_run_task reads: NATIVE or N64")
set_property(CACHE PC_AUDIO_TASK_FORM PROPERTY STRINGS NATIVE N64)
option(PC_AUDIO_SDL2 "Build the SDL2 output backend when SDL2 is found" ON)
option(PC_AUDIO_ALSA "Build the ALSA output backend when the ALSA headers are found" ON)

find_package(Threads REQUIRED)

add_library(pcaudio STATIC
    musyx.c
    musyx_lut.c
    audio_out.c
    resample.c
    out_wav.c)
set_target_properties(pcaudio PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON)
target_include_directories(pcaudio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_compile_definitions(pcaudio PRIVATE PC_AUDIO_DEFAULT_FORM=PC_AUDIO_FORM_${PC_AUDIO_TASK_FORM})
target_link_libraries(pcaudio PUBLIC Threads::Threads)
if(NOT WIN32)
    target_link_libraries(pcaudio PUBLIC m)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pcaudio PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

set(PCAUDIO_HAVE_SDL2 OFF)
if(PC_AUDIO_SDL2 AND RUSH_SDL2_SHARED)
    # the top-level build's SDL2 (installed or fetched), shared with port/gfx and port/input
    if(RUSH_SDL2_TARGET)
        target_sources(pcaudio PRIVATE out_sdl.c)
        target_compile_definitions(pcaudio PUBLIC PC_AUDIO_HAVE_SDL2)
        target_link_libraries(pcaudio PUBLIC ${RUSH_SDL2_TARGET})
        set(PCAUDIO_HAVE_SDL2 ON)
        message(STATUS "port/audio: SDL2 backend on (${RUSH_SDL2_TARGET})")
    else()
        message(STATUS "port/audio: SDL2 not found, no SDL2 backend")
    endif()
elseif(PC_AUDIO_SDL2)
    find_package(SDL2 CONFIG QUIET)
    if(SDL2_FOUND)
        target_sources(pcaudio PRIVATE out_sdl.c)
        target_compile_definitions(pcaudio PUBLIC PC_AUDIO_HAVE_SDL2)
        if(TARGET SDL2::SDL2)
            target_link_libraries(pcaudio PUBLIC SDL2::SDL2)
        else()
            target_include_directories(pcaudio PRIVATE ${SDL2_INCLUDE_DIRS})
            target_link_libraries(pcaudio PUBLIC ${SDL2_LIBRARIES})
        endif()
        set(PCAUDIO_HAVE_SDL2 ON)
        message(STATUS "port/audio: SDL2 backend on (${SDL2_VERSION})")
    else()
        message(STATUS "port/audio: SDL2 not found, no SDL2 backend")
    endif()
endif()
if(PC_AUDIO_ALSA AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(ALSA QUIET)
    if(ALSA_FOUND)
        target_sources(pcaudio PRIVATE out_alsa.c)
        target_compile_definitions(pcaudio PUBLIC PC_AUDIO_HAVE_ALSA)
        target_link_libraries(pcaudio PUBLIC ALSA::ALSA)
        message(STATUS "port/audio: ALSA backend on (${ALSA_VERSION_STRING})")
    else()
        message(STATUS "port/audio: ALSA headers not found, no ALSA backend")
    endif()
endif()

# ---------------------------------------------------------------- tests
# Each test links its own copy of the port/ultra functions the library calls (tests/stubs.c). Tests are PIE,
# as the game is: native-form pointers below 4 GB are taken for N64 addresses.
enable_testing()
include(CheckPIESupported)
check_pie_supported()
set(PCAUDIO_TESTS adpcm pcm resample envmix mix delay forms output e2e volume)
if(PCAUDIO_HAVE_SDL2 OR ALSA_FOUND)
    list(APPEND PCAUDIO_TESTS backends)   # SDL2's disk driver, ALSA's null device
endif()
add_custom_target(pcaudio_tests)
foreach(t ${PCAUDIO_TESTS})
    add_executable(test_audio_${t} tests/test_${t}.c tests/stubs.c tests/ref.c)
    set_target_properties(test_audio_${t} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
        EXPORT_COMPILE_COMMANDS OFF)   # test programs define main: kept out of tools/port/census.py's view
    target_link_libraries(test_audio_${t} PRIVATE pcaudio)
    target_include_directories(test_audio_${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME audio_${t} COMMAND test_audio_${t})
    set_tests_properties(audio_${t} PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    add_dependencies(pcaudio_tests test_audio_${t})
endforeach()
