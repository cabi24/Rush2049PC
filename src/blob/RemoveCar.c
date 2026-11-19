/* RemoveCar.c: take a car out of the scene at the end of a race (MAT3 visuals.c, RemoveCar() with RemoveVisuals()). */
#include "game/game.h"


/* If car `slot` is in the scene, remove its frame object (and so the parts under it), then stop each of its
 * visuals: an animator is called with op 0 (stop: it hides its own objects), a visual without one has its object
 * hidden. MAT3 also frees the car's shadow strips here; the N64's shadow is a polygon (StartShadow). */
void RemoveCar(s16 slot)
{
  tCarData *car;
  void (*func)(tVisual *, s16);
  s16 objnum;
  s16 i;
  tVisual *v;
  int one;
  do {
    car = &pc_cars[slot];
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
