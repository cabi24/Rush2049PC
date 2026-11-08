/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * N64 per-player HUD bar animation. Packed AnimID chooses player (bits 4..7)
 * and crop kind (bits 0..3). Hide inactive/unavailable players, select the
 * split-screen texture, position it, and crop the bar from a signed car value.
 * The native crop assignment order is Right, Top, Bot. Reordering the final
 * independent stores changes IDO's temporary register cycle.
 *
 * Shared source contract: arcade game/hud.c Hidden and LIB/blit.h, revision
 * 845329d7b36f5a384c5625ed9a0aef584ab46139. The complete N64 bar callback and
 * per-player tables are platform-specific. Object prefixes/strides below are
 * observed native layouts, not fabricated local storage. No padding locals,
 * dead reads, stand-ins, altered call contracts, or keep settings are used.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct Blit {
    const char *Name;
    void *image;
    void *Info;
    s16 TexIndex;
    s16 X, Y;
    u16 Z;
    s16 Width, Height;
    u8 Alpha, Flip;
    s8 Hide;
    u8 Init;
    s16 Top, Bot, Left, Right;
    s16 color;
    u16 unknown26;
    s32 (*AnimFunc)(struct Blit *);
    u32 AnimID;
} Blit;
typedef struct { u8 other0[10]; s8 flag; u8 other11[1979]; s16 index; u8 other1992[8]; s16 value; u8 other2002[54]; } Car2056;
typedef struct { u8 other0[239]; s8 mode; u8 other240[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 other240_308[0x380 - 0x308]; void *input; u8 other240_384[0x3B8 - 0x384]; } Object952;
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s16 gNumWindows,gNumPlayers;
extern s8 gShowTach;
extern char tachometer_md_name[],tachometer_sm_name[];
extern s32 tachPos[][4][2];

extern void UpdateBlit(Blit *);
extern void RenameBlit(Blit *,const char *,s32);
s32 animTach(Blit *blt)
{
    s32 slot, kind, hide;
    f32 amount;
    slot=(blt->AnimID & 0xf0)>>4;
    kind=blt->AnimID & 0xf;
    if(slot>=gNumWindows) {
        if(slot>=gNumPlayers) blt->AnimFunc=0;
        if (blt->Hide != 1) {
            blt->Hide = 1;
            UpdateBlit(blt);
        }
        return 1;
    }
    hide = gShowTach == 0 || PC_MODEL_AUTOTRANS(slot) != 0 ||
           PC_CAR_FINISHED(PC_MODEL_SLOT(slot)) == 1;
    if (hide != blt->Hide) {
        blt->Hide = hide;
        UpdateBlit(blt);
    }
    if (blt->Hide) return 1;
    if(gNumWindows==2) RenameBlit(blt,tachometer_md_name,0);
    else if(gNumWindows>=3) RenameBlit(blt,tachometer_sm_name,0);
    blt->X=tachPos[gNumWindows-1][slot][0]-blt->Width/2;
    blt->Y=tachPos[gNumWindows-1][slot][1];
    if(kind==0) blt->Bot=blt->Height/2-1;
    else if(kind==1) {
        amount=(PC_MODEL_S7D0(slot)*1.35f)*0.0001f;
        if(amount<0.0f) amount=-amount;
        if(amount>1.0f) amount=1.0f;
        blt->Right=(s32)((blt->Width-1)*amount);
        blt->Top=blt->Height/2;
        blt->Bot=blt->Height-1;
    }
    blt->Alpha=254;
    UpdateBlit(blt);
    return 1;
}
