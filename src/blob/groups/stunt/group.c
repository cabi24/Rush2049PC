/* The stunt arena's scoring (MAT3 stunt.c): each frame every player's car is watched for flips, spins, rolls, air
 * time, two-wheel driving and wild combinations of them (CheckForStunt), each one adding a trick (AddStunt) to the
 * run, which the landing banks for its points and a crash loses (CrashedStunt). */
#include "game/game.h"

float fabsf(float);

/* A player's stunt in progress (MAT3 tStuntInfo, gStuntInfo[4], 0x7C bytes; include/game lays it out as bytes). */
typedef struct Ply {
    s32 flags;                   /* 0x00 active: the trackers running (0x1 on the two wheels of a side, 0x2 on the front
                                  * wheels, 0x4 on the back ones, 0x10 a spin, 0x20 a roll, 0x40 a flip, 0x80 the
                                  * combination, 0x100 air time; above, each axis's turn direction) */
    f32 stuntW[4][3];            /* 0x04 rotation snapshots and sums (MAT3 stuntW) */
    f32 t34, t38, t3C, t40, t44, t48, t4C, t50; /* 0x34 time[4]: each tracker's start and end times */
    f32 t54;                     /* 0x54 endTime */
    u8 pad58[0x24];              /* 0x58 initUV */
} Ply;
VIEW(Ply[], gStuntInfo);
#define gStuntInfo gStuntInfo_view

/* The car record (MAT3 tCarData; the PC's pc_car, host layout): the fields the stunts read. */
typedef struct PCar {
    u8 pad00[0xE8];
    s32 appearance;              /* 0x0E8 */
    u8 padEC[0x110 - 0xEC]; struct pc_visual visuals[21]; u8 padEC_308[0x358 - 0x308];
    s8 crash_state;              /* 0x358 */
    u8 pad359[0x368 - 0x359];
    f32 vx, vy, vz;              /* 0x368 stuntW: its rotation this step about its axes */
    u8 pad374[0x380 - 0x374]; void *input; u8 pad374_384[0x3B8 - 0x384];
} PCar;

/* The model record (MAT3 tModelData): the fields the stunts read. */
typedef struct HudRec {
    u8 pad000[0x3F8];
    s16 wing;                    /* 0x3F8 */
    u8 pad3FA[0x5EC - 0x3FA];
    f32 airdist[4];              /* 0x5EC each tyre's height off the ground */
    u8 pad5FC[0x61C - 0x5FC];
    u16 sviscode[4];             /* 0x61C each tyre's surface's visual code */
    u8 pad624[0x6C4 - 0x624];
    s16 moving_state;            /* 0x6C4 resurrect.moving_state: -1 unless being put back */
    u8 pad6C6[0x808 - 0x6C6];
} HudRec;

extern void *memset(void *s, s32 c, u32 n);

VIEW(f32[3], stunt_GCabsW);
#define stunt_GCabsW stunt_GCabsW_view

void AddStunt(s32 idx, s32 code);

/* Adds trick `code` to player `idx`'s run: 0-4 and 7-8 (flips, spins, rolls ...) 5 points, 5 and 6 (the combination,
 * two wheels) 3, 9 (air time) 5; a new kind of trick in a run of more than one adds to the bonus multiplier. Code 11
 * is the landing: unless crashed, the run's bonuses (a clean single trick or two with air: 15 or 25 and its name; a
 * wing bonus by how long the wing showed; 3 to 10 kinds of trick: 10 to 1000) multiply its points, which bank into
 * the total and the best; the HUD's copies take the run, which then clears. */
void AddStunt(s32 idx, s32 code)
{
    s32 hits;
    tStuntPlayer *p;
    s32 i;

    p = &gStuntPlayer[idx];
    p->done = 0;
    p->show = 1;
    p->time = gModelTime;
    if (code != 11) {
        if (p->stunts[code] == 0 && p->numstunts > 0) {
            p->bonus++;
            p->showBonus++;
        }
        p->stunts[code]++;
    }
    if (code != p->lastStuntType) {
        p->lastStuntType = code;
        p->numstunts++;
    }
    switch (code) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 7:
    case 8:
        i = 5;
        break;
    case 5:
    case 6:
        i = 3;
        break;
    case 9:
        i = 5;
        break;
    case 11:
        p->done = 1;
        i = 0;
        break;
    }
    p->points += i;
    p->bonusHud = p->bonus;
    if (code == 11) {
        hits = 0;
        if (p->crash == 0) {
            for (i = 0; i < 10; i++) {
                if (p->stunts[i] > 0) {
                    hits++;
                }
                p->totalStunts[i] += p->stunts[i];
            }
            if ((p->stunts[0] != 0 || p->stunts[1] != 0 || p->stunts[2] != 0 || p->stunts[3] != 0 || p->stunts[4] != 0) && p->wingTime <= 1.0f && (hits == 1 || (hits == 2 && p->stunts[9] != 0))) {
                if (p->stunts[4] != 0) {
                    p->doneBonus = 11;
                    p->bonus += 15;
                } else if (p->stunts[0] != 0 || p->stunts[1] != 0) {
                    p->bonus += 25;
                    p->doneBonus = 13;
                } else {
                    p->bonus += 25;
                    p->doneBonus = 12;
                }
            } else if (p->stunts[9] != 0 || p->stunts[5] != 0 || p->stunts[0] != 0 || p->stunts[1] != 0 || p->stunts[2] != 0 || p->stunts[3] != 0 || p->stunts[4] != 0) {
                if (p->wingTime == 0.0f) {
                    p->bonus += 7;
                    p->wingBonus = 1;
                } else if (p->wingTime <= 3.0f) {
                    p->bonus += 2;
                    p->wingBonus = 2;
                }
            }
            if (hits >= 3) {
                p->doneBonus = hits;
                switch (hits) {
                case 3: p->bonus += 10; break;
                case 4: p->bonus += 20; break;
                case 5: p->bonus += 40; break;
                case 6: p->bonus += 60; break;
                case 7: p->bonus += 80; break;
                case 8: p->bonus += 100; break;
                case 9: p->bonus += 500; break;
                case 10: p->bonus += 1000; break;
                }
            }
            if (p->bonus >= 2) {
                p->points = p->points * p->bonus;
            }
            p->totalPoints += p->points;
            if (p->maxStuntPoints < p->points) {
                p->maxStuntPoints = p->points;
            }
        }
        p->doneBonusHud = p->doneBonus;
        p->pointsHud = p->points;
        p->wingBonusHud = p->wingBonus;
        for (i = 0; i < 10; i++) {
            p->stuntsHud[i] = p->stunts[i];
            p->stunts[i] = 0;
        }
        p->doneBonus = 0;
        p->wingBonus = 0;
        p->bonus = 1;
        p->showBonus = 0;
        p->numstunts = 0;
        p->points = 0;
        p->wingTime = 0.0f;
        memset(&gStuntInfo[idx], 0, 0x7C);
        gStuntInfo[idx].flags |= 0x15000000;
    }
}



/* The flip tracker (flag 0x40) of player `idx`: started when the car turns mostly about its x axis, it sums the
 * turn; too much about the other axes aborts it; a flip's worth (within flip_window_min..max, or past
 * flip_landing_min once the car turns back, lands or is touched by the road: appearance 0x100000) adds a forward
 * (0) or backward (1) flip and unwinds the sum. CheckSpin (0x10, y: 4) and CheckRoll (0x20, z: 2 and 3) are its
 * siblings. stunt_j counts the tyres on the ground, stunt_chW0..2 say an axis's turn changed direction. */
void CheckFlip(s32 idx) {
    Ply *p;
    PCar *c;
    f32 v[3];
    s32 fl;

    p = &gStuntInfo[idx];
    fl = p->flags;
    if (fl & 0x40) {
        if (stunt_chW0 != 0 || stunt_j >= 3) {
            p->flags = fl & ~0x40;
        } else {
            c = (PCar *)(uintptr_t)PC_CAR(idx);
            p->stuntW[1][0] += c->vx;
            p->stuntW[1][1] += c->vy;
            p->stuntW[1][2] += c->vz;
        }
        v[0] = fabsf(p->stuntW[1][0]);
        v[1] = fabsf(p->stuntW[1][1]);
        v[2] = fabsf(p->stuntW[1][2]);
        if (flip_abort_angle < v[1] || flip_abort_angle < v[2]) {
            p->flags &= ~0x40;
            return;
        }
        if ((flip_window_min < v[0] && v[0] < flip_window_max) || ((stunt_chW0 != 0 || stunt_j > 0 || ((c = (PCar *)(uintptr_t)PC_CAR(idx))->appearance & 0x100000)) && flip_landing_min < v[0])) {
            p->flags &= ~0x80;
            if (p->stuntW[1][0] > 0.0f) {
                AddStunt(idx, 0);
                p->stuntW[1][0] += flip_unwind_negative;
            } else {
                AddStunt(idx, 1);
                p->stuntW[1][0] += flip_unwind_positive;
            }
            p->stuntW[1][2] = 0.0f;
            p->stuntW[1][1] = 0.0f;
        }
    } else if (stunt_okToStart != 0 && stunt_GCabsW[1] < stunt_GCabsW[0] && stunt_GCabsW[2] < stunt_GCabsW[0]) {
        p->flags = fl | 0x40;
        c = (PCar *)(uintptr_t)PC_CAR(idx);
        p->stuntW[1][0] = c->vx;
        p->stuntW[1][1] = c->vy;
        p->stuntW[1][2] = c->vz;
    }
}

/* The spin tracker (flag 0x10, about y): as CheckFlip. */
void CheckSpin(s32 idx) {
    Ply *p;
    PCar *c;
    f32 v[3];
    f32 d;
    s32 fl;

    p = &gStuntInfo[idx];
    fl = p->flags;
    if (fl & 0x10) {
        if (stunt_chW1 != 0 || stunt_j >= 3) {
            p->flags = fl & ~0x10;
        } else {
            c = (PCar *)(uintptr_t)PC_CAR(idx);
            p->stuntW[2][0] += c->vx;
            p->stuntW[2][1] += c->vy;
            p->stuntW[2][2] += c->vz;
        }
        v[0] = fabsf(p->stuntW[2][0]);
        v[1] = fabsf(p->stuntW[2][1]);
        v[2] = fabsf(p->stuntW[2][2]);
        if (spin_abort_angle < v[0] || spin_abort_angle < v[2]) {
            p->flags &= ~0x10;
            return;
        }
        if ((spin_window_min < v[1] && v[1] < spin_window_max) || ((stunt_chW1 != 0 || stunt_j > 0 || ((c = (PCar *)(uintptr_t)PC_CAR(idx))->appearance & 0x100000)) && spin_landing_min < v[1])) {
            p->flags &= ~0x80;
            AddStunt(idx, 4);
            d = (p->stuntW[2][1] > 0.0f) ? spin_unwind_negative : spin_unwind_positive;
            p->stuntW[2][2] = 0.0f;
            p->stuntW[2][0] = 0.0f;
            p->stuntW[2][1] = p->stuntW[2][1] + d;
        }
    } else if (stunt_okToStart != 0 && stunt_GCabsW[0] < stunt_GCabsW[1] && stunt_GCabsW[2] < stunt_GCabsW[1]) {
        p->flags = fl | 0x10;
        c = (PCar *)(uintptr_t)PC_CAR(idx);
        p->stuntW[2][0] = c->vx;
        p->stuntW[2][1] = c->vy;
        p->stuntW[2][2] = c->vz;
    }
}

/* The roll tracker (flag 0x20, about z, both ways): as CheckFlip. */
void CheckRoll(s32 idx) {
    Ply *p;
    PCar *c;
    f32 v[3];
    s32 fl;

    p = &gStuntInfo[idx];
    fl = p->flags;
    if (fl & 0x20) {
        if (stunt_chW2 != 0 || stunt_j >= 3) {
            p->flags = fl & ~0x20;
        } else {
            c = (PCar *)(uintptr_t)PC_CAR(idx);
            p->stuntW[0][0] += c->vx;
            p->stuntW[0][1] += c->vy;
            p->stuntW[0][2] += c->vz;
        }
        v[0] = fabsf(p->stuntW[0][0]);
        v[1] = fabsf(p->stuntW[0][1]);
        v[2] = fabsf(p->stuntW[0][2]);
        if (roll_abort_angle_x < v[0] || roll_abort_angle_y < v[1]) {
            p->flags &= ~0x20;
            return;
        }
        if ((roll_window_min < v[2] && v[2] < roll_window_max) || ((stunt_chW2 != 0 || stunt_j > 0 || (PC_CAR_FLAGS(idx) & 0x100000)) && roll_landing_min < v[2])) {
            p->flags &= ~0x80;
            if (p->stuntW[0][2] > 0.0f) {
                AddStunt(idx, 3);
                p->stuntW[0][2] += roll_unwind_negative;
            } else {
                AddStunt(idx, 2);
                p->stuntW[0][2] += roll_unwind_positive;
            }
            p->stuntW[0][1] = 0.0f;
            p->stuntW[0][0] = 0.0f;
        }
    } else if (stunt_okToStart != 0 && stunt_GCabsW[1] < stunt_GCabsW[2] && stunt_GCabsW[0] < stunt_GCabsW[2]) {
        p->flags = fl | 0x20;
        c = (PCar *)(uintptr_t)PC_CAR(idx);
        p->stuntW[0][0] = c->vx;
        p->stuntW[0][1] = c->vy;
        p->stuntW[0][2] = c->vz;
    }
}

/* The back-wheels tracker (flag 4, tricks 7): started with only the back tyres down, the front ones well up; when it
 * ends, a trick for each 0.15 seconds of it past the first quarter second. Check2WheelFront (flag 2, tricks 8) is
 * its mirror. */
void Check2WheelBack(s32 idx) {
    Ply *p;
    HudRec *h;
    s32 fl;
    f32 r;
    f32 dt;
    f32 zero;

    p = &gStuntInfo[idx];
    h = PC_MODEL(idx);
    fl = p->flags;
    if (fl & 4) {
        if (stunt_j >= 3 || stunt_f > 0 || h->airdist[2] > 0.75f || h->airdist[3] > 0.75f || h->airdist[0] < 1.0f || h->airdist[1] < 1.0f) {
            p->flags = fl & ~4;
            p->t50 = gModelTime;
            r = p->t50 - p->t4C;
            if (r > 0.25f) {
                r -= 0.25f;
                while (r > 0.0f) {
                    AddStunt(idx, 7);
                    r -= 0.15f;
                }
            }
        }
    } else if ((stunt_j == 2 && stunt_b == 2) || ((h->airdist[2] < 0.75f || h->airdist[3] < 0.75f) && h->airdist[0] > 2.5f && h->airdist[1] > 2.5f)) {
        p->flags = fl | 4;
        p->t4C = gModelTime;
        p->t50 = -1.0f;
    }
}

/* The front-wheels tracker (flag 2, tricks 8): Check2WheelBack's mirror. */
void Check2WheelFront(s32 idx) {
    Ply *p;
    HudRec *h;
    s32 fl;
    f32 r;
    f32 dt;
    f32 zero;

    p = &gStuntInfo[idx];
    h = PC_MODEL(idx);
    fl = p->flags;
    if (fl & 2) {
        if (stunt_j >= 3 || stunt_b > 0 || h->airdist[0] > 0.75f || h->airdist[1] > 0.75f || h->airdist[2] < 1.0f || h->airdist[3] < 1.0f) {
            p->flags = fl & ~2;
            p->t48 = gModelTime;
            r = p->t48 - p->t44;
            if (r > 0.25f) {
                r -= 0.25f;
                while (r > 0.0f) {
                    AddStunt(idx, 8);
                    r -= 0.15f;
                }
            }
        }
    } else if ((stunt_j == 2 && stunt_f == 2) || ((h->airdist[0] < 0.75f || h->airdist[1] < 0.75f) && h->airdist[2] > 2.5f) || h->airdist[3] > 2.5f) {
        p->flags = fl | 2;
        p->t44 = gModelTime;
        p->t48 = -1.0f;
    }
}

/* Player `idx` crashed: the run lands without its points (AddStunt 11 with crash set) and the trackers clear. */
void CrashedStunt(s32 idx) {
    gStuntPlayer[idx].crash = 1;
    if (gStuntPlayer[idx].numstunts != 0) {
        AddStunt(idx, 11);
    }
    memset(&gStuntInfo[idx], 0, 0x7C);
    gStuntInfo[idx].flags |= 0x15000000;
}
/* The trackers below are separate functions in this file, called by CheckForStunt; the game also has empty
 * Check2WheelSide, CheckAirTime and CheckMulti (src/blob/*.c, reached through the function table), so these take
 * pc_ names (docs/port/PROTOTYPES.md). */
#define Check2WheelSide pc_Check2WheelSide
#define CheckAirTime pc_CheckAirTime
#define CheckMulti pc_CheckMulti

/* The two-wheels-on-one-side tracker (flag 1, tricks 6), as Check2WheelBack. */
void Check2WheelSide(s32 idx) {
    Ply *p;
    f32 r;

    p = &gStuntInfo[idx];
    if (p->flags & 1) {
        if (stunt_j >= 3 || stunt_j < 2) {
            p->t40 = gModelTime;
            r = p->t40 - p->t3C;
            p->flags &= ~1;
            if (r > 0.25f) {
                r -= 0.25f;
                while (r > 0.0f) {
                    AddStunt(idx, 6);
                    r -= 0.15f;
                }
            }
        }
    } else if (stunt_j == 2 && (stunt_l == 2 || stunt_r == 2)) {
        p->flags |= 1;
        p->t40 = -1.0f;
        p->t3C = gModelTime;
    }
}

/* The air time tracker (flag 0x100, tricks 9): a trick for each second in the air past 5. */
void CheckAirTime(s32 idx) {
    Ply *p;
    f32 r;

    p = &gStuntInfo[idx];
    if (p->flags & 0x100) {
        if (stunt_j > 0 || (PC_CAR_FLAGS(idx) & 0x100000)) {
            p->t38 = gModelTime;
            r = p->t38 - p->t34;
            p->flags &= ~0x100;
            while (r > 5.0f) {
                AddStunt(idx, 9);
                r -= 1.0f;
            }
        }
    } else if (stunt_j == 0) {
        p->flags |= 0x100;
        p->t38 = -1.0f;
        p->t34 = gModelTime;
    }
}

/* The combination tracker (flag 0x80, trick 5): turns about two axes or more, more than 27 degrees each and 180 in
 * all, while in the air. */
void CheckMulti(s32 idx) {
    Ply *p;
    PCar *c;
    s32 n;
    f32 s;

    p = &gStuntInfo[idx];
    c = PC_CAR(idx);
    if (p->flags & 0x80) {
        n = 0;
        if (stunt_j >= 3) {
            p->flags &= ~0x80;
        } else if (stunt_j == 0) {
            p->stuntW[3][0] += fabsf(c->vx);
            p->stuntW[3][1] += fabsf(c->vy);
            p->stuntW[3][2] += fabsf(c->vz);
        }
        if (0.4712389f < p->stuntW[3][0]) {
            n = 1;
        }
        if (0.4712389f < p->stuntW[3][1]) {
            n++;
        }
        if (0.4712389f < p->stuntW[3][2]) {
            n++;
        }
        if (n >= 2 && 3.1415927f <= (s = p->stuntW[3][2] + (p->stuntW[3][0] + p->stuntW[3][1]))) {
            p->flags &= ~0x70;
            AddStunt(idx, 5);
            p->stuntW[3][0] = 0.0f;
            p->stuntW[3][1] = 0.0f;
            p->stuntW[3][2] = 0.0f;
        }
    } else if (stunt_okToStart != 0) {
        p->flags |= 0x80;
        p->stuntW[3][0] = fabsf(c->vx);
        p->stuntW[3][1] = fabsf(c->vy);
        p->stuntW[3][2] = fabsf(c->vz);
    }
}

/* One frame of the stunt arena: once coasting with no stunt under way for 2.69 seconds the stunts are done
 * (gStuntsDone). For each player: the wing's time during a stunt; a crash or a respawn clears the trackers; else
 * the tyres on the ground (stunt_j, l, r, f, b), the turn about each axis and its changes of direction, the
 * trackers, and a landing a second after the air time ended banks the run (AddStunt 11). */
void CheckForStunt(void) {
    Ply *ply;
    PCar *car;
    HudRec *hud;
    s32 i;
    s32 ok;
    f32 *pdt;

    pdt = (f32 *)(uintptr_t)&gFrameSeconds;
    if (gCoastFlag != 0) {
        ok = 1;
        for (i = 0; i < gNumPlayers; i++) {
            if (gStuntInfo[i].flags & 0x1F7) {
                stunt_eTime = 0.0f;
                ok = 0;
            }
        }
        if (ok != 0) {
            *(f32 *)(uintptr_t)&stunt_eTime += *pdt;
            if (2.69f <= stunt_eTime) {
                gStuntsDone = 1;
            }
        }
    }
    for (i = 0; i < gNumPlayers; i++) {
        ply = &gStuntInfo[i];
        car = PC_CAR(i);
        hud = PC_MODEL(i);
        if ((ply->flags & 0x1F0) && hud->wing != 0) {
            gStuntPlayer[i].wingTime += *pdt;
        }
        if ((ply->flags & 0xC08FFFFF) && (car->crash_state != 0 || hud->moving_state != -1)) {
            ply->flags = 0x15000000;
        } else {
            stunt_j = stunt_l = stunt_r = stunt_f = stunt_b = 0;
            if (hud->sviscode[0] != 8) {
                stunt_j++;
                stunt_l++;
                stunt_f++;
            }
            if (hud->sviscode[1] != 8) {
                stunt_j++;
                stunt_r++;
                stunt_f++;
            }
            if (hud->sviscode[2] != 8) {
                stunt_j++;
                stunt_l++;
                stunt_b++;
            }
            if (hud->sviscode[3] != 8) {
                stunt_j++;
                stunt_r++;
                stunt_b++;
            }
            stunt_okToStart = stunt_j < 3 && car->crash_state == 0 && hud->moving_state == -1;
            stunt_GCabsW[0] = fabsf(car->vx);
            stunt_GCabsW[1] = fabsf(car->vy);
            stunt_GCabsW[2] = fabsf(car->vz);
            stunt_chW0 = stunt_chW1 = stunt_chW2 = 0;
            if ((ply->flags & 0x01000000) && car->vx < 0.0f) {
                ply->flags &= 0xFEFFFFFF;
                ply->flags |= 0x02000000;
                stunt_chW0 = 1;
            } else if (ply->flags & 0x02000000) {
                if (car->vx >= 0.0f) {
                    ply->flags &= 0xFDFFFFFF;
                    ply->flags |= 0x01000000;
                    stunt_chW0 = 1;
                }
            }
            if ((ply->flags & 0x04000000) && car->vy < 0.0f) {
                ply->flags &= 0xFBFFFFFF;
                ply->flags |= 0x08000000;
                stunt_chW1 = 1;
            } else if (ply->flags & 0x08000000) {
                if (car->vy >= 0.0f) {
                    ply->flags &= 0xF7FFFFFF;
                    ply->flags |= 0x04000000;
                    stunt_chW1 = 1;
                }
            }
            if ((ply->flags & 0x10000000) && car->vz < 0.0f) {
                ply->flags &= 0xEFFFFFFF;
                ply->flags |= 0x20000000;
                stunt_chW2 = 1;
            } else if (ply->flags & 0x20000000) {
                if (car->vz >= 0.0f) {
                    ply->flags &= 0xDFFFFFFF;
                    ply->flags |= 0x10000000;
                    stunt_chW2 = 1;
                }
            }
            CheckRoll(i);
            CheckSpin(i);
            CheckFlip(i);
            CheckMulti(i);
            CheckAirTime(i);
            Check2WheelSide(i);
            Check2WheelFront(i);
            Check2WheelBack(i);
            if (ply->flags & 0x1F7) {
                ply->t54 = -1.0f;
            } else if (stunt_j >= 3) {
                if (gStuntPlayer[i].numstunts == 0) {
                    gStuntPlayer[i].wingTime = 0.0f;
                }
                if (ply->t54 == -1.0f && gStuntPlayer[i].numstunts != 0) {
                    ply->t54 = gModelTime;
                }
            }
            if (stunt_j >= 3 && gStuntPlayer[i].numstunts != 0) {
                if (ply->t38 != -1.0f && (gModelTime - ply->t38) > 1) {
                    ply->t38 = -1.0f;
                    AddStunt(i, 11);
                }
            }
            car->appearance &= 0xFFEFFFFF;
        }
    }
}

