/* ProcessEmitters (MAT3 audiomgr.c _ProcessEmitters): the 3D sound's mix, each audio manager pass: each emitter's
 * volume, left-right pan and front-back pan from where the listeners are (one per player). */
#include "game/game.h"
#include "game/audiomgr.h"

/* PC: [audio] engines (port/config/voices.c, docs/port/AUDIO.md 10). Before the pass, each drone engine layer's
 * distance to each listener goes to pc_engines_note (the voices module knows which emitters are the drones' engines),
 * which then picks, per listener, the drones whose engines play; the pass sends each emitter's gain through
 * pc_engine_gain, the chosen ones' as the game computed it, the others' faded to 0. At original nothing is gathered
 * and every gain is the game's. */
static void pc_engines_gather(void)
{
    tSfxEmitter *source;
    tSfxListener *listener;
    f32 direction[3];
    s32 i;
    if (!pc_engines_choosing())
        return;
    for (source = (tSfxEmitter *) gActiveEmitters.head; source; source = source->ll.next) {
        for (i = 0, listener = gActiveListeners.head; listener; listener = listener->ll.next, i++) {
            direction[0] = source->pos[0] - listener->pos[0];
            direction[1] = source->pos[1] - listener->pos[1];
            direction[2] = source->pos[2] - listener->pos[2];
            pc_engines_note(source->sndTag, i, normalize_vector_or_x(direction));
        }
    }
    pc_engines_choose(gActiveListeners.count);
}
#define PC_ENGINE_GAIN(source, gain) pc_engine_gain((source)->sndTag, gain)
/* *d = v clamped to 0..1. */
void clamp_gain(f32 *d, f32 v)
{
    *d = v<0.0f?0.0f:v>1.0f?1.0f:v;
}

/* Under the audio lock, for each active emitter: a one-shot effect whose tag plays (kFxPlaying), with no message
 * in flight, that has finished frees its tag; one not paused gets, from each listener, a gain falling linearly from
 * its maxVol to its minVol over its fadeDistance (or its maxVol), and the direction to it in the listener's frame
 * (look, up and their cross product); the gains' sum (clamped to 1) is the volume; the sideways component times
 * (1 - gain) and the front-back component, each averaged weighted by the gains and clamped to -1..1, are the pans;
 * the emitter's basePitch is the pitch. */
void ProcessEmitters(void)
{
    tSfxEmitter *source, *next;
    tFxSequence *tag;
    f32 out[4];
    tSfxListener *listener;
    f32 distance, total, totalPan, totalDepth, weight;
    s32 i, kind;
    f32 direction[3], projected[3], basis[3][3];
    f32 panValues[5], depthValues[5], gains[5];

    audio_lock(); pc_engines_gather();
    source=(tSfxEmitter *) gActiveEmitters.head;
    while (source) {
        next=source->ll.next;
        tag=ConvertTag(source->sndTag);
        kind=tag->state; pc_engine_state(source->sndTag, kind);
        if (kind==kFxPlaying && !tag->numCmds && !tag->loop &&
            !AudioEffectPlaying(tag->sysId)) {
            FreeFxTag(tag);
        } else if (!tag->paused) {
            i=0;
            total=0.0f;
            totalPan=0.0f;
            totalDepth=0.0f;
            out[0]=source->basePitch;
            for (listener=gActiveListeners.head; listener; listener=listener->ll.next, i++) {
                direction[0]=source->pos[0]-listener->pos[0];
                direction[1]=source->pos[1]-listener->pos[1];
                direction[2]=source->pos[2]-listener->pos[2];
                distance=normalize_vector_or_x(direction);
                basis[2][0]=listener->look[0];
                basis[2][1]=listener->look[1];
                basis[2][2]=listener->look[2];
                basis[1][0]=listener->up[0];
                basis[1][1]=listener->up[1];
                basis[1][2]=listener->up[2];
                basis[0][0]=basis[1][1]*basis[2][2]-basis[1][2]*basis[2][1];
                basis[0][1]=basis[1][2]*basis[2][0]-basis[1][0]*basis[2][2];
                basis[0][2]=basis[1][0]*basis[2][1]-basis[1][1]*basis[2][0];
                projected[0]=basis[0][0]*direction[0]+basis[0][1]*direction[1]+basis[0][2]*direction[2];
                projected[1]=basis[1][0]*direction[0]+basis[1][1]*direction[1]+basis[1][2]*direction[2];
                projected[2]=basis[2][0]*direction[0]+basis[2][1]*direction[1]+basis[2][2]*direction[2];
                panValues[i]=projected[0];
                depthValues[i]=projected[2];
                if (source->fadeDistance>0.0f) {
                    gains[i]=source->maxVol-(source->maxVol-source->minVol)*(distance/source->fadeDistance);
                    if (gains[i]<0.0f) gains[i]=0.0f;
                } else gains[i]=source->maxVol;
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
            AudChangeEffect(source->sndTag,PC_ENGINE_GAIN(source, out[3]),out[2],out[1],out[0]);
        }
        source=next;
    }
    audio_unlock();
}

