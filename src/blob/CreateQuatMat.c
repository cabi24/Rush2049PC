/* CreateQuatMat.c: a rotation matrix from a quaternion (MAT3 fmath.c, CreateQuatMat()).
 *
 * The scripted camera paths (targetpaths) keep their key orientations as quaternions, interpolate them
 * (InterpQuats) and turn the result into the camera's matrix with this. */
#include "game/game.h"

/* Write the rotation of quaternion q = (x, y, z, w), w last (MakeUvsFromQuat's quaternions have it first), into the
 * 3x3 matrix mat (nine floats, row by row). With `normal` set, q is taken to be a unit quaternion (the scale is 2);
 * else it is normalised on the way (2 / |q|^2, 0 for a zero quaternion). The off-diagonal signs are the game's own,
 * not the textbook matrix's. */
void CreateQuatMat(f32 *mat, f32 q[], int normal)
{
	f32 s;
	f32 xs,	ys,	zs;
	if (!normal) {
		f32 Nq;
		Nq = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];
		s = (Nq > 0.0f) ? (2.0f / Nq) : 0.0f;
	} else
		s = 2.0f;
	xs = q[0]*s;
	ys = q[1]*s;
	zs = q[2]*s;
	{
		f32 xx = q[0]*xs;
		f32 yy = q[1]*ys;
		f32 zz = q[2]*zs;
		mat[0]= 1.0f - (yy + zz);
		mat[4]= 1.0f - (xx + zz);
		mat[8]= 1.0f - (xx + yy);
	}
	{
		f32 xy = q[0]*ys;
		f32 wz = q[3]*zs;
		mat[1]= -(xy + wz);
		mat[3]= -(xy - wz);
	}
	{
		f32 yz = q[1]*zs;
		f32 wx = q[3]*xs;
		mat[5]= -(yz + wx);
		mat[7]= -(yz - wx);
	}
	{
		f32 xz = q[0]*zs;
		f32 wy = q[3]*ys;
		mat[2]= xz - wy;
		mat[6]= xz + wy;
	}
}
