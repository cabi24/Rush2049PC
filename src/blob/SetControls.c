/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * SetControls (0x800D7E88, 496 B) = SetControls(int resetToDefault) of controls.c (MAT3 GameCube controls.c
 * SetControls + SetPlayerControls; identity "lead", confirmed here by structure and globals).
 * For every player: the control bytes come from gDefaultControls or gNewControls[controller]
 * (gNewControls, 10 bytes per controller on N64, read in place: no local copy, no rumble byte); if the player's
 * controller set changed (gControlsSet) it is cleared and the controls are saved to the player's
 * profile (SavePlayerControls) and, without a profile or when resetting, to gJustPlayers
 * (gJustPlayers); then SetPlayerControls maps each byte through its gControlTable into
 * gPlayers[p].controls[] (gPlayers, 76-byte records).
 * Shaping: SetPlayerControls is written in place as a block (GC has it as a separate function). As an inlined
 * static helper its parameter copy of p stays a coloured web (an s3 save with no code) and pushes the hoisted
 * &gDefaultControls (retail s8) out of the callee-saved set (111/124 words).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;

typedef struct Player {
    u8 pad00;
    u8 controller;        /* 0x01 */
    u8 pad02[0x18 - 2];
    u32 controls[10];     /* 0x18 */
    u8 pad40[0x48 - 0x40];
    void **save;          /* 0x48 */
} Player;

extern s16 gNumPlayers;
extern Player gPlayers[];         /* gPlayers */
extern s8 gDefaultControls[10];           /* gDefaultControls */
extern s8 gNewControls[][10];         /* gNewControls */
extern s8 gControlsSet[];             /* gControlsSet */
extern void *gJustPlayers[];          /* gJustPlayers */
extern u32 gControlTable[];            /* SetPlayerControls' gControlTable */
void SavePlayerControls(void **playerh, s8 *controls);

void SetControls(s32 resetToDefault)
{
    s32 p;
    s8 *controls;
    for (p = 0; p < gNumPlayers; p++) {
        if (resetToDefault) {
            controls = gDefaultControls;
        } else {
            controls = gNewControls[gPlayers[p].controller];
        }
        if (gPlayers[p].controller != 5 && gControlsSet[gPlayers[p].controller]) {
            gControlsSet[gPlayers[p].controller] = 0;
            if (gPlayers[p].save) SavePlayerControls(gPlayers[p].save, controls);
            if (!gPlayers[p].save || resetToDefault) SavePlayerControls(&gJustPlayers[gPlayers[p].controller], controls);
        }
        {
            s32 i;
            for (i = 0; i < 10; i++) gPlayers[p].controls[i] = gControlTable[controls[i]];
        }
    }
}
