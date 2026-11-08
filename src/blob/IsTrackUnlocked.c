/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { s8 b[19]; } Ent19;
extern s16 gNumPlayers;
extern Ent19 gUnlockedTracks[];
s32 IsTrackUnlocked(s32 idx) {
    s32 r = 0;
    s32 i;
    for (i = 0; i < gNumPlayers; i++) {
        if (gUnlockedTracks[i].b[idx]) r = 1;
    }
    return r;
}
