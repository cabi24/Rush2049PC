#include "game/game.h"

/* PC: the model record keeps its N64 layout, and its car descriptor pointer (+0) is a u32 window address
 * (docs/port/CARS.md). */
#define MODEL_P(b, o) ((void *) PC_N64P(*((u32 *) (((s8 *) b) + o))))
void steer_constants(void *arg0)
{
  void *temp_v0;
  temp_v0 = MODEL_P(arg0, 0);
  *((f32 *) (((s8 *) arg0) + 0x3C4)) = (f32) ((540.0f / (*((f32 *) (((s8 *) temp_v0) + 0x50)))) * steer_deg_to_rad);
  *((f32 *) (((s8 *) arg0) + 0x3C8)) = (f32) (((*((f32 *) (((s8 *) temp_v0) + 0x54))) * (*((f32 *) (((s8 *) temp_v0) + 0x50)))) / ((*((f32 *) (((s8 *) arg0) + 0x5BC))) * ((*((f32 *) (((s8 *) temp_v0) + 0x90))) / (*((f32 *) (((s8 *) arg0) + 0x5C8))))));
}
