/* port/input/stick.c: a host stick or keys to the N64 stick (docs/port/FRONTEND.md).
 *
 * Dead zone: radial, and the travel past it is rescaled from 0, so the first value past it is small rather than a
 * jump (v5 did the same per axis). Full tilt is clamped, so a square-reporting pad's corner and two arrow keys
 * both give the full diagonal. Gate: an N64 pad's stick moves inside an octagon, reaching about 80 left, right,
 * up and down and about (70, 70) on the diagonals. With diagonal = d and range = r, full tilt in direction t
 * reaches the octagon whose vertices are (r, 0) and (d, d): in the first octant (|x| >= |y|) the edge is the
 * line through those two, at distance r d / (c d + s (r - d)) along (c, s). A round gate (d = 0) reaches r. */
#include <math.h>

#include "input_internal.h"

double pc_axis_deadzone(double v, double deadzone)
{
    double m = fabs(v);
    if (m <= deadzone)
        return 0;
    m = (m - deadzone) / (1 - deadzone);
    if (m > 1)
        m = 1;
    return v < 0 ? -m : m;
}

static int8_t to_s8(double v)
{
    long n = lround(v);
    return (int8_t)(n > 127 ? 127 : n < -128 ? -128 : n);
}

void pc_stick_shape(double x, double y, const PcStickShape *shape, int8_t *out_x, int8_t *out_y)
{
    double m = hypot(x, y), reach;
    if (m <= shape->deadzone || m == 0) {
        *out_x = *out_y = 0;
        return;
    }
    double c = fabs(x) / m, s = fabs(y) / m, t = (m - shape->deadzone) / (1 - shape->deadzone);
    if (t > 1)
        t = 1;
    if (shape->diagonal > 0) {
        double r = shape->range, d = shape->diagonal;
        if (s > c) {   /* the octants are symmetric about the diagonal */
            double k = c;
            c = s;
            s = k;
        }
        reach = r * d / (c * d + s * (r - d));
    } else {
        reach = shape->range;
    }
    *out_x = to_s8(x / m * reach * t);
    *out_y = to_s8(y / m * reach * t);
}
