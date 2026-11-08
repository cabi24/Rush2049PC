/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* SetSheenColor: replacement source for the locked single. Struct form against the
 * ObjInstanceList table ObjInstanceList (MAT3 zoid.c / zoidtexture.c / visuals.c); matches standalone and in
 * the whole-program unit, and its inlined copies share one table base inside AnimateRocketFlame/AnimateFrame
 * The two accessors are repeated here (identical to their own singles) so umerge inlines them as retail
 * does; the unit keeps one definition of each.
 * (the m2c singles use &ObjInstanceList_mat/&D_8012E73C bases, which split that web). One statement per line:
 * as1 schedules by source line, a one-line body schedules the param home store differently. */
typedef union Color { unsigned char b[4]; unsigned int w; } Color;
typedef struct Node Node;
typedef struct ObjInstance {
    unsigned int flags;         /* 0x00 */
    char pad4[4];
    Node *mat;                  /* 0x08 */
    char padC[8];
    unsigned short index;       /* 0x14 */
    char pad16[0x26];
    unsigned int color;         /* 0x3C */
    unsigned int envcolor;      /* 0x40 */
} ObjInstance;
typedef struct CarParts { int frame; int extraparts[3]; int hood; int sheen; char tail[0x28]; } CarParts;
extern ObjInstance ObjInstanceList[];
extern CarParts gCarParts[];

/* ZOID_SetColor */
void ZOID_SetColor(short objnum, Color *col);

/* ZOID_SetEnvColor */
void ZOID_SetEnvColor(short objnum, Color *col);

/* SetSheenColor */
void SetSheenColor(short slot, Color *col, Color *env)
{
    CarParts *car;
    car = &gCarParts[slot];
    ZOID_SetColor(car->sheen, col);
    ZOID_SetEnvColor(car->sheen, env);
}
