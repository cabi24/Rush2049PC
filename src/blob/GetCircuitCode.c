#include "game/game.h"

typedef struct 
{
  u8 pad0[8];
  s32 unk8;
} SavedPlayerSlot;
VIEW(SavedPlayerSlot **, gPlayers_save);
#define gPlayers_save gPlayers_save_view

VIEW(s32, gCircuitCars);
#define gCircuitCars gCircuitCars_view

VIEW(s32, controls_buttonsInited);
#define controls_buttonsInited controls_buttonsInited_view

void GetPasswordString(intptr_t arg0);

/* PC: an input record (gPlayers) is 80 bytes, its pointer at +0x48 is 8 (docs/port/POINTERS.md 10), and
 * SaveSlot.data is at the profile record's PC offset (pc_prelude.h, POINTERS.md 9.10); the save data it points to
 * is read pointer-sized. */
#define INPUT_SIZE 80
#define SLOT_DATA PROFILE_OFS(0x2C)
void GetCircuitCode(intptr_t arg0)
{
  s8 *var_s0;
  u8 *new_var3;
  SavedPlayerSlot **new_var2;
  void *temp_s0;
  int new_var;
  temp_s0 = (void *) ((*(*((intptr_t **) ((**((s8 ***) (((s8 *) (&gPlayers_save)) + (gCircuitPlayer * INPUT_SIZE)))) + SLOT_DATA)))) + 0x6F4);
  ContinueCircuit();
 do { gCircuitTrack -= 1; NewPassword(0x56U, 0xEU); StorePasswordElement((u32) (*((s8 *) (((s8 *) temp_s0) + 8))), 3U); new_var3 = (u8 *) (((s8 *) temp_s0) + 5); StorePasswordElement((u32) (*new_var3), 1U); StorePasswordElement((u32) (*((s8 *) (((s8 *) temp_s0) - -9))), 5U); StorePasswordElement(*((u32 *) (((s8 *) temp_s0) + 0xC)), 0x14U); StorePasswordElement((u32) (*((u8 *) (((s8 *) temp_s0) + 7))), 3U); var_s0 = (s8 *) &gCircuitCars; do { StorePasswordElement((u32) (*((u16 *) (((s8 *) var_s0) + 2))), 9U); var_s0 += 0x4C; } while (var_s0 != ((s8 *) &controls_buttonsInited)); GetPasswordString(arg0); } while (0);
}

