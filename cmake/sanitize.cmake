# Sanitizer builds (docs/port/SANITIZE.md): -DRUSH_SANITIZE=address,undefined (or undefined, or thread).
#
#   port/ code and the test programs: the sanitizers asked for, UBSan with every check (plus float-cast-overflow);
#   the game C (game_flags): AddressSanitizer or ThreadSanitizer as asked, and of UBSan only RUSH_SANITIZE_GAME_UB,
#     a curated set: the m2c code's views shorter than their data, wrapping arithmetic (-fwrapv) and unaligned
#     N64 records are its design, not bugs. AddressSanitizer leaves the game's own C data alone (asan-globals=0):
#     a runtime image's C data is one range the loader saves and puts back whole (port/data/overlays.ld), and the
#     game walks from one symbol to the next as the N64's layout allows, so redzones between them don't belong;
#   targets with the PC_NO_SANITIZE property, none: the probe and the -O0 types copy, whose nm and DWARF the
#     generators read (gen_data's tables stay the same files), and data.c, whose own storage must stay in N64 order
#     without redzones between.
#
# AddressSanitizer needs aarch64: on x86-64 its shadow memory starts at 0x7FFF8000 and covers the RDRAM window
# (0x80000000, and 0xA0000000 is in its shadow gap), which the game needs at those addresses. ThreadSanitizer's
# x86-64 layout leaves the window to the program; on aarch64 it needs a 39-, 42- or 48-bit address space.
# The ctest environment (sanitize_env.cmake, written here) points the runtimes at tools/port/sanitize/, stops at the
# first finding, and leaves SIGSEGV and friends to port/ultra's fault handler, whose report and exit status the tests
# check (a run outside ctest keeps the runtimes' handlers, which the fault handler chains to: misc.c).
set(RUSH_SANITIZE "" CACHE STRING "Sanitizers, comma-separated: address, undefined, thread (docs/port/SANITIZE.md)")
set(RUSH_SANITIZE_GAME_UB
    "bounds,shift,integer-divide-by-zero,float-cast-overflow,pointer-overflow,null,vla-bound,unreachable"
    CACHE STRING "The UBSan checks for the game C in a build with undefined (docs/port/SANITIZE.md)")
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
    if(PC_SANITIZE_ADDRESS AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
        message(FATAL_ERROR "RUSH_SANITIZE: AddressSanitizer's x86-64 shadow (0x7FFF8000-0x10007FFF7FFF) covers "
                "the RDRAM window at 0x80000000 and 0xA0000000; build address on aarch64, and undefined or thread "
                "here (docs/port/SANITIZE.md)")
    endif()
    set(pc_port "")
    set(pc_game "")
    if(PC_SANITIZE_ADDRESS)
        list(APPEND pc_port address)
        list(APPEND pc_game address)
    endif()
    if(PC_SANITIZE_THREAD)
        list(APPEND pc_port thread)
        list(APPEND pc_game thread)
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
    target_compile_options(game_flags INTERFACE "$<${pc_on}:-fno-sanitize=all>")
    if(pc_game_list)
        target_compile_options(game_flags INTERFACE "$<${pc_on}:-fsanitize=${pc_game_list}>")
    endif()
    if(PC_SANITIZE_ADDRESS)
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            target_compile_options(game_flags INTERFACE "$<${pc_on}:SHELL:--param asan-globals=0>")
        else()
            target_compile_options(game_flags INTERFACE "$<${pc_on}:SHELL:-mllvm -asan-globals=0>")
        endif()
    endif()
    string(REPLACE ";" "," pc_link "${PC_SANITIZE}")
    add_link_options(-fsanitize=${pc_link})
    # The runtimes' options for ctest's programs (and the ones the test scripts start), unless already set.
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
        "  set(ENV{UBSAN_OPTIONS} \"print_stacktrace=1:halt_on_error=1:suppressions=${pc_supp}/ubsan.supp\")\n"
        "endif()\n"
        "if(NOT DEFINED ENV{TSAN_OPTIONS})\n"
        "  set(ENV{TSAN_OPTIONS} \"halt_on_error=1:second_deadlock_stack=1:handle_segv=0:handle_sigbus=0:handle_sigfpe=0:handle_sigill=0:suppressions=${pc_supp}/tsan.supp\")\n"
        "endif()\n")
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES ${CMAKE_BINARY_DIR}/sanitize_env.cmake)
    message(STATUS "Sanitizers: port/ ${pc_port_list}; game C ${pc_game_list}")
endif()
