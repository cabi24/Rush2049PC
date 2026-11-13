/* ContinueCircuit: a circuit's races and drones, from its seed (MAT3 circuit.c). */
#include "game/game.h"

/* A player's circuit in progress (MAT3 tCircuit), in their profile's save data at 0x6F4. No pointers: the same
 * layout on both. */
struct tCircuit {
    u32 checksum;       /* 0x00 */
    u8 active;          /* 0x04 */
    u8 death;           /* 0x05 the death option: out of the circuit after a crash */
    u8 temp;            /* 0x06 entered by a circuit code: the points so far are gTempCircuitPoints' */
    u8 difficulty;      /* 0x07 */
    s8 type;            /* 0x08 kCircuitBeginner ... kCircuitExtreme */
    s8 track;           /* 0x09 the races driven */
    u8 pad0A[2];
    u32 seed;           /* 0x0C the circuit's random seed (its tracks, its drones) */
    f32 totalTime;      /* 0x10 */
    u8 ranks[6][9];     /* 0x14 each car's place in each race, 3 bits a race */
};

/* A profile's save data (the player's tSaveData): its circuit. */
typedef struct { u8 pad[0x6F4]; struct tCircuit info; } Sess;
/* A player's profile (SaveSlot, host layout: pc_prelude.h): its save data's handle. */
typedef struct { void *next; void *prev; void *fileh; u8 pad_C[0x2C - 0xC]; Sess **s; } A;
/* A player's input record (gPlayers): its profile slot and profile. */
typedef struct { u8 pad0; u8 selector; u8 pad[0x46]; A **profile; } InRec;
extern InRec gPlayers[];

/* The circuit's drones: the cars after the players. */
#define RI(e) gCircuitCars[(e) + gNumPlayers]

/* A random number from 0 to `max`, from the circuit's seed. */
static u16 irand(u16 max) {
    return (u32)seeded_rand(&gCircuitSeed) % (max + 1);
}
/* Rebuilds the player's circuit from its record: from its seed, its races (a full circuit, type 3, picks tracks at
 * random, a track at most four times and not twice running, with the direction and mirror not repeating; the others
 * run each track forwards then backwards), each race's fog and wind, the drones' drivers and their skills
 * (circuit_drone_skills, shuffled); from its ranks, each car's places and points so far (or the circuit code's
 * points), and the cars' standings. */
void ContinueCircuit(void) {
    s32 n, i, k, j, r, cnt;
    s32 idx[6];
    s32 m;
    s32 x;
    Sess *v;
    struct tCircuit *info;
    u8 pad[8];
    v = *((*gPlayers[gCircuitPlayer].profile)->s);
    info = &v->info;
    gCircuitSeed = info->seed;
    n = gNumCircuitTracks[info->type];
    if (expansion_pak_present == 0 && n >= 5) n = 5;
    if (info->type == 3) gCircuitSize = n * 4;
    else gCircuitSize = n * 2;
    gCircuitTrack = (info->track < gCircuitSize - 1) ? info->track : gCircuitSize - 1;
    for (i = 0; i < n; i++)
        gCircuitNumTrackRaces[i] = 0;
    cnt = 6 - gNumPlayers;
    for (i = 0; i < cnt; i++) {
    again:
        RI(i).name = irand(54);
        for (j = 0; j < i; j++)
            if (RI(i).name == RI(j).name) goto again;
    }
    for (i = 0; i < cnt; i++)
        idx[i] = i;
    for (i = 0; i < cnt; i++) {
        do { k = irand(cnt - 1); } while (i == k);
        idx[i] ^= idx[k]; idx[k] ^= idx[i]; idx[i] ^= idx[k];
    }
    for (i = 0; i < cnt; i++)
        for (k = 0; k != 12; k++)
            RI(idx[i]).skill[k] = circuit_drone_skills[i][k];
    for (k = 0; k < 12; k++) {
        do { x = irand(11); } while (k == x);
        for (i = 0; i < cnt; i++) {
            RI(idx[i]).skill[k] ^= RI(idx[i]).skill[x];
            RI(idx[i]).skill[x] ^= RI(idx[i]).skill[k];
            RI(idx[i]).skill[k] ^= RI(idx[i]).skill[x];
        }
    }
    for (i = 0; i <= gCircuitTrack; i++) {
        if (info->type == 3) {
            gCircuitTracks[i].trackNum = irand(n - 1);
            gCircuitTracks[i].backward = (u16)(seeded_rand(&gCircuitSeed) & 1);
            gCircuitTracks[i].mirror = irand(1);
            if (i != 0) {
                k = 0;
                while ((gCircuitTracks[i].trackNum == gCircuitTracks[i - 1].trackNum && k < n) || gCircuitNumTrackRaces[gCircuitTracks[i].trackNum] == 4) {
                    gCircuitTracks[i].trackNum++;
                    if (gCircuitTracks[i].trackNum >= n) gCircuitTracks[i].trackNum = 0;
                    k++;
                }
                for (;;) {
                    for (k = 0; k < i; k++) {
                        if (gCircuitTracks[i].trackNum == gCircuitTracks[k].trackNum && gCircuitTracks[i].backward == gCircuitTracks[k].backward && gCircuitTracks[i].mirror == gCircuitTracks[k].mirror) {
                            r = (gCircuitTracks[i].backward | (gCircuitTracks[i].mirror << 1)) + 1;
                            r = r % 4;
                            gCircuitTracks[i].backward = r & 1;
                            gCircuitTracks[i].mirror = (r >> 1) & 1;
                            break;
                        }
                    }
                    if (k == i) break;
                }
            }
            gCircuitNumTrackRaces[gCircuitTracks[i].trackNum]++;
        } else {
            gCircuitTracks[i].trackNum = i % n;
            gCircuitTracks[i].backward = i >= n;
        }
        r = irand(gCircuitFog[info->type].range);
        gCircuitTracks[i].fog = r < 4 ? 0 : r < 7 ? 1 : r < 9 ? 2 : 3;
        gCircuitTracks[i].wind = irand(gCircuitWind[info->type].range);
    }
    m = 6 - (gNumPlayers == 1 ? 0 : 2);
    for (i = 0; i < m; i++) {
        gCircuitCars[i].totalPoints = info->temp ? gTempCircuitPoints[gPlayers[gCircuitPlayer].selector][i] : 0;
        for (j = 0; j < info->track; j++) {
            if (info->temp && j < gTempCircuitFirstTrack[gPlayers[gCircuitPlayer].selector]) {
                gCircuitCars[i].places[j] = 6;
                gCircuitCars[i].points[j] = 0;
            } else {
                gCircuitCars[i].places[j] = 0;
                for (r = 0, x = j * 3; r < 3; r++, x++)
                    gCircuitCars[i].places[j] |= ((info->ranks[i][x >> 3] >> (x & 7)) & 1) << r;
                gCircuitCars[i].points[j] = gCircuitPoints[gCircuitCars[i].places[j]];
                gCircuitCars[i].totalPoints += gCircuitCars[i].points[j];
            }
        }
        for (; j < gCircuitSize; j++) {
            gCircuitCars[i].places[j] = 6;
            gCircuitCars[i].points[j] = 0;
        }
    }
    for (; i < 6; i++) {
        gCircuitCars[i].totalPoints = 0;
        for (j = 0; j < gCircuitSize; j++) {
            gCircuitCars[i].places[j] = 6;
            gCircuitCars[i].points[j] = 0;
        }
    }
    for (i = 0; i < 6; i++)
        idx[i] = i;
    for (i = 0; i < 5; i++)
        for (k = i + 1; k < 6; k++)
            if (gCircuitCars[idx[i]].totalPoints < gCircuitCars[idx[k]].totalPoints) {
                idx[i] ^= idx[k]; idx[k] ^= idx[i]; idx[i] ^= idx[k];
            }
    for (i = 0; i < 6; i++)
        gCircuitCars[idx[i]].totalPlace = i;
}

/* The circuit's current race slot, gCircuitTracks[gCircuitTrack], as the game reads it. Before ContinueCircuit has
 * set up a circuit's races gCircuitTrack can be -1 (the track select of a circuit not begun), and the game then read
 * the five bytes before the table, which on the PC are the bytes the N64's layout put there: the last byte of
 * audio_cursorSpeed as memory holds it (on a little-endian PC its top byte, where the N64 had its lowest), the circuit
 * player's number (gCircuitPlayer) and three bytes of padding, zero. That slot is built from those here, so the game
 * reads what it did. */
const CircuitTrackSlot *CurrentCircuitTrack(void) {
    static CircuitTrackSlot before;
    if (gCircuitTrack >= 0) {
        return &gCircuitTracks[gCircuitTrack];
    }
    before.trackNum = ((const u8 *)&audio_cursorSpeed)[sizeof audio_cursorSpeed - 1];
    before.backward = gCircuitPlayer;
    before.mirror = 0;
    before.fog = 0;
    before.wind = 0;
    return &before;
}
