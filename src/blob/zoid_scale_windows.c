/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;
typedef float f32;
typedef struct Record {void *first; u16 *bounds; u8 pad8[28]; f32 x0, y0, x1, y1; u8 pad52[20];} Record;
extern u8 gNumZoidWindows;
extern Record ZoidWinInfo[];
void zoid_scale_windows(f32 x, f32 y) {
    s32 i;
    Record *record;
    for (i = 0; i < gNumZoidWindows; i++) {
        record = &ZoidWinInfo[i];
        record->bounds[0] = (u32)((f32)(u32)record->bounds[0] * x);
        record->bounds[2] = (u32)((f32)(u32)record->bounds[2] * x);
        record->x0 *= x;
        record->x1 *= x;
        record->bounds[1] = (u32)((f32)(u32)record->bounds[1] * y);
        record->bounds[3] = (u32)((f32)(u32)record->bounds[3] * y);
        record->y0 *= y;
        record->y1 *= y;
    }
}
