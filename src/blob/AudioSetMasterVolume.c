/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef float f32;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef int s32;
extern f32 music_volume_scale;
extern void sound_fade_master_volume(u8, u16, s32, s32);
void AudioSetMasterVolume(f32 value, u16 channel, s32 first, s32 second) {
    if (first) sound_fade_master_volume((u8)N64_F2U(value * 127.0f * music_volume_scale), channel, 1, 0);
    if (second) sound_fade_master_volume((u8)N64_F2U(value * 127.0f), channel, 0, 1);
}
