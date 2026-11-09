/* The sound server's work of each retrace (MAT3 audiosfx.c's AudioSfxServer) and the start of an effect on MusyX
 * (the N64 form of MAT3's audiosfxps2.c _AudioPlayEffect). The audio manager thread runs AudioSfxServer after its
 * priority pass: it frees the instances whose sound has ended and carries out the queued commands against MusyX
 * (sound_start_sfx, sound_set_volume ...), retrying the ones MusyX refuses on the next retraces. */
#include "game/game.h"
#include "game/audiomgr.h"

/* The queues in this file's view of the header's (gCmdQ without its volatile). */


/* PC: the volume and pan conversions of AudioPlayEffectInternal are IDO's (N64_F2U, pc_prelude.h). AudioSfxServer's
 * conversions of a volume, a pan, a front/back pan and a pitch to MusyX's units are IDO's inlined (u32)f, whose
 * signed step, (s32)f, is undefined in C for NaN or out of range, which the synthetic cartridge's sounds reach (a
 * NaN front/back pan at a race's set-up: the sanitizers' float-cast-overflow). There IDO's code gives -1, the low
 * byte 0xFF; AArch64 gave 0. */
#define PC_CVT_S32(f) ((f) > -2147483648.0f && (f) < 2147483648.0f ? (s32)(f) : -1)

/* A command that MusyX refused this many times is dropped (but a play command never is; MAT3 drops any after
 * 20). */
#define SFX_CMD_MAX_FAILS 10
/* The stop-all pass stops fewer than this share of the instances per retrace (N64 only). */
#define SFX_STOP_ALL_SHARE 4

/* Starts the effect of instance theSound on MusyX, at volume (0 to 1) and left/right pan (-1 to 1): a MusyX sfx
 * voice for its effect (the low half of soundId: PC note below), whose handle the instance keeps. Returns 1, or 0
 * when MusyX has no voice for it (the command is retried next retrace).
 * PC: soundId is read as the u16 at +0xC, its low half on the PC; the N64's +0xE, the same half there. Read as a
 * whole s32 on the N64 layout it would have been its high half, 0, and every effect would have started sfx 0
 * (docs/port/AUDIO.md 9). */
s32 AudioPlayEffectInternal(tSfxVoice *theSound, f32 volume, f32 pan) {
    if ((theSound->sound = sound_start_sfx(*(u16 *)&theSound->soundId, 0xFF, 0xFF)) == -1) {
        pc_snd_count(PC_SND_FX_REFUSED); return 0;
    }
    sound_set_volume(theSound->sound, (u8) N64_F2U(volume * 127.0f));
    sound_set_pan(theSound->sound, (u8) N64_F2U((pan + 1.0f) * 0.5f * 127.0f));
    theSound->started = 1;
    return 1;
}

#define sound_set_doppler(m2c_zero, identifier, value) sound_set_doppler(identifier, value)

/* Takes cmd off the command queue and puts it back on the free queue (MAT3's _FreeCmd). */
#define FREE_CMD(cmd) (RemoveLLE(&gCmdQ, (cmd)), InsertLLE(&gFreeQ, (cmd), gFreeQ.head))

/* A float in MusyX's byte or word units, IDO's conversion: a negative or invalid value is -1 (PC note above). */
static s32 sfx_units(f32 value) {
    s32 units = PC_CVT_S32(value);
    if (units < 0) {
        units = -1;
    }
    return units;
}

/* Carries out one change command (MAT3's case 3): each of its values that is not AUDIO_NO_CHANGE goes to the
 * instance's MusyX voice (volume 0..1 to 0..127, the pans -1..1 to 0..127, the pitch as a doppler factor of 8192
 * units); a voice that has ended (-1) takes none. Returns 0 when MusyX refused one, so that the command is retried
 * (the values already set are set again then). */
static s32 sfx_change(tSfxCmd *cmd) {
    s32 voice;
    f32 doppler;

    if (cmd->vol != -2.0f) {
        voice = cmd->sound->sound;
        if (voice != -1 && sound_set_volume(voice, sfx_units(cmd->vol * 127.0f) & 0xFF) != 0) {
            return 0;
        }
    }
    if (cmd->lrpan != -2.0f) {
        voice = cmd->sound->sound;
        if (voice != -1 && sound_set_pan(voice, sfx_units((cmd->lrpan + 1.0f) * 0.5f * 127.0f) & 0xFF) != 0) {
            return 0;
        }
    }
    if (cmd->fbpan != -2.0f) {
        voice = cmd->sound->sound;
        if (voice != -1 && sound_set_span(voice, sfx_units((cmd->fbpan + 1.0f) * 0.5f * 127.0f) & 0xFF) != 0) {
            return 0;
        }
    }
    if (cmd->pitch != -2.0f) {
        voice = cmd->sound->sound;
        if (voice != -1) {
            doppler = (cmd->pitch * 8192.0f) - 1.0f;
            if (doppler < 0.0f) {
                doppler = 0.0f;
            }
            if (sound_set_doppler(0, voice, sfx_units(doppler) & 0xFFFF) != 0) {
                return 0;
            }
        }
    }
    return 1;
}

/* Carries out one stop command (MAT3's case 2): keys the voice off once, then frees the instance when the voice
 * has gone (at once if it had none). Returns 0 while the voice still sounds or MusyX refused the key-off: the
 * command is retried next retrace. */
static s32 sfx_stop(tSfxVoice *sound) {
    s32 voice = sound->sound;

    if (voice == -1) {
        sound->used = 0;
        sound->index = 0;
        return 1;
    }
    if (sound->stopped == 0 && sound_key_off(voice) != 0) {
        return 0;
    }
    sound->stopped = 1;
    if (sound_check(sound->sound) != -1) {
        return 0;
    }
    sound->used = 0;
    sound->index = 0;
    return 1;
}

/* Each retrace: gives back the channels freed last retrace; stops a share of the instances AudioStopAllEffects
 * marked; frees the instances whose MusyX voice has ended (and their queued commands); then runs the queue from
 * its oldest command. A command whose instance is being stopped is dropped (but the stop itself), one whose
 * instance failed this retrace waits, and one that MusyX refuses marks its instance failed (so its later commands
 * wait for the next retrace, in order) and is kept for the next retrace, up to SFX_CMD_MAX_FAILS times. */
void AudioSfxServer(void) {
    /* gStoppedChannels, s32[5]: the channels stopped this retrace in [4] */
    s32 *stopped = (s32 *) &gStoppedChannels;
    s32 *count;
    s32 older;
    s32 numInstances;
    s32 found;
    s32 i;
    s32 ok;
    tSfxVoice *sound;
    tSfxCmd *cmd;
    tSfxCmd *next;

    if (audiosfx_gInited == 0) {
        return;
    }
    gNumChannels += stopped[4];
    /* Original game: meant to age the counts, it copies each of stopped[3] .. stopped[0] onto itself (as the
     * decompiled N64 code reads), so only [4] is used: as in MAT3, which keeps one count, the channels freed on one
     * retrace come back on the next. */
    count = &stopped[4];
    do {
        older = count[-1];
        count = count - 1;
        *count = older;
    } while ((uintptr_t) count >= (uintptr_t) &stopped[1]);
    stopped[4] = 0;

    if (gStopAllEffects != 0) {
        numInstances = gNumInstances;
        found = 0;
        for (i = 0; i < numInstances; i++) {
            sound = &gSfxInstances[i];
            if (sound->used != 0 && sound->stopAll != 0 && sound->stop == 0) {
                /* the N64 stops fewer than a quarter of them per retrace: the rest wait for the next */
                if (++found == numInstances / SFX_STOP_ALL_SHARE) {
                    break;
                }
                AudioStopEffect(sound->index);
            }
        }
        if (i == numInstances) {
            gStopAllEffects = 0;
        }
    }

    /* MAT3 also updates each playing instance here (_AudioUpdateEffect, the GameCube's pitch glide); the N64 only
     * frees the ended ones. */
    for (i = 0; i < gNumInstances; i++) {
        sound = &gSfxInstances[i];
        if (sound->used != 0 && sound->started != 0 && sound_check(sound->sound) == -1) {
            for (cmd = gCmdQ.tail; cmd != NULL; cmd = next) {
                next = cmd->dlle.prev;
                if (cmd->sound == sound) {
                    FREE_CMD(cmd);
                }
            }
            stopped[4] += 1;
            sound->index = 0;
            sound->used = 0;
        }
    }

    for (cmd = gCmdQ.tail; cmd != NULL; cmd = next) {
        sound = cmd->sound;
        next = cmd->dlle.prev;
        if (sound != NULL && sound->stop != 0 && cmd->cmd != kSfxCmdStop) {
            FREE_CMD(cmd);
            continue;
        }
        if (sound != NULL && sound->fail != 0) {
            continue;
        }
        switch (cmd->cmd) {
        case kSfxCmdPlay:
            ok = gNumChannels > 0 && AudioPlayEffectInternal(sound, cmd->vol, cmd->lrpan) != 0;
            if (ok) {
                gNumChannels -= 1;
            }
            break;
        case kSfxCmdStop:
            ok = sfx_stop(sound);
            if (ok) {
                stopped[4] += 1;
            }
            break;
        case kSfxCmdChange:
            ok = sfx_change(cmd);
            break;
        case kSfxCmdMono:
            sound_set_mono((s32) cmd->vol & 0xFF);
            ok = 1;
            break;
        default:
            ok = 1;
            break;
        }
        if (ok) {
            FREE_CMD(cmd);
        } else {
            cmd->failCounter = (u8) (cmd->failCounter + 1);
            if (cmd->sound != NULL) {
                cmd->sound->fail = 1;
            }
            if ((s32) cmd->failCounter >= SFX_CMD_MAX_FAILS && cmd->cmd != kSfxCmdPlay) {
                FREE_CMD(cmd);
            }
        }
    }

    for (cmd = gCmdQ.tail; cmd != NULL; cmd = cmd->dlle.prev) {
        if (cmd->sound != NULL) {
            cmd->sound->fail = 0;
        }
    }
}
