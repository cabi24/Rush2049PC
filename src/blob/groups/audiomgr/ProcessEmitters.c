/* ProcessEmitters (MAT3 audiomgr.c _ProcessEmitters): the 3D sound's mix, each audio manager pass: each emitter's
 * volume, left-right pan and front-back pan from where the listeners are (one per player). */
#include "game/game.h"

/* PC: an fx tag (audiomgr_thread.c's tFxSequence) is 88 bytes, its links and list host-sized (docs/port/SYNTH.md). */
typedef struct Effect {u8 pad0[28];s32 kind;u8 pad32[4];s8 state,delay;u8 persistent;u8 pad39[33];s32 voice;void *data;} Effect;
typedef struct Source {struct Source *next,*prev;u32 flags;f32 position[3];f32 radius,nearVolume,farVolume,parameter;s32 handle;} Source;
typedef struct Listener {struct Listener *next,*prev;u32 flags;f32 position[3];u8 pad24[12];f32 right[3],forward[3];} Listener;

/* PC: [audio] engines (port/config/voices.c, docs/port/AUDIO.md 10). Before the pass, each drone engine layer's
 * distance to each listener goes to pc_engines_note (the voices module knows which emitters are the drones' engines),
 * which then picks, per listener, the drones whose engines play; the pass sends each emitter's gain through
 * pc_engine_gain, the chosen ones' as the game computed it, the others' faded to 0. At original nothing is gathered
 * and every gain is the game's. */
static void pc_engines_gather(void)
{
    Source *source;
    Listener *listener;
    f32 direction[3];
    s32 i;
    if (!pc_engines_choosing())
        return;
    for (source = (Source *) gActiveEmitters.head; source; source = source->next) {
        for (i = 0, listener = gActiveListeners.head; listener; listener = listener->next, i++) {
            direction[0] = source->position[0] - listener->position[0];
            direction[1] = source->position[1] - listener->position[1];
            direction[2] = source->position[2] - listener->position[2];
            pc_engines_note(source->handle, i, normalize_vector_or_x(direction));
        }
    }
    pc_engines_choose(gActiveListeners.count);
}
#define PC_ENGINE_GAIN(source, gain) pc_engine_gain((source)->handle, gain)
/* *d = v clamped to 0..1. */
void clamp_gain(f32 *d, f32 v)
{
    *d = v<0.0f?0.0f:v>1.0f?1.0f:v;
}

/* Under the audio lock, for each active emitter: a one-shot effect (kind 2) that has finished frees its tag; a
 * playing one gets, from each listener, a gain falling linearly from its near volume to its far volume over its
 * radius (or its near volume), and the direction to it in the listener's frame (right, forward and their cross
 * product); the gains' sum (clamped to 1) is the volume; the sideways component times (1 - gain) and the
 * front-back component, each averaged weighted by the gains and clamped to -1..1, are the pans; the emitter's
 * parameter is the pitch. */
void ProcessEmitters(void)
{
    Source *source, *next;
    Effect *effect;
    f32 out[4];
    Listener *listener;
    f32 distance, total, totalPan, totalDepth, weight;
    s32 i, kind;
    f32 direction[3], projected[3], basis[3][3];
    f32 panValues[5], depthValues[5], gains[5];

    audio_lock(); pc_engines_gather();
    source=(Source *) gActiveEmitters.head;
    while (source) {
        next=source->next;
        effect=ConvertTag(source->handle);
        kind=effect->kind; pc_engine_state(source->handle, kind);
        if (kind==2 && !effect->persistent && !effect->delay &&
            !AudioEffectPlaying(effect->voice)) {
            FreeFxTag(effect);
        } else if (!effect->state) {
            i=0;
            total=0.0f;
            totalPan=0.0f;
            totalDepth=0.0f;
            out[0]=source->parameter;
            for (listener=gActiveListeners.head; listener; listener=listener->next, i++) {
                direction[0]=source->position[0]-listener->position[0];
                direction[1]=source->position[1]-listener->position[1];
                direction[2]=source->position[2]-listener->position[2];
                distance=normalize_vector_or_x(direction);
                basis[2][0]=listener->right[0];
                basis[2][1]=listener->right[1];
                basis[2][2]=listener->right[2];
                basis[1][0]=listener->forward[0];
                basis[1][1]=listener->forward[1];
                basis[1][2]=listener->forward[2];
                basis[0][0]=basis[1][1]*basis[2][2]-basis[1][2]*basis[2][1];
                basis[0][1]=basis[1][2]*basis[2][0]-basis[1][0]*basis[2][2];
                basis[0][2]=basis[1][0]*basis[2][1]-basis[1][1]*basis[2][0];
                projected[0]=basis[0][0]*direction[0]+basis[0][1]*direction[1]+basis[0][2]*direction[2];
                projected[1]=basis[1][0]*direction[0]+basis[1][1]*direction[1]+basis[1][2]*direction[2];
                projected[2]=basis[2][0]*direction[0]+basis[2][1]*direction[1]+basis[2][2]*direction[2];
                panValues[i]=projected[0];
                depthValues[i]=projected[2];
                if (source->radius>0.0f) {
                    gains[i]=source->nearVolume-(source->nearVolume-source->farVolume)*(distance/source->radius);
                    if (gains[i]<0.0f) gains[i]=0.0f;
                } else gains[i]=source->nearVolume;
                weight=gains[i];
                total+=gains[i];
                panValues[i] = panValues[i] - (panValues[i] * weight);
            }
            clamp_gain(&out[3],total);
            for (i=0; i<gActiveListeners.count; i++) {
                if (total!=0.0f) {
                    totalPan+=panValues[i]*(gains[i]/total);
                    totalDepth+=depthValues[i]*(gains[i]/total);
                }
            }
            out[2]=totalPan < -1.0f ? -1.0f : totalPan>1.0f ? 1.0f : totalPan;
            out[1]=totalDepth < -1.0f ? -1.0f : totalDepth>1.0f ? 1.0f : totalDepth;
            AudChangeEffect(source->handle,PC_ENGINE_GAIN(source, out[3]),out[2],out[1],out[0]);
        }
        source=next;
    }
    audio_unlock();
}

