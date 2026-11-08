/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Sequence start, [0x800979A0,0x80097AC4), 292 bytes.
 * The second word argument is genuinely homed but unused in retail.
 * The accepted GetWaveInstanceAddr is inlined; ValidateWaveInstance preserves a3.
 * External services remain ordinary O32 call boundaries.
 * Base: research packet / (plain form, 12 words: the final
 * song_handle store went through `lui at` instead of retail's materialised address in v1).
 * : the compiled-out holder `if (song_handle) {}` after the last store (disclosed; it emits no code)
 * gives the song_handle address web a later occurrence, so uopt colours the store's piece (v1), as retail.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef int s32;
typedef struct ResourceGroup ResourceGroup;
typedef struct SampleRecord SampleRecord;
typedef struct ResourceOffsets ResourceOffsets;
typedef struct SequenceOptions SequenceOptions;
typedef struct SeqEntry { u16 h0; u16 h2; } SeqEntry;
extern u8 music_disabled;
extern s32 song_handle, song_number, song_wave;
extern void *song_data;
extern SeqEntry song_groups[];
extern ResourceGroup *musyx_project;
extern void *musyx_samples;
extern SampleRecord *musyx_sdir;
extern ResourceOffsets *musyx_pool;
s32 LoadWave(s32, s32, s32, s32, void *);
void LockWaveInstance(s32);
void *GetWaveInstanceAddr(s32);
s32 sound_push_group(ResourceGroup *, u16, void *, SampleRecord *, ResourceOffsets *);
s32 sound_start_song(u16, u16, void *, SequenceOptions *);

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
