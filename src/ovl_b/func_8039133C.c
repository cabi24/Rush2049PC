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
#ifdef TARGET_PC
/* PC: the objects are the level-object nodes of the pool D_80143FC8 (D_80143FD8 is its active list). They keep their
 * N64 layout in the arena block D_8014D280: the link is a 32-bit window address and the flags byte is at +4
 * (docs/port/POINTERS.md 10). A slot holds a host pointer, so it is 16 bytes, and the heap block holds count of them. */
typedef struct Object { u32 next; u8 flags; } Object;
#define NEXT(object) ((Object *)(uintptr_t)(object)->next)
#define SLOT_SIZE (s32)sizeof(Slot)
#else
typedef struct Object { struct Object *next; u8 flags; } Object;
#define NEXT(object) object->next
#define SLOT_SIZE 8
#endif
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
extern State D_80399AE0;
extern Object *D_80143FD8;
extern void *audio_dma_sync(Heap *, u32);
extern f32 func_8008B2E4(f32);
void func_8039133C(s32 reuse)
{
    Object *object;
    s32 index;
    if (reuse == 0) {
        D_80399AE0.slots = audio_dma_sync(0, D_80399AE0.count * SLOT_SIZE);
    }
    index = 0;
    for (object = D_80143FD8; object != 0; object = NEXT(object)) {
        if (object->flags & 0x20) {
            D_80399AE0.slots[index].object = object;
            D_80399AE0.slots[index].active = 1;
            D_80399AE0.slots[index].count = 1;
            index++;
        }
    }
    D_80399AE0.delay = func_8008B2E4(10.0f) + 5.0f;
    D_80399AE0.phase_a = func_8008B2E4(45.0f) + 15.0f;
    D_80399AE0.phase_b = func_8008B2E4(45.0f) + 15.0f;
    if (func_8008B2E4(1.0f) > 0.5f) {
        D_80399AE0.phase_b += 45.0f;
    } else {
        D_80399AE0.phase_a += 45.0f;
    }
}
