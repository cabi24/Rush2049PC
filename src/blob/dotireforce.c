/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Historical label dotireforce is misleading: this is the N64
 * descendant of arcade dotireforce (reference/repos/rushtherock/game/tires.c),
 * proven by its callee frictioncircle == frictioncircle and the shared
 * suspension / anti-roll / damping logic.  Computes one tire's force vector:
 * suspension normal force (with an N64-only idt stabilisation polynomial),
 * friction-circle side force and traction, N64-only traction/side-force
 * scaling for tires[2] and tires[3], then rotates the result with
 * WorldVector (arcade bodtorw).  N64 drops the arcade ottirev and airfact
 * formals; axis order is side 0, normal 1, traction 2.
 *
 * Needs -O3 (-O2: 38 words, tire homed instead of kept in t0).
 * Shaping facts, each verified necessary:
 * - the arcade's unused locals wheelalpha, spd, temp, maxnorm are kept: they
 *   supply 16 bytes of the 96-byte frame (without them 20 words differ);
 * - literal types follow the arcade: `= 0` (int) for the three zero stores
 *   but 0.0f in the comparisons (two separate zero webs, f12 and f16), and
 *   int `- 1` against 1.0f elsewhere (separate 1.0 web).
 * dotireforce_idt_coef..dotireforce_side_scale_base are this function's own float literals in retail
 * .rodata, referenced as externs so the scorer verifies their addresses.
 */
typedef float f32;
typedef signed char s8;
typedef signed short s16;
typedef int s32;

typedef struct {
    char pad0[80];
    f32 sideforce;
    f32 traction;
    char pad58[4];
} Tire;

typedef struct {
    char pad0[24];
    f32 unk18;
    f32 unk1C;
} Sub;

enum n64p_Sub { n64p_Sub_ };
typedef struct {
    char pad0[4];
    enum n64p_Sub sub;
    char pad8[1];
    s8 unk9;
    s8 unkA;
    s8 unkB;
    s8 unkC;
    char padD[0x3F4 - 0xD];
    s16 unk3F4;
    char pad3F6[0x430 - 0x3F6];
    Tire tires[4];
    char pad5A0[16];
    f32 unk5B0;
    f32 unk5B4;
    char pad5B8[4];
    f32 weight;
    f32 mass;
    char pad5C4[0x638 - 0x5C4];
    f32 idt;
    char pad63C[0x720 - 0x63C];
    f32 unk720;
} Model;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Sub: (Sub *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

extern f32 rear_traction_by_tune[][3];
extern f32 rear_traction_by_surface[][6];
extern f32 dotireforce_idt_coef, dotireforce_idt_offset, dotireforce_side_drop, dotireforce_side_scale_mode2, dotireforce_side_scale_slope, dotireforce_side_scale_base;
extern s8 gSpecialTires;
f32 fabsf(f32);
#pragma intrinsic(fabsf)

void frictioncircle(Model *m, f32 *tirev, f32 normal, f32 torque, Tire *tire, f32 *sfp, f32 *trp);
void WorldVector(f32 *, f32 *, void *);

void dotireforce(Model *m, f32 *tirev, void *tireuvs, Tire *tire, f32 torque, f32 *forcevec,
                          f32 suscomp, f32 otsuscomp, f32 springrate, f32 arspringrate, f32 cdamping,
                          f32 rdamping, s32 poortract) {
    f32 wheelalpha, spd, temp, normal, maxnorm, damping;
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
        traction *= rear_traction_by_tune[m->unkC][m->unkB];
        traction *= 1.0f + (rear_traction_by_surface[m->unk9][m->unk3F4] - 1) * (1.0f - fabsf(m->unk720));
        traction *= N64P(m->sub)->unk18;
        sideforce *= m->unk5B0 * (1.0f - N64P(m->sub)->unk1C * dotireforce_side_drop);
        if (poortract) {
            sideforce *= (gSpecialTires == 2) ? dotireforce_side_scale_mode2 : m->unk5B4 * dotireforce_side_scale_slope + dotireforce_side_scale_base;
        }
    }

    tireforcevec[2] = traction;
    tireforcevec[0] = sideforce;
    WorldVector(tireforcevec, forcevec, tireuvs);
    tire->sideforce = sideforce;
    tire->traction = traction;
}
