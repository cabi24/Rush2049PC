#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue { s32 pad[6]; } OSMesgQueue;
typedef struct OSScClient { struct OSScClient *next; OSMesgQueue *msgQ; } OSScClient;
typedef struct OSSched OSSched;

typedef struct tDLLE { void *next; void *prev; } tDLLE;
typedef struct tDLList { char handle; char dbl; int numElements; void *head; void *tail; } tDLList;

typedef struct tBSTE { void *left; void *right; } tBSTE;

typedef struct tFxSequence {
    tDLLE ll;
    tDLList *list;
    unsigned int id;
    int state;
    int type;
    signed char paused;
    signed char loop;
    unsigned char numCmds;
    unsigned char priority;
    int wpriority;
    float startTime;
    float vol;
    float lrpan;
    float fbpan;
    float pitch;
    int fxid;
    int subId;
    unsigned int sysId;
    void *data;
} tFxSequence;

typedef struct anon_te_tag_c { tBSTE te; tFxSequence *tag; } anon_te_tag_c;

typedef struct anon_ll_id_3c {
    tDLLE ll;
    unsigned int id;
    signed char free;
    signed char active;
    signed char stop;
    float startTime;
    float timeout;
    unsigned char next;
    int ids[4];
    int subId;
    int replace;
    unsigned char priority;
    int sndTag;
} anon_ll_id_3c;

typedef struct { signed short type; char cmd; signed char used; } anon_type_cmd_4;
typedef struct { anon_type_cmd_4 hdr; int song; signed char loop; } anon_hdr_song_c;
typedef struct { anon_type_cmd_4 hdr; int replace; float vol; float lrpan; float fbpan; tFxSequence *tag; } anon_hdr_replace_18;
typedef struct { anon_type_cmd_4 hdr; tFxSequence *tag; } anon_hdr_tag_8;
typedef struct { anon_type_cmd_4 hdr; float vol; float lrpan; float fbpan; float pitch; tFxSequence *tag; } anon_hdr_vol_18;
typedef struct { anon_type_cmd_4 hdr; float vol; float time; signed char music; signed char sfx; } anon_hdr_vol_10;
typedef struct { anon_type_cmd_4 hdr; signed char mono; } anon_hdr_mono_6;
typedef union tAudioMsg {
    anon_type_cmd_4 hdr;
    anon_hdr_song_c playSong;
    anon_hdr_replace_18 playEffect;
    anon_hdr_tag_8 startEffect;
    anon_hdr_vol_18 changeEffect;
    anon_hdr_tag_8 intEffect;
    anon_hdr_tag_8 stopEffect;
    anon_hdr_vol_10 setMasterVol;
    anon_hdr_mono_6 mono;
} tAudioMsg;

VIEW(OSSched, gScheduler);
#define gScheduler gScheduler_view
VIEW(OSMesgQueue, audio_lock_queue);
#define audio_lock_queue audio_lock_queue_view
VIEW(OSMesgQueue, audio_command_queue);
#define audio_command_queue audio_command_queue_view

/* PC: the priority pass's command lists and the messages they hold are PC data, PC_SND_MSG_SLOTS long
 * (pc_game_types.h, docs/port/DECOUPLE.md 9, AUDIO.md 10.4): a pass sends at most a message each, and the manager's
 * queue holds the run's messages, PC_SND_MSG_LOOP(128): 128 at original. */
#define gStartCmds (*(tAudioMsg *(*)[PC_SND_MSG_SLOTS])pc_snd_start_cmds)
#define gStopCmds (*(tAudioMsg *(*)[PC_SND_MSG_SLOTS])pc_snd_stop_cmds)
#define PC_SND_MSG_LOOP(n) (pc_snd_messages())

VIEW(tDLList, gPlayingFxTags);
#define gPlayingFxTags gPlayingFxTags_view
VIEW(tDLList, gInterruptedFxTags);
#define gInterruptedFxTags gInterruptedFxTags_view
VIEW(tDLList, gActiveSequences);
#define gActiveSequences gActiveSequences_view

#define gInited gInited_AudioMgr

void osCreateMesgQueue(OSMesgQueue *, OSMesg *, s32);

s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
s32 osSendMesg(OSMesgQueue *, OSMesg, s32);

void AudioSongStop(int song);

/* ClearBST and WalkBST of the fx tag tree, whose tOrderedList has the PC's layout (the pointers are the host's, so
 * the root is at +16): the game's own ClearBST and WalkBST read the N64's offsets (the root at +8). */
static void clear_fx_tag_tree(tOrderedList *tree)
{
    tree->root = 0;
    tree->numElements = 0;
}

static void walk_fx_tag_tree(tOrderedList *tree, void (*WalkFunc)(void *, int))
{
    int num = 0; WalkBSTInternal(tree, tree->root, &num, WalkFunc);
}

/* SHAPING (code-free): an inlined call whose int result is discarded. uopt keeps the `str $2`, so the block that
 * holds it survives jump threading; ugen emits `move v0,zero` and as1 deletes it, leaving retail's separate
 * `b end; nop` blocks (see the header of audio_manager_thread_entry). Stands in for an unknown N64 statement. */
static int _AudioNop(void) { return 0; }

static void _InsertSorted(tFxSequence *tag)
{
    anon_te_tag_c *stag;
    stag = &gSortedFxTags[gSortedFxTagTree.numElements];
    stag->tag = tag;
    InsertBSTE(&gSortedFxTagTree, stag);
}

/* PC: a start or stop the full queue refuses gives its message back (transform/group.c's AudChangeEffect says
 * when the queue and the pool run out); the next priority pass sends it again. */
static void send_fx_command(tAudioMsg *cmd)
{
    if (osSendMesg(&audio_command_queue, cmd, 0) != 0) {
        cmd->startEffect.tag->numCmds--;
        cmd->hdr.used = 0;
    }
}
/* PC: a start for a tag already playing starts nothing. The priority pass runs on this thread, at a retrace's message,
 * and sends its starts behind any retrace messages already queued: when the host holds the thread back by a retrace
 * or more, the next pass finds the tag still waiting and sends its start again. On the N64 the thread empties the
 * queue each retrace and that never happens; a second start would give the tag a second voice and lose the first,
 * a loop then playing on with nothing to stop it (with more voices nothing takes it back: docs/port/AUDIO.md 10.6). */
#define PC_START_PLAYING(tag) if ((tag)->state == 2) { pc_snd_count(PC_SND_START_PLAYING); break; }

static void _ProcessPriorities(void)
{
    tFxSequence *tag, *next;
    int i;
    osRecvMesg(&audio_lock_queue, 0, 1);
    clear_fx_tag_tree(&gSortedFxTagTree); pc_snd_walk_begin();
    gNumStopCmds = 0;
    gNumStartCmds = 0;
    for (tag = gPlayingFxTags.head; tag; tag = next) {
        next = tag->ll.next;
        if (!AudioEffectPlaying(tag->sysId)) {
            if (tag->loop) InterruptFxTag(tag);
            else FreeFxTag(tag);
        } else {
            tag->wpriority = WeightedPriority(tag);
            _InsertSorted(tag);
        }
    }
    gNumFxPlaying = gPlayingFxTags.numElements;
    gNumFxPending = gNumFxInterrupted = 0;
    for (tag = gInterruptedFxTags.head; tag; tag = next) {
        next = tag->ll.next;
        if (!tag->loop) {
            tag->startTime -= 1.0f;
            if (tag->startTime < -10.0f) {
                FreeFxTag(tag);
                continue;
            }
        }
        if (tag->state == 1) gNumFxPending++;
        else gNumFxInterrupted++;
        tag->wpriority = WeightedPriority(tag);
        _InsertSorted(tag);
    }
    walk_fx_tag_tree(&gSortedFxTagTree, UpdateFx);
    osSendMesg(&audio_lock_queue, 0, 0);
    for (i = 0; i < gNumStopCmds; i++) send_fx_command(gStopCmds[i]);
    for (i = 0; i < gNumStartCmds; i++) send_fx_command(gStartCmds[i]);
}

/* MAT3 fake.c MyGetTime(); on N64 the six calls of MAT3 _PeriodicMaintenance survive only as inline sites (their
 * timer arithmetic is not in the N64 code): each site reserves 8 frame bytes, which makes PeriodicMaintenance's frame
 * 184 exact, and no _PeriodicMaintenance stub precedes PeriodicMaintenance (retail: ProcessEmitters ends at its entry). */
static unsigned int MyGetTime(void) { return 0; }

static void AudioPeriodicMaintenance(void)
{
    osRecvMesg(&audio_lock_queue, 0, 1);
    AudioSfxServer();
    osSendMesg(&audio_lock_queue, 0, 0);
}

/* PeriodicMaintenance = _PeriodicMaintenance@audiomgr.c (MAT3 GC 0x800A1EA4) with _ProcessPriorities inlined. Private IPA
 * callee of the thread (uses s0-s8, f20, f22 without saving them): internal, not kept. */
void PeriodicMaintenance(void)
{
    if (gInited) {
        MyGetTime();
        ProcessEmitters();
        ProcessSequences();
        MyGetTime();
        MyGetTime();
        _ProcessPriorities();
        MyGetTime();
    }
    MyGetTime();
    AudioPeriodicMaintenance();
    MyGetTime();
}

void audio_manager_thread_entry(void *arg)
{
    OSScClient client;
    OSMesg msg;
    tAudioMsg *cmd;
    tFxSequence *tag;
    tFxSequence *next;

    osCreateMesgQueue(&audio_command_queue, audio_cmd_queue_msgs, PC_SND_MSG_LOOP(128));
    osScAddClient(&gScheduler, &client, &audio_command_queue);
    for (;;) {
        osRecvMesg(&audio_command_queue, &msg, 1);
        if (((tAudioMsg *)msg)->hdr.type == 1) {
            PeriodicMaintenance();
        } else {
            cmd = msg;
            if (cmd->hdr.type) {}
            osRecvMesg(&audio_lock_queue, 0, 1);
            switch (cmd->hdr.cmd) {
                goto skip; skip:        /* SHAPING (code-free, unreachable): see header (3) */
            case 0:
                if (audiomgr_gSong != cmd->playSong.song && audiomgr_gSong < 0) {
                    audiomgr_gSong = cmd->playSong.song;
                    AudioSongPlay(audiomgr_gSong, cmd->playSong.loop);
                }
                break;
            case 1:
                if (audiomgr_gSong < 0) {
                    _AudioNop();
                } else {
                    AudioSongStop(audiomgr_gSong);
                    audiomgr_gSong = -1;
                }
                break;
            case 2:
                if (cmd->playEffect.tag->numCmds) {} cmd->playEffect.tag->numCmds--;
                if (cmd->playEffect.tag->state == 0) {
                    FreeFxTag(cmd->playEffect.tag);
                    break;
                }
                cmd->playEffect.tag->vol = cmd->playEffect.vol;
                cmd->playEffect.tag->lrpan = cmd->playEffect.lrpan;
                cmd->playEffect.tag->fbpan = 0.0f;
                cmd->playEffect.tag->pitch = 1.0f;
                if (cmd->playEffect.replace != 0) {
                    for (tag = gPlayingFxTags.head; tag; tag = tag->ll.next) {
                        if (tag != cmd->playEffect.tag && tag->state != 0
                            && tag->fxid == cmd->playEffect.tag->fxid
                            && tag->subId == cmd->playEffect.tag->subId
                            && tag->type == cmd->playEffect.tag->type) break;
                    }
                    if (!tag) {
                        for (tag = gInterruptedFxTags.head; tag; tag = tag->ll.next) {
                            if (tag != cmd->playEffect.tag && tag->state == 1
                                && tag->fxid == cmd->playEffect.tag->fxid
                                && tag->subId == cmd->playEffect.tag->subId
                                && tag->type == cmd->playEffect.tag->type) break;
                        }
                    }
                    if (tag) {
                        if (tag->state == 2 || tag->state == 1) {
                            if (cmd->playEffect.replace == 2) {
                                FreeFxTag(cmd->playEffect.tag);
                                if (tag->state == 1) tag->startTime = -1.0f;
                                _AudioNop();
                                break;
                            } else if (tag->state == 2) AudioStopEffect(tag->sysId);
                        }
                        FreeFxTag(tag);
                    }
                }
                if (cmd->playEffect.fbpan != -2.0f)
                    cmd->playEffect.tag->fbpan = cmd->playEffect.fbpan;
                else cmd->playEffect.tag->fbpan = 0.0f;
                cmd->playEffect.tag->state = 3;
                break;
            case 3:
                if (cmd->startEffect.tag->numCmds) {} cmd->startEffect.tag->numCmds--;
                if (cmd->startEffect.tag->state == 0) {
                    FreeFxTag(cmd->startEffect.tag);
                    break;
                } PC_START_PLAYING(cmd->startEffect.tag)
                cmd->startEffect.tag->sysId = AudioPlayEffect(cmd->startEffect.tag->fxid,
                    cmd->startEffect.tag->vol, cmd->startEffect.tag->lrpan);
                if (cmd->startEffect.tag->sysId == -1) {
                    _AudioNop();
                } else {
                    tag = cmd->startEffect.tag;
                    RemoveLLE(tag->list, tag);
                    InsertLLE(&gPlayingFxTags, tag, gPlayingFxTags.head);
                    tag->list = &gPlayingFxTags;
                    if (cmd->startEffect.tag->fbpan != 0.0f)
                        AudioSetEffectFBPan(cmd->startEffect.tag->sysId, cmd->startEffect.tag->fbpan);
                    if (cmd->startEffect.tag->pitch != 1.0f)
                        AudioSetEffectPitch(cmd->startEffect.tag->sysId, cmd->startEffect.tag->pitch);
                    cmd->startEffect.tag->state = 2;
                    if (gAudioSeconds) {} cmd->startEffect.tag->startTime = gAudioSeconds;
                }
                break;
            case 4:
                if (cmd->changeEffect.tag->numCmds) {} cmd->changeEffect.tag->numCmds--;
                if (cmd->changeEffect.tag->state == 0) {
                    FreeFxTag(cmd->changeEffect.tag);
                    break;
                }
                if (cmd->changeEffect.vol != -2.0f) {
                    if (cmd->changeEffect.tag->state == 2)
                        AudioSetEffectVolume(cmd->changeEffect.tag->sysId, cmd->changeEffect.vol);
                    cmd->changeEffect.tag->vol = cmd->changeEffect.vol;
                }
                if (cmd->changeEffect.lrpan != -2.0f) {
                    if (cmd->changeEffect.tag->state == 2)
                        AudioSetEffectLRPan(cmd->changeEffect.tag->sysId, cmd->changeEffect.lrpan);
                    cmd->changeEffect.tag->lrpan = cmd->changeEffect.lrpan;
                }
                if (cmd->changeEffect.fbpan != -2.0f) {
                    if (cmd->changeEffect.tag->state == 2)
                        AudioSetEffectFBPan(cmd->changeEffect.tag->sysId, cmd->changeEffect.fbpan);
                    cmd->changeEffect.tag->fbpan = cmd->changeEffect.fbpan;
                }
                if (cmd->changeEffect.pitch != -2.0f) {
                    if (cmd->changeEffect.tag->state == 2 && !cmd->changeEffect.tag->paused)
                        AudioSetEffectPitch(cmd->changeEffect.tag->sysId, cmd->changeEffect.pitch);
                    cmd->changeEffect.tag->pitch = cmd->changeEffect.pitch;
                }
                _AudioNop();
                break;
            case 5:
                cmd->intEffect.tag->numCmds--;
                if (cmd->intEffect.tag->state == 0) {
                    FreeFxTag(cmd->intEffect.tag);
                    break;
                }
                AudioStopEffect(cmd->intEffect.tag->sysId);
                if (cmd->intEffect.tag->loop) InterruptFxTag(cmd->intEffect.tag);
                else FreeFxTag(cmd->intEffect.tag);
                break;
            case 6:
                cmd->stopEffect.tag->numCmds--;
                if (cmd->stopEffect.tag->state == 0) {
                    FreeFxTag(cmd->stopEffect.tag);
                    break;
                }
                if (cmd->stopEffect.tag->state == 2) AudioStopEffect(cmd->stopEffect.tag->sysId);
                FreeFxTag(cmd->stopEffect.tag);
                break;
            case 7:
                { anon_ll_id_3c *s; for (s = gActiveSequences.head; s; s = s->ll.next) s->stop = 1; }
                AudioStopAllEffects();
                for (tag = gPlayingFxTags.head; tag; tag = next) {
                    next = tag->ll.next;
                    FreeFxTag(tag);
                }
                for (tag = gInterruptedFxTags.head; tag; tag = next) {
                    next = tag->ll.next;
                    FreeFxTag(tag);
                }
                _AudioNop();
                break;
            case 8:
                for (tag = gPlayingFxTags.head; tag; tag = next) {
                    next = tag->ll.next;
                    if (tag->state == 2) AudioSetEffectPitch(tag->sysId, 0.0f);
                    tag->paused = 1;
                }
                for (tag = gInterruptedFxTags.head; tag; tag = next) {
                    next = tag->ll.next;
                    tag->paused = 1;
                }
                _AudioNop();
                break;
            case 9:
                for (tag = gPlayingFxTags.head; tag; tag = next) {
                    next = tag->ll.next;
                    if (tag->paused && tag->state == 2) {
                        tag->paused = 0;
                        AudioSetEffectPitch(tag->sysId, tag->pitch);
                    }
                }
                for (tag = gInterruptedFxTags.head; tag; tag = next) {
                    next = tag->ll.next;
                    tag->paused = 0;
                }
                _AudioNop();
                break;
            case 10:
                AudioSetMasterVolume(cmd->setMasterVol.vol, (unsigned int)(1000.0f * cmd->setMasterVol.time),
                    cmd->setMasterVol.music, cmd->setMasterVol.sfx);
                break;
            case 11:
                AudioSetMono(cmd->mono.mono);
                break;
            default:
                _AudioNop();
                break;
            }
            cmd->hdr.used = 0;
            audio_unlock();
        }
    }
}
