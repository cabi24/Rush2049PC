#include "game/game.h"

VIEW(s8, gStopAllEffects);
#define gStopAllEffects gStopAllEffects_view

VIEW(intptr_t, gSfxInstances);
#define gSfxInstances gSfxInstances_view

VIEW(s32, gFreeQ);
#define gFreeQ gFreeQ_view

VIEW(s32, gCmdQ);
#define gCmdQ gCmdQ_view

M2C_UNK sound_set_volume();                    /* extern */

/* PC: the sound entry's resource (audiosfx/group.c's Entry, s32 at +0xC) is the effect to start, read
 * as its low half, the u16 at +0xE on the N64 and at +0xC here; +0xE is its high half, 0, so every effect started
 * sfx 0 (docs/port/AUDIO.md 9). The volume and pan conversions are IDO's (pc_prelude.h). */
#define SFX_ENTRY_ITEM 0xC
/* PC: AudioSfxServer's four conversions of a volume, a pan, a front/back pan and a pitch to an unsigned int are
 * IDO's inlined (u32)f (the FCSR's flags read, which the PC's m2c reads as 0); its signed step, (s32)f, is undefined in
 * C for NaN or out of range, which the synthetic cartridge's sounds reach (a NaN front/back pan at a race's set-up:
 * the sanitizers' float-cast-overflow). There IDO's code gives -1, the low byte 0xFF; AArch64 gave 0. */
#define PC_CVT_S32(f) ((f) > -2147483648.0f && (f) < 2147483648.0f ? (s32)(f) : -1)

s32 AudioPlayEffectInternal(void *ipa_s0, f32 ipa_f20, f32 ipa_f22) {
    if ((M2C_FIELD(ipa_s0, s32 *, 0x10) = sound_start_sfx(M2C_FIELD(ipa_s0, u16 *, SFX_ENTRY_ITEM), 0xFF, 0xFF)) == -1) {
        pc_snd_count(PC_SND_FX_REFUSED); return 0;
    }
    sound_set_volume(M2C_FIELD(ipa_s0, s32 *, 0x10), (u8) N64_F2U(ipa_f20 * 127.0f));
    sound_set_pan(M2C_FIELD(ipa_s0, s32 *, 0x10), (u8) N64_F2U((ipa_f22 + 1.0f) * 0.5f * 127.0f));
    M2C_FIELD(ipa_s0, s8 *, 1) = 1;
    return 1;
}

/* PC: AudioSfxServer walks the sound request list (audiosfx/group.c's List and Node), whose pointers
 * are 8 bytes here; its 24-byte entries have none. Its offsets into a node and the list header are these
 * (docs/port/MUSYX_DATA.md 8.4). */
typedef struct { void *prev, *next; u8 kind, busy, pad[2]; void *entry; f32 value, stream, priority, route; } PcSfxNode;
typedef struct { u32 pad[2]; void *tail, *head; } PcSfxList;
#define SFX_NODE_NEXT __builtin_offsetof(PcSfxNode, next)
#define SFX_NODE_KIND __builtin_offsetof(PcSfxNode, kind)
#define SFX_NODE_BUSY __builtin_offsetof(PcSfxNode, busy)
#define SFX_NODE_ENTRY __builtin_offsetof(PcSfxNode, entry)
#define SFX_NODE_VALUE __builtin_offsetof(PcSfxNode, value)
#define SFX_NODE_STREAM __builtin_offsetof(PcSfxNode, stream)
#define SFX_NODE_PRIORITY __builtin_offsetof(PcSfxNode, priority)
#define SFX_NODE_ROUTE __builtin_offsetof(PcSfxNode, route)
#define SFX_LIST_HEAD __builtin_offsetof(PcSfxList, head)

#define sound_set_doppler(m2c_zero, identifier, value) sound_set_doppler(identifier, value)

void AudioSfxServer(void) {
    /* gStoppedChannels, s32[5]: the channels stopped in the last frames, [4] this frame's count */
    s32 *stopped = (s32 *) &gStoppedChannels;
    f32 temp_f0;
    f32 temp_f0_2;
    f32 temp_f0_3;
    f32 temp_f0_4;
    f32 temp_f10;
    f32 temp_f2;
    f32 temp_f4;
    f32 temp_f6;
    f32 var_f2;
    s32 *temp_s2;
    s32 *temp_s2_2;
    s32 *var_s1_2;
    s32 *var_s1_3;
    s32 *var_s1_4;
    s32 *var_v0;
    s32 temp_a0_2;
    s32 temp_a0_3;
    s32 temp_a0_4;
    s32 temp_a1;
    s32 temp_a2;
    s32 temp_t6;
    s32 temp_v0;
    intptr_t temp_v0_2;
    s32 var_a1;
    s32 var_a1_2;
    s32 var_a1_3;
    s32 var_a1_4;
    s32 var_s0;
    s32 var_s0_2;
    s32 var_s1;
    s32 var_s4;
    s32 var_s4_2;
    s32 var_v0_2;
    s32 var_v0_3;
    s32 var_v0_4;
    s32 var_v0_5;
    s32 var_v0_6;
    u8 temp_v0_3;
    void *temp_a0;
    void *temp_a0_5;
    void *temp_v1;
    void *temp_v1_2;
    void *temp_v1_3;

    if (audiosfx_gInited != 0) {
        gNumChannels += stopped[4];
        var_v0 = &stopped[4];
        do {
            temp_t6 = M2C_FIELD(var_v0, s32 *, -4);
            var_v0 = var_v0 - 1;
            *var_v0 = temp_t6;
        } while ((uintptr_t) var_v0 >= (uintptr_t) &stopped[1]);
        stopped[4] = 0;
        if (gStopAllEffects != 0) {
            temp_v0 = gNumInstances;
            var_s4 = 0;
            var_s1 = 0;
            var_s0 = 0;
            if (temp_v0 > 0) {
loop_5:
                temp_v1 = gSfxInstances + var_s0;
                if ((M2C_FIELD(temp_v1, s8 *, 0) != 0) && (M2C_FIELD(temp_v1, s8 *, 4) != 0) && (M2C_FIELD(temp_v1, s8 *, 2) == 0)) {
                    var_s1 += 1;
                    if (var_s1 != (temp_v0 / 4)) {
                        AudioStopEffect(M2C_FIELD(temp_v1, s32 *, 8));
                        goto block_10;
                    }
                } else {
block_10:
                    var_s4 += 1;
                    var_s0 += 0x18;
                    if (var_s4 < temp_v0) {
                        goto loop_5;
                    }
                }
            }
            if (var_s4 == temp_v0) {
                gStopAllEffects = 0;
            }
        }
        var_s4_2 = 0;
        var_s0_2 = 0;
        if (gNumInstances > 0) {
            do {
                temp_v1_2 = gSfxInstances + var_s0_2;
                if ((M2C_FIELD(temp_v1_2, s8 *, 0) != 0) && (M2C_FIELD(temp_v1_2, s8 *, 1) != 0) && (sound_check(M2C_FIELD((var_s0_2 + gSfxInstances), s32 *, 0x10)) == -1)) {
                    var_s1_2 = M2C_FIELD(&gCmdQ, s32 **, SFX_LIST_HEAD);
                    temp_v0_2 = gSfxInstances;
                    if (var_s1_2 != NULL) {
                        do {
                            temp_s2 = M2C_FIELD(var_s1_2, s32 **, SFX_NODE_NEXT);
                            if ((temp_v0_2 + var_s0_2) == M2C_FIELD(var_s1_2, intptr_t *, SFX_NODE_ENTRY)) {
                                RemoveLLE(&gCmdQ, var_s1_2);
                                InsertLLE(&gFreeQ, var_s1_2, M2C_FIELD(&gFreeQ, s32 **, 8));
                            }
                            var_s1_2 = temp_s2;
                        } while (temp_s2 != NULL);
                    }
                    temp_v1_3 = var_s0_2 + temp_v0_2;
                    stopped[4] += 1;
                    M2C_FIELD(temp_v1_3, s32 *, 8) = 0;
                    M2C_FIELD(temp_v1_3, s8 *, 0) = 0;
                }
                var_s4_2 += 1;
                var_s0_2 += 0x18;
            } while (var_s4_2 < gNumInstances);
        }
        var_s1_3 = M2C_FIELD(&gCmdQ, s32 **, SFX_LIST_HEAD);
        if (var_s1_3 != NULL) {
            do {
                temp_a0 = M2C_FIELD(var_s1_3, void **, SFX_NODE_ENTRY);
                temp_s2_2 = M2C_FIELD(var_s1_3, s32 **, SFX_NODE_NEXT);
                if ((temp_a0 != NULL) && (M2C_FIELD(temp_a0, s8 *, 2) != 0) && (M2C_FIELD(var_s1_3, u8 *, SFX_NODE_KIND) != 2)) {
                    RemoveLLE(&gCmdQ, var_s1_3);
                    InsertLLE(&gFreeQ, var_s1_3, M2C_FIELD(&gFreeQ, s32 **, 8));
                } else if ((temp_a0 == NULL) || (M2C_FIELD(temp_a0, s8 *, 5) == 0)) {
                    temp_v0_3 = M2C_FIELD(var_s1_3, u8 *, SFX_NODE_KIND);
                    switch (temp_v0_3) {            /* irregular */
                    case 0:
                        if (gNumChannels > 0) {
                            if (AudioPlayEffectInternal(temp_a0, M2C_FIELD(var_s1_3, f32 *, SFX_NODE_VALUE), M2C_FIELD(var_s1_3, f32 *, SFX_NODE_STREAM)) == 0) {
                                goto block_109;
                            }
                            gNumChannels -= 1;
                        default:
block_108:
                            RemoveLLE(&gCmdQ, var_s1_3);
                            InsertLLE(&gFreeQ, var_s1_3, M2C_FIELD(&gFreeQ, s32 **, 8));
                        } else {
block_109:
                            M2C_FIELD(var_s1_3, u8 *, SFX_NODE_BUSY) = (u8) (M2C_FIELD(var_s1_3, u8 *, SFX_NODE_BUSY) + 1);
                            if (M2C_FIELD(var_s1_3, void **, SFX_NODE_ENTRY) != NULL) {
                                M2C_FIELD(M2C_FIELD(var_s1_3, void **, SFX_NODE_ENTRY), s8 *, 5) = 1;
                            }
                            if (((s32) M2C_FIELD(var_s1_3, u8 *, SFX_NODE_BUSY) >= 0xA) && (M2C_FIELD(var_s1_3, u8 *, SFX_NODE_KIND) != 0)) {
                                RemoveLLE(&gCmdQ, var_s1_3);
                                InsertLLE(&gFreeQ, var_s1_3, M2C_FIELD(&gFreeQ, s32 **, 8));
                            }
                        }
                        break;
                    case 2:
                        temp_a1 = M2C_FIELD(temp_a0, s32 *, 0x10);
                        var_v0_2 = 1;
                        if (temp_a1 == -1) {
                            M2C_FIELD(temp_a0, s8 *, 0) = 0;
                            M2C_FIELD(temp_a0, s32 *, 8) = 0;
                        } else if ((M2C_FIELD(temp_a0, s8 *, 3) == 0) && (sound_key_off(temp_a1, temp_a1) != 0)) {
                            var_v0_2 = 0;
                        } else {
                            M2C_FIELD(temp_a0, s8 *, 3) = 1;
                            if (sound_check(M2C_FIELD(temp_a0, s32 *, 0x10)) != -1) {
                                var_v0_2 = 0;
                            } else {
                                M2C_FIELD(temp_a0, s8 *, 0) = 0;
                                M2C_FIELD(temp_a0, s32 *, 8) = 0;
                                var_v0_2 = 1;
                            }
                        }
                        if (var_v0_2 == 0) {
                            goto block_109;
                        }
                        stopped[4] += 1;
                        goto block_108;
                    case 3:
                        temp_f0 = M2C_FIELD(var_s1_3, f32 *, SFX_NODE_VALUE);
                        if (temp_f0 != -2.0f) {
                            temp_a2 = M2C_FIELD(temp_a0, s32 *, 0x10);
                            if (temp_a2 == -1) {
                                goto block_62;
                            }
                            temp_f6 = temp_f0 * 127.0f;
                            if (M2C_ERROR(/* cfc1 */) & 0x78) {
                                if (!(M2C_ERROR(/* cfc1 */) & 0x78)) {
                                    var_a1 = (s32) (temp_f6 - 2.1474836e9f) | 0x80000000;
                                } else {
                                    goto block_58;
                                }
                            } else {
                                var_a1 = PC_CVT_S32(temp_f6);
                                if (var_a1 < 0) {
block_58:
                                    var_a1 = -1;
                                }
                            }
                            if (sound_set_volume(temp_a2, var_a1 & 0xFF, temp_a2) != 0) {
                                var_v0_3 = 0;
                            } else {
block_62:
                                var_v0_3 = 1;
                            }
                            if (var_v0_3 == 0) {
                                goto block_109;
                            }
                            goto block_65;
                        }
block_65:
                        temp_f0_2 = M2C_FIELD(var_s1_3, f32 *, SFX_NODE_STREAM);
                        if (temp_f0_2 != -2.0f) {
                            temp_a0_2 = M2C_FIELD(M2C_FIELD(var_s1_3, void **, SFX_NODE_ENTRY), s32 *, 0x10);
                            if (temp_a0_2 == -1) {
                                goto block_75;
                            }
                            temp_f4 = (temp_f0_2 + 1.0f) * 0.5f * 127.0f;
                            if (M2C_ERROR(/* cfc1 */) & 0x78) {
                                if (!(M2C_ERROR(/* cfc1 */) & 0x78)) {
                                    var_a1_2 = (s32) (temp_f4 - 2.1474836e9f) | 0x80000000;
                                } else {
                                    goto block_71;
                                }
                            } else {
                                var_a1_2 = PC_CVT_S32(temp_f4);
                                if (var_a1_2 < 0) {
block_71:
                                    var_a1_2 = -1;
                                }
                            }
                            if (sound_set_pan(temp_a0_2, var_a1_2 & 0xFF) != 0) {
                                var_v0_4 = 0;
                            } else {
block_75:
                                var_v0_4 = 1;
                            }
                            if (var_v0_4 == 0) {
                                goto block_109;
                            }
                            goto block_78;
                        }
block_78:
                        temp_f0_3 = M2C_FIELD(var_s1_3, f32 *, SFX_NODE_PRIORITY);
                        if (temp_f0_3 != -2.0f) {
                            temp_a0_3 = M2C_FIELD(M2C_FIELD(var_s1_3, void **, SFX_NODE_ENTRY), s32 *, 0x10);
                            if (temp_a0_3 == -1) {
                                goto block_88;
                            }
                            temp_f10 = (temp_f0_3 + 1.0f) * 0.5f * 127.0f;
                            if (M2C_ERROR(/* cfc1 */) & 0x78) {
                                if (!(M2C_ERROR(/* cfc1 */) & 0x78)) {
                                    var_a1_3 = (s32) (temp_f10 - 2.1474836e9f) | 0x80000000;
                                } else {
                                    goto block_84;
                                }
                            } else {
                                var_a1_3 = PC_CVT_S32(temp_f10);
                                if (var_a1_3 < 0) {
block_84:
                                    var_a1_3 = -1;
                                }
                            }
                            if (sound_set_span(temp_a0_3, var_a1_3 & 0xFF) != 0) {
                                var_v0_5 = 0;
                            } else {
block_88:
                                var_v0_5 = 1;
                            }
                            if (var_v0_5 == 0) {
                                goto block_109;
                            }
                            goto block_91;
                        }
block_91:
                        temp_f2 = M2C_FIELD(var_s1_3, f32 *, SFX_NODE_ROUTE);
                        if (temp_f2 != -2.0f) {
                            temp_a0_4 = M2C_FIELD(M2C_FIELD(var_s1_3, void **, SFX_NODE_ENTRY), s32 *, 0x10);
                            if (temp_a0_4 == -1) {
                                goto block_104;
                            }
                            temp_f0_4 = (temp_f2 * 8192.0f) - 1.0f;
                            if (temp_f0_4 < 0.0f) {
                                var_f2 = 0.0f;
                            } else {
                                var_f2 = temp_f0_4;
                            }
                            if (M2C_ERROR(/* cfc1 */) & 0x78) {
                                if (!(M2C_ERROR(/* cfc1 */) & 0x78)) {
                                    var_a1_4 = (s32) (var_f2 - 2.1474836e9f) | 0x80000000;
                                } else {
                                    goto block_100;
                                }
                            } else {
                                var_a1_4 = PC_CVT_S32(var_f2);
                                if (var_a1_4 < 0) {
block_100:
                                    var_a1_4 = -1;
                                }
                            }
                            if (sound_set_doppler(0, temp_a0_4, var_a1_4 & 0xFFFF) != 0) {
                                var_v0_6 = 0;
                            } else {
block_104:
                                var_v0_6 = 1;
                            }
                            if (var_v0_6 == 0) {
                                goto block_109;
                            }
                            goto block_108;
                        }
                        goto block_108;
                    case 4:
                        sound_set_mono((s32) M2C_FIELD(var_s1_3, f32 *, SFX_NODE_VALUE) & 0xFF);
                        goto block_108;
                    }
                }
                var_s1_3 = temp_s2_2;
            } while (temp_s2_2 != NULL);
        }
        var_s1_4 = M2C_FIELD(&gCmdQ, s32 **, SFX_LIST_HEAD);
        if (var_s1_4 != NULL) {
            do {
                temp_a0_5 = M2C_FIELD(var_s1_4, void **, SFX_NODE_ENTRY);
                if (temp_a0_5 != NULL) {
                    M2C_FIELD(temp_a0_5, s8 *, 5) = 0;
                }
                var_s1_4 = M2C_FIELD(var_s1_4, s32 **, SFX_NODE_NEXT);
            } while (var_s1_4 != NULL);
        }
    }
}

/* stand-in caller: keeps AudioPlayEffectInternal out of line under -O3 */
