/* src/rom/sound/sound_services.h: the game's services to the sound library (MUSYX_DATA.md 8.1). The library calls
 * nothing of the OS itself: sound_set_services installs these hooks, the game's audio_driver_services, which the
 * library's files call through sound_services. The record is include/game/soundlib.h's SoundServices. */
#ifndef SOUND_SERVICES_H
#define SOUND_SERVICES_H

#include "game/soundlib.h"

extern SoundServices sound_services;

#endif
