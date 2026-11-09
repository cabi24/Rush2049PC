/* The audio manager's thread (MAT3 audiomgr.c): the game's sound requests (AudPlaySong, AudPlayEffect ...) arrive
 * as commands on audio_command_queue and are carried out here, on the sound system (AudioSongPlay,
 * AudioPlayEffect ...); each retrace (the scheduler's message, the queue being its client) the periodic pass runs
 * the 3D mix (ProcessEmitters), the sequences, the priorities (which effect tags get a voice) and the sound
 * system's own service. AudioInit (save/AudioInit.c) makes the thread. */
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

/* PC: the priority pass's command lists and the messages they hold are PC data, PC_SND_MSG_SLOTS long
 * (pc_game_types.h, docs/port/DECOUPLE.md 9, AUDIO.md 10.4): a pass sends at most a message each, and the manager's
 * queue holds the run's messages, PC_SND_MSG_LOOP(128): 128 at original. */
#define gStartCmds (*(tAudioMsg *(*)[PC_SND_MSG_SLOTS])pc_snd_start_cmds)
#define gStopCmds (*(tAudioMsg *(*)[PC_SND_MSG_SLOTS])pc_snd_stop_cmds)
#define PC_SND_MSG_LOOP(n) (pc_snd_messages())

#define gInited gInited_AudioMgr

void osCreateMesgQueue(OSMesgQueue *, OSMesg *, s32);

s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
s32 osSendMesg(OSMesgQueue *, OSMesg, s32);

void AudioSongStop(int song);

/* ClearBST and WalkBST of the fx tag tree (tOrderedList, the host layout: the root at +16). The game's ClearBST
 * and WalkBST do the same since 2026-11-08 (until then they read the N64's offsets); these copies stay because
 * port/assets/tests/test_mgrstart.c links this file without them. */
/* Empties the priority tree (ClearBST). */
static void clear_fx_tag_tree(tOrderedList *tree)
{
    tree->root = 0;
    tree->numElements = 0;
}

/* Calls WalkFunc on each tag of the priority tree in order (WalkBST). */
static void walk_fx_tag_tree(tOrderedList *tree, void (*WalkFunc)(void *, int))
{
    int num = 0; WalkBSTInternal(tree, tree->root, &num, WalkFunc);
}

/* The audio manager's commands (tAudioMsg.hdr.cmd). */
#define AUDIO_CMD_PLAY_SONG 0
#define AUDIO_CMD_STOP_SONG 1
#define AUDIO_CMD_PLAY_EFFECT 2     /* a tag set up to start (its start command follows from the priority pass) */
#define AUDIO_CMD_START_EFFECT 3
#define AUDIO_CMD_CHANGE_EFFECT 4
#define AUDIO_CMD_INTERRUPT_EFFECT 5
#define AUDIO_CMD_STOP_EFFECT 6
#define AUDIO_CMD_STOP_ALL_EFFECTS 7
#define AUDIO_CMD_PAUSE_EFFECTS 8
#define AUDIO_CMD_RESUME_EFFECTS 9
#define AUDIO_CMD_MASTER_VOLUME 10
#define AUDIO_CMD_MONO 11
/* An effect tag's state (tFxSequence.state). */
#define FX_TAG_FREE 0
#define FX_TAG_WAITING 1        /* interrupted or not yet started: waits for a voice */
#define FX_TAG_PLAYING 2
#define FX_TAG_STARTING 3       /* asked to play: the priority pass will start it */
/* "Leave this value as it is" in a change command. */
#define AUDIO_NO_CHANGE (-2.0f)

/* Puts tag in the priority tree (gSortedFxTagTree, ordered by OrderFx) through the next free sorted-tag node. */
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
#define PC_START_PLAYING(tag) if ((tag)->state == FX_TAG_PLAYING) { pc_snd_count(PC_SND_START_PLAYING); break; }

/* The priority pass (MAT3 _ProcessPriorities): under the audio lock, frees the finished one-shot tags (a looping
 * one that stopped is interrupted, to start again), ages the waiting ones (a one-shot waiting ten passes is
 * dropped), counts them, and walks all the playing and waiting tags in priority order (UpdateFx), which decides
 * which to stop and which to start; then sends those stop and start commands to this thread's own queue. */
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
        if (tag->state == FX_TAG_WAITING) gNumFxPending++;
        else gNumFxInterrupted++;
        tag->wpriority = WeightedPriority(tag);
        _InsertSorted(tag);
    }
    walk_fx_tag_tree(&gSortedFxTagTree, UpdateFx);
    osSendMesg(&audio_lock_queue, 0, 0);
    for (i = 0; i < gNumStopCmds; i++) send_fx_command(gStopCmds[i]);
    for (i = 0; i < gNumStartCmds; i++) send_fx_command(gStartCmds[i]);
}

/* The sound system's own service (AudioSfxServer), under the audio lock. */
static void AudioPeriodicMaintenance(void)
{
    osRecvMesg(&audio_lock_queue, 0, 1);
    AudioSfxServer();
    osSendMesg(&audio_lock_queue, 0, 0);
}

/* The audio manager's periodic pass, each retrace (MAT3 _PeriodicMaintenance; MAT3 also times its parts): once
 * the manager is up, the 3D mix, the sequences and the priorities; then the sound system's service. */
void PeriodicMaintenance(void)
{
    if (gInited) {
        ProcessEmitters();
        ProcessSequences();
        _ProcessPriorities();
    }
    AudioPeriodicMaintenance();
}

/* The audio manager's thread: makes its command queue (a client of the scheduler, so a retrace message arrives
 * each retrace) and serves it forever: a retrace runs the periodic pass; a command is carried out under the audio
 * lock and its message given back to the pool. Play song: when no song plays. Stop song. Play effect: the tag set up
 * to start, after any tag of the same effect it replaces (replace 2: keep the old and drop the new). Start effect:
 * the tag gets a voice and moves to the playing list. Change effect: its volume, pans and pitch (each unless
 * AUDIO_NO_CHANGE; the pitch kept while paused). Interrupt, stop, stop all (and every sequence), pause and resume
 * all, the master volume, mono. A command for a tag freed meanwhile frees the tag. */
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
        if (((tAudioMsg *)msg)->hdr.type == OS_SC_RETRACE_MSG) {
            PeriodicMaintenance();
        } else {
            cmd = msg;
            osRecvMesg(&audio_lock_queue, 0, 1);
            switch (cmd->hdr.cmd) {
            case AUDIO_CMD_PLAY_SONG:
                if (audiomgr_gSong != cmd->playSong.song && audiomgr_gSong < 0) {
                    audiomgr_gSong = cmd->playSong.song;
                    AudioSongPlay(audiomgr_gSong, cmd->playSong.loop);
                }
                break;
            case AUDIO_CMD_STOP_SONG:
                if (audiomgr_gSong >= 0) {
                    AudioSongStop(audiomgr_gSong);
                    audiomgr_gSong = -1;
                }
                break;
            case AUDIO_CMD_PLAY_EFFECT:
                cmd->playEffect.tag->numCmds--;
                if (cmd->playEffect.tag->state == FX_TAG_FREE) {
                    FreeFxTag(cmd->playEffect.tag);
                    break;
                }
                cmd->playEffect.tag->vol = cmd->playEffect.vol;
                cmd->playEffect.tag->lrpan = cmd->playEffect.lrpan;
                cmd->playEffect.tag->fbpan = 0.0f;
                cmd->playEffect.tag->pitch = 1.0f;
                if (cmd->playEffect.replace != 0) {
                    for (tag = gPlayingFxTags.head; tag; tag = tag->ll.next) {
                        if (tag != cmd->playEffect.tag && tag->state != FX_TAG_FREE
                            && tag->fxid == cmd->playEffect.tag->fxid
                            && tag->subId == cmd->playEffect.tag->subId
                            && tag->type == cmd->playEffect.tag->type) break;
                    }
                    if (!tag) {
                        for (tag = gInterruptedFxTags.head; tag; tag = tag->ll.next) {
                            if (tag != cmd->playEffect.tag && tag->state == FX_TAG_WAITING
                                && tag->fxid == cmd->playEffect.tag->fxid
                                && tag->subId == cmd->playEffect.tag->subId
                                && tag->type == cmd->playEffect.tag->type) break;
                        }
                    }
                    if (tag) {
                        if (tag->state == FX_TAG_PLAYING || tag->state == FX_TAG_WAITING) {
                            if (cmd->playEffect.replace == 2) {
                                FreeFxTag(cmd->playEffect.tag);
                                if (tag->state == FX_TAG_WAITING) tag->startTime = -1.0f;
                                break;
                            } else if (tag->state == FX_TAG_PLAYING) AudioStopEffect(tag->sysId);
                        }
                        FreeFxTag(tag);
                    }
                }
                if (cmd->playEffect.fbpan != AUDIO_NO_CHANGE)
                    cmd->playEffect.tag->fbpan = cmd->playEffect.fbpan;
                else cmd->playEffect.tag->fbpan = 0.0f;
                cmd->playEffect.tag->state = FX_TAG_STARTING;
                break;
            case AUDIO_CMD_START_EFFECT:
                cmd->startEffect.tag->numCmds--;
                if (cmd->startEffect.tag->state == FX_TAG_FREE) {
                    FreeFxTag(cmd->startEffect.tag);
                    break;
                } PC_START_PLAYING(cmd->startEffect.tag)
                cmd->startEffect.tag->sysId = AudioPlayEffect(cmd->startEffect.tag->fxid,
                    cmd->startEffect.tag->vol, cmd->startEffect.tag->lrpan);
                if (cmd->startEffect.tag->sysId != -1) {
                    tag = cmd->startEffect.tag;
                    RemoveLLE(tag->list, tag);
                    InsertLLE(&gPlayingFxTags, tag, gPlayingFxTags.head);
                    tag->list = &gPlayingFxTags;
                    if (cmd->startEffect.tag->fbpan != 0.0f)
                        AudioSetEffectFBPan(cmd->startEffect.tag->sysId, cmd->startEffect.tag->fbpan);
                    if (cmd->startEffect.tag->pitch != 1.0f)
                        AudioSetEffectPitch(cmd->startEffect.tag->sysId, cmd->startEffect.tag->pitch);
                    cmd->startEffect.tag->state = FX_TAG_PLAYING;
                    cmd->startEffect.tag->startTime = gAudioSeconds;
                }
                break;
            case AUDIO_CMD_CHANGE_EFFECT:
                cmd->changeEffect.tag->numCmds--;
                if (cmd->changeEffect.tag->state == FX_TAG_FREE) {
                    FreeFxTag(cmd->changeEffect.tag);
                    break;
                }
                if (cmd->changeEffect.vol != AUDIO_NO_CHANGE) {
                    if (cmd->changeEffect.tag->state == FX_TAG_PLAYING)
                        AudioSetEffectVolume(cmd->changeEffect.tag->sysId, cmd->changeEffect.vol);
                    cmd->changeEffect.tag->vol = cmd->changeEffect.vol;
                }
                if (cmd->changeEffect.lrpan != AUDIO_NO_CHANGE) {
                    if (cmd->changeEffect.tag->state == FX_TAG_PLAYING)
                        AudioSetEffectLRPan(cmd->changeEffect.tag->sysId, cmd->changeEffect.lrpan);
                    cmd->changeEffect.tag->lrpan = cmd->changeEffect.lrpan;
                }
                if (cmd->changeEffect.fbpan != AUDIO_NO_CHANGE) {
                    if (cmd->changeEffect.tag->state == FX_TAG_PLAYING)
                        AudioSetEffectFBPan(cmd->changeEffect.tag->sysId, cmd->changeEffect.fbpan);
                    cmd->changeEffect.tag->fbpan = cmd->changeEffect.fbpan;
                }
                if (cmd->changeEffect.pitch != AUDIO_NO_CHANGE) {
                    if (cmd->changeEffect.tag->state == FX_TAG_PLAYING && !cmd->changeEffect.tag->paused)
                        AudioSetEffectPitch(cmd->changeEffect.tag->sysId, cmd->changeEffect.pitch);
                    cmd->changeEffect.tag->pitch = cmd->changeEffect.pitch;
                }
                break;
            case AUDIO_CMD_INTERRUPT_EFFECT:
                cmd->intEffect.tag->numCmds--;
                if (cmd->intEffect.tag->state == FX_TAG_FREE) {
                    FreeFxTag(cmd->intEffect.tag);
                    break;
                }
                AudioStopEffect(cmd->intEffect.tag->sysId);
                if (cmd->intEffect.tag->loop) InterruptFxTag(cmd->intEffect.tag);
                else FreeFxTag(cmd->intEffect.tag);
                break;
            case AUDIO_CMD_STOP_EFFECT:
                cmd->stopEffect.tag->numCmds--;
                if (cmd->stopEffect.tag->state == FX_TAG_FREE) {
                    FreeFxTag(cmd->stopEffect.tag);
                    break;
                }
                if (cmd->stopEffect.tag->state == FX_TAG_PLAYING) AudioStopEffect(cmd->stopEffect.tag->sysId);
                FreeFxTag(cmd->stopEffect.tag);
                break;
            case AUDIO_CMD_STOP_ALL_EFFECTS:
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
                break;
            case AUDIO_CMD_PAUSE_EFFECTS:
                for (tag = gPlayingFxTags.head; tag; tag = next) {
                    next = tag->ll.next;
                    if (tag->state == FX_TAG_PLAYING) AudioSetEffectPitch(tag->sysId, 0.0f);
                    tag->paused = 1;
                }
                for (tag = gInterruptedFxTags.head; tag; tag = next) {
                    next = tag->ll.next;
                    tag->paused = 1;
                }
                break;
            case AUDIO_CMD_RESUME_EFFECTS:
                for (tag = gPlayingFxTags.head; tag; tag = next) {
                    next = tag->ll.next;
                    if (tag->paused && tag->state == FX_TAG_PLAYING) {
                        tag->paused = 0;
                        AudioSetEffectPitch(tag->sysId, tag->pitch);
                    }
                }
                for (tag = gInterruptedFxTags.head; tag; tag = next) {
                    next = tag->ll.next;
                    tag->paused = 0;
                }
                break;
            case AUDIO_CMD_MASTER_VOLUME:
                AudioSetMasterVolume(cmd->setMasterVol.vol, (unsigned int)(1000.0f * cmd->setMasterVol.time),
                    cmd->setMasterVol.music, cmd->setMasterVol.sfx);
                break;
            case AUDIO_CMD_MONO:
                AudioSetMono(cmd->mono.mono);
                break;
            default:
                break;
            }
            cmd->hdr.used = 0;
            audio_unlock();
        }
    }
}
