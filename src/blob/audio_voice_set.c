/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef float f32;
typedef int s32;
typedef unsigned int u32;
typedef unsigned char u8;
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif
typedef struct Voice Voice;
struct Voice {
    Voice *next;
#ifdef TARGET_PC
    Voice *reserved4;   /* the doubly linked list's back link (func_8008E3C0), pointer-sized here */
#else
    u32 reserved4;
#endif
    f32 start;
    intptr_t handle;
    u8 pad10[12];
    u8 bytes[4];
    s32 enabled;
};
#ifdef TARGET_PC
/* PC: a StatLL list header's host layout, as every view has it (docs/port/POINTERS.md 9): doubly flag, element
 * count and size, then the element array, the active list and the free list pointer-sized (40 bytes). */
typedef struct VoiceLists {
    u8 flag;
    s32 count, size;
    void *mem;
    Voice *active;
    Voice *other;
} VoiceLists;
#else
typedef struct VoiceLists {
    u8 reserved[16];
    Voice *active;
    Voice *other;
} VoiceLists;
#endif
extern VoiceLists D_80155220;
extern f32 D_8002EB90;
extern void func_8008C074(intptr_t, s32, s32, s32, u8 *, s32, s32);
extern void func_8008D0C0(intptr_t);
extern void func_800AFA84(VoiceLists *, Voice *);
#ifdef TARGET_PC
/* PC: a float to u32 that is negative while a voice's start is ahead of the clock. Undefined in C, and the hosts
 * differ: x86-64 and IDO's code for the N64 go through a signed conversion and keep the bits (-125 is 0xFFFFFF83, no
 * fade), aarch64's fcvtzu gives 0 (the fade at full). So through s32 below 0, as the N64 (docs/port/SANITIZE.md). */
static u32 pc_f2u(f32 x) { return x < 0.0f ? (u32)(s32)x : (u32)x; }
#define F2U pc_f2u
#else
#define F2U (u32)
#endif
void audio_voice_set(void) {
    Voice *p, *next;
    s32 count = 0;
    u32 amount;
    f32 rate;
    for (p = D_80155220.other; p; p = p->next) count++;
    p = D_80155220.active;
    while (p) {
        if (!p->enabled) {
            p = p->next;
            continue;
        }
        {
            rate = (D_8002EB90 - p->start) / (((f32)count + (f32)count)/100.0f + 2.0f);
            amount = F2U(rate * 255.0f + 63.0f);
            if (amount < 255) {
                p->bytes[3] = 255 - amount;
                if (p->bytes[3] > 192) p->bytes[3] = 192;
                func_8008C074(p->handle, 4, 0, 0, p->bytes, 0, 0);
                p = p->next;
            } else {
                next = p->next;
                func_8008D0C0(p->handle);
                func_800AFA84(&D_80155220, p);
                p = next;
            }
        }
    }
}
