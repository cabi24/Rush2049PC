#include "game/game.h"

s16 enginetorque(void *arg0, s16 arg1, s16 arg2, uintptr_t arg3);   /* arg3: s16 *curve, zero-extended */
/* ... the curve's 32-bit N64 address (the model's +0x40C) as the pointer at it (pc_prelude.h,
 * docs/port/NOFIXED.md 2.3) */
#define CURVE(a) ((uintptr_t)PC_N64P(a))

typedef struct CarE {
  u8 pad0[0xA]; s8 fA;
  u8 padB[0x3D0 - 0xB]; f32 f3D0;
  u8 pad3D4[0x3F4 - 0x3D4]; s16 f3F4;
  u8 pad3F6[0x404 - 0x3F6]; f32 f404; f32 f408; u32 f40C;    /* the torque curve's window address (model + 0x40C) */
  u8 pad410[0x614 - 0x410]; s32 f614; s32 f618;
  u8 pad61C[0x640 - 0x61C]; s8 f640;
} CarE;
void engine(CarE *arg0)
{
  if (arg0->f640 != 0) {
    arg0->f404 = enginetorque(arg0, (s16) (s32) (arg0->f408 * engine_rpm_scale_crashed), 0, CURVE(arg0->f40C));
  } else if (arg0->f3F4 < 0) {
    arg0->f404 = enginetorque(arg0, (s16) (s32) (arg0->f408 * engine_rpm_scale_reverse), (s16) (s32) (arg0->f3D0 * 128.0f), CURVE(arg0->f40C));
  } else {
    arg0->f404 = enginetorque(arg0, (s16) (s32) (arg0->f408 * engine_rpm_scale), (s16) (s32) (arg0->f3D0 * 128.0f), CURVE(arg0->f40C));
  }
  if ((arg0->f614 == 1 || arg0->f614 == 2) && (arg0->f618 == 1 || arg0->f618 == 2)) {
    arg0->f404 = arg0->f404 * 1.5f;
  }
  if (arg0->fA != 0 && arg0->f3F4 != 4) {
    arg0->f404 = arg0->f404 * autotrans_torque_scale;
  }
}
