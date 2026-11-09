/* SavePlayerCarSetting: save one of a profile's car settings (MAT3 save.c). */
#include "game/game.h"

extern void PS_WriteFile(void *, void *, u32);

/* Sets setting field of car index in the profile's file to value; when it changed, the car's settings get their
 * checksum again and are marked to write. The file must be read. */
void SavePlayerCarSetting(tProfile **profile,u8 index,u8 field,s8 value) {
 tSaveData *base=*(*profile)->datah;
 if(base->carSettings[index].settings[field] != value) {
  base->carSettings[index].settings[field]=value;
  base->carSettings[index].checksum=CalcChecksumInternal((u8 *)base->carSettings[index].settings,12);
  PS_WriteFile((*profile)->fileh,&base->carSettings[index],16);
 }
}
