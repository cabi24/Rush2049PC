/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct ReplayStream {
    u8 pad0[5];
    s8 mode;
    u8 pad6[0x34 - 6];
    f32 dt;
    u8 pad38[0x4C - 0x38];
#ifdef TARGET_PC
    u32 data;           /* a heap block (the window): the ghost header keeps its N64 layout, file data */
#else
    u8 *data;
#endif
    u32 pos;
    u32 count;
} ReplayStream;
#ifdef TARGET_PC
/* PC: a ghost's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10): its header handle at +56 */
typedef struct ReplayOwner { void *ll; void **fileh; u8 pad10[0x24 - 0x08]; void **datah; ReplayStream **stream; } ReplayOwner;
#define DATA(s) ((u8 *)(uintptr_t)(s)->data)
#else
typedef struct ReplayOwner { u8 pad0[0x28]; ReplayStream **stream; } ReplayOwner;
#define DATA(s) s->data
#endif
typedef struct ReplaySlot { ReplayOwner *owner; } ReplaySlot;
typedef struct Model {
    u8 pad0[0x720];
    volatile f32 steer;
    u8 pad724[4];
    f32 brake;
    f32 gas;
    s8 gear;
    s8 flagB;
    s8 flagA;
    u8 pad733[0x808 - 0x733];
} Model;
#ifdef TARGET_PC
typedef struct Car { u8 pad0[239]; s8 locked; u8 pad1[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 pad1_308[0x380 - 0x308]; void *input; u8 pad1_384[0x3B8 - 0x384]; } Car;
#else
typedef struct Car { u8 pad0[239]; s8 locked; u8 pad1[952 - 240]; } Car;
#endif

extern f32 D_8002AFB8;
extern volatile s8 D_80114738;
extern ReplaySlot *D_80152698[];
extern s32 D_80143FF4;
extern Model D_8014A250[];
extern Car D_80152818[];

#define ABS(x) ((x) >= 0 ? (x) : -(x))

s32 func_800E5D64(s32 idx, f32 *out)
{
    ReplayStream *s;
    f32 f;

    *out = D_8002AFB8;
    if (D_80114738) {
        return 0;
    }
    if (D_80152698[idx] == 0) {
        return 0;
    }
    s = *D_80152698[idx]->owner->stream;
    if (s->mode == 0) {
        return 0;
    }
    if (s->mode > 0) {
        *out = s->dt;
        if (D_8002AFB8 != s->dt && 0.02f == s->dt) {
            if (ABS(D_80143FF4) % 6 == 2) {
                return -1;
            }
        }
        if (s->pos == s->count) {
            D_8014A250[idx].flagA = 0;
            D_8014A250[idx].flagB = 0;
            D_8014A250[idx].steer = 0.0f;
            D_8014A250[idx].gas = 0.0f;
            D_8014A250[idx].brake = 0.0f;
        } else {
            D_8014A250[idx].gear = (s8) (DATA(s)[s->pos] & 7) - 1;
            D_8014A250[idx].flagA = (DATA(s)[s->pos] & 8) >> 3;
            D_8014A250[idx].flagB = (DATA(s)[s->pos] & 0x10) >> 4;
            D_8014A250[idx].steer = (f32) ((s8 *) DATA(s))[s->pos + s->count] / 127;
            D_8014A250[idx].gas = (f32) (DATA(s)[s->pos + s->count + s->count] >> 4) / 15.0f;
            D_8014A250[idx].brake = (f32) (DATA(s)[s->pos + s->count + s->count] & 0xF) / 15.0f;
            s->pos++;
        }
        if (D_8002AFB8 != s->dt && 0.016666668f == s->dt) {
            if (ABS(D_80143FF4) % 5 == 2) {
                return 1;
            }
        }
        return 0;
    } else {
        if (D_80152818[idx].locked) {
            return 0;
        }
        if (!(s->pos < s->count)) {
            return 0;
        }
        DATA(s)[s->pos] = (D_8014A250[idx].gear + 1) | (D_8014A250[idx].flagA * 8) | (D_8014A250[idx].flagB * 16);
        if (D_8014A250[idx].steer * 127.0f < 0.0f) {
            f = D_8014A250[idx].steer * 127.0f - 0.5f;
        } else {
            f = D_8014A250[idx].steer * 127.0f + 0.5f;
        }
        DATA(s)[s->pos + s->count] = (s32) f;
        DATA(s)[s->pos + s->count + s->count] = ((u8) (D_8014A250[idx].gas * 15.0f + 0.5f) * 16) | (u8) (D_8014A250[idx].brake * 15.0f + 0.5f);
        s->pos++;
        return 0;
    }
}
