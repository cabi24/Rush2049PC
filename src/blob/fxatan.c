/* fxatan.c: the arctangent's rational approximation (MAT3 atan.c, fxatan(), static there); see fatan.c. */
#include "game/game.h"

/* The arctangent of x for |x| up to sqrt(2) - 1 (fsatan reduces to that range): x times
 * P(x^2) / Q(x^2), Hart and Cheney's degree-4 polynomials with the coefficients atan_p4 .. atan_p0 and
 * atan_q4 .. atan_q1. Q's constant term is P's (MAT3's Q0 equals P0), hence atan_p0 * 1.0f. The copies of x^2 and
 * of atan_p0 are the decompiled code's and are kept: removing them changes the unoptimised code. */
f32 fxatan(f32 x)
{
  float argsq_b;
  f32 argsq_c;
  f32 argsq;
  f32 p0;
  argsq = x * x;
  p0 = atan_p0;
  argsq_b = x * x;
  argsq_c = argsq;
  if (0) { return 0.0f; }
  return (((((((((atan_p4 * argsq_b) + atan_p3) * argsq_b) + atan_p2) * argsq_b) + atan_p1) * argsq_c) + p0) / (((((((((argsq + atan_q4) * argsq_b) + atan_q3) * argsq_b) + atan_q2) * argsq_b) + atan_q1) * argsq_b) + (p0 * 1.0f))) * x;
}
