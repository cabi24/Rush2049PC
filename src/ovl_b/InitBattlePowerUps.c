/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
/* Image B: native object selection and random phase initialization.
 * No whole-function arcade donor is established. The allocator is the accepted
 * audio_heap group's ordinary (Heap *, u32) boundary; historical names are not
 * a claim that this is an audio routine. */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef int s32;
typedef unsigned int u32;
typedef float f32;
/* PC: the objects are the level-object nodes of the pool gTargetList (gTargetList_head is its active list). They keep their
 * N64 layout in the arena block gTargetArray: the link is a 32-bit window address and the flags byte is at +4
 * (docs/port/POINTERS.md 10). A slot holds a host pointer, so it is 16 bytes, and the heap block holds count of them. */
typedef struct Object { u32 next; u8 flags; } Object;
#define NEXT(object) ((Object *)PC_N64P((object)->next))
#define SLOT_SIZE (s32)sizeof(Slot)
typedef struct Slot { Object *object; s16 active; s16 count; } Slot;
typedef struct State {
    s8 count;
    u8 unknown_1[3];
    f32 delay;
    f32 phase_a;
    f32 phase_b;
    Slot *slots;
} State;
typedef struct Heap Heap;
extern State gBattlePowerUps;
extern Object *gTargetList_head;
extern void *audio_dma_sync(Heap *, u32);
extern f32 Random(f32);
void InitBattlePowerUps(s32 reuse)
{
    Object *object;
    s32 index;
    if (reuse == 0) {
        gBattlePowerUps.slots = audio_dma_sync(0, gBattlePowerUps.count * SLOT_SIZE);
    }
    index = 0;
    for (object = gTargetList_head; object != 0; object = NEXT(object)) {
        if (object->flags & 0x20) {
            gBattlePowerUps.slots[index].object = object;
            gBattlePowerUps.slots[index].active = 1;
            gBattlePowerUps.slots[index].count = 1;
            index++;
        }
    }
    gBattlePowerUps.delay = Random(10.0f) + 5.0f;
    gBattlePowerUps.phase_a = Random(45.0f) + 15.0f;
    gBattlePowerUps.phase_b = Random(45.0f) + 15.0f;
    if (Random(1.0f) > 0.5f) {
        gBattlePowerUps.phase_b += 45.0f;
    } else {
        gBattlePowerUps.phase_a += 45.0f;
    }
}
