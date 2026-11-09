#include "game/game.h"

VIEW(s32, gGameState);
#define gGameState gGameState_view

void updateFrontEnd(void)
{
  if (gGameState & 2)
  {
    updateLicenseScreen();
  }
  else
    if (gGameState & 0x40000000)
  {
    updateWarningScreen();
  }
  else
    if (gGameState & 4)
  {
    updateTitleScreen();
  }
  else
    if (gGameState & 8)
  {
    updateDemoGame();
  }
  else
    if (gGameState & 0x10)
  {
    gMirrorWaves = 0;
    MirrorWaveInstance(FindWaveInstanceInternal(0x36, -1));
    updateStartScreen();
  }
  else
    if (gGameState & 0x2000)
  {
    updateRecordScreen();
  }
  else
  {
    if (gGameState & 0x20)
    {
      updatePlayerScreen();
    }
    else
      if (gGameState & 0x40)
    {
      updateModeScreen();
    }
    else
      if (gGameState & 0x4000)
    {
      updateCircuitScreen();
    }
    else
      if (gGameState & 0x80)
    {
      updateTrackScreen();
    }
    else
      if (gGameState & 0x100)
    {
      updateCarScreen();
    }
    else
      if (gGameState & 0x200)
    {
      updateOptionScreen();
    }
    else
      if (gGameState & 0x400)
    {
      updateControlScreen();
    }
    else
      if (gGameState & 0x800)
    {
      updateAudioScreen();
    }
    else
      if (gGameState & 0x1000)
    {
      updateVideoScreen();
    }
    else
      if (gGameState & 0x04000000)
    {
      updatePakScreen();
    }
    else
      if (gGameState & 0x08000000)
    {
      updateCheatScreen();
    }
  }
  updateBG();
  SaveServer(0, 0);
}

