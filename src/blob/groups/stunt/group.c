#include "game/game.h"

float fabsf(float); /* : m2c emits fabsf for abs.s; undeclared it was an implicit int call */
#pragma intrinsic (fabsf)

/* : trick-score records, hand-derived from the retail words */
typedef struct Stunt {           /* gStuntPlayer[player], 0x78 bytes */
    s16 active;                  /* 0x00 */
    s16 done;                    /* 0x02 */
    f32 time;                    /* 0x04 */
    s32 best;                    /* 0x08 */
    s32 cur;                     /* 0x0C */
    s32 last;                    /* 0x10 */
    s32 total;                   /* 0x14 */
    s32 count;                   /* 0x18 */
    s32 lastcode;                /* 0x1C */
    s16 cnt[10];                 /* 0x20 */
    s16 prevcnt[10];             /* 0x34 */
    s16 hist[10];                /* 0x48 */
    u8 pad5C[4];
    f32 f60;                     /* 0x60 */
    s8 s64;
    s8 s65;
    s8 s66;
    s8 s67;
    s32 mult;                    /* 0x68 */
    s32 prev;                    /* 0x6C */
    s32 s70;
    s32 flag;                    /* 0x74 */
} Stunt;
VIEW(Stunt[], gStuntPlayer);
#define gStuntPlayer gStuntPlayer_view
typedef struct Ply {             /* gStuntInfo[player], 0x7C bytes */
    s32 flags;                   /* 0x00 */
    f32 a04, a08, a0C;
    f32 a10, a14, a18;
    f32 a1C, a20, a24;
    f32 a28, a2C, a30;
    f32 t34, t38, t3C, t40, t44, t48, t4C, t50, t54;
    u8 pad58[0x24];
} Ply;
VIEW(Ply[], gStuntInfo);
#define gStuntInfo gStuntInfo_view
typedef struct PCar {            /* gGameCar[], 0x3B8 bytes */
    u8 pad00[0xE8];
    s32 fE8;
    u8 padEC[0x110 - 0xEC]; struct pc_visual visuals[21]; u8 padEC_308[0x358 - 0x308];
    s8 f358;
    u8 pad359[0x368 - 0x359];
    f32 vx, vy, vz;
    u8 pad374[0x380 - 0x374]; void *input; u8 pad374_384[0x3B8 - 0x384];
} PCar;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
typedef struct HudRec {          /* model[], 0x808 bytes */
    u8 pad000[0x3F8];
    s16 f3F8;
    u8 pad3FA[0x5EC - 0x3FA];
    f32 f5EC, f5F0, f5F4, f5F8;
    u8 pad5FC[0x61C - 0x5FC];
    u16 s61C, s61E, s620, s622;
    u8 pad624[0x6C4 - 0x624];
    s16 f6C4;
    u8 pad6C6[0x808 - 0x6C6];
} HudRec;

extern void *memset(void *s, s32 c, u32 n);

VIEW(f32[3], stunt_GCabsW);
#define stunt_GCabsW stunt_GCabsW_view

void AddStunt();

/* ---- unit: AddStunt and its callers (agent) ----
 * Stunt / trick scoring (labels are historical; nothing here is a camera):
 *   AddStunt(idx, code)  record one trick event for player idx (code 11 = landing: bank the combo)
 *   Check2WheelBack / Check2WheelFront / CheckFlip / CheckSpin / CheckRoll   per-player trackers
 *   CrashedStunt(idx)        end of run: bank and clear
 *   CheckForStunt()    per-frame driver over all players
 * CheckFlip/26C4/2944 are the three already-locked members of src/blob/groups/stunt,
 * unchanged except that their calls now pass (idx, code) -- see the note on AddStunt.
 * No stand-ins: every function has its real callers in this unit.
 */
/* AddStunt (0x800C1B60, 297 words).
 * IPA: idx arrives in $t1, code in $a0; $s0 is used without being saved.
 * Three things were needed beyond the earlier hand-written body:
 *  - the source parameter order is (idx, code): every caller sets $t1 before $a0
 *    (`move t1,s1; jal; li a0,7`), and callers evaluate arguments in source order;
 *  - `hits` is not a local but a third, never-passed parameter (old-style definition, callers pass two
 *    arguments).  Retail's frame has exactly two local words (p, i); as a local, hits is a third and the
 *    frame is 40 instead of 32; reusing `code` for it moves the incoming parameter from $a0 to $a1;
 *  - the points to add are kept in `i` (the later loop counter): one variable, one register ($v1),
 *    written to its home in every switch arm because the default arm leaves it unset.
 * The two jump tables (0x80123E94, 0x80123EC4) are this function's own .rodata: 4 relocations unverified.
 */
void AddStunt(idx, code, hits)
s32 idx;
s32 code;
s32 hits;
{
    Stunt *p;
    s32 i;

    p = &gStuntPlayer[idx];
    p->flag = 0;
    p->active = 1;
    p->time = gModelTime;
    if (code != 11) {
        if (p->cnt[code] == 0 && p->count > 0) {
            p->mult++;
            p->s70++;
        }
        p->cnt[code]++;
    }
    if (code != p->lastcode) {
        p->lastcode = code;
        p->count++;
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
        p->flag = 1;
        i = 0;
        break;
    }
    p->cur += i;
    p->prev = p->mult;
    if (code == 11) {
        hits = 0;
        if (p->done == 0) {
            for (i = 0; i < 10; i++) {
                if (p->cnt[i] > 0) {
                    hits++;
                }
                p->hist[i] += p->cnt[i];
            }
            if ((p->cnt[0] != 0 || p->cnt[1] != 0 || p->cnt[2] != 0 || p->cnt[3] != 0 || p->cnt[4] != 0) && p->f60 <= 1.0f && (hits == 1 || (hits == 2 && p->cnt[9] != 0))) {
                if (p->cnt[4] != 0) {
                    p->s66 = 11;
                    p->mult += 15;
                } else if (p->cnt[0] != 0 || p->cnt[1] != 0) {
                    p->mult += 25;
                    p->s66 = 13;
                } else {
                    p->mult += 25;
                    p->s66 = 12;
                }
            } else if (p->cnt[9] != 0 || p->cnt[5] != 0 || p->cnt[0] != 0 || p->cnt[1] != 0 || p->cnt[2] != 0 || p->cnt[3] != 0 || p->cnt[4] != 0) {
                if (p->f60 == 0.0f) {
                    p->mult += 7;
                    p->s64 = 1;
                } else if (p->f60 <= 3.0f) {
                    p->mult += 2;
                    p->s64 = 2;
                }
            }
            if (hits >= 3) {
                p->s66 = hits;
                switch (hits) {
                case 3: p->mult += 10; break;
                case 4: p->mult += 20; break;
                case 5: p->mult += 40; break;
                case 6: p->mult += 60; break;
                case 7: p->mult += 80; break;
                case 8: p->mult += 100; break;
                case 9: p->mult += 500; break;
                case 10: p->mult += 1000; break;
                }
            }
            if (p->mult >= 2) {
                p->cur = p->cur * p->mult;
            }
            p->total += p->cur;
            if (p->best < p->cur) {
                p->best = p->cur;
            }
        }
        p->s67 = p->s66;
        p->last = p->cur;
        p->s65 = p->s64;
        for (i = 0; i < 10; i++) {
            p->prevcnt[i] = p->cnt[i];
            p->cnt[i] = 0;
        }
        p->s66 = 0;
        p->s64 = 0;
        p->mult = 1;
        p->s70 = 0;
        p->count = 0;
        p->cur = 0;
        p->f60 = 0.0f;
        memset(&gStuntInfo[idx], 0, 0x7C);
        gStuntInfo[idx].flags |= 0x15000000;
    }
}



/* AddStunt and the callers below are hand-written from the retail words */
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
            p->a10 += c->vx;
            p->a14 += c->vy;
            p->a18 += c->vz;
        }
        v[0] = fabsf(p->a10);
        v[1] = fabsf(p->a14);
        v[2] = fabsf(p->a18);
        if (flip_abort_angle < v[1] || flip_abort_angle < v[2]) {
            p->flags &= ~0x40;
            return;
        }
        if ((flip_window_min < v[0] && v[0] < flip_window_max) || ((stunt_chW0 != 0 || stunt_j > 0 || ((c = (PCar *)(uintptr_t)PC_CAR(idx))->fE8 & 0x100000)) && flip_landing_min < v[0])) {
            p->flags &= ~0x80;
            if (p->a10 > 0.0f) {
                AddStunt(idx, 0);
                p->a10 += flip_unwind_negative;
            } else {
                AddStunt(idx, 1);
                p->a10 += flip_unwind_positive;
            }
            p->a18 = 0.0f;
            p->a14 = 0.0f;
        }
    } else if (stunt_okToStart != 0 && stunt_GCabsW[1] < stunt_GCabsW[0] && stunt_GCabsW[2] < stunt_GCabsW[0]) {
        p->flags = fl | 0x40;
        c = (PCar *)(uintptr_t)PC_CAR(idx);
        p->a10 = c->vx;
        p->a14 = c->vy;
        p->a18 = c->vz;
    }
}

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
            p->a1C += c->vx;
            p->a20 += c->vy;
            p->a24 += c->vz;
        }
        v[0] = fabsf(p->a1C);
        v[1] = fabsf(p->a20);
        v[2] = fabsf(p->a24);
        if (spin_abort_angle < v[0] || spin_abort_angle < v[2]) {
            p->flags &= ~0x10;
            return;
        }
        if ((spin_window_min < v[1] && v[1] < spin_window_max) || ((stunt_chW1 != 0 || stunt_j > 0 || ((c = (PCar *)(uintptr_t)PC_CAR(idx))->fE8 & 0x100000)) && spin_landing_min < v[1])) {
            p->flags &= ~0x80;
            AddStunt(idx, 4);
            d = (p->a20 > 0.0f) ? spin_unwind_negative : spin_unwind_positive;
            p->a24 = 0.0f;
            p->a1C = 0.0f;
            p->a20 = p->a20 + d;
        }
    } else if (stunt_okToStart != 0 && stunt_GCabsW[0] < stunt_GCabsW[1] && stunt_GCabsW[2] < stunt_GCabsW[1]) {
        p->flags = fl | 0x10;
        c = (PCar *)(uintptr_t)PC_CAR(idx);
        p->a1C = c->vx;
        p->a20 = c->vy;
        p->a24 = c->vz;
    }
}

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
            p->a04 += c->vx;
            p->a08 += c->vy;
            p->a0C += c->vz;
        }
        v[0] = fabsf(p->a04);
        v[1] = fabsf(p->a08);
        v[2] = fabsf(p->a0C);
        if (roll_abort_angle_x < v[0] || roll_abort_angle_y < v[1]) {
            p->flags &= ~0x20;
            return;
        }
        if ((roll_window_min < v[2] && v[2] < roll_window_max) || ((stunt_chW2 != 0 || stunt_j > 0 || (PC_CAR_FLAGS(idx) & 0x100000)) && roll_landing_min < v[2])) {
            p->flags &= ~0x80;
            if (p->a0C > 0.0f) {
                AddStunt(idx, 3);
                p->a0C += roll_unwind_negative;
            } else {
                AddStunt(idx, 2);
                p->a0C += roll_unwind_positive;
            }
            p->a08 = 0.0f;
            p->a04 = 0.0f;
        }
    } else if (stunt_okToStart != 0 && stunt_GCabsW[1] < stunt_GCabsW[2] && stunt_GCabsW[0] < stunt_GCabsW[2]) {
        p->flags = fl | 0x20;
        c = (PCar *)(uintptr_t)PC_CAR(idx);
        p->a04 = c->vx;
        p->a08 = c->vy;
        p->a0C = c->vz;
    }
}

/* Check2WheelBack (0x800C2004, 130 words): two-wheel (side) tracker; Check2WheelFront is its mirror image.
 * idx is an IPA parameter in $s1.  The repeat loop is a plain `while (r > 0.0f)` with the literal 0.15f:
 * uopt hoists zero and 0.15f into $f22/$f24 by itself and the 0.25f pair lands in $f0.  0.15f is own
 * .rodata (0x80123EE4): 2 relocations unverified (an extern is not hoisted out of the loop). */
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
        if (stunt_j >= 3 || stunt_f > 0 || h->f5F4 > 0.75f || h->f5F8 > 0.75f || h->f5EC < 1.0f || h->f5F0 < 1.0f) {
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
    } else if ((stunt_j == 2 && stunt_b == 2) || ((h->f5F4 < 0.75f || h->f5F8 < 0.75f) && h->f5EC > 2.5f && h->f5F0 > 2.5f)) {
        p->flags = fl | 4;
        p->t4C = gModelTime;
        p->t50 = -1.0f;
    }
}

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
        if (stunt_j >= 3 || stunt_b > 0 || h->f5EC > 0.75f || h->f5F0 > 0.75f || h->f5F4 < 1.0f || h->f5F8 < 1.0f) {
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
    } else if ((stunt_j == 2 && stunt_f == 2) || ((h->f5EC < 0.75f || h->f5F0 < 0.75f) && h->f5F4 > 2.5f) || h->f5F8 > 2.5f) {
        p->flags = fl | 2;
        p->t44 = gModelTime;
        p->t48 = -1.0f;
    }
}

/* CrashedStunt (0x800C3578, 37 words): no locals at all -- with a named `p` the pointer saved around
 * memset lands at 32(sp) instead of 36(sp). */
void CrashedStunt(s32 idx) {
    gStuntPlayer[idx].done = 1;
    if (gStuntPlayer[idx].count != 0) {
        AddStunt(idx, 11);
    }
    memset(&gStuntInfo[idx], 0, 0x7C);
    gStuntInfo[idx].flags |= 0x15000000;
}
/* CheckForStunt: NOT matched. */
/* The manifest owners of Check2WheelSide, CheckAirTime and CheckMulti are empty deleted-procedure
 * stubs (src/blob/Check2WheelSide.c, CheckAirTime.c, CheckMulti.c), which this group's calls bind
 * to on PC. The bodies written here get their own names (docs/port/PROTOTYPES.md). */
#define Check2WheelSide pc_Check2WheelSide
#define CheckAirTime pc_CheckAirTime
#define CheckMulti pc_CheckMulti
/* CheckForStunt (0x800C2BE0, 614 words) -- strict MATCH in this real -O3 unit.
 * Per-frame stunt/trick driver over all players (nothing here is a camera; the label is historical):
 * idle-timer bookkeeping (stunt_eTime, 2.69 s), wheel-contact counters from the HUD record, axis-flip
 * detection on the car velocity, then the per-axis trackers (CheckRoll/26C4/2430, called with idx in
 * $t1 by IPA), three more trackers that umerge inlined (below), the side trackers Check2WheelFront/2004
 * (idx in $s1), and the landing check that banks the combo (AddStunt(i, 11)).
 *
 * Source structure that the match depends on (each verified by compile):
 *  - Check2WheelSide / CheckAirTime / CheckMulti are real functions inlined by umerge (one call site,
 *    not kept): they leave the three caller-less `jr ra; nop` stubs at 0x800C2418/2420/2428, and retail
 *    calls run in reverse address order 2944, 26C4, 2430, 2428, 2420, 2418, 220C, 2004.  They are written
 *    like their siblings but WITHOUT a cached `fl` local (p->flags directly; uopt's CSE gives retail's
 *    reloads).  The 0x80 tracker needs the sum in a named float (`s`), assigned inside the condition.
 *  - Both player loops index by `i` (gStuntInfo[i], gGameCar[i]); uopt strength-reduces them into
 *    $s2/$s4 in the preheader, and the first loop's induction pointer shares $s2 with the second.
 *  - The stunt record is addressed as gStuntPlayer[i] (no pointer local): a store through a Stunt* local
 *    kills the PRE web of stunt_j in the tail and rotates v0/v1/a1 through the whole body.
 *  - No `fl` local at all for ply->flags; the flip pairs are two stores (`&= ~A; |= B`) as in retail.
 *  - `*(f32 *)(u32)&stunt_eTime += *pdt` (address-laundered) makes the store go through the hoisted
 *    address register like the loads; `pdt` is the laundered &gFrameSeconds (frame delta).
 * - Chained zeroing assignments, `x++` counters, int 1 in `(gModelTime - t38) > 1` (findings).
 * Literals (2.69f, 0.15f, 0.4712389f, 3.1415927f) are natural; own .rodata verified at 0x80123F38..0x80123F54.
 */
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
            p->a28 += fabsf(c->vx);
            p->a2C += fabsf(c->vy);
            p->a30 += fabsf(c->vz);
        }
        if (0.4712389f < p->a28) {
            n = 1;
        }
        if (0.4712389f < p->a2C) {
            n++;
        }
        if (0.4712389f < p->a30) {
            n++;
        }
        if (n >= 2 && 3.1415927f <= (s = p->a30 + (p->a28 + p->a2C))) {
            p->flags &= ~0x70;
            AddStunt(idx, 5);
            p->a28 = 0.0f;
            p->a2C = 0.0f;
            p->a30 = 0.0f;
        }
    } else if (stunt_okToStart != 0) {
        p->flags |= 0x80;
        p->a28 = fabsf(c->vx);
        p->a2C = fabsf(c->vy);
        p->a30 = fabsf(c->vz);
    }
}

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
        if ((ply->flags & 0x1F0) && hud->f3F8 != 0) {
            gStuntPlayer[i].f60 += *pdt;
        }
        if ((ply->flags & 0xC08FFFFF) && (car->f358 != 0 || hud->f6C4 != -1)) {
            ply->flags = 0x15000000;
        } else {
            stunt_j = stunt_l = stunt_r = stunt_f = stunt_b = 0;
            if (hud->s61C != 8) {
                stunt_j++;
                stunt_l++;
                stunt_f++;
            }
            if (hud->s61E != 8) {
                stunt_j++;
                stunt_r++;
                stunt_f++;
            }
            if (hud->s620 != 8) {
                stunt_j++;
                stunt_l++;
                stunt_b++;
            }
            if (hud->s622 != 8) {
                stunt_j++;
                stunt_r++;
                stunt_b++;
            }
            stunt_okToStart = stunt_j < 3 && car->f358 == 0 && hud->f6C4 == -1;
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
                if (gStuntPlayer[i].count == 0) {
                    gStuntPlayer[i].f60 = 0.0f;
                }
                if (ply->t54 == -1.0f && gStuntPlayer[i].count != 0) {
                    ply->t54 = gModelTime;
                }
            }
            if (stunt_j >= 3 && gStuntPlayer[i].count != 0) {
                if (ply->t38 != -1.0f && (gModelTime - ply->t38) > 1) {
                    ply->t38 = -1.0f;
                    AddStunt(i, 11);
                }
            }
            car->fE8 &= 0xFFEFFFFF;
        }
    }
}

