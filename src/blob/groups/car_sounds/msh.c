/* A human's car's bumps, scrapes and thumps (MAT3 audio.c's _UpdateBumps and _CheckForces; the arcade game's
 * carsnd.c has the same logic, do_bump_sounds and check_forces_on_car). Each frame UpdateCarSounds calls
 * UpdateBumps, which first gathers the frame's body forces (CheckForces, GetForceAndPeak in ded78.c), then plays
 * a bump for a settled hit, a scrape while a side rubs, and a thump when the car lands or hits harder than before. */
#include "game/game.h"

/* The model record's fields this file reads (MAT3 tModelData, the N64's layout): bodyForce (the four corners),
 * centerForceLast, the speed at +0x3F0, crashflag, thumpflag and mthumpflag (this frame's and the last thump's
 * kind), thump_side, slot. */
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

/* gGameState's bit that silences the thumps. */
#define GAME_STATE_NO_THUMPS 0x200000
/* The effects (fxid): the first of 13 bump sounds, the scrape loop, the thumps of the three collision kinds. */
#define SFX_BUMP_FIRST 48.0f
#define SFX_SCRAPE 19
#define SFX_THUMP_1 9
#define SFX_THUMP_2 5
#define SFX_THUMP_3 3

#define range(a,b,c) ((a<b)?b:((a>c)?c:a))

/* The bumps, scrapes and thumps of the car of model m, one frame. A crashed car's are reset. Bumps: the settled
 * hits of the corners whose peak is at least half the strongest play one of 13 bump sounds, panned by which
 * corners (gBumpLRTab, gBumpFBTab), its volume from the force; else a hit of the centre plays one by its
 * direction. Scrapes, above 20 of speed with no bump: a side touching (body forces) starts one short scrape,
 * and a tenth of a second later the scrape loop, until the side lets go. Thumps: a new collision kind plays its
 * thump, at most six a second. */
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
        if (gBump[p].point[i].bump && gBump[p].point[i].peak > high_force) {
            high_force = gBump[p].point[i].peak;
        }
    }
    volume = range(high_force * 0.01f, 150.0f, 235.0f);
    volume = volume / 256.0f;
    volume = volume * 0.65f;
    for (i = 0; i < 4; i++) {
        if (gBump[p].point[i].bump) {
            if (gBump[p].point[i].peak > high_force * 0.5f) {
                bump_index |= 1 << i;
            }
            gBump[p].point[i].peak = 0.0f;
            gBump[p].point[i].bump = 0;
        }
    }
    if (bump_index) {
        if (gBumpTime[p] == 0.0f) {
            AudPlayEffectVP((s32)(Random(13.0f) + SFX_BUMP_FIRST), m->player, 1, 1, volume,
                                gBumpLRTab[bump_index], gBumpFBTab[bump_index]);
            gBumpTime[p] = gSeconds;
        } else {
            AudPlayEffectVP((s32)(Random(13.0f) + SFX_BUMP_FIRST), m->player, 1, 1, volume,
                                gBumpLRTab[bump_index], gBumpFBTab[bump_index]);
            gBumpTime[p] = 0.0f;
        }
    } else {
        bump_index = 0;
        if (gBump[p].point[4].bump) {
            high_force = 0.0f;
            for (i = 0; i < 3; i++) {
                if (fabsf(gBump[p].point[4].peakVec[i]) > high_force) {
                    high_force = fabsf(gBump[p].point[4].peakVec[i]);
                }
            }
            volume = range(high_force * 0.001f, 180.0f, 235.0f);
            volume = volume / 256.0f;
            if (fabsf(gBump[p].point[4].peakVec[0]) > high_force * 0.5f) {
                if (gBump[p].point[4].peakVec[0] < 0.0f) {
                    bump_index |= 1;
                } else {
                    bump_index |= 2;
                }
            }
            if (fabsf(gBump[p].point[4].peakVec[1]) > high_force * 0.5f) {
                if (gBump[p].point[4].peakVec[1] < 0.0f) {
                    bump_index |= 4;
                } else {
                    bump_index |= 8;
                }
            }
            if (fabsf(gBump[p].point[4].peakVec[2]) > high_force * 0.5f) {
                bump_index |= 1;
            }
            gBump[p].point[4].peak = 0.0f;
            gBump[p].point[4].bump = 0;
        }
        if (bump_index) {
            AudPlayEffectVP((s32)(Random(13.0f) + SFX_BUMP_FIRST), m->player, 1, 1, volume,
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
            AudPlayEffectVP((s32)(Random(13.0f) + SFX_BUMP_FIRST), m->player, 1, 1, 0.65f, 1, 0.0f);
            break;
        case 2:
            AudPlayEffectVP((s32)(Random(13.0f) + SFX_BUMP_FIRST), m->player, 1, 1, 0.65f, -1, 0.0f);
            break;
        case 3:
            AudPlayEffectVP((s32)(Random(13.0f) + SFX_BUMP_FIRST), m->player, 1, 1, 0.65f, 0.0f, 0.0f);
            break;
        case 4:
            gScrapes[p] = AudPlayEffectVP(SFX_SCRAPE, m->player, 1, 2, 0.65f, 1, 0.0f);
            break;
        case 5:
            gScrapes[p] = AudPlayEffectVP(SFX_SCRAPE, m->player, 1, 2, 0.65f, -1, 0.0f);
            break;
        case 6:
            gScrapes[p] = AudPlayEffectVP(SFX_SCRAPE, m->player, 1, 2, 0.65f, 0.0f, 0.0f);
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
            if (!(gGameState & GAME_STATE_NO_THUMPS)) {
                switch (m->last_collision_kind) {
                case 1:
                    AudPlayEffectVP(SFX_THUMP_1, m->player, 1, 2, 1.0f, gBumpLRTab[m->collision_direction], gBumpFBTab[m->collision_direction]);
                    break;
                case 2:
                    AudPlayEffectVP(SFX_THUMP_2, m->player, 1, 2, 1.0f, gBumpLRTab[m->collision_direction], gBumpFBTab[m->collision_direction]);
                    break;
                case 3:
                    AudPlayEffectVP(SFX_THUMP_3, m->player, 1, 2, 1.0f, gBumpLRTab[m->collision_direction], gBumpFBTab[m->collision_direction]);
                    break;
                }
            }
        }
    }
}

/* Gathers this frame's forces of the car of model m: each body corner's and the centre's (GetForceAndPeak, with a
 * threshold of 5000). */
void CheckForces(AudioModel2056 *m) {
    s16 k;

    for (k = 0; k < 4; k++) {
        GetForceAndPeak(k, m->player, m->body_force[k], 5000.0f);
    }
    GetForceAndPeak(4, m->player, m->center_force, 5000.0f);
}
