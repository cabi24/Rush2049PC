/* updateFrontEnd: the front end's frame (MAT3 front.c updateFrontEnd). GameLoop calls it in every front-end state
 * (GS_FRONTEND_MASK). */
#include "game/game.h"

#define WAVE_START_MENU 0x36        /* the start menu's models */

/* Run the screen of the current state (one state bit is set), then the background's frame and the profile saver
 * (SaveServer). On the start menu mirror mode goes off first and the menu's models are turned back the right way
 * round (MirrorWaveInstance flips a wave's triangles to match gMirrorWaves), in case a mirrored race left them so. */
void updateFrontEnd(void)
{
  if (gGameState & GS_LICENSE)
  {
    updateLicenseScreen();
  }
  else if (gGameState & GS_WARNING)
  {
    updateWarningScreen();
  }
  else if (gGameState & GS_TITLE)
  {
    updateTitleScreen();
  }
  else if (gGameState & GS_DEMO)
  {
    updateDemoGame();
  }
  else if (gGameState & GS_START)
  {
    gMirrorWaves = 0;
    MirrorWaveInstance(FindWaveInstanceInternal(WAVE_START_MENU, -1));
    updateStartScreen();
  }
  else if (gGameState & GS_RECORDS)
  {
    updateRecordScreen();
  }
  else if (gGameState & GS_PLAYER)
  {
    updatePlayerScreen();
  }
  else if (gGameState & GS_MODE)
  {
    updateModeScreen();
  }
  else if (gGameState & GS_CIRCUIT)
  {
    updateCircuitScreen();
  }
  else if (gGameState & GS_TRACK)
  {
    updateTrackScreen();
  }
  else if (gGameState & GS_CAR)
  {
    updateCarScreen();
  }
  else if (gGameState & GS_OPTIONS)
  {
    updateOptionScreen();
  }
  else if (gGameState & GS_CONTROLS)
  {
    updateControlScreen();
  }
  else if (gGameState & GS_AUDIO)
  {
    updateAudioScreen();
  }
  else if (gGameState & GS_VIDEO)
  {
    updateVideoScreen();
  }
  else if (gGameState & GS_PAK)
  {
    updatePakScreen();
  }
  else if (gGameState & GS_CHEAT)
  {
    updateCheatScreen();
  }
  updateBG();
  SaveServer(0, 0);
}
