/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * SetCircuitCode (0x800FE080, 1,068 B) -- historical label. Decodes a packed
 * 70-bit setup code from a byte string into the current player's session record:
 * SetPasswordString(input) loads the string into the bit reader
 * (NewPassword sets the 86-bit/14-char geometry), GetPasswordElement(&v, n) reads
 * n bits. Fields: mode (3), option (1), length (5), seed (20), level (3) and six
 * 9-bit values. Validity: length in [1, 4*gNumCircuitTracks[mode]), the largest value
 * must not exceed gCircuitPoints[0]*length, and the values must sum to
 * length * sum(gCircuitPoints[0..5]). On success the fields are stored in the session
 * record (player handle -> object -> resource data + 1780), the length and the six
 * values (as s16) in the per-selector tables gTempCircuitFirstTrack / gTempCircuitPoints, then
 * ContinueCircuit(). Returns 1, or 0 for a rejected code.
 *
 * N64-only (no arcade counterpart found). Shaping notes (no devices):
 *  - declaration order sets the frame: total_weights/total_values sit between
 *    values[] and ranks[], and i/j before session (session's spill home 64(sp)).
 *  - the first sum's init is in the for-init (`for (total_weights = 0, i = 0; ...)`):
 *    a separate `total_weights = 0;` statement schedules `move a0,zero` ahead of the
 *    strength-reduced pointer setup (3 words).
 * Prior drafts: (130/267).
 */
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;

typedef struct Resource { u8 *data; } Resource;
typedef struct Object { void *next; void *prev; void *fileh; u8 pad0_C[0x2C - 0xC]; Resource *resource; } Object;
typedef struct Handle { Object *object; } Handle;
typedef struct Player76 { u8 index, selector; u8 pad2[70]; Handle *handle; } Player76;
typedef struct Session76 {
    u8 pad0[5], option, enabled, level, mode, length;
    u8 pad10[2];
    u32 seed;
} Session76;

extern Player76 gPlayers[];
extern u8 gCircuitPlayer;
extern u8 gTempCircuitFirstTrack[];
extern s16 gTempCircuitPoints[][6];
extern u32 gNumCircuitTracks[];
extern u32 gCircuitPoints[6];

extern s32 SetPasswordString(u8 *input);
extern s32 NewPassword(u32 a, u32 b);
extern s32 GetPasswordElement(u32 *out, u32 bits);
extern void NewCircuit(s32 mode);
extern void ContinueCircuit(void);

s32 SetCircuitCode(u8 *input)
{
    u32 mode, option, length, seed, level;
    u32 values[6];
    u32 total_weights, total_values;
    u32 ranks[6];
    s32 i, j;
    Session76 *session;

    session = (Session76 *)(gPlayers[gCircuitPlayer].handle->object->resource->data + 1780);
    NewPassword(86, 14);
    if (!SetPasswordString(input)) return 0;
    GetPasswordElement(&mode, 3);
    GetPasswordElement(&option, 1);
    GetPasswordElement(&length, 5);
    GetPasswordElement(&seed, 20);
    GetPasswordElement(&level, 3);
    for (i = 0; i < 6; i++) GetPasswordElement(&values[i], 9);
    if (length == 0 || length >= gNumCircuitTracks[mode] * 4) return 0;
    for (i = 0; i < 6; i++) ranks[i] = i;
    for (i = 0; i < 5; i++) {
        for (j = i + 1; j < 6; j++) {
            if (values[ranks[i]] < values[ranks[j]]) {
                ranks[i] ^= ranks[j];
                ranks[j] ^= ranks[i];
                ranks[i] ^= ranks[j];
            }
        }
    }
    if (gCircuitPoints[0] * length < values[ranks[0]]) return 0;
    for (total_weights = 0, i = 0; i < 6; i++) total_weights += gCircuitPoints[i];
    total_values = 0;
    for (i = 0; i < 6; i++) total_values += values[ranks[i]];
    if (total_values != total_weights * length) return 0;
    NewCircuit(mode);
    session->enabled = 1;
    session->option = option;
    session->length = length;
    gTempCircuitFirstTrack[gPlayers[gCircuitPlayer].selector] = length;
    session->seed = seed;
    session->level = level;
    for (i = 0; i < 6; i++) gTempCircuitPoints[gPlayers[gCircuitPlayer].selector][i] = values[i];
    ContinueCircuit();
    return 1;
}
