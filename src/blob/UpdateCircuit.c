/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * UpdateCircuit: end-of-round result record for the current player (gCircuitPlayer), N64-only (no arcade
 * ancestor).  `values` holds one byte per car (6, or 4 when gNumPlayers != 1); for player 2
 * (gCircuitPlayer > 0) the first two entries are swapped.  Each value's low three bits are packed into the
 * player's session record (save data + 1780: 9 bytes per car at +20, bit position gCircuitTrack * 3); then
 * score += time, length++ (rounds), ContinueCircuit(), gCircuitTrack--, and RecordCircuit() (end-of-race
 * statistics) once length reaches gCircuitSize.  In mode 3 the session level (+7) is raised when the best
 * place (gCircuitCars[0/1].place, min over both players in two-player) and the points lead of first over
 * second place (gCircuitCars[].score, Ranking76 stride 76) are good enough for the current level.
 * Finally SavePlayerCircuit(player handle).
 * the scorer fn: MATCH; whole-program unit: EQUAL, 0 locked bodies differ.
 * Base: B80 native draft (113/225).
 *
 * Shaping / disclosures:
 *  - the xor swap is one source line (as a SWAP macro): with three lines as1 orders the three byte
 *    stores [0],[1],[0]; on one line it emits retail's [0],[0],[1] (as1 uses the line number).
 *  - `data` and `session = data + 1780` are both locals: retail keeps the record base unfolded
 *    (`addiu a3,t5,1780` for the loop, `addiu t2,t5,1780` after it) instead of folding 1780 into offsets.
 *  - `level` is read once into a local (retail loads session->level once for the whole condition).
 *  - `first`/`second` are uninitialised on paths where no car holds place 0/1 (retail reads their stack
 *    homes there); declaration order fixes those homes (48/44).  No unused locals.
 */
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef struct Resource { u8 *data; } Resource;
typedef struct Object { void *next; void *prev; void *fileh; u8 opaque_C[0x2C - 0xC]; Resource *resource; } Object;
typedef struct Handle { Object *object; } Handle;
typedef struct Player76 { u8 index,selector; u8 opaque[70]; Handle *handle; } Player76;
typedef struct Session76 {
    u8 prefix[5], option, enabled, level;
    s8 mode, length;
    u8 gap[2];
    unsigned int seed;
    float score;
    u8 packed[56];
} Session76;
typedef struct Ranking76 { u8 index,place; u16 score; u8 rest[72]; } Ranking76;
extern Player76 gPlayers[];
extern Ranking76 gCircuitCars[];
extern u8 gCircuitPlayer;
extern s8 gCircuitTrack, gCircuitSize;
extern s16 gNumPlayers;
extern void ContinueCircuit(void),RecordCircuit(void),SavePlayerCircuit(Handle *);
#define SWAP(a, b) { (a) ^= (b); (b) ^= (a); (a) ^= (b); }

void UpdateCircuit(u8 *values, float time)
{
    u8 *data;
    Session76 *session;
    int count, i, j, bit, best;
    int first, second, difference, level;

    data = gPlayers[gCircuitPlayer].handle->object->resource->data;
    session = (Session76 *)(data + 1780);
    count = 6 - (gNumPlayers == 1 ? 0 : 2);
    if (gCircuitPlayer > 0) {
        SWAP(values[0], values[1]);
    }
    for (i = 0; i < count; i++) {
        bit = gCircuitTrack * 3;
        for (j = 0; j < 3; j++) {
            session->packed[i * 9 + (bit >> 3)] =
                (session->packed[i * 9 + (bit >> 3)] & ~(1 << (bit & 7))) |
                ((values[i] & 1) << (bit & 7));
            values[i] >>= 1;
            bit++;
        }
    }
    session->score += time;
    session->length++;
    ContinueCircuit();
    gCircuitTrack--;
    if (session->length >= gCircuitSize) RecordCircuit();
    if (session->mode == 3) {
        if (gNumPlayers == 1) best = gCircuitCars[0].place;
        else best = gCircuitCars[0].place < gCircuitCars[1].place ? gCircuitCars[0].place : gCircuitCars[1].place;
        for (i = 0; i < 6; i++) {
            if (gCircuitCars[i].place == 0) first = i;
            else if (gCircuitCars[i].place == 1) second = i;
        }
        difference = gCircuitCars[first].score - gCircuitCars[second].score;
        level = session->level;
        if ((best == 0 && level < 5 && difference >= 20) ||
            (best == 0 && level < 4 && difference >= 6) ||
            (best < 2 && level < 3) ||
            (best < 3 && level < 2) ||
            (best < 4 && level <= 0)) session->level = level + 1;
    }
    SavePlayerCircuit(gPlayers[gCircuitPlayer].handle);
}
