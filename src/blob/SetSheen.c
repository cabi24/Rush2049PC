/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * SetSheen (historical label): set or clear the texture of one car part.
 *   part = 0..12, car = 0..3 index the 64-byte car-part records at gCarParts (13 per car,
 *   the 3328-byte block cleared by InitVisuals); tex = -1 .. 10.
 *   tex == -1: MBOX_HideObject(rec->model, 0, 1 << car) (hide the part for that car's view);
 *   otherwise look the texture name sheenName[tex] up with MBOX_FindTexture_Sub
 *   (MBOX_FindTexture_WORK, warning on), apply it to the part's object (ZOID_SetTexture((s16)model, tex, -1))
 *   and MBOX_ShowObject(rec->model, 0, 1 << car).
 * Arcade ancestor: not identified (N64 car-select part code).
 * Shaping quirks: the record is indexed 1-D as gCarParts[car * 13 + part] (uopt distributes the
 *   *64 and ugen spends one more temp than the 2-D [car][part] form, which shifts the temp ring);
 *   the car mask is a switch, not 1 << car; declaration order p, t, index, mask gives the 56-byte
 *   frame with mask at sp+44 and index at sp+46.  -O2 does not match.  Whole-program unit: EQUAL.
 */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef unsigned short u16;
typedef int s32; typedef unsigned int u32;

typedef struct {
    u8 pad0[20];
    s32 model;      /* 20 */
    u8 pad24[40];
} CarPart;          /* 64 */

extern CarPart gCarParts[];
extern char *sheenName[];
extern volatile u8 gNumModels;

void *MBOX_FindTexture_WORK(char *name, u16 *index, s8 lo, s8 hi, s32 err);
void ZOID_SetTexture(s16 object, void *tex, s32 mode);
void MBOX_HideObject(s32 model, s32 mode, s32 mask);
void MBOX_ShowObject(s32 model, s32 mode, s32 mask);

void SetSheen(s16 part, s16 car, s16 tex)
{
    CarPart *p;
    void *t;
    u16 index;
    s16 mask;

    if (tex < -1 || tex >= 11) {
        return;
    }
    switch (car) {
    case 0:
        mask = 1;
        break;
    case 1:
        mask = 2;
        break;
    case 2:
        mask = 4;
        break;
    case 3:
        mask = 8;
        break;
    }
    if (tex == -1) {
        MBOX_HideObject(gCarParts[car * 13 + part].model, 0, mask);
    } else {
        t = MBOX_FindTexture_WORK(sheenName[tex], &index, 0, gNumModels - 1, 1);
        p = &gCarParts[car * 13 + part];
        ZOID_SetTexture(p->model, t, -1);
        MBOX_ShowObject(p->model, 0, mask);
    }
}
