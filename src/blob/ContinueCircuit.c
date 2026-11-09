#include "game/game.h"

typedef struct { u8 pad[6]; u8 flag6; u8 pad1; s8 mode; s8 lim; u8 pad2[2]; u32 seed; u8 pad3[4]; u8 bits[6 * 9]; } Info;
typedef struct { u8 pad[0x6F4]; Info info; } Sess;
typedef struct { void *next; void *prev; void *fileh; u8 pad_C[0x2C - 0xC]; Sess **s; } A;
typedef struct { u8 id; u8 b1; u16 h2; u8 b4[24]; u8 b28[24]; s16 skill[12]; } Rec;
typedef struct Slot { u8 a, b, c, d, e; } Slot;
typedef struct { u16 hi, lo; } Pair;
typedef struct { u8 pad0; u8 b1; u8 pad[0x46]; A **a; } InRec;
extern InRec gPlayers[];
VIEW(Rec[], gCircuitCars);
#define gCircuitCars gCircuitCars_view
VIEW(Slot[], gCircuitTracks);
#define gCircuitTracks gCircuitTracks_view

VIEW(s32[], gCircuitPoints);
#define gCircuitPoints gCircuitPoints_view
VIEW(Pair[], gCircuitFog);
#define gCircuitFog gCircuitFog_view
VIEW(Pair[], gCircuitWind);
#define gCircuitWind gCircuitWind_view

#define RI(e) gCircuitCars[(e) + gNumPlayers]

static u16 irand(u16 max) {
    return (u32)seeded_rand(&gCircuitSeed) % (max + 1);
}
void ContinueCircuit(void) {
    s32 n, i, k, j, r, cnt;
    s32 idx[6];
    s32 m;
    s32 x;
    Sess *v;
    Info *info;
    u8 pad[8];
    v = *((*gPlayers[gCircuitPlayer].a)->s);
    info = &v->info;
    gCircuitSeed = info->seed;
    n = gNumCircuitTracks[info->mode];
    if (expansion_pak_present == 0 && n >= 5) n = 5;
    if (info->mode == 3) gCircuitSize = n * 4;
    else gCircuitSize = n * 2;
    gCircuitTrack = (info->lim < gCircuitSize - 1) ? info->lim : gCircuitSize - 1;
    for (i = 0; i < n; i++)
        gCircuitNumTrackRaces[i] = 0;
    cnt = 6 - gNumPlayers;
    for (i = 0; i < cnt; i++) {
    again:
        RI(i).id = irand(54);
        for (j = 0; j < i; j++)
            if (RI(i).id == RI(j).id) goto again;
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
        if (info->mode == 3) {
            gCircuitTracks[i].a = irand(n - 1);
            gCircuitTracks[i].b = (u16)(seeded_rand(&gCircuitSeed) & 1);
            gCircuitTracks[i].c = irand(1);
            if (i != 0) {
                k = 0;
                while ((gCircuitTracks[i].a == gCircuitTracks[i - 1].a && k < n) || gCircuitNumTrackRaces[gCircuitTracks[i].a] == 4) {
                    gCircuitTracks[i].a++;
                    if (gCircuitTracks[i].a >= n) gCircuitTracks[i].a = 0;
                    k++;
                }
                for (;;) {
                    for (k = 0; k < i; k++) {
                        if (gCircuitTracks[i].a == gCircuitTracks[k].a && gCircuitTracks[i].b == gCircuitTracks[k].b && gCircuitTracks[i].c == gCircuitTracks[k].c) {
                            r = (gCircuitTracks[i].b | (gCircuitTracks[i].c << 1)) + 1;
                            r = r % 4;
                            gCircuitTracks[i].b = r & 1;
                            gCircuitTracks[i].c = (r >> 1) & 1;
                            break;
                        }
                    }
                    if (k == i) break;
                }
            }
            gCircuitNumTrackRaces[gCircuitTracks[i].a]++;
        } else {
            gCircuitTracks[i].a = i % n;
            gCircuitTracks[i].b = i >= n;
        }
        r = irand(gCircuitFog[info->mode].lo);
        gCircuitTracks[i].d = r < 4 ? 0 : r < 7 ? 1 : r < 9 ? 2 : 3;
        gCircuitTracks[i].e = irand(gCircuitWind[info->mode].lo);
    }
    m = 6 - (gNumPlayers == 1 ? 0 : 2);
    for (i = 0; i < m; i++) {
        gCircuitCars[i].h2 = info->flag6 ? gTempCircuitPoints[gPlayers[gCircuitPlayer].b1][i] : 0;
        for (j = 0; j < info->lim; j++) {
            if (info->flag6 && j < gTempCircuitFirstTrack[gPlayers[gCircuitPlayer].b1]) {
                gCircuitCars[i].b4[j] = 6;
                gCircuitCars[i].b28[j] = 0;
            } else {
                gCircuitCars[i].b4[j] = 0;
                for (r = 0, x = j * 3; r < 3; r++, x++)
                    gCircuitCars[i].b4[j] |= ((info->bits[i * 9 + (x >> 3)] >> (x & 7)) & 1) << r;
                gCircuitCars[i].b28[j] = gCircuitPoints[gCircuitCars[i].b4[j]];
                gCircuitCars[i].h2 += gCircuitCars[i].b28[j];
            }
        }
        for (; j < gCircuitSize; j++) {
            gCircuitCars[i].b4[j] = 6;
            gCircuitCars[i].b28[j] = 0;
        }
    }
    for (; i < 6; i++) {
        gCircuitCars[i].h2 = 0;
        for (j = 0; j < gCircuitSize; j++) {
            gCircuitCars[i].b4[j] = 6;
            gCircuitCars[i].b28[j] = 0;
        }
    }
    for (i = 0; i < 6; i++)
        idx[i] = i;
    for (i = 0; i < 5; i++)
        for (k = i + 1; k < 6; k++)
            if (gCircuitCars[idx[i]].h2 < gCircuitCars[idx[k]].h2) {
                idx[i] ^= idx[k]; idx[k] ^= idx[i]; idx[i] ^= idx[k];
            }
    for (i = 0; i < 6; i++)
        gCircuitCars[idx[i]].b1 = i;
}
