/* libultra's matrix utilities (mtxutil.c): float matrices to the RSP's fixed-point form and back, identities. */
#include "../rom_tu.h"

/* The float matrix mf as the RSP's fixed-point Mtx m (16.16: integer halves first, fractions after). */
void guMtxF2L(float mf[4][4], Mtx *m)
{
	int	i, j;
	int	e1,e2;
	int	*ai,*af;


	ai=(int *) &(*m)[0][0];
	af=(int *) &(*m)[2][0];

	for (i=0; i<4; i++)
	for (j=0; j<2; j++) {
		e1=FTOFIX32(mf[i][j*2]);
		e2=FTOFIX32(mf[i][j*2+1]);
		*(ai++) = ( e1 & 0xffff0000 ) | ((e2 >> 16)&0xffff);
		*(af++) = ((e1 << 16) & 0xffff0000) | (e2 & 0xffff);
	}
}

/* The fixed-point Mtx m as a float matrix mf. */
void guMtxL2F(f32 mf[4][4], Mtx *m) {
    s32 i, j;
    u32 e1, e2;
    u32 *ai, *af;
    s32 q1, q2;

    ai = (u32 *)&(*m)[0][0];
    af = (u32 *)&(*m)[2][0];

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 2; j++) {
            e1 = (*ai & 0xffff0000) | ((*af >> 16) & 0xffff);
            e2 = ((*(ai++) << 16) & 0xffff0000) | (*(af++) & 0xffff);
            q1 = *(s32 *)&e1;
            q2 = *(s32 *)&e2;

            mf[i][j * 2] = (f32)q1 * (1.0f / (f32)0x00010000);
            mf[i][j * 2 + 1] = (f32)q2 * (1.0f / (f32)0x00010000);
        }
    }
}

/* mf = the identity. */
void guMtxIdentF(f32 mtx[4][4]) {
    s32 i, j;
    f32 one = 1.0f;
    f32 zero = 0.0f;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            if (i == j) {
                mtx[i][j] = one;
            } else {
                mtx[i][j] = zero;
            }
        }
    }
}

/* m = the identity, fixed-point. */
void guMtxIdent(u16 *mtx) {
    f32 temp[4][4];

    guMtxIdentF(temp);
    guMtxF2L(temp, mtx);
}

