#ifndef RUSH_STATIC_DEBUG_CONTEXT_H
#define RUSH_STATIC_DEBUG_CONTEXT_H
#include "../src/rom/rom_tu.h"

/* Baseline C68-C83 debug-recipe context:26 native bodies/8656 static bytes.
 * Declarations and genuine IEEE carrier only; no owned storage or ROM claims.
 * Existing SDK/m2c declarations remain unchanged. Final formatter bodies are
 * separately sourced BSD C81; complete notices travel with their sources. */
extern s8 decompress_lock_ready;
extern OSMesg decompress_lock_message;
extern OSMesgQueue decompress_lock_queue;
extern void init_decompress_lock(void);
/* port/ultra/pi.c's decoders: src is a cartridge address or offset, or (the game image) a host pointer. */
extern s32 lzss_decode(uintptr_t src, void *dst);
extern s32 unzipdata_from_rom(uintptr_t src, void *dst, s32 heap_window);

typedef struct {
    unsigned sign:1;
    unsigned exponent:11;
    unsigned fraction:20;
    unsigned low;
} DoubleBits;
typedef union {
    double value;
    DoubleBits bits;
} DoubleUnion;

extern float gRetraceRate, gRetraceTime;
extern float video_x_scale_factor, video_y_scale_factor, retrace_time_pal, retrace_time_ntsc;
extern s16 video_tv_type, video_mode_index;
extern s16 video_h_start, video_h_end;
extern s16 video_v_start, video_v_end;
extern s16 video_extra_width, video_extra_height;
extern s16 video_offset_x, video_offset_y, video_width, video_height;
extern s16 video_pending_frames;
extern s16 video_default_extra_width[], video_default_extra_height[];
/* video_default_x/Y retain their accepted existing m2c declarations. */
extern s16 video_offset_bounds[][8];
extern OSViMode *video_base_mode, *video_adjusted_mode;
extern OSViMode osViModeTable[], video_adjusted_modes[];
extern void zoid_scale_windows(float, float), video_build_adjusted_mode(void);
extern u32 osSetIntMask(u32);

extern OSThread gIdleThread, main_thread;
extern u8 gStackIdle[];
extern void __osInitialize_common(void), idle_thread_entry(void *), main_thread_entry(void *);
extern OSMesgQueue gViModeTable;
extern OSMesg gViModeLan1[];
/* main_thread_stack retains its accepted existing m2c declaration. */

extern double modf(double, double *);
extern double gPerspFov, gPerspAspect;
extern u8 *__round_helper(double, s32 *, u8 *, u8 *, u8, u8 *);
extern u8 *__write_exponent(u8 *, s32, s32);
/* Historical fcvt is the real three-argument formatter worker. */
extern int fcvt(char *, const char *, char *);

#endif /* RUSH_STATIC_DEBUG_CONTEXT_H */
