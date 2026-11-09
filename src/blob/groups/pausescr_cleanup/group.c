/* updatePauseStateCleanup: the pause menu's tear-down when its state changes (a pausescr.c function; MAT3's
 * updatePauseState does the same in its uninit part). */
#include "game/game.h"

/* A profile (MAT3 tPlayerProfile) in its PC layout: datah (PC 0x38, N64 0x2C) is its save data, NULL for a
 * guest. */
typedef struct SaveData SaveData;
typedef struct Profile { void *next; void *prev; void *fileh; u8 opaque_C[0x2C - 0xC]; SaveData *datah; } Profile;
typedef struct Handle { Profile *object; } Handle;
/* A player's pad record (MAT3 tPadState, gPlayers): only its profile. */
typedef struct Player { u8 opaque[72]; Handle *handle; } Player;

VIEW(uintptr_t, pause_gTitlePane);
#define pause_gTitlePane pause_gTitlePane_view
extern Player gPlayers[];

extern void RemoveBlit(void *);

/* Remove the pause menu's blits and panes for the state it is leaving. Leaving the pause altogether (kNoPause) also
 * removes the dimming blit and saves each player's profile that has save data (SaveSelectPlayer: the settings may
 * have changed); every page's state removes the title, menu, "are you sure" and rumble blits. */
void updatePauseStateCleanup(void)
{
    int i;
    Player *player;
    switch(gPauseState) {
    case kNoPause:
        if(pause_DimBlit) {
            RemoveBlit(pause_DimBlit);
            pause_DimBlit=0;
        }
        player=gPlayers;
        for(i=0;i<gNumPlayers;i++,player++) {
            if(player->handle->object->datah)SaveSelectPlayer(player->handle);
        }
        /* fall through: the pages' blits go too */
    case kPauseMainScreen: case kPauseAudioScreen: case kPauseVideoScreen: case kPauseControlsScreen:
    case kPauseOptionsScreen: case kPauseRumbleScreen: case kPauseRestart: case kPauseQuit:
        if(pause_TitleBlit) {RemoveBlit(pause_TitleBlit);pause_TitleBlit=0;}
        if(pause_PauseBlit) {RemoveBlit(pause_PauseBlit);pause_PauseBlit=0;}
        if(pause_SureBlit) {RemoveBlit(pause_SureBlit);pause_SureBlit=0;}
        if(pause_RumbleBlit) {RemoveBlit(pause_RumbleBlit);pause_RumbleBlit=0;}
        break;
    }
    RemovePanes(0);
    pause_gTitlePane=0;
}
