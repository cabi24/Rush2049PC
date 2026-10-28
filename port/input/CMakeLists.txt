# port/input and port/config tests (docs/port/FRONTEND.md). Part of the top-level build (the sources build into
# pcfrontend there); run them with ctest --test-dir build/pc -R frontend.
#
#   frontend_test       no SDL: the INI reader, the settings and their layers, the file's location and first run,
#                       the stick math
#   frontend_sdl_test   SDL2 builds: keyboard and hotkey events (SDL_PushEvent), controllers through SDL's virtual
#                       joysticks (hot-plug, every binding, sticks, triggers, rumble), focus and pause, and
#                       scripted presses merged over live input; through port/gfx's real window and event loop on
#                       SDL's offscreen video driver, so no display is needed
#   frontend_menu_test  the settings overlay, headless: moving through it, live changes, Save into a settings
#                       file in place, scripted MENU input
#   frontend.precedence the executable itself: rush2049pc --print-config over a settings file, the environment
#                       and options
set(PC_FRONTEND_TEST_FLAGS -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
set(PC_FRONTEND_TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/test-scratch)
file(MAKE_DIRECTORY ${PC_FRONTEND_TEST_DIR})

add_executable(frontend_test tests/test_frontend.c ${CMAKE_SOURCE_DIR}/port/config/config.c stick.c)
target_include_directories(frontend_test PRIVATE ${CMAKE_SOURCE_DIR}/port/config ${CMAKE_SOURCE_DIR}/port/input
                           ${CMAKE_SOURCE_DIR}/port/include)
target_compile_options(frontend_test PRIVATE ${PC_FRONTEND_TEST_FLAGS})
target_link_libraries(frontend_test PRIVATE m)
foreach(c ini settings layers locate stick)
    add_test(NAME frontend.${c} COMMAND frontend_test ${c} ${PC_FRONTEND_TEST_DIR})
endforeach()

# The real program's layers: the file says scale 2, audio wav and a ROM path; the environment says audio null;
# the command line says scale 3. Expected: the ROM from the file, audio from the environment, scale from the
# command line, and a --set binding.
set(PC_FRONTEND_INI ${PC_FRONTEND_TEST_DIR}/precedence.ini)
file(WRITE ${PC_FRONTEND_INI} "[game]\nrom = /roms/rush2049.z64\n[video]\nscale = 2\n[audio]\nbackend = wav\n")
add_test(NAME frontend.precedence
         COMMAND rush2049pc --config ${PC_FRONTEND_INI} --scale 3 --set keyboard.A=Space --print-config)
set_tests_properties(frontend.precedence PROPERTIES ENVIRONMENT "RUSH2049_AUDIO=null;RUSH_GFX_SCALE=4"
    PASS_REGULAR_EXPRESSION "rom = /roms/rush2049.z64.*\nscale = 3.*\nbackend = null.*\\[keyboard\\].*\nA = Space\n")

# The settings overlay, headless (no SDL needed): port/input with port/gfx's renderer, whose window is then an
# offscreen image; input as the overlay's actions and --press scripts. One directory per case: they run in parallel.
add_executable(frontend_menu_test tests/test_menu.c tests/stubs.c $<TARGET_OBJECTS:pcfrontend>)
target_include_directories(frontend_menu_test PRIVATE ${CMAKE_SOURCE_DIR}/port/config ${CMAKE_SOURCE_DIR}/port/input
                           ${CMAKE_SOURCE_DIR}/port/include ${CMAKE_SOURCE_DIR}/port/gfx/tests)
target_compile_options(frontend_menu_test PRIVATE ${PC_FRONTEND_TEST_FLAGS} -Wno-unused-function)
target_link_libraries(frontend_menu_test PRIVATE pcgfx ${RUSH_SDL2_TARGET} m)
foreach(c nav live save script mouse pages)
    add_test(NAME frontend.menu.${c} COMMAND frontend_menu_test ${c} ${PC_FRONTEND_TEST_DIR}/menu-${c})
endforeach()
set_target_properties(frontend_menu_test PROPERTIES EXPORT_COMPILE_COMMANDS OFF)

if(RUSH_SDL2_TARGET)
    add_executable(frontend_sdl_test tests/test_sdl.c tests/stubs.c $<TARGET_OBJECTS:pcfrontend>)
    target_include_directories(frontend_sdl_test PRIVATE ${CMAKE_SOURCE_DIR}/port/config
                               ${CMAKE_SOURCE_DIR}/port/input ${CMAKE_SOURCE_DIR}/port/include
                               ${CMAKE_SOURCE_DIR}/port/gfx/tests)
    target_compile_options(frontend_sdl_test PRIVATE ${PC_FRONTEND_TEST_FLAGS}
                           -Wno-unused-function)   # test_gbi.h's encoders
    target_link_libraries(frontend_sdl_test PRIVATE pcgfx ${RUSH_SDL2_TARGET} m)
    if(WIN32)   # no EGL for SDL's offscreen driver there: a real window (port/gfx/CMakeLists.txt)
        set(PC_FRONTEND_SDL_DRIVER windows)
    else()
        set(PC_FRONTEND_SDL_DRIVER offscreen)
    endif()
    foreach(c keyboard hotkeys focus controller hotplug generic rumble menu mouse script)
        add_test(NAME frontend.sdl.${c} COMMAND frontend_sdl_test ${c} ${PC_FRONTEND_TEST_DIR})
        set_tests_properties(frontend.sdl.${c} PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=${PC_FRONTEND_SDL_DRIVER}")
    endforeach()
    set_target_properties(frontend_sdl_test PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
endif()
# Test programs define main: keep them out of compile_commands.json, which tools/port/census.py reads.
set_target_properties(frontend_test PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
