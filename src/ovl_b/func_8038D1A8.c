/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef int s32;
typedef unsigned char u8;
void struct_fields_init(void *head, void *pool, s32 size, s32 count, s32 flags);
#ifdef TARGET_PC
/* PC: the list headers have the StatLL host layout (40 bytes, shots.c) and the element arrays their elements' host
 * layout, runtime_b_trail's Trail (64 bytes on the N64, 72 here) and Fade (28, 48), which are linked through their
 * first pointer. These declarations give them that storage (gen_data), and the lists the host element size
 * (docs/port/POINTERS.md 9). */
typedef struct { u8 dbl; s32 numElements, elementSize; void *list, *head, *store; } PcStatLL;
typedef struct { void *next; s32 unknown04, flags; float lifetime, origin[3], previous[3], lower[3], upper[3]; } PcTrail;
typedef struct { void *next; s32 unknown04; void *object; float lifetime, interval; u8 color[4]; void *source; } PcFade;
extern PcStatLL D_80395EE8[1];
extern PcTrail D_80395F00[50];
extern PcStatLL D_80396B80[1];
extern PcFade D_80396B98[150];
#define ELEMENT_SIZE(n64, type) sizeof(type)
#else
extern u8 D_80395EE8[];
extern u8 D_80395F00[];
extern u8 D_80396B80[];
extern u8 D_80396B98[];
#define ELEMENT_SIZE(n64, type) n64
#endif

void func_8038D1A8(void)
{
    struct_fields_init(D_80395EE8, D_80395F00, ELEMENT_SIZE(64, PcTrail), 50, 0);
    struct_fields_init(D_80396B80, D_80396B98, ELEMENT_SIZE(28, PcFade), 150, 0);
}
