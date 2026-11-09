/* src/rom/sound/vector.c: the vector maths of the MusyX sound library's 3D sound (streams_3d.c): transforming a
 * point, normalizing, the cross product, inverting a listener's transform. MusyX is the N64's sound library: no MAT3
 * counterpart. */
#include "../rom_tu.h"

/* A point, and a transform: a 3x3 matrix (rows) and a translation */
typedef struct SalVector { float x, y, z; } SalVector;
typedef struct SalMatrix { float m[3][3]; float t[3]; } SalMatrix;
typedef struct Vector { float x, y, z; } Vector;
extern float sqrtf(float);

/* out = mat * in + mat's translation. */
void sound_transform_point(const SalMatrix *mat, const SalVector *in, SalVector *out)
{
    out->x = mat->m[0][0] * in->x + mat->m[0][1] * in->y + mat->m[0][2] * in->z + mat->t[0];
    out->y = mat->m[1][0] * in->x + mat->m[1][1] * in->y + mat->m[1][2] * in->z + mat->t[1];
    out->z = mat->m[2][0] * in->x + mat->m[2][1] * in->y + mat->m[2][2] * in->z + mat->t[2];
}

/* Scales vec to length 1 and returns the length it had (no check for 0). */
float sound_normalize(Vector *vec)
{
    float length;
    length = sqrtf(vec->x * vec->x + vec->y * vec->y + vec->z * vec->z);
    vec->x /= length;
    vec->y /= length;
    vec->z /= length;
    return length;
}

/* out = a x b. */
void sound_cross(Vector *out, const Vector *a, const Vector *b)
{
    out->x = (a->y * b->z) - (a->z * b->y);
    out->y = (a->z * b->x) - (a->x * b->z);
    out->z = (a->x * b->y) - (a->y * b->x);
}

/* out = the inverse of transform in: the 3x3 part inverted by its cofactors over its determinant, and the
 * translation that undoes in's (-inverse * t). */
void sound_invert_transform(SalMatrix *out, const SalMatrix *in) {
  float cofactor00;
  float cofactor10;
  float cofactor20;
  float inverse_det;

  cofactor00 = in->m[1][1] * in->m[2][2] - in->m[2][1] * in->m[1][2];
  cofactor10 = -(in->m[1][0] * in->m[2][2] - in->m[2][0] * in->m[1][2]);
  cofactor20 = in->m[1][0] * in->m[2][1] - in->m[2][0] * in->m[1][1];
  inverse_det = 1.f / (in->m[0][0] * cofactor00 + in->m[0][1] * cofactor10 + in->m[0][2] * cofactor20);
  out->m[0][0] = inverse_det * cofactor00;
  out->m[1][0] = inverse_det * cofactor10;
  out->m[2][0] = inverse_det * cofactor20;
  out->m[0][1] = -inverse_det * (in->m[0][1] * in->m[2][2] - in->m[2][1] * in->m[0][2]);
  out->m[1][1] = inverse_det * (in->m[0][0] * in->m[2][2] - in->m[2][0] * in->m[0][2]);
  out->m[2][1] = -inverse_det * (in->m[0][0] * in->m[2][1] - in->m[2][0] * in->m[0][1]);
  out->m[0][2] = inverse_det * (in->m[0][1] * in->m[1][2] - in->m[1][1] * in->m[0][2]);
  out->m[1][2] = -inverse_det * (in->m[0][0] * in->m[1][2] - in->m[1][0] * in->m[0][2]);
  out->m[2][2] = inverse_det * (in->m[0][0] * in->m[1][1] - in->m[1][0] * in->m[0][1]);
  out->t[0] = (-in->t[0] * out->m[0][0] - in->t[1] * out->m[0][1]) - in->t[2] * out->m[0][2];
  out->t[1] = (-in->t[0] * out->m[1][0] - in->t[1] * out->m[1][1]) - in->t[2] * out->m[1][2];
  out->t[2] = (-in->t[0] * out->m[2][0] - in->t[1] * out->m[2][1]) - in->t[2] * out->m[2][2];
}
