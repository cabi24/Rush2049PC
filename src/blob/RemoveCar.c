/* RemoveCar.c: take a car out of the scene at the end of a race (MAT3 visuals.c, RemoveCar() with RemoveVisuals()). */
#include "game/game.h"

/* A car visual (tVisual) on the PC: 0x20 bytes, next and func host pointers (docs/port/CARS.md 3). */
typedef struct tVisual {
  void *next;
  s16 index;
  s16 objnum;                   /* its scene object, -1 none */
  u8 padC[0x18 - 0xC];          /* slot, data, timeStamp */
  M2C_UNK (*func)(struct tVisual *v, M2C_UNK op);   /* its animator */
} tVisual;

/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields RemoveCar reads. */
typedef struct tCarData
{
  u8 pad000[0xF4];
  s32 objnum;                   /* +0x0F4 the frame's scene object, -1 none */
  u8 pad0F8[0x110 - 0xF8];
  tVisual visuals[21];          /* +0x110 */
} tCarData;

/* If car `slot` is in the scene, remove its frame object (and so the parts under it), then stop each of its
 * visuals: an animator is called with op 0 (stop: it hides its own objects), a visual without one has its object
 * hidden. MAT3 also frees the car's shadow strips here; the N64's shadow is a polygon (StartShadow). */
void RemoveCar(s16 slot)
{
  tCarData *car;
  M2C_UNK (*func)(tVisual *, M2C_UNK);
  s16 objnum;
  s16 i;
  tVisual *v;
  int one;
  do {
    car = PC_CAR(slot);
    if (car->objnum >= 0) {
      MBOX_HideObject(gCarParts[slot].frame, 2, 15);
      v = car->visuals;
      i = 0;
      do {
        func = v->func;
        if (func != NULL) {
          func(v, 0);
        } else {
          objnum = v->objnum;
          if (objnum >= 0) {
            MBOX_HideObject(objnum, 1, 15);
            v->objnum = -(one = 1);
          }
        }
        i += 1;
        v += 1;
      } while (i < CAR_VISUALS);
    }
  } while (0);
}
