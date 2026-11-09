/* AudioSongPlay: starts a song on MusyX (the N64 form of MAT3's audiosongps2.c AudioSongPlay). */
#include "game/game.h"

typedef struct ResourceGroup ResourceGroup;
typedef struct SampleRecord SampleRecord;
typedef struct ResourceOffsets ResourceOffsets;

/* A song's sound group (song_groups[song]): its id in the project, MusyX's group id. */
typedef struct SongGroup { u16 other; u16 id; } SongGroup;

VIEW(SongGroup[], song_groups);
#define song_groups song_groups_view
VIEW(ResourceGroup *, musyx_project);
#define musyx_project musyx_project_view

VIEW(SampleRecord *, musyx_sdir);
#define musyx_sdir musyx_sdir_view
VIEW(ResourceOffsets *, musyx_pool);
#define musyx_pool musyx_pool_view

/* The wave (asset file) of song 0; song n is this plus n. */
#define WAVE_FIRST_SONG 10

/* Starts song (an index of gSongNames) unless the music is off (AudioInitSys) or it already plays: loads and
 * locks its wave (the song's sequence data), pushes its sound group and starts it as a MusyX song
 * (song_handle). loop is not read: every song is started the same way. AudioSongStop undoes it. */
void AudioSongPlay(s32 song, s32 loop) {
    SongGroup *group;

    if (music_disabled == 0) {
        if (song_handle == -1 || song != song_number) {
            song_wave = LoadWave(song + WAVE_FIRST_SONG, 0, 0, 0, 0);
            LockWaveInstance(song_wave);
            song_data = GetWaveInstanceAddr(song_wave);
            group = &song_groups[song];
            sound_push_group(musyx_project, group->id, musyx_samples, musyx_sdir, musyx_pool);
            song_handle = sound_start_song(group->id, song, song_data, 0);
            song_number = song;
            if (song_handle) {}
        }
    }
}
