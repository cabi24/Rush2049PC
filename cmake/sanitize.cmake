# Sanitizer builds (docs/port/SANITIZE.md): -DRUSH_SANITIZE=address,undefined (or undefined, or thread).
#
#   port/ code and the test programs: the sanitizers asked for, UBSan with every check (plus float-cast-overflow);
#   the game C (game_flags): AddressSanitizer as asked, its globals with redzones as every datum's (data.c's objects,
#     the game C's own data: docs/port/NATIVE.md 18), ThreadSanitizer only with RUSH_SANITIZE_GAME_THREAD, and of
#     UBSan only RUSH_SANITIZE_GAME_UB,
#     a curated set: the m2c code's views shorter than their data, wrapping arithmetic (-fwrapv) and unaligned
#     N64 records are its design, not bugs;
#   targets with the PC_NO_SANITIZE property, none: the -O0 types copy, whose nm and DWARF the generators read
#     (gen_data's tables stay the same files).
#
# AddressSanitizer runs on x86-64 and aarch64: nothing of the game is at a fixed address (docs/port/NATIVE.md 17), so
# its shadow memory, which on x86-64 starts at 0x7FFF8000, has the low addresses to itself. ThreadSanitizer on aarch64
# needs a 39-, 42- or 48-bit address space.
# The ctest environment (sanitize_env.cmake, written here) points the runtimes at tools/port/sanitize/, stops at the
# first finding, and leaves SIGSEGV and friends to port/ultra's fault handler, whose report and exit status the tests
# check (a run outside ctest keeps the runtimes' handlers, which the fault handler chains to: misc.c).
set(RUSH_SANITIZE "" CACHE STRING "Sanitizers, comma-separated: address, undefined, thread (docs/port/SANITIZE.md)")
set(RUSH_SANITIZE_GAME_UB
    "bounds,shift,integer-divide-by-zero,float-cast-overflow,pointer-overflow,null,vla-bound,unreachable"
    CACHE STRING "The UBSan checks for the game C in a build with undefined (docs/port/SANITIZE.md)")
# The game C under ThreadSanitizer faults at the synthetic cartridge's first screen (a texture lookup comes back empty
# in InitBlit), which no other build does; not understood yet (docs/port/SANITIZE.md 6). The game's
# threads run one at a time, so the races TSan finds are the port's, whose code it instruments either way.
option(RUSH_SANITIZE_GAME_THREAD "With thread: instrument the game C too (docs/port/SANITIZE.md)" OFF)
string(REPLACE "," ";" PC_SANITIZE "${RUSH_SANITIZE}")
set(PC_SANITIZE_ADDRESS OFF)
set(PC_SANITIZE_THREAD OFF)
set(PC_SANITIZE_UB OFF)
foreach(s ${PC_SANITIZE})
    if(s STREQUAL "address")
        set(PC_SANITIZE_ADDRESS ON)
    elseif(s STREQUAL "thread")
        set(PC_SANITIZE_THREAD ON)
    elseif(s STREQUAL "undefined")
        set(PC_SANITIZE_UB ON)
    else()
        message(FATAL_ERROR "RUSH_SANITIZE: '${s}' is not address, undefined or thread")
    endif()
endforeach()
if(PC_SANITIZE)
    if(WIN32)
        message(FATAL_ERROR "RUSH_SANITIZE: Linux only (MinGW's GCC has no sanitizer runtimes)")
    endif()
    if(PC_SANITIZE_ADDRESS AND PC_SANITIZE_THREAD)
        message(FATAL_ERROR "RUSH_SANITIZE: address and thread can't be combined")
    endif()
    set(pc_port "")
    set(pc_game "")
    if(PC_SANITIZE_ADDRESS)
        list(APPEND pc_port address)
        list(APPEND pc_game address)
    endif()
    if(PC_SANITIZE_THREAD)
        list(APPEND pc_port thread)
        if(RUSH_SANITIZE_GAME_THREAD)
            list(APPEND pc_game thread)
        endif()
    endif()
    if(PC_SANITIZE_UB)
        list(APPEND pc_port undefined float-cast-overflow)
        if(RUSH_SANITIZE_GAME_UB)
            list(APPEND pc_game ${RUSH_SANITIZE_GAME_UB})
        endif()
    endif()
    string(JOIN "," pc_port_list ${pc_port})
    string(JOIN "," pc_game_list ${pc_game})
    set(pc_on "$<NOT:$<BOOL:$<TARGET_PROPERTY:PC_NO_SANITIZE>>>")
    set(pc_off "$<BOOL:$<TARGET_PROPERTY:PC_NO_SANITIZE>>")
    # Frame pointers for the runtimes' fast unwinder; -fno-sanitize-recover is left to UBSAN_OPTIONS (halt_on_error)
    # so that a hunt can print every finding of a run.
    add_compile_options("$<${pc_on}:-fsanitize=${pc_port_list}>" "$<${pc_on}:-fno-omit-frame-pointer>"
                        "$<${pc_off}:-fno-sanitize=all>")
    # SHELL: keeps CMake from dropping these as repeats of the port's options: with thread, or a game list equal to
    # the port's, the game C was left with -fno-sanitize=all.
    target_compile_options(game_flags INTERFACE "$<${pc_on}:SHELL:-fno-sanitize=all>")
    if(pc_game_list)
        target_compile_options(game_flags INTERFACE "$<${pc_on}:SHELL:-fsanitize=${pc_game_list}>")
    endif()
    string(REPLACE ";" "," pc_link "${PC_SANITIZE}")
    add_link_options(-fsanitize=${pc_link})
    # The runtimes' options for ctest's programs (and the ones the test scripts start), unless already set. Each
    # runtime leaves the fatal signals to the program, whose own handler writes the backtrace to its log
    # (app_firstrun's fault check); Clang's UBSan runtime takes them by default, GCC's doesn't.
    set(pc_supp ${CMAKE_SOURCE_DIR}/tools/port/sanitize)
    file(WRITE ${CMAKE_BINARY_DIR}/sanitize_env.cmake
        "# Written by cmake/sanitize.cmake: the sanitizer runtimes' options for ctest (docs/port/SANITIZE.md).\n"
        "if(NOT DEFINED ENV{ASAN_OPTIONS})\n"
        "  set(ENV{ASAN_OPTIONS} \"abort_on_error=0:detect_leaks=1:handle_segv=0:handle_sigbus=0:handle_sigfpe=0:handle_sigill=0:suppressions=${pc_supp}/asan.supp\")\n"
        "endif()\n"
        "if(NOT DEFINED ENV{LSAN_OPTIONS})\n"
        "  set(ENV{LSAN_OPTIONS} \"suppressions=${pc_supp}/lsan.supp:print_suppressions=0\")\n"
        "endif()\n"
        "if(NOT DEFINED ENV{UBSAN_OPTIONS})\n"
        "  set(ENV{UBSAN_OPTIONS} \"print_stacktrace=1:halt_on_error=1:handle_segv=0:handle_sigbus=0:handle_sigfpe=0:handle_sigill=0:suppressions=${pc_supp}/ubsan.supp\")\n"
        "endif()\n"
        "if(NOT DEFINED ENV{TSAN_OPTIONS})\n"
        "  set(ENV{TSAN_OPTIONS} \"halt_on_error=1:second_deadlock_stack=1:report_signal_unsafe=0:handle_segv=0:handle_sigbus=0:handle_sigfpe=0:handle_sigill=0:suppressions=${pc_supp}/tsan.supp\")\n"
        "endif()\n")
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES ${CMAKE_BINARY_DIR}/sanitize_env.cmake)
    message(STATUS "Sanitizers: port/ ${pc_port_list}; game C ${pc_game_list}")
endif()
