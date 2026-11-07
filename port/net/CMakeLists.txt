# port/net: online play's network library (docs/port/NETWORK.md) and its tests.
#
# Part of the top-level PC build (add_subdirectory), or on its own (quick, and for the Windows cross build):
#   cmake -S port/net -B build/net -G Ninja && cmake --build build/net && ctest --test-dir build/net
#   cmake -S port/net -B build/net-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake
#
#   net_unit                 BLAKE2s, the wire format, addresses, the simulator, sockets on loopback; on a virtual
#                            clock the handshake with files and loss, join codes, builds, BYE, the timeout, a desync,
#                            spoofed and malformed packets, the input assignment across delay changes
#   net_sim_<model>          two peers through a RUSH_SIM_NET model for 10,000 ticks on a virtual clock: the same
#                            hash at every tick, the delay's trace, stalls, the rate
#   net_fuzz                 the parser and running sessions fed random and mutated packets
#   net_nat_unit, net_nat_fuzz   the router's port mapping's messages: PCP, NAT-PMP, SSDP, URLs, XML, SOAP, HTTP;
#                            hostile input refused; every parser fuzzed
#   net_nat_<case>           the port mapping against a fake router on 127.0.0.1 (tools/port/nat_check.py)
#   net_threads              two threads over UDP on 127.0.0.1 in real time (realtime)
#   net_loopback_<model>     two processes (net_peer) over UDP on 127.0.0.1 in real time (realtime)
#   net_desync, net_disconnect, net_bye, net_sigstop   the same two processes: a desync, a killed peer, BYE, a
#                            stopped process (Linux)
#   net_rejoin, net_rejoin_host, net_outage, net_recover, net_recover_repeat   rejoins (docs/port/NETWORK.md 12): a
#                            killed guest (host) started again rejoins and replays the log; a 20 s outage; a desync
#                            rebuilt from, and one that repeats
#
# rush2049pc links pcnet: port/app/netplay.c attaches the game to it (docs/port/NETPLAY.md "Status").
cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(Rush2049PC_net C)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif()
    enable_testing()
    find_package(Python3 3.9 COMPONENTS Interpreter)
    set(PC_PYTHON ${Python3_EXECUTABLE})
    if(WIN32)
        add_link_options(-static)   # as the top level: no MinGW runtime DLLs (winpthread's, for __thread)
    endif()
endif()
if(NOT WIN32)
    find_package(Threads REQUIRED)
endif()
set(PC_NET_TOOLS ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/port)
set(PC_NET_FLAGS -std=gnu11 -Wall -Wextra -Wno-unused-parameter)

add_library(pcnet STATIC net.c net_proto.c net_sock.c net_sim.c net_blake2s.c net_os.c net_nat.c net_upnp.c)
target_include_directories(pcnet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(pcnet PRIVATE ${PC_NET_FLAGS})
if(WIN32)
    target_link_libraries(pcnet PUBLIC ws2_32)   # Winsock 2 (docs/port/RELEASE.md's DLLs); Windows' own threads
else()
    target_link_libraries(pcnet PUBLIC Threads::Threads)
endif()

add_library(pcnet_testlib STATIC tests/toy.c tests/vnet.c tests/driver.c)
target_include_directories(pcnet_testlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_compile_options(pcnet_testlib PRIVATE ${PC_NET_FLAGS})
target_link_libraries(pcnet_testlib PUBLIC pcnet)
add_executable(net_test tests/net_test.c)
target_compile_options(net_test PRIVATE ${PC_NET_FLAGS})
target_link_libraries(net_test PRIVATE pcnet_testlib)
add_executable(net_peer tests/net_peer.c)
target_compile_options(net_peer PRIVATE ${PC_NET_FLAGS})
target_link_libraries(net_peer PRIVATE pcnet_testlib)
if(WIN32)
    target_link_libraries(net_peer PRIVATE winmm)   # timeBeginPeriod
    target_link_libraries(net_test PRIVATE winmm)
endif()
add_executable(nat_test tests/nat_test.c)   # the router's port mapping (docs/port/NETWORK.md 13)
target_compile_options(nat_test PRIVATE ${PC_NET_FLAGS})
target_link_libraries(nat_test PRIVATE pcnet)
# Test programs define main: out of compile_commands.json, which census.py reads as the game and the PC layer.
set_target_properties(pcnet_testlib net_test net_peer nat_test PROPERTIES EXPORT_COMPILE_COMMANDS OFF)

add_test(NAME net_unit COMMAND net_test unit)
set_tests_properties(net_unit PROPERTIES TIMEOUT 120)
add_test(NAME net_nat_unit COMMAND nat_test unit)
add_test(NAME net_nat_fuzz COMMAND nat_test fuzz 200000)
set_tests_properties(net_nat_unit net_nat_fuzz PROPERTIES TIMEOUT 300)
add_test(NAME net_fuzz COMMAND net_test fuzz 300000)
set_tests_properties(net_fuzz PROPERTIES TIMEOUT 300)
set(PC_NET_MODELS clean lan20 wan80 bad150 shift pause burst)
foreach(m ${PC_NET_MODELS})
    add_test(NAME net_sim_${m} COMMAND net_test sim ${m} 10000)
    set_tests_properties(net_sim_${m} PROPERTIES TIMEOUT 300)
endforeach()
add_test(NAME net_threads COMMAND net_test threads wan80 480)
set_tests_properties(net_threads PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE LABELS realtime)
if(PC_PYTHON)
    set(PC_NET_CHECK ${PC_PYTHON} ${PC_NET_TOOLS}/net_check.py --exe $<TARGET_FILE:net_peer>
        --work ${CMAKE_CURRENT_BINARY_DIR}/net-check)
    foreach(m ${PC_NET_MODELS})
        add_test(NAME net_loopback_${m} COMMAND ${PC_NET_CHECK} loopback --model ${m})
        set_tests_properties(net_loopback_${m} PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE LABELS realtime)
    endforeach()
    foreach(c desync disconnect bye)
        add_test(NAME net_${c} COMMAND ${PC_NET_CHECK} ${c})
        set_tests_properties(net_${c} PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE LABELS realtime)
    endforeach()
    if(NOT WIN32)
        add_test(NAME net_sigstop COMMAND ${PC_NET_CHECK} sigstop)
        set_tests_properties(net_sigstop PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE LABELS realtime)
    endif()
    # The router's port mapping against tools/port/fake_router.py on 127.0.0.1 (never the real router): UPnP, PCP,
    # NAT-PMP, the order, renewals, the removal, a killed process left to the lease, refusals, CGNAT, no router,
    # hostile answers (tools/port/nat_check.py; docs/port/NETWORK.md 13)
    set(PC_NAT_CHECK ${PC_PYTHON} ${PC_NET_TOOLS}/nat_check.py --exe $<TARGET_FILE:nat_test>)
    foreach(c upnp pcp natpmp order kill keep refused cgnat double norouter permanent conflict hostile)
        add_test(NAME net_nat_${c} COMMAND ${PC_NAT_CHECK} ${c})
        set_tests_properties(net_nat_${c} PROPERTIES TIMEOUT 120)
    endforeach()
    foreach(c rejoin rejoin-host outage recover recover-repeat)
        string(REPLACE "-" "_" t ${c})
        add_test(NAME net_${t} COMMAND ${PC_NET_CHECK} ${c})
        set_tests_properties(net_${t} PROPERTIES TIMEOUT 240 RUN_SERIAL TRUE LABELS realtime)
    endforeach()
endif()
