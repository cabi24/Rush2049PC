/* A human's car's bumps, scrapes and thumps (MAT3 audio.c's _UpdateBumps and _CheckForces; the arcade game's
 * carsnd.c has the same logic, do_bump_sounds and check_forces_on_car). Each frame UpdateCarSounds calls
 * UpdateBumps, which first gathers the frame's body forces (CheckForces, GetForceAndPeak in GetForceAndPeak.c), then plays
 * a bump for a settled hit, a scrape while a side rubs, and a thump when the car lands or hits harder than before. */
#include "game/game.h"

f32 fabsf(f32);

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
void UpdateBumps(tModelData *m) {
    s32 i;
    s16 bump_index, scrape_side, scrape_snd;
    f32 volume, high_force;
    s32 p;

    p = m->slot;
    if (m->crashflag != 0) {
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
            AudPlayEffectVP((s32)(Random(13.0f) + SFX_BUMP_FIRST), m->slot, 1, 1, volume,
                                gBumpLRTab[bump_index], gBumpFBTab[bump_index]);
            gBumpTime[p] = gSeconds;
        } else {
            AudPlayEffectVP((s32)(Random(13.0f) + SFX_BUMP_FIRST), m->slot, 1, 1, volume,
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
            AudPlayEffectVP((s32)(Random(13.0f) + SFX_BUMP_FIRST), m->slot, 1, 1, volume,
                                gBumpLRTab[bump_index], gBumpFBTab[bump_index]);
        }
    }

    if (bump_index == 0 && m->magvel > 20.0f) {
        scrape_side = 0;
        if (m->bodyForce[0][0] != 0.0f || m->bodyForce[2][0] != 0.0f) {
            scrape_side |= 1;
        }
        if (m->bodyForce[1][0] != 0.0f || m->bodyForce[3][0] != 0.0f) {
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
            AudPlayEffectVP((s32)(Random(13.0f) + SFX_BUMP_FIRST), m->slot, 1, 1, 0.65f, 1, 0.0f);
            break;
        case 2:
            AudPlayEffectVP((s32)(Random(13.0f) + SFX_BUMP_FIRST), m->slot, 1, 1, 0.65f, -1, 0.0f);
            break;
        case 3:
            AudPlayEffectVP((s32)(Random(13.0f) + SFX_BUMP_FIRST), m->slot, 1, 1, 0.65f, 0.0f, 0.0f);
            break;
        case 4:
            gScrapes[p] = AudPlayEffectVP(SFX_SCRAPE, m->slot, 1, 2, 0.65f, 1, 0.0f);
            break;
        case 5:
            gScrapes[p] = AudPlayEffectVP(SFX_SCRAPE, m->slot, 1, 2, 0.65f, -1, 0.0f);
            break;
        case 6:
            gScrapes[p] = AudPlayEffectVP(SFX_SCRAPE, m->slot, 1, 2, 0.65f, 0.0f, 0.0f);
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
    if (m->mthumpflag > m->thumpflag) {
        m->mthumpflag = m->thumpflag;
    }
    if (m->mthumpflag != m->thumpflag) {
        m->mthumpflag = m->thumpflag;
        if (gThumpTime[p] == 0.0f) {
            gThumpTime[p] = gSeconds;
            if (!(gGameState & GS_COUNTDOWN)) {
                switch (m->mthumpflag) {
                case 1:
                    AudPlayEffectVP(SFX_THUMP_1, m->slot, 1, 2, 1.0f, gBumpLRTab[m->thump_side], gBumpFBTab[m->thump_side]);
                    break;
                case 2:
                    AudPlayEffectVP(SFX_THUMP_2, m->slot, 1, 2, 1.0f, gBumpLRTab[m->thump_side], gBumpFBTab[m->thump_side]);
                    break;
                case 3:
                    AudPlayEffectVP(SFX_THUMP_3, m->slot, 1, 2, 1.0f, gBumpLRTab[m->thump_side], gBumpFBTab[m->thump_side]);
                    break;
                }
            }
        }
    }
}

/* Gathers this frame's forces of the car of model m: each body corner's and the centre's (GetForceAndPeak, with a
 * threshold of 5000). */
void CheckForces(tModelData *m) {
    s16 k;

    for (k = 0; k < 4; k++) {
        GetForceAndPeak(k, m->slot, m->bodyForce[k], 5000.0f);
    }
    GetForceAndPeak(4, m->slot, m->centerForceLast, 5000.0f);
}
