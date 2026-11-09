/* LoadCars.c (loadcars): load the race's car models (MAT3 carload.c, LoadCars()). */
#include "game/game.h"

#define CAR_WAVE 88             /* the wave (asset file) of car type 0; type c is CAR_WAVE + c */
#define CAR_PALETTE_BYTES 512   /* what a car needs besides its file: its palette's copy (car_palette_init) */

/* Load the cars of a race, or with `select` the car screen's (InitSelectCarLoad). For each of the race's slots: a
 * drone (or any car of the attract demo) gets a random car type no earlier slot has and that type's colours
 * (car_color_choice_a/b/c); a human keeps the type it chose. The type's file is loaded (LoadWave, its model number in
 * gCarModelNums) and its palette set up (car_palette_init). When the heap is too short for the file and the
 * palette: a drone takes the next type, from its own on, not loaded yet that fits, else the next that fits, else an
 * earlier slot's car; a human the first type that fits, else the first slot's. The slots past the race's share the
 * first slot's car.
 * PC: a heap too short here is logged the first time and counted (port/ultra/heap.c, docs/port/MEMORY.md 5); the
 * choice is the N64's.
 * Original game: the retry `while (numLoadedCars > gNumPlayers && car >= 13)` never holds (a type is below 13), so
 * the shared car is the first random pick (MAT3 has the same test). */
void LoadCars(s32 select)
{
 s32 i;
 s32 j;
 s32 numLoadedCars;
 if (select) {
  InitSelectCarLoad();
  return;
 }
 numLoadedCars=0;
 for(i=0;i<13;i++) {
  if(i<PC_RACE_CARS && ((gGameState&GS_DEMO)||PC_MODEL_DRONE_TYPE(i)==1)) {   /* GS_DEMO, DRONE_TYPE_AI */
again:
   PC_LINK_CAR(i)=Random(13.0f);
   for(j=0;j<i;j++) {
    if(PC_LINK_CAR(i)==PC_LINK_CAR(j))goto again;
   }
   PC_LINK_COLOR0(i)=car_color_choice_a[PC_LINK_CAR(i)];
   PC_LINK_COLOR1(i)=car_color_choice_b[PC_LINK_CAR(i)];
   PC_LINK_COLOR2(i)=car_color_choice_c[PC_LINK_CAR(i)];
   if(heap_total_free(0)<gWaveSizes[PC_LINK_CAR(i)+CAR_WAVE]+CAR_PALETTE_BYTES) { pc_heap_short("LoadCars, a drone's car", gWaveSizes[PC_LINK_CAR(i)+CAR_WAVE]+CAR_PALETTE_BYTES);
    for(j=0;j<13;j++) {
     if(FindWaveInstance(PC_LINK_CAR(i)+CAR_WAVE)<0 &&
        heap_total_free(0)>=gWaveSizes[PC_LINK_CAR(i)+CAR_WAVE]+CAR_PALETTE_BYTES)goto load;
     PC_LINK_CAR(i)=(PC_LINK_CAR(i)+1)%13;
    }
    for(j=0;j<13;j++) {
     if(heap_total_free(0)>=gWaveSizes[PC_LINK_CAR(i)+CAR_WAVE]+CAR_PALETTE_BYTES)goto load;
     PC_LINK_CAR(i)=(PC_LINK_CAR(i)+1)%13;
    }
    do { j=Random(numLoadedCars); }
    while(numLoadedCars>gNumPlayers && PC_LINK_CAR(j)>=13);
    PC_LINK_CAR(i)=PC_LINK_CAR(j);
    gCarModelNums[i]=gCarModelNums[j];
   } else {
load:
    gCarModelNums[i]=LoadWave(PC_LINK_CAR(i)+CAR_WAVE,1,0,0,0);
    numLoadedCars++;
   }
   car_palette_init(i,PC_LINK_CAR(i),0);
  } else if(i<PC_RACE_CARS && PC_MODEL_DRONE_TYPE(i)==2) {   /* DRONE_TYPE_HUMAN */
   if(heap_total_free(0)<gWaveSizes[PC_LINK_CAR(i)+CAR_WAVE]+CAR_PALETTE_BYTES) { pc_heap_short("LoadCars, a player's car", gWaveSizes[PC_LINK_CAR(i)+CAR_WAVE]+CAR_PALETTE_BYTES);
    for(PC_LINK_CAR(i)=0;PC_LINK_CAR(i)<13;PC_LINK_CAR(i)++) {
     if(heap_total_free(0)>=gWaveSizes[PC_LINK_CAR(i)+CAR_WAVE]+CAR_PALETTE_BYTES)goto load2;
    }
    PC_LINK_CAR(i)=PC_LINK_CAR(0);
    gCarModelNums[i]=gCarModelNums[0];
    car_palette_init(i,PC_LINK_CAR(i),0);
   } else {
load2:
    gCarModelNums[i]=LoadWave(PC_LINK_CAR(i)+CAR_WAVE,1,0,0,0);
    car_palette_init(i,PC_LINK_CAR(i),0);
    numLoadedCars++;
   }
  } else {
   PC_LINK_CAR(i)=PC_LINK_CAR(0);
   gCarModelNums[i]=gCarModelNums[0];
  }
 }
}
