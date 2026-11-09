/* dotireforce.c: one tire's force on the car (MAT3 tires.c, dotireforce()).
 *
 * forces1 calls it for each tire each step: the suspension's force gives the tire's load, frictioncircle the
 * forward and side forces it can take, and the rear (driven) tires' are scaled by the car's parts. */
#include "game/game.h"

/* A tire in the model record (MAT3 anon_tradius_springK_5c, tModelData.tires[i]), the fields kept here. */
typedef struct Tire {
    char pad0[0x50];
    f32 sideforce;          /* +0x50 the last side force */
    f32 traction;           /* +0x54 the last forward force */
    char pad58[4];
} Tire;

/* The tune record (MAT3 anon_diffbl_weight_scale_2c), its grip scales. */
typedef struct {
    char pad0[0x18];
    f32 fwdfk;              /* +0x18 the forward grip's */
    f32 sidefk;             /* +0x1C the side grip's */
} tTune;

enum n64p_tTune { n64p_tTune_ };
/* The model record (tModelData), the fields dotireforce reads. */
typedef struct tModelData {
    char pad0[4];
    enum n64p_tTune tune;   /* +0x004 the tune record (a window address) */
    char pad8[1];
    s8 trans;               /* +0x009 the transmission choice */
    s8 autotrans;           /* +0x00A */
    s8 handling;            /* +0x00B the handling (tune) choice */
    s8 engine_type;         /* +0x00C the engine choice */
    char padD[0x3F4 - 0xD];
    s16 gear;               /* +0x3F4 */
    char pad3F6[0x430 - 0x3F6];
    Tire tires[4];          /* +0x430 */
    char pad5A0[16];
    f32 sidefk;             /* +0x5B0 the tires' side grip (CalcTireSideForce) */
    f32 offroadtires;       /* +0x5B4 the tires' off-road factor */
    char pad5B8[4];
    f32 weight;             /* +0x5BC */
    f32 mass;               /* +0x5C0 */
    char pad5C4[0x638 - 0x5C4];
    f32 idt;                /* +0x638 1 / dt, steps a second */
    char pad63C[0x720 - 0x63C];
    f32 wheel;              /* +0x720 mainin.wheel, the steering -1..1 */
} tModelData;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_tTune: (tTune *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

f32 fabsf(f32);

/* Tire `tire`'s force on model m, into forcevec (the car's frame). tirev is its contact patch's velocity and tireuvs
 * its frame (calctireuv), torque its drive torque; suscomp its corner's suspension compression and otsuscomp the
 * other side's on the same axle; springrate, arspringrate (the anti-roll bar), cdamping and rdamping (compression and
 * rebound) its corner's constants; poortract the rear tires on dirt or water.
 * - The load: the spring and the anti-roll bar (both corners compressed) less the damping; compressed past 10 ft
 *   (the car sunk into the road) and moving up slower than 1 ft/s, a quarter of the mass pushing it out instead;
 *   then scaled by a step-rate polynomial (dotireforce_idt_coef, _offset: MAT3 0.000047744 idt - 0.017552, times
 *   idt), at least 0 and at most the car's weight.
 * - The forward and side forces from frictioncircle. A rear tire's forward force is scaled by
 *   rear_traction_by_tune ([engine][handling]), by rear_traction_by_surface ([transmission][gear]: the label is
 *   4 bytes into MAT3's carTransTraction, which MAT3 reads at [transmission][gear + 1]) less the more the wheel is
 *   turned, and by the tune's fwdfk; its side force by the tires' sidefk less 0.1 of the tune's
 *   (dotireforce_side_drop), and with poor traction by 0.7 for the special tires cheat or 0.9 plus 0.1 of the
 *   off-road factor.
 * The forces are kept in the tire too. */
void dotireforce(tModelData *m, f32 *tirev, void *tireuvs, Tire *tire, f32 torque, f32 *forcevec,
                          f32 suscomp, f32 otsuscomp, f32 springrate, f32 arspringrate, f32 cdamping,
                          f32 rdamping, s32 poortract) {
    f32 wheelalpha, spd, temp, normal, maxnorm, damping;   /* wheelalpha, spd, temp, maxnorm: unused */
    f32 sideforce, traction, arforce;
    f32 tireforcevec[3];

    if (suscomp > 0.0f && otsuscomp > 0.0f) {
        arforce = (suscomp - otsuscomp) * arspringrate;
    } else {
        arforce = 0.0f;
    }

    if (tirev[1] < 0.0f) {
        damping = cdamping;
    } else {
        damping = rdamping;
    }

    if (suscomp > 10.0f) {
        if (tirev[1] < 1.0f) {
            tireforcevec[1] = (1.0f - tirev[1]) * m->mass * -0.25f * m->idt;
        } else {
            tireforcevec[1] = arforce + suscomp * springrate - tirev[1] * damping;
        }
    } else if (suscomp > 0.0f) {
        tireforcevec[1] = arforce + suscomp * springrate - tirev[1] * damping;
    } else {
        tireforcevec[1] = 0;
    }

    tireforcevec[1] *= m->idt * (m->idt * dotireforce_idt_coef - dotireforce_idt_offset);
    normal = tireforcevec[1];
    if (normal < 0.0f) {
        normal = 0;
        tireforcevec[1] = 0;
    } else if (normal > m->weight) {
        normal = m->weight;
    }
    frictioncircle(m, tirev, normal, torque, tire, &sideforce, &traction);

    if (tire == &m->tires[2] || tire == &m->tires[3]) {
        traction *= rear_traction_by_tune[m->engine_type][m->handling];
        traction *= 1.0f + (rear_traction_by_surface[m->trans][m->gear] - 1) * (1.0f - fabsf(m->wheel));
        traction *= N64P(m->tune)->fwdfk;
        sideforce *= m->sidefk * (1.0f - N64P(m->tune)->sidefk * dotireforce_side_drop);
        if (poortract) {
            sideforce *= (gSpecialTires == 2) ? dotireforce_side_scale_mode2 : m->offroadtires * dotireforce_side_scale_slope + dotireforce_side_scale_base;
        }
    }

    tireforcevec[2] = traction;
    tireforcevec[0] = sideforce;
    WorldVector(tireforcevec, forcevec, tireuvs);
    tire->sideforce = sideforce;
    tire->traction = traction;
}
