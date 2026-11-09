/* include/game/game.h: the game's shared declarations. Every game file includes this first.
 *
 * One declaration of each type, datum and function the game's files share, by subsystem (docs/code/README.md
 * "The headers"). types.h holds the types, each subsystem's header its data and its functions' prototypes. */
#ifndef GAME_GAME_H
#define GAME_GAME_H

#include "game/m2c.h"       /* what the decompiler left in the code: M2C_FIELD, M2C_UNK */
#include "game/view.h"      /* VIEW: a file's own view of a shared datum */
#include "game/gbi.h"       /* the display-list encodings (F3DEX2 and RDP commands, modes, formats) */

#include "game/system.h"    /* start-up, threads, the scheduler, timers, the frame pipeline, video, the overlays */
#include "game/memory.h"    /* the heap, the asset wads and waves, lists and trees */
#include "game/maths.h"     /* vectors, matrices, angles, random numbers */
#include "game/audio.h"     /* the sound manager, effects, music and the MusyX sound library */
#include "game/scene.h"     /* the scene objects (mbox and zoid), models, textures, windows, the sky, the world */
#include "game/blit.h"      /* the 2D blitter and panes */
#include "game/text.h"      /* fonts and text */
#include "game/input.h"     /* controllers and the Rumble Pak */
#include "game/save.h"      /* the Controller Pak, profiles, records, options and passwords */
#include "game/track.h"     /* tracks, the road tree, targets, paths and checkpoints */
#include "game/cars.h"      /* the cars, their visuals, skids and smoke */
#include "game/physics.h"   /* the car simulation: collision, the road, the drivetrain, tyres, respawning */
#include "game/race.h"      /* the game loop and the race: state, countdown, drones, stunts, battle, ghosts */
#include "game/camera.h"    /* the cameras */
#include "game/hud.h"       /* the HUD */
#include "game/frontend.h"  /* the front end's screens (runtime image A) */

#endif /* GAME_GAME_H */
