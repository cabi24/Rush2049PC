/* MakeQuatFromUvs.c: a quaternion from an orientation matrix (MAT3 resurrect.c, MakeQuatFromUvs()).
 *
 * A respawn (Resurrect) turns the car's crash orientation and its respawn orientation into quaternions, and
 * UpdateResurrectingCar interpolates between them during the flight and turns the result back with
 * MakeUvsFromQuat. */
#include "game/game.h"

/* Component i of the quaternion, likewise. */
#define Q_AT(q, i) (*((f32 *) (((s8 *) (q)) + (i) * 4)))

/* The quaternion q = (w, x, y, z), w first and not normalised (MakeUvsFromQuat normalises), of the orientation
 * matrix uvs. Of the four standard formulas it uses the one whose large first term (1 plus or minus the diagonal
 * elements) is largest, judged by the signs of the diagonal, which keeps the result well conditioned. The
 * local copies are the original code's. */
void MakeQuatFromUvs(f32 (*uvs)[3], f32 *q)
{
  f32 u00;
  f32 u11;
  f32 u22;
  float first;
  u00 = UV_AT(uvs, 0, 0);
  if (u00 >= 0.0f)
  {
    u22 = UV_AT(uvs, 2, 2);
    u11 = UV_AT(uvs, 1, 1);
    if ((u22 + u11) >= 0.0f)
    {
      first = (1.0f + u00) + u11;
      Q_AT(q, 0) = u22 + first;
      Q_AT(q, 1) = UV_AT(uvs, 2, 1) - UV_AT(uvs, 1, 2);
      Q_AT(q, 2) = UV_AT(uvs, 0, 2) - UV_AT(uvs, 2, 0);
      Q_AT(q, 3) = UV_AT(uvs, 1, 0) - UV_AT(uvs, 0, 1);
      return;
    }
    Q_AT(q, 0) = UV_AT(uvs, 2, 1) - UV_AT(uvs, 1, 2);
    Q_AT(q, 1) = ((1.0f + UV_AT(uvs, 0, 0)) - UV_AT(uvs, 1, 1)) - UV_AT(uvs, 2, 2);
    Q_AT(q, 2) = UV_AT(uvs, 1, 0) + UV_AT(uvs, 0, 1);
    Q_AT(q, 3) = UV_AT(uvs, 0, 2) + UV_AT(uvs, 2, 0);
    return;
  }
  if (UV_AT(uvs, 2, 2) <= UV_AT(uvs, 1, 1))
  {
    Q_AT(q, 0) = UV_AT(uvs, 0, 2) - UV_AT(uvs, 2, 0);
    Q_AT(q, 1) = UV_AT(uvs, 1, 0) + UV_AT(uvs, 0, 1);
    Q_AT(q, 2) = ((1.0f - UV_AT(uvs, 0, 0)) + UV_AT(uvs, 1, 1)) - UV_AT(uvs, 2, 2);
    Q_AT(q, 3) = UV_AT(uvs, 2, 1) + UV_AT(uvs, 1, 2);
    return;
  }
  Q_AT(q, 0) = UV_AT(uvs, 1, 0) - UV_AT(uvs, 0, 1);
  Q_AT(q, 1) = UV_AT(uvs, 0, 2) + UV_AT(uvs, 2, 0);
  Q_AT(q, 2) = UV_AT(uvs, 2, 1) + UV_AT(uvs, 1, 2);
  Q_AT(q, 3) = ((1.0f - UV_AT(uvs, 0, 0)) - UV_AT(uvs, 1, 1)) + UV_AT(uvs, 2, 2);
}
