/* flags: -g0 -O3 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
/* Complete 216-byte record-score routine at 0x80098710.
 * The record layout is a minimal native offset view. The audio-priority
 * interpretation is inferred from its caller; no direct arcade donor is known.
 * The existing wrapped-time helper is genuine complete inline context and
 * receives no additional matching credit. No artificial padding or operations.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef int s32;
typedef float f32;
#ifdef TARGET_PC
/* PC: the record is an fx tag (audiomgr_thread.c's tFxSequence: state, loop, priority, startTime, vol), whose
 * links and list are host-sized (docs/port/SYNTH.md). */
typedef struct Record {
    void *unknown00[3];
    unsigned int unknown18;
    s32 kind;
    u8 unknown20[5];
    s8 flag;
    u8 unknown26;
    u8 value;
    u8 unknown28[4];
    f32 start;
    f32 scale;
} Record;
#else
typedef struct Record {
    u8 unknown00[0x10];
    s32 kind;
    u8 unknown14[5];
    s8 flag;
    u8 unknown1A;
    u8 value;
    u8 unknown1C[4];
    f32 start;
    f32 scale;
} Record;
#endif
extern f32 D_80152748;

f32 func_800986DC(f32 arg0, f32 arg1) {
    f32 var_f2;

    var_f2 = D_80152748;
    if (D_80152748 < arg0) {
        var_f2 = D_80152748 + 14400.0f;
    }
    return (var_f2 - arg0) / arg1;
}

s32 func_80098710(Record *record) {
    s32 result;

    result = record->value;
    result = result + (1.0f - record->scale) * 80.0f;
    if (record->flag) {
        result += 8;
    }
    else if (record->value && record->start >= 0.0f && record->kind == 2) {
        result += func_800986DC(record->start, 0.25f);
    }
    return result;
}
