/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
/* Runtime image B: 0x8038A8CC..0x8038A95C, 144 bytes.
 * Initialize the 25-slot surface-object ring and resolve its texture index.
 * N64-specific body, no whole-function arcade donor or original type claim.
 * Ring widths and size are witnessed by B:8038A408. The texture lookup uses
 * the unchanged accepted 800B24EC MBOX_FindTexture_Sub boundary.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct RingState {
    s8 wrapped;
    u8 unknown01;
    u16 next;
    void *objects[25];
} RingState;
typedef struct NameEntry { char name[36]; } NameEntry;
extern RingState DamagePolys;
extern s16 damTexture;
extern char scorch_texture_name[];
extern volatile u8 gNumModels;
extern NameEntry *MBOX_FindTexture_WORK(char *, s16 *, s8, s8, s32);
void initWorldDamage(void)
{
    s32 i;
    DamagePolys.wrapped = 0;
    DamagePolys.next = 0;
    MBOX_FindTexture_WORK(scorch_texture_name, &damTexture, 0, gNumModels - 1, 1);
    for (i = 0; i < 25; i++) {
        DamagePolys.objects[i] = 0;
    }
}
