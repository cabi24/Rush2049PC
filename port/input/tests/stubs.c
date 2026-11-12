/* port/input/tests/stubs.c: the port/ultra (and port/audio) functions port/input and port/gfx call, for
 * frontend_sdl_test and frontend_menu_test. The clock and the rumble pak's motor are the test's to set; the volume
 * and each controller's accessory (pc_si_set_pak) are recorded. Host pointers (the test is PIE) are the only
 * addresses used, so there is no RDRAM. */
#include <stddef.h>
#include <stdint.h>

#include "pc_game_types.h"
#include "pc_port.h"

/* The cars' sound records, game data in the program (port/data), which port/config/voices.c reads. */
struct pc_engine_sound pc_engine_sounds[PC_MAX_LINKS];
signed char pc_car_sounds_on[PC_MAX_LINKS];

uint64_t test_now_ns;
int test_motor[4];
double test_volume = -1;   /* the last pc_audio_set_volume (port/audio's) */
int test_pak[4] = {-1, -1, -1, -1};   /* the last pc_si_set_pak per channel (-1: none yet) */
int test_pak_sets;                    /* how many */

uint64_t pc_host_ns(void) { return test_now_ns; }
int pc_si_motor(int ch) { return ch >= 0 && ch < 4 ? test_motor[ch] : 0; }
int pc_si_set_pak(int ch, int kind)
{
    if (ch >= 0 && ch < 4)
        test_pak[ch] = kind, test_pak_sets++;
    return kind;
}
int pc_si_pak(int ch) { return ch >= 0 && ch < 4 && test_pak[ch] > 0 && test_pak[ch] < 3 ? test_pak[ch] : 0; }
void pc_audio_set_volume(double volume) { test_volume = volume; }

void pc_mem_register_be(const void *start, size_t size) { (void)start, (void)size; }
void pc_mem_unregister_be(const void *start) { (void)start; }
int pc_mem_is_be(const void *p) { (void)p; return 0; }
int pc_mem_is_model_data(const void *p) { (void)p; return 0; }
