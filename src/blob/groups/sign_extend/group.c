/* Complete N64 wrapper: narrow the third input to a signed 16-bit index.
 * The other three 32-bit inputs pass through unchanged to the real allocator.
 * Build through group.json with the standard whole-program IDO O3 pipeline.
 * N64-specific resource-record code; no arcade source equivalent established.
 */
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif

s32 func_8008E26C(u32 value, uintptr_t data, s16 index, u32 flags);

#ifdef TARGET_PC
/* This wrapper ends in a call to func_8008E26C (MBOX_NewObject), whose result stays in v0 through the void
 * return; every caller reads it. On the PC the wrapper returns it (docs/port/PROTOTYPES.md). */
#define PC_V0_S32 s32
#define PC_V0_RETURN return
#else
#define PC_V0_S32 void
#define PC_V0_RETURN
#endif
PC_V0_S32 sign_extend_call(u32 value, uintptr_t data, s32 index, u32 flags)
{
    PC_V0_RETURN func_8008E26C(value, data, (s16) index, flags);
}
