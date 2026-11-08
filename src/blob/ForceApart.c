/* flags: -g0 -O3 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
/*
 * Historical label ForceApart is misleading: this is the N64
 * descendant of arcade ForceApart (reference/repos/rushtherock/game/
 * collision.c): normalise dir, scale to 40000, rotate through each car's
 * matrix with BodyVector and push `car` and the other car of the pair
 * (m == m1 ? m2 : m1) apart with equal and opposite forces at +0x124.
 *
 * Needs -O3: at -O2 IDO puts `car` in s0 (90/91 words differ); at -O3 it is
 * kept in t0 and spilled around the call like the target.
 * force_apart_min_dist_sq/force_apart_force/force_apart_force_over_dist are the function's own float literals
 * (0.0001f, 40000.0f, 40000.0f) in retail .rodata, referenced as externs so
 * the scorer can verify the addresses.
 * Quirk: `mag` is an unused local that supplies the last 8 bytes of the
 * 80-byte frame (arcade ForceApart has a `force` scalar in that position).
 */
typedef float f32;
typedef int s32;
typedef signed char s8;

typedef struct {
    char pad0[0x124];
    f32 unk124[3];
    char pad130[0x510];
    s8 unk640;
    char pad641[0x15F];
    f32 unk7A0[3][3];
} Car;

extern f32 sqrtf(f32);
#pragma intrinsic(sqrtf)
extern f32 force_apart_min_dist_sq;
extern f32 force_apart_force;
extern f32 force_apart_force_over_dist;

void BodyVector(f32 *arg0, f32 *arg1, f32 *arg2);

void ForceApart(Car *car, Car *a, Car *b, f32 *dir) {
    Car *other;
    f32 len;
    f32 mag;
    f32 force[3];
    f32 out[3];
    s32 i;

    if (car == a) {
        other = b;
    } else {
        other = a;
    }
    len = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
    if (len < force_apart_min_dist_sq) {
        force[0] = force_apart_force;
        force[1] = 0.0f;
        force[2] = 0.0f;
    } else {
        len = force_apart_force_over_dist / sqrtf(len);
        for (i = 0; i < 3; i++) {
            force[i] = dir[i] * len;
        }
    }
    BodyVector(force, out, car->unk7A0[0]);
    car->unk124[0] = car->unk124[0] + out[0];
    car->unk124[1] = car->unk124[1] + out[1];
    car->unk124[2] = car->unk124[2] + out[2];
    BodyVector(force, out, other->unk7A0[0]);
    other->unk124[0] = other->unk124[0] - out[0];
    other->unk124[1] = other->unk124[1] - out[1];
    other->unk124[2] = other->unk124[2] - out[2];
}
