/* MakeUvsFromQuat.c: an orientation matrix from a quaternion (MAT3 resurrect.c, MakeUvsFromQuat()).
 *
 * The inverse of MakeQuatFromUvs: UpdateResurrectingCar turns the quaternion it interpolates during a respawn's
 * flight back into the car's matrix. The arcade's code, with its double constants made float on the N64. */
#include "game/game.h"

/* Fill the orientation matrix uvs from the quaternion q = (w, x, y, z), w first (q1 is w here), normalising it on the
 * way (1 / |q|^2; a zero quaternion gives a zero matrix). The qNM are the products of components N and M. */
void MakeUvsFromQuat(f32 q[], f32 uvs[][3]) {
    f32 Nq, q1, q2, q3, q4, q11, q22, q33, q44;
    f32 q12, q13, q14, q23, q24, q34;
    f32 s;

    q1 = q[0];      q2 = q[1];      q3 = q[2];      q4 = q[3];

    q11 = q1*q1;    q22 = q2*q2;    q33 = q3*q3;    q44 = q4*q4;
    q14 = q1*q4;    q13 = q1*q3;    q12 = q1*q2;
    q24 = q2*q4;    q23 = q2*q3;
    q34 = q3*q4;

    Nq = q11 + q22 + q33+ q44;
    s = (Nq > 0.0f) ? (1.0f / Nq) : 0.0f;

    uvs[0][0] = s * (q11 + q22 - q33 - q44);
    uvs[0][1] = s * (2 * (q23 - q14));
    uvs[0][2] = s * (2 * (q24 + q13));
    uvs[1][0] = s * (2 * (q23 + q14));
    uvs[1][1] = s * (q11 - q22 + q33 - q44);
    uvs[1][2] = s * (2 * (q34 - q12));
    uvs[2][0] = s * (2 * (q24 - q13));
    uvs[2][1] = s * (2 * (q34 + q12));
    uvs[2][2] = s * (q11 - q22 - q33 + q44);
}
