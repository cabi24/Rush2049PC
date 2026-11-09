#include "game/game.h"

typedef struct ResourceGroup ResourceGroup;
typedef struct SampleRecord SampleRecord;
typedef struct ResourceOffsets ResourceOffsets;

typedef struct SeqEntry { u16 h0; u16 h2; } SeqEntry;

VIEW(SeqEntry[], song_groups);
#define song_groups song_groups_view
VIEW(ResourceGroup *, musyx_project);
#define musyx_project musyx_project_view

VIEW(SampleRecord *, musyx_sdir);
#define musyx_sdir musyx_sdir_view
VIEW(ResourceOffsets *, musyx_pool);
#define musyx_pool musyx_pool_view

void AudioSongPlay(s32 arg0, s32 arg1) {
    SeqEntry *e;

    if (music_disabled == 0) {
        if (song_handle == -1 || arg0 != song_number) {
            song_wave = LoadWave(arg0 + 10, 0, 0, 0, 0);
            LockWaveInstance(song_wave);
            song_data = GetWaveInstanceAddr(song_wave);
            e = &song_groups[arg0];
            sound_push_group(musyx_project, e->h2, musyx_samples, musyx_sdir, musyx_pool);
            song_handle = sound_start_song(e->h2, arg0, song_data, 0);
            song_number = arg0;
            if (song_handle) {}
        }
    }
}
