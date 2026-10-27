/* port/input/tests/stubs.c: the port/ultra (and port/audio) functions port/input and port/gfx call, for
 * frontend_sdl_test and frontend_menu_test. The clock and the rumble pak's motor are the test's to set, and the
 * volume is recorded. Host pointers (the test is PIE) are the only addresses used, so there is no RDRAM. */
#include <stddef.h>
#include <stdint.h>

#include "pc_port.h"

uint64_t test_now_ns;
int test_motor[4];
double test_volume = -1;   /* the last pc_audio_set_volume (port/audio's) */

uint64_t pc_host_ns(void) { return test_now_ns; }
int pc_si_motor(int ch) { return ch >= 0 && ch < 4 ? test_motor[ch] : 0; }
void pc_audio_set_volume(double volume) { test_volume = volume; }

void *pc_n64_ptr(uint32_t n64_addr) { (void)n64_addr; return NULL; }
uint32_t pc_n64_addr(const void *host) { (void)host; return 0; }
void pc_mem_register_be(const void *start, size_t size) { (void)start, (void)size; }
void pc_mem_unregister_be(const void *start) { (void)start; }
int pc_mem_is_be(const void *p) { (void)p; return 0; }
