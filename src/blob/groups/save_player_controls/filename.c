/* SavePlayerControls: save a profile's controls (MAT3 save.c). */
#include "game/game.h"

extern void *memcpy(void *, void *, unsigned);

/* Copies the 11 bytes of controls (the controls and the rumble flag) into the profile and its file when they differ
 * from the file's, gives them their checksum again and marks them to write. Nothing for a profile whose file is not
 * read. */
void SavePlayerControls(tProfile **profile, u8 *controls)
{
tSaveData *data;
 if ((*profile)->datah != 0) {
 data = *(*profile)->datah;
 if (game_memcmp(data->controls, controls, 11) != 0) {
 memcpy((*profile)->controls, controls, 11);
 memcpy(data->controls, controls, 11);
 data->contChecksum = CalcChecksumInternal(data->controls, 11);
 PS_WriteFile((*profile)->fileh, (u8 *) &data->contChecksum, 15);
 }
 }
}
