#include "game/game.h"

/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * UpdateBumps (0x800DEF68, 2976 B): EQUAL (744 words) in the whole-program unit, with its real
 * caller UpdateCarSounds (draft) and its real callees GetForceAndPeak / InitBumps (both EQUAL here).
 * Provisional only because the caller UpdateCarSounds does not match yet (s1 = model is an IPA register
 * parameter set by that caller). , 2026-10-06.
 *
 * Arcade ancestor (proven by structure and constants): do_bump_sounds() + check_forces_on_car() in
 * reference/repos/rushtherock/game/carsnd.c.  N64 differences: per-player arrays (bump_time gBumpTime,
 * scrape_state gScrapeState, scrape_time gScrapeTime, scrape handle gScrapes, thump timer gThumpTime),
 * one AudPlayEffectVP (positional sound) call per event, volume as f32 (range()/256 [*0.65]),
 * random sample 48+Random(13) via the locked Random.
 *
 * Shaping that the words prove:
 *  - CheckForces is the arcade check_forces_on_car, defined AFTER the caller (as in carsnd.c) and
 *    internal (force_internal).  umerge inlines it into its single caller; the deleted body is the retail
 *    stub at 0x800DEF60 (EQUAL, 2 words).  Inlined, its DED78 calls carry its later line numbers, which
 *    decides an as1 tie (mtc1 $f20 vs the bump_time load); with the loop written in the caller 2 words
 *    differ, and defined before the caller umerge inlines DED78 into it instead.
 *  - kill_scrape_sound() is written out at each of its three sites (a macro or an __inline function
 *    schedules the gScrapeTime store differently).
 *  - scrape pan arguments are int literals 1 / -1 (own constant webs; 0.0f stays shared with $f20).
 *  - `volume = volume / 256.0f; volume = volume * 0.65f;` as two statements.
 *  - own .rodata (natural literals, verify at splice): 5000.0f 0.01f 0.65f 0.001f, jump table,
 *    0.1f, jump table, 0.16666667f at 0x80124324..0x8012436C.
 */
#ifndef ROOT_IMPACT_SOUND_H
#define ROOT_IMPACT_SOUND_H

typedef struct AudioModel2056 {
    u8 to_body_force[196];f32 body_force[4][3];
    u8 to_center_force[60];f32 center_force[3];
    u8 to_speed[692];f32 speed;
    u8 to_crash[588];s8 crash;u8 gap1601;
    s8 collision_kind,last_collision_kind;s16 collision_direction;
    u8 to_player[384];s16 player;u8 tail[64];
} AudioModel2056;

VIEW(volatile f32, gSeconds);
#define gSeconds gSeconds_view

VIEW(s32, gGameState);
#define gGameState gGameState_view

f32 fabsf(f32);
#pragma intrinsic(fabsf)
#endif

#define range(a,b,c) ((a<b)?b:((a>c)?c:a))

void UpdateBumps(AudioModel2056 *m) {
    s32 i;
    s16 bump_index, scrape_side, scrape_snd;
    f32 volume, high_force;
    s32 p;

    p = m->player;
    if (m->crash != 0) {
        InitBumps(p);
        return;
    }
    CheckForces(m);

    bump_index = 0;
    high_force = 0.0f;
    if (gBumpTime[p] != 0.0f) {
        if (gSeconds - gBumpTime[p] > 1.0f || gSeconds < gBumpTime[p]) {
            gBumpTime[p] = 0.0f;
        }
    }
    for (i = 0; i < 4; i++) {
        if (gBump[p].point[i].flag && gBump[p].point[i].force > high_force) {
            high_force = gBump[p].point[i].force;
        }
    }
    volume = range(high_force * 0.01f, 150.0f, 235.0f);
    volume = volume / 256.0f;
    volume = volume * 0.65f;
    for (i = 0; i < 4; i++) {
        if (gBump[p].point[i].flag) {
            if (gBump[p].point[i].force > high_force * 0.5f) {
                bump_index |= 1 << i;
            }
            gBump[p].point[i].force = 0.0f;
            gBump[p].point[i].flag = 0;
        }
    }
    if (bump_index) {
        if (gBumpTime[p] == 0.0f) {
            AudPlayEffectVP((s32)(Random(13.0f) + 48.0f), m->player, 1, 1, volume,
                                gBumpLRTab[bump_index], gBumpFBTab[bump_index]);
            gBumpTime[p] = gSeconds;
        } else {
            AudPlayEffectVP((s32)(Random(13.0f) + 48.0f), m->player, 1, 1, volume,
                                gBumpLRTab[bump_index], gBumpFBTab[bump_index]);
            gBumpTime[p] = 0.0f;
        }
    } else {
        bump_index = 0;
        if (gBump[p].point[4].flag) {
            high_force = 0.0f;
            for (i = 0; i < 3; i++) {
                if (fabsf(gBump[p].point[4].vector[i]) > high_force) {
                    high_force = fabsf(gBump[p].point[4].vector[i]);
                }
            }
            volume = range(high_force * 0.001f, 180.0f, 235.0f);
            volume = volume / 256.0f;
            if (fabsf(gBump[p].point[4].vector[0]) > high_force * 0.5f) {
                if (gBump[p].point[4].vector[0] < 0.0f) {
                    bump_index |= 1;
                } else {
                    bump_index |= 2;
                }
            }
            if (fabsf(gBump[p].point[4].vector[1]) > high_force * 0.5f) {
                if (gBump[p].point[4].vector[1] < 0.0f) {
                    bump_index |= 4;
                } else {
                    bump_index |= 8;
                }
            }
            if (fabsf(gBump[p].point[4].vector[2]) > high_force * 0.5f) {
                bump_index |= 1;
            }
            gBump[p].point[4].force = 0.0f;
            gBump[p].point[4].flag = 0;
        }
        if (bump_index) {
            AudPlayEffectVP((s32)(Random(13.0f) + 48.0f), m->player, 1, 1, volume,
                                gBumpLRTab[bump_index], gBumpFBTab[bump_index]);
        }
    }

    if (bump_index == 0 && m->speed > 20.0f) {
        scrape_side = 0;
        if (m->body_force[0][0] != 0.0f || m->body_force[2][0] != 0.0f) {
            scrape_side |= 1;
        }
        if (m->body_force[1][0] != 0.0f || m->body_force[3][0] != 0.0f) {
            scrape_side |= 2;
        }
        scrape_snd = 0;
        switch (gScrapeState[p]) {
        case 0:
            gScrapeTime[p] = gSeconds;
            gScrapeState[p] = scrape_side;
            scrape_snd = scrape_side;
            break;
        case 1:
        case 2:
        case 3:
            if (scrape_side == 0) {
                AudStopEffect(gScrapes[p]);
                gScrapes[p] = -1;
                gScrapeState[p] = 0;
                gScrapeTime[p] = 0.0f;
            }
            else if (gSeconds - gScrapeTime[p] > 0.1f || gSeconds < gScrapeTime[p]) {
                gScrapeState[p] += 3;
                scrape_snd = gScrapeState[p];
            }
            break;
        case 4:
        case 5:
        case 6:
            if (scrape_side == 0) {
                AudStopEffect(gScrapes[p]);
                gScrapes[p] = -1;
                gScrapeState[p] = 0;
                gScrapeTime[p] = 0.0f;
            }
            break;
        }
        switch (scrape_snd) {
        case 1:
            AudPlayEffectVP((s32)(Random(13.0f) + 48.0f), m->player, 1, 1, 0.65f, 1, 0.0f);
            break;
        case 2:
            AudPlayEffectVP((s32)(Random(13.0f) + 48.0f), m->player, 1, 1, 0.65f, -1, 0.0f);
            break;
        case 3:
            AudPlayEffectVP((s32)(Random(13.0f) + 48.0f), m->player, 1, 1, 0.65f, 0.0f, 0.0f);
            break;
        case 4:
            gScrapes[p] = AudPlayEffectVP(19, m->player, 1, 2, 0.65f, 1, 0.0f);
            break;
        case 5:
            gScrapes[p] = AudPlayEffectVP(19, m->player, 1, 2, 0.65f, -1, 0.0f);
            break;
        case 6:
            gScrapes[p] = AudPlayEffectVP(19, m->player, 1, 2, 0.65f, 0.0f, 0.0f);
            break;
        }
    } else {
        AudStopEffect(gScrapes[p]);
        gScrapes[p] = -1;
        gScrapeState[p] = 0;
        gScrapeTime[p] = 0.0f;
    }

    if (gThumpTime[p] != 0.0f) {
        if (gSeconds - gThumpTime[p] > 0.16666667f || gSeconds < gThumpTime[p]) {
            gThumpTime[p] = 0.0f;
        }
    }
    if (m->last_collision_kind > m->collision_kind) {
        m->last_collision_kind = m->collision_kind;
    }
    if (m->last_collision_kind != m->collision_kind) {
        m->last_collision_kind = m->collision_kind;
        if (gThumpTime[p] == 0.0f) {
            gThumpTime[p] = gSeconds;
            if (!(gGameState & 0x200000)) {
                switch (m->last_collision_kind) {
                case 1:
                    AudPlayEffectVP(9, m->player, 1, 2, 1.0f, gBumpLRTab[m->collision_direction], gBumpFBTab[m->collision_direction]);
                    break;
                case 2:
                    AudPlayEffectVP(5, m->player, 1, 2, 1.0f, gBumpLRTab[m->collision_direction], gBumpFBTab[m->collision_direction]);
                    break;
                case 3:
                    AudPlayEffectVP(3, m->player, 1, 2, 1.0f, gBumpLRTab[m->collision_direction], gBumpFBTab[m->collision_direction]);
                    break;
                }
            }
        }
    }
}

/* arcade check_forces_on_car(); deleted after inlining -> retail stub CheckForces */
void CheckForces(AudioModel2056 *m) {
    s16 k;

    for (k = 0; k < 4; k++) {
        GetForceAndPeak(k, m->player, m->body_force[k], 5000.0f);
    }
    GetForceAndPeak(4, m->player, m->center_force, 5000.0f);
}
