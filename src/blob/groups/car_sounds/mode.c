#include "game/game.h"

typedef struct LayerState {s32 handle;f32 level,style;u8 other12[8];} LayerState;
typedef struct LayerSet {LayerState layer[3];} LayerSet;
typedef struct ModelView {u8 other0[1564];u16 contact[4];u8 other1572[308];s16 steering;u8 other1882[108];s16 index;u8 other1992[48];f32 power[4];} ModelView;
VIEW(LayerSet[], gRoadNoises);
#define gRoadNoises gRoadNoises_view

void SetSoundVP(LayerState *state,f32 level,f32 style)
{
    if (level!=state->level) {
        state->level=level;
        AudChangeEffectVol(state->handle,level);
    }
    if (style!=state->style) {
        state->style=style;
        AudChangeEffectPitch(state->handle,style);
    }
}
void UpdateRoadSounds(ModelView *model)
{
    s32 i,count0,count1,count2;
    f32 sum0,sum1,sum2;
    f32 fraction,scale;
    s16 slot;
    LayerSet *set;
    slot=model->index;
    count0=0; count1=0; count2=0;
    sum1=0.0f; sum0=0.0f; sum2=0.0f;
    for (i=0;i<4;i++) {
        if (count0|count1|count2|i) {}
        if (model->contact[i]==0) {
            fraction=(f32)(model->steering>>2)/160.0f;
            if(fraction<0.0f) fraction=-fraction;
            fraction=fraction<0.0f ? 0.0f : fraction>1 ? 1 : fraction;
            fraction=1-(1-fraction)*(1-fraction);
            count0++;
            fraction/=count0==1 ? 2.0f : count0==2 ? 4.0f : 8.0f;
            sum0+=fraction;
        } else if(model->contact[i]==1) {
            fraction=(f32)(model->steering>>2)/80.0f;
            if(fraction!=0.0f) {
                if(fraction<0.0f) fraction=-fraction;
                fraction=fraction<0.0f ? 0.0f : fraction>1 ? 1 : fraction;
                fraction=1-(1-fraction)*(1-fraction);
            }
            if(fraction<model->power[i])fraction=model->power[i];
            count1++;
            fraction/=count1==1 ? 2.0f : count1==2 ? 4.0f : 8.0f;
            sum1+=fraction;
        } else if(model->contact[i]==2 || model->contact[i]==3) {
            fraction=(f32)(model->steering>>2)/80.0f;
            count2++;
            if(fraction!=0.0f) {
                if(fraction<0.0f)fraction=-fraction;
                fraction=fraction<0.0f ? 0.0f : fraction>1 ? 1 : fraction;
                fraction=1-(1-fraction)*(1-fraction);
            }
            fraction/=count2==1 ? 2.0f : count2==2 ? 4.0f : 8.0f;
            sum2+=fraction;
        }
    }
    if(sum0<sum1 && sum2<sum1) {
        set=&gRoadNoises[slot];
        if(set->layer[0].handle==-1) {
            AudStopEffect(set->layer[1].handle);
            InitSoundState(&set->layer[1]);
            AudStopEffect(set->layer[2].handle);
            InitSoundState(&set->layer[2]);
            set->layer[0].handle=AudPlayEffect(11,model->index,1,2);
        }
        SetSoundVP(&set->layer[0],sum1*0.7f,(sum1>0.5f ? sum1 : 0.5f)-0.25f);
    } else if(sum0<sum2 && sum1<sum2) {
        set=&gRoadNoises[slot];
        if(set->layer[1].handle==-1) {
            AudStopEffect(set->layer[0].handle);
            InitSoundState(&set->layer[0]);
            AudStopEffect(set->layer[2].handle);
            InitSoundState(&set->layer[2]);
            if(sum2>0.33f)AudPlayEffect(21,model->index,2,1);
            set->layer[1].handle=AudPlayEffect(20,model->index,1,2);
        }
        SetSoundVP(&set->layer[1],sum2,1.0f);
    } else {
        set=&gRoadNoises[slot];
        if(set->layer[2].handle==-1) {
            AudStopEffect(set->layer[0].handle);
            InitSoundState(&set->layer[0]);
            AudStopEffect(set->layer[1].handle);
            InitSoundState(&set->layer[1]);
        }
    }
}
#define FIELD(p,t,o) (*(t *)((u8 *)(p)+(o)))
/* PC: an input record (gPlayers) is 80 bytes, its pointer at +0x48 is 8 (docs/port/POINTERS.md 10). */
#define INPUT_SIZE 80

VIEW(volatile f32, gSeconds);
#define gSeconds gSeconds_view

VIEW(f32[3], unitmatrix_origin);
#define unitmatrix_origin unitmatrix_origin_view
extern u8 gPlayers[];

s32 AudEmitEffect(f32 *,intptr_t,f32,f32,f32,f32,s32,s32,s32,u8);

void AudUpdateEmitter(s32,f32 *,f32 *,f32,f32);

typedef struct {
    u8 pad0[4];
    u16 r0;          /* 0x04 */
    u16 r1;          /* 0x06 */
    u16 b0;          /* 0x08 */
    u16 b1;          /* 0x0A */
    u16 b2;          /* 0x0C */
    u8 padE[2];
    f32 p0;          /* 0x10 */
    f32 p1;          /* 0x14 */
    f32 p2;          /* 0x18 */
    u8 pad1C[4];
} EngTbl; /* 0x20 */
typedef struct {
    EngTbl *tbl;
    LayerState rec[4];
} SndSet; /* 0x54 */
typedef struct MODELDAT {
    u8 pad0[0x22C];
    f32 pos[3];      /* 0x22C */
    u8 pad238[0x404 - 0x238];
    f32 f404;        /* 0x404 */
    u8 pad408[0x7C6 - 0x408];
    s16 slot;        /* 0x7C6 */
    u8 pad7C8[4];
    s8 unk7CC;       /* 0x7CC */
    u8 pad7CD[3];
    s16 rpm;         /* 0x7D0 */
} MODELDAT;

/*
 * update_skid_emitter (HYPOTHESIS): the one deleted-static stub in this file (jr ra; nop before E0050).
 * Modelled as a non-static internal wrapper around E05F0's AudUpdateEmitter call.  Its 4-parameter
 * (16-byte) inline area is exactly the block retail E05F0 needs after the inlined AudChangeEffectPitch
 * area (retail homes: level 180, ehu h 168). Identity unproven.
 */
void update_skid_emitter(LayerState *e, f32 *pos, f32 level, f32 style)
{
    AudUpdateEmitter(e->handle,pos,unitmatrix_origin,level,style);
}
/*
 * UpdateEngineSound: EQUAL (360 words) in the whole-program unit with its REAL caller UpdateCarSounds
 * (jal at 0x800E0688; the parameter is passed in the caller's out-arg slot 0(sp), s0-s8/f20-f30 unsaved).
 * Provisional until UpdateCarSounds itself matches. Body from (per-car engine-loop sound update) with:
 *  - StopSound is NOT redefined: the locked kept body is inlined cross-file by umerge.
 * - colouring tie (8-word residual, b0/b1 float conversions, save 15 each): broken by the
 *    compiled-out `if (rpm < t->b1) {}` in the final else arm (adds one use to the b1 conversion web).
 *    Shaping quirk, disclosed.
 *  - `f32 unused[2]` is still the exact frame residual (retail frame 72; without it 19 words differ).
 */
void UpdateEngineSound(MODELDAT *m) {
    f32 rpm;
    f32 load;
    f32 unused[2];
    s32 slot;
    s32 i;
    SndSet *set;
    EngTbl *t;
    f32 vol;
    f32 pitch;
    f32 *pos;
    f32 *ref;

    slot = m->slot;
    if (gGameSoundsEnabled == 0) {
        return;
    }
    if (m->rpm < 0) {
        rpm = -m->rpm;
    } else {
        rpm = m->rpm;
    }
    load = m->f404;
    if (!(gGameState & 0x400000) && m->unk7CC != 2 && !(rpm < 890.0f)) {
        rpm = 890.0f;
    }
    set = PC_ENGINE_SOUND(slot);
    ref = unitmatrix_origin;
    for (i = 0; i < 2; i++) {
        if (set->rec[i].handle == -1) {
            return;
        }
        t = &set->tbl[i];
        vol = ((f32) t->r0 / (f32) t->r1) * (rpm / t->r0 - 1.0f) + 1.0f;
        vol = vol < 0.0f ? 0.0f : vol > 2.0f ? 2.0f : vol;
        if (rpm < t->b0) {
            pitch = t->p0;
        } else if (rpm < t->b1) {
            pitch = (rpm - t->b0) / (t->b1 - t->b0);
            pitch = (t->p1 - t->p0) * pitch + t->p0;
        } else if (rpm < t->b2) {
            pitch = (rpm - t->b1) / (t->b2 - t->b1);
            pitch = (t->p2 - t->p1) * pitch + t->p1;
        } else {
            if (rpm < t->b1) {}
            pitch = t->p2;
        }
        pitch *= 0.85f + ((load + 200.0f) / 900.0f) * 0.15f;
        if (m->unk7CC == 2) {
            pitch *= 0.8f - 0.05f * (gRace == 2 ? 1 : gNumPlayers);
            if (vol != set->rec[i].style) {
                set->rec[i].style = vol;
                AudChangeEffectPitch(set->rec[i].handle, vol);
            }
            if (pitch != set->rec[i].level) {
                set->rec[i].level = pitch;
                AudChangeEffectVol(set->rec[i].handle, pitch);
            }
        } else {
            pitch *= 0.75f;
            pos = m->pos;
            if (gGameState & 0x400000) {
                if (set->rec[3].handle != -1) {
                    StopSound(&set->rec[3], 1);
                }
            } else if (0.75f < gCountDown) {
                if (set->rec[3].handle == -1 || AudEmitterPlaying(set->rec[3].handle) == 0) {
                    if ((s32) Random(5.0f) == 0) {
                        set->rec[3].handle = AudEmitEffect(ref, (s32) ref, 400.0f, 0.0f, 1.0f, 0.0f,
                            (s32) (Random(3.0f) + 98.0f), slot, 0, 128);
                        AudUpdateEmitter(set->rec[3].handle, pos, ref, 0.8f, -2.0f);
                    }
                }
            }
            if (vol != set->rec[i].style) {
                set->rec[i].style = vol;
            } else {
                vol = -2.0f;
            }
            AudUpdateEmitter(set->rec[i].handle, pos, ref, pitch, vol);
        }
    }
}
/*
 * UpdateCarSounds: EQUAL (332 words) in the whole-program unit with its real callees/callers
 * (30 rows -> 2 words -> 0). Own.rodata 0.6f (0x3F19999A) / 0.1f (0x3DCCCCCD) at
 * 0x8012438C / 0x80124390, checked against the image.
 * Alias (and independently): the FIRST t0 access of the set path (`level != .level`) and of the
 * camera path (`style != ...style`) indexes D_80140640[slot] directly.  uopt f_base_in_reg emits
 * `.noalias $8,$sp` only when the first access after a register-map change has an islda (global address)
 * root; an access through the copy-propagated `entry` variable has an isvar root and marks the pair aliased.
 * Loop: constant bound `i!=4` with
 *  - `if (model->contact[i]) continue;`: the continue edge makes uopt number the latch's constant-4 web
 *    before the body's &sPitchRange web, so 4 wins the save tie (3.333 each) and takes a1, the table a2.
 *  - plain `if (x<value) x=value;` max updates (not ternaries): each removes one basic block.  uopt's
 *    per-procedure callee-saved cost is 0.25 per block; the continue form adds blocks and without these
 *    two removals model's s3 cost (21.55) exceeds its caller-saved cost (21.3) and model leaves s3.
 *  - `level=value=0.0f` in the for-init after i=0, on the loop line: preheader order and as1 line ties.
 * The dead `value` init orders value's web before weighted's (6 words without it). Disclosed.
 * Frame: 11 function-level locals with `level` LAST (retail home 180 = -44), AudChangeEffectPitch
 * inlined (h home 168), then the 16-byte update_skid_emitter camera-wrapper area -> all homes equal retail
 * (140,144,148,168,180,220).  d1/d2/d3 are UNUSED locals that are exactly that frame residual (frame 216
 * without d1): disclosed.
 * Other shaping: AudUpdateEmitter' 3rd parameter is `f32 *`; gCarHorn is `s8 [][13]`;
 * volatile gSeconds; int `+1`; power*D operand order; direct entry->handle reads; `& 0x10` operand order.
 */
#define MODE(m) FIELD(m,s8,1996)
void UpdateCarSounds(ModelView *model)
{
    s32 original_slot=model->index;
    s32 slot;
    LayerState *entry;
    f32 style;
    f32 weighted,value;
    s32 i;
    s32 d1,d2,d3;
    f32 level;
    if (!gGameSoundsEnabled) return;
    if (PC_ENGINE_PHASE(original_slot)) {
        PC_ENGINE_PHASE(original_slot)=0;
        return;
    }
    PC_ENGINE_PHASE(original_slot)=1;
    if (!PC_CAR_SOUNDS_ON(original_slot)) return;
    UpdateEngineSound((MODELDAT *)model);
    slot=model->index;
    if (MODE(model)==2) {
        style=0.5f;
        for(i=0,level=value=0.0f;i!=4;i++) {
            if(model->contact[i]) continue;
            weighted=model->power[i]*sPitchRange[i];
            value=weighted-sPitchRange[i]/2.0f+1;
            if(style<value) style=value;
            value=weighted*0.6f;
            if(level<value) level=value;
        }
        entry=PC_SCREECH(slot);
        if(entry->handle==-1 && level>0.0f) {
            if(MODE(model)==2) {
                entry->handle=AudPlayEffect(36,model->index,2,2);
            } else {
                entry->handle=AudEmitEffect((f32 *)((u8 *)model+556),(s32)unitmatrix_origin,400.0f,0.0f,1.0f,0.0f,36,slot,0,130);
            }
        }
        if(entry->handle!=-1) {
            if(level<=0.0f) {
                if(MODE(model)==2) AudStopEffect(entry->handle);
                else AudStopEmitter(entry->handle);
                InitSoundState(entry);
            } else if(MODE(model)==2) {
                if (level!=PC_SCREECH_LEVEL(slot)) {
                    entry->level=level;
                    AudChangeEffectVol(entry->handle,level);
                }
                if (style!=entry->style) {
                    entry->style=style;
                    AudChangeEffectPitch(entry->handle,style);
                }
            } else {
                if(style!=PC_SCREECH_STYLE(slot)) entry->style=style;
                else style=-2.0f;
                update_skid_emitter(entry,(f32 *)((u8 *)model+556),level,style);
            }
        }
    }
    if(MODE(model)!=2)return;
    UpdateRoadSounds(model);
    UpdateBumps(model);
    if(gSeconds<sLastHornTime[original_slot])sLastHornTime[original_slot]=0.0f;
    if(!(FIELD(model,s32,2004)&0x10) &&
       (FIELD(gPlayers,s32,original_slot*INPUT_SIZE+4)&FIELD(gPlayers,s32,original_slot*INPUT_SIZE+56)) &&
       gSeconds-sLastHornTime[original_slot]>0.1f) {
        sLastHornTime[original_slot]=gSeconds;
        gHornSounds[original_slot]=AudPlayEffect(gCarHorn[original_slot + 1][FIELD(model,u8,8)]+26,original_slot,1,4);
    }
    if(FIELD(model,s16,1628)==1) {
        gBogSounds[original_slot]=AudPlayEffect(25,original_slot,2,1);
        FIELD(model,s16,1628)=2;
    } else if(FIELD(model,s16,1628)==3) {
        AudStopEffect(gBogSounds[original_slot]);
        gBogSounds[original_slot]=-1;
        FIELD(model,s16,1628)=0;
    }
}
