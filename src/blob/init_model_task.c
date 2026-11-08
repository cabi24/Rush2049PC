/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * init_model_task(n) -- per-model rescale over the six 0x808-byte MODELDAT records at D_8014A250.
 * Stores -n to gModelIteration and (-n) * gRetraceTime to gModelTime. For each record: outside gameplay mode
 * 2, for record 0, or when its divisor (+0x718) is 0, it takes the new values directly (+0x710 = -n,
 * +0x714 = gModelTime, +0x718 = the scale); otherwise +0x710 = round(gModelTime / divisor) (round
 * half away from zero through `q -/+ 0.5f` truncation) and +0x714 = gModelTime. No arcade ancestor
 * identified.
 *
 * Shaping (draft was 26/57):
 *  - the loop body reads the globals gModelIteration and gModelTime back instead of locals holding the
 *    values just stored. The indexed stores into model[] do not alias those globals, so uopt
 *    keeps them in registers; reading them back is what makes &gModelIteration a coloured web (retail's a3)
 *    and gives retail's FP colouring (gModelTime's value in f12, the divisor load in f2). With locals
 * v/f both were wrong.
 *  - goto loop (a for/do-while loop is unrolled by -O3: 26 extra words) and the inverted condition
 * order `!= 2 || i == 0 || divisor == 0.0f` are.
 * Also MATCH at -O2. Leaf, no frame.
 */
typedef int s32;
typedef float f32;

typedef struct {
    char pad0[0x710];
    s32 unk710;
    f32 unk714;
    f32 unk718;
    char pad71C[0x808 - 0x71C];
} Model;

extern s32 gModelIteration;
extern f32 gModelTime;
extern f32 gRetraceTime;
extern s32 gRace;
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

void init_model_task(s32 arg0)
{
    f32 q;
    s32 i;
    f32 scale;

    gModelIteration = -arg0;
    scale = gRetraceTime;
    gModelTime = gModelIteration * scale;
    i = 0;
loop:
    if (gRace != 2 || i == 0 || PC_MODEL_DT(i) == 0.0f) {
        PC_MODEL_ITERATION(i) = gModelIteration;
        PC_MODEL_MODEL_TIME(i) = gModelTime;
        PC_MODEL_DT(i) = scale;
    } else {
        q = gModelTime / PC_MODEL_DT(i);
        if (q < 0.0f) {
            PC_MODEL_ITERATION(i) = q - 0.5f;
        } else {
            PC_MODEL_ITERATION(i) = q + 0.5f;
        }
        PC_MODEL_MODEL_TIME(i) = gModelTime;
    }
    i++;
    if (i < PC_RACE_CARS) goto loop;
}
