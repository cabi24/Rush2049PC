/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * camera_transform (historical label; 0x80097CA0, 2228 B) = N64 AudioSfxServer@audiosfx.c (MAT3 audiosfx.c
 * 0x8009d9ac; Dreamcast audiosfx.c AudioSfxServer): the per-frame sound-effect command server. MAT3 port with
 * the N64 libaudio helper bodies (handles, 127-scaled volume/pan, 8192 pitch scale) as the MAT3 statics.
 * MP_TargetSteerPos = _AudioPlayEffect (locked, IPA s0 / $f20 / $f22 parameters).
 * best: NOT a match. Unit: `FAIL camera_transform: 499 of 557 words differ; compiled body is 556 words | words 189
 * ops 59 | frame 88/88`. Compiled-out devices (disclosed): `do {} while (0);` as the last statement of the instance loop
 * body and `if (i) {}` in the cmd-scan loop (they put the frame at retail's 88 and `sound` in s0 for case 2);
 * `1.f` (not `1.0f`) in the pitch scale keeps 1.0f from being hoisted into $f28 (retail rematerialises it).
 * Group recipe used for sweeps: this file + a private MP_TargetSteerPos copy + __standin_MP_TargetSteerPos (keep both
 * camera_transform and the stand-in), as the installed MP_TargetSteerPos group does.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct tSfxVoice {
    s8 used;            /* 0x00 */
    s8 started;         /* 0x01 */
    s8 stop;            /* 0x02 */
    s8 stopped;         /* 0x03 */
    s8 stopAll;         /* 0x04 */
    s8 fail;            /* 0x05 */
    u8 pad6[2];
    u32 index;          /* 0x08 */
    u16 bank;           /* 0x0C */
    u16 soundId;        /* 0x0E */
    u32 handle;         /* 0x10 */
    u32 pad14;
} tSfxVoice; /* 0x18 */

typedef struct tDLLE {
    struct tDLLE *next; /* 0x00 */
    struct tDLLE *prev; /* 0x04 */
} tDLLE;

typedef struct tSfxCmd {
    tDLLE dlle;         /* 0x00 */
    u8 cmd;             /* 0x08 */
    u8 failCounter;     /* 0x09 */
    u8 padA[2];
    tSfxVoice *sound;   /* 0x0C */
    f32 vol;            /* 0x10 */
    f32 lrpan;          /* 0x14 */
    f32 fbpan;          /* 0x18 */
    f32 pitch;          /* 0x1C */
} tSfxCmd; /* 0x20 */

typedef struct tDLList {
    u32 pad0[2];
    void *head;         /* 0x08 */
    void *tail;         /* 0x0C */
} tDLList;

extern tSfxVoice *D_80144C48;           /* gSfxInstances */
extern s32 D_80144DB8;                  /* gNumChannels */
extern s32 D_801460C8[5];               /* gStoppedChannels */
extern s32 D_801460F4;                  /* gNumInstances */
extern s8 D_8011028C;                   /* gStopAllEffects */
extern s8 D_80110284;                   /* gInited */
extern tDLList D_80146160;              /* gCmdQ */
extern tDLList D_80146138;              /* gFreeQ */

#define gSfxInstances D_80144C48
#define gNumChannels D_80144DB8
#define gStoppedChannels D_801460C8
#define gNumInstances D_801460F4
#define gStopAllEffects D_8011028C
#define gInited D_80110284
#define gCmdQ D_80146160
#define gFreeQ D_80146138

void audio_timing_sync(u32 sound);                              /* AudioStopEffect */
void func_8009211C(tDLList *list, void *e);                     /* RemoveLLE */
void func_80091FBC(tDLList *list, void *e, void *before);        /* InsertLLE */
s32 MP_TargetSteerPos(tSfxVoice *sound, f32 vol, f32 pan);      /* _AudioPlayEffect */
s32 func_8001FE58(u32 handle);
s32 func_8001FEA4(u32 handle, u8 vol);
s32 func_8001FF4C(u32 handle, u8 pan);
s32 func_8001FFA0(u32 handle, u16 pitch);
s32 func_8001FFF4(u32 handle, u8 pan);
u32 func_800201D0(u32 handle);
void func_80020518(u8 mono);

#define _FreeSfxInstance(theSound) { (theSound)->used = 0; (theSound)->index = 0; }

static void _FreeCmd(tSfxCmd *cmd)
{
    func_8009211C(&gCmdQ, cmd);
    func_80091FBC(&gFreeQ, cmd, gFreeQ.head);
}

static s32 _AudioEffectPlaying(tSfxVoice *sound)
{
    return func_800201D0(sound->handle) != (u32)-1;
}

static s32 _AudioStopEffect(tSfxVoice *sound)
{
    if (sound->handle == (u32)-1) {
        _FreeSfxInstance(sound);
        return 1;
    }
    if (!sound->stopped && func_8001FE58(sound->handle)) {
        return 0;
    }
    sound->stopped = 1;
    if (func_800201D0(sound->handle) != (u32)-1) {
        return 0;
    }
    _FreeSfxInstance(sound);
    return 1;
}

static s32 _AudioSetEffectVolume(tSfxVoice *sound, f32 vol)
{
    if (sound->handle == (u32)-1) {
        return 1;
    }
    if (func_8001FEA4(sound->handle, (u32)(vol * 127.0f))) {
        return 0;
    }
    return 1;
}

static s32 _AudioSetEffectLRPan(tSfxVoice *sound, f32 pan)
{
    if (sound->handle == (u32)-1) {
        return 1;
    }
    if (func_8001FFF4(sound->handle, (u32)((pan + 1.0f) * 0.5f * 127.0f))) {
        return 0;
    }
    return 1;
}

static s32 _AudioSetEffectFBPan(tSfxVoice *sound, f32 pan)
{
    if (sound->handle == (u32)-1) {
        return 1;
    }
    if (func_8001FF4C(sound->handle, (u32)((pan + 1.0f) * 0.5f * 127.0f))) {
        return 0;
    }
    return 1;
}

static s32 _AudioSetEffectPitch(tSfxVoice *sound, f32 pitch)
{
    f32 p;

    if (sound->handle == (u32)-1) {
        return 1;
    }
     p = pitch * 8192.0f - 1.f;
    if (p < 0.0f) p = 0.0f;
    if (func_8001FFA0(sound->handle, (u32)p)) {
        return 0;
    }
    return 1;
}

static s32 _AudioSetMono(s32 mono)
{
    func_80020518(mono);
    return 1;
}

void camera_transform(void)
{
    tSfxCmd *cmd;
    tSfxCmd *next;
    s32 i;
    s32 n;

    if (!gInited) {
        return;
    }
    gNumChannels += gStoppedChannels[sizeof(gStoppedChannels) / sizeof(gStoppedChannels[0]) - 1];
    for (i = sizeof(gStoppedChannels) / sizeof(gStoppedChannels[0]) - 1; i > 0; i--) {
        gStoppedChannels[i] = gStoppedChannels[i - 1];
    }
    gStoppedChannels[0] = 0;
    if (gStopAllEffects) {
        n = 0;
        for (i = 0; i < gNumInstances; i++) {
            if (gSfxInstances[i].used && gSfxInstances[i].stopAll && !gSfxInstances[i].stop) {
                if (++n == gNumInstances / 4) {
                    break;
                }
                audio_timing_sync(gSfxInstances[i].index);
            }
        }
        if (i == gNumInstances) {
            gStopAllEffects = 0;
        }
    }
    for (i = 0; i < gNumInstances; i++) {
        if (gSfxInstances[i].used && gSfxInstances[i].started) {
            if (!_AudioEffectPlaying(&gSfxInstances[i])) {
                for (cmd = gCmdQ.tail; cmd; cmd = next) {
                    next = (tSfxCmd *)cmd->dlle.prev;
                    if (cmd->sound == &gSfxInstances[i]) {
    if (i) {}
                        _FreeCmd(cmd);
                    }
                }
                gStoppedChannels[0]++;
                _FreeSfxInstance(&gSfxInstances[i]);
            }
        }
    do {} while (0);
    }
    for (cmd = gCmdQ.tail; cmd; cmd = next) {
        next = (tSfxCmd *)cmd->dlle.prev;
        if (cmd->sound && cmd->sound->stop && cmd->cmd != 2) {
            _FreeCmd(cmd);
            continue;
        }
        if (cmd->sound && cmd->sound->fail) {
            continue;
        }
        switch (cmd->cmd) {
        case 0:
            if (gNumChannels <= 0 || !MP_TargetSteerPos(cmd->sound, cmd->vol, cmd->lrpan)) {
                goto fail;
            }
            gNumChannels--;
            break;
        case 2:
            if (!_AudioStopEffect(cmd->sound)) {
                goto fail;
            }
            gStoppedChannels[0]++;
            break;
        case 3:
            if (cmd->vol != -2.0f && !_AudioSetEffectVolume(cmd->sound, cmd->vol)) {
                goto fail;
            }
            if (cmd->lrpan != -2.0f && !_AudioSetEffectLRPan(cmd->sound, cmd->lrpan)) {
                goto fail;
            }
            if (cmd->fbpan != -2.0f && !_AudioSetEffectFBPan(cmd->sound, cmd->fbpan)) {
                goto fail;
            }
            if (cmd->pitch != -2.0f && !_AudioSetEffectPitch(cmd->sound, cmd->pitch)) {
                goto fail;
            }
            break;
        case 4:
            if (!_AudioSetMono(cmd->vol)) {
                goto fail;
            }
            break;
        }
        _FreeCmd(cmd);
        continue;
    fail:
        cmd->failCounter++;
        if (cmd->sound) {
            cmd->sound->fail = 1;
        }
        if (cmd->failCounter >= 10 && cmd->cmd != 0) {
            _FreeCmd(cmd);
        }
    }
    for (cmd = gCmdQ.tail; cmd; cmd = (tSfxCmd *)cmd->dlle.prev) {
        if (cmd->sound) {
            cmd->sound->fail = 0;
        }
    }
}

