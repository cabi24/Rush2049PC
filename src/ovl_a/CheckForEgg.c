/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
/* Image A input-sequence gate, 0x8038F648..0x8038F734, 236 bytes.
 * Checks successive held/pressed mask pairs, resets on timeout or unrelated
 * input, and returns 1 when the incremented step reaches the byte count.
 * N64-native reconstruction; no proven arcade-source ancestor.
 *
 * SHAPING: the two optimized-away unsigned *1 expressions keep both sides
 * of each equality as expressions; with the mask variable written first,
 * IDO emits the observed branch operand order. Ordinary comparisons without
 * these identities differ at +0x90/+0x98. No instructions or storage are added.
 * The volatile clock contract follows existing same-symbol accepted sources
 * car_sounds/race_countdown_display; historical declaration
 * and asynchronous-writer behavior remain unproven.
 *
 * The union provides the full 164-byte record word view while keeping count
 * at byte zero (held/pressed at byte 4/84). This uses IDO union representation,
 * not portable serialization. Caller must supply valid sequence storage and
 * a step in 0..19 whenever a record is read. No general input-bounds claim.
 */
typedef unsigned char u8;
typedef unsigned int u32;
typedef int s32;
typedef union InputSequence {
    struct {
    u8 count;
    u8 unknown1[3];
    u32 held[20];
    u32 pressed[20];
    } fields;
    u32 words[41];
} InputSequence;
extern InputSequence *gEgg;
extern float gNextTimeOut;
extern s32 gEggIdx;
extern volatile float gSeconds;
extern u32 gActiveEdges,gActiveLevels;
s32 CheckForEgg(void)
{
    u32 held, pressed, other;
    u32 *step;
    if (!gEgg) return 0;
    if (gEggIdx && gNextTimeOut <= gSeconds)
        gEggIdx = 0;
    step = &gEgg->words[gEggIdx];
    pressed = step[21];
    held = step[1];
    other = ~(pressed | held);
    if (other & gActiveEdges) gEggIdx = 0;
    else if ((held * 1) == (gActiveLevels & held) &&
             (pressed * 1) == (gActiveEdges & pressed) &&
             !(other & gActiveLevels)) {
        gNextTimeOut = gSeconds + 1.0f;
        gEggIdx++;
        if (gEggIdx == gEgg->fields.count) return 1;
    }
    return 0;
}
