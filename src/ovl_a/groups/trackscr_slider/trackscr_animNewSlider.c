/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Image A:803AE63C..803AE940, complete projected settings-bar callback.
 * Native-authoritative 64-byte slot views; the accessed record is slot+21.
 * BLIT callback contract is witnessed by NewMultiBlit and DrawBlits.
 * Direct global indexing lets uopt home the derived slot address at sp+44;
 * the removed pointer local used sp+40. Both frames are 64 bytes. No buffer,
 * extra declaration, artificial read or ABI change is needed. Three natural
 * angle literals own the verified range 803B99D4..803B99E0.
 * No exact arcade donor is claimed. */
typedef unsigned char u8;
typedef signed char s8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct Blit {
    char *Name; void *Image; void *Info;
    s16 TexIndex, X, Y; u16 State; s16 Width, Height;
    u8 Alpha, Flip; s8 Hide; u8 Init;
    s16 Top, Bot, Left, Right, Color; u16 reserved26;
    s32 (*AnimFunc)(struct Blit *); u32 AnimID;
} Blit;
typedef union MenuSlot {
    float matrix[16];
    struct { float x,y,depth; u8 rest[48]; u8 alpha; u8 tail[3]; } state;
} MenuSlot;
typedef struct ProjectContext {
    float rotation[3][3], position[3]; u8 opaque[104];
} ProjectContext;
extern MenuSlot trackscr_buttonInfo[];
extern ProjectContext gCamera[];
extern s8 gFogIntensity, gWind, gDifficulty, gCatchup;
extern s8 Hidden(Blit *,s32);
extern void ZOID_ProjectWindow(s32,float *,ProjectContext *,void *,s16 *);
extern void UpdateBlit(Blit *);
s32 trackscr_animNewSlider(Blit *blt)
{
    s32 segment=blt->AnimID&15;
    s32 marker=blt->AnimID>>16;
    s16 position[2];


    if (Hidden(blt,
        (0.6981316805f<trackscr_buttonInfo[marker+21].state.depth && trackscr_buttonInfo[marker+21].state.depth<2.443460941f)
        || !trackscr_buttonInfo[marker+21].state.alpha))
        return 1;
    ZOID_ProjectWindow(0,&trackscr_buttonInfo[marker+21].matrix[12],&gCamera[0],0,position);
    blt->X=position[0]-blt->Width/2;
    blt->Y=position[1]-(blt->Height/4)/2;
    blt->Top=0;
    blt->Bot=blt->Height/4-1;
    blt->Left=0;
    blt->Right=blt->Width-1;
    blt->Alpha=trackscr_buttonInfo[marker+21].state.alpha;
    if (blt->Alpha==255)
        blt->Alpha=254;
    if (1.570796371f<trackscr_buttonInfo[marker+21].state.depth) {
        blt->Top+=blt->Height/2;
        blt->Bot+=blt->Height/2;
    }
    if (segment==1) {
        blt->Top+=blt->Height/4;
        blt->Bot+=blt->Height/4;
        switch (marker) {
        case 14: blt->Right=(blt->Width-1)*gFogIntensity/3; break;
        case 15: blt->Right=(blt->Width-1)*gWind/4; break;
        case 18: blt->Right=(blt->Width-1)*gDifficulty/5; break;
        case 19: blt->Right=(blt->Width-1)*gCatchup/2; break;
        }
    }
    UpdateBlit(blt);
    return 1;
}
