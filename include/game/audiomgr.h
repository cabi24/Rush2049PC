/* include/game/audiomgr.h: the sound manager's records (MAT3 audiomgr.c and audiosfx.c), in the PC's layout.
 *
 * The game asks for sounds through the sound manager (AudPlayEffect, AudEmitEffect, AudPlaySong ...): each request
 * takes an fx tag (tFxSequence) and posts a message (tAudioMsg) to the audio manager thread, which runs it and,
 * each retrace, the 3D emitters (tSfxEmitter) against the listeners (tSfxListener), the timed sequences
 * (tAudioSequence) and the priority pass (the tags sorted by weighted priority, tSortedFxTag). What it starts goes
 * to the sound server (MAT3 audiosfx.c): an effect instance (tSfxVoice) and a command (tSfxCmd) that
 * AudioSfxServer carries out against the MusyX library. docs/code/audio.md has the whole picture.
 *
 * Every record holds host pointers (its list links first), so each lays out as the PC's compiler does: 8-byte
 * links, and every field after them later than in MAT3's (GameCube) and the N64's 32-bit layout. Field names are
 * MAT3's (decomp/include/rush/rush_types.h); the comments give the PC offsets.
 *
 * Not in game.h: the sound manager thread's file (src/blob/groups/audiomgr/audiomgr_thread.c) and its neighbours
 * still define their own copies of tFxSequence and tAudioMsg; once they include this header instead, these types
 * can move into types.h. Include it after game/game.h. */
#ifndef GAME_AUDIOMGR_H
#define GAME_AUDIOMGR_H

#include "game/types.h"

/* A list element's links (MAT3 ll.c's tDLLE). The lists are AudioList (MAT3 tDLList): InsertLLE puts an element
 * before another (or at the tail), RemoveLLE takes it out. */
typedef struct tDLLE {
    void *next; /* 0x0 */
    void *prev; /* 0x8 */
} tDLLE;

/* An fx tag's state (MAT3 enum kFx*): which list it is on and whether the sound server has a voice for it. */
enum {
    kFxInvalid = 0,     /* free (on gFreeFxTags) */
    kFxPending = 1,     /* asked for, not yet given a voice (on gInterruptedFxTags) */
    kFxPlaying = 2,     /* playing on a voice of the sound server (on gPlayingFxTags) */
    kFxInterrupted = 3  /* a looped sound whose voice the priority pass took away; restarts when it ranks again */
};

/* An fx tag's type (MAT3): a plain effect, an emitter's (data is its tSfxEmitter) or a sequence's step. */
enum {
    kFxStandalone = 0,
    kFxEmitter = 1,
    kFxSequence = 2
};

/* What a new effect does to a playing one with the same effect and sub id (AudPlayEffect's replace). */
enum {
    kFxIndividual = 0,  /* play alongside it */
    kFxReplace = 1,     /* stop it and play the new one */
    kFxNoReplace = 2    /* keep it and drop the new one */
};

/* A sound value a change leaves as it is (AudChangeEffect, the commands' volume, pan and pitch). */
#define AUDIO_NO_CHANGE (-2.0f)

/* An fx tag (MAT3 tFxSequence, 0x58 bytes on the PC): one sound the game asked for. Its handle (id) is
 * (generation << gTagShift) | index into gFxTags; ConvertTag gives the tag of a handle, or NULL once the tag has
 * been freed and reused. */
typedef struct tFxSequence {
    tDLLE ll;           /* 0x0: on list */
    AudioList *list;    /* 0x10: gFreeFxTags, gInterruptedFxTags or gPlayingFxTags */
    u32 id;             /* 0x18: the handle */
    s32 state;          /* 0x1C: kFxInvalid ... kFxInterrupted */
    s32 type;           /* 0x20: kFxStandalone, kFxEmitter, kFxSequence */
    s8 paused;          /* 0x24: AudPauseAllEffects' pitch 0 (the pause command) */
    s8 loop;            /* 0x25: gEffectLoop[fxid]: a looped sound */
    u8 numCmds;         /* 0x26: messages in flight that name this tag: it is freed only when none are */
    u8 priority;        /* 0x27: the caller's priority */
    s32 wpriority;      /* 0x28: the priority pass's weighted priority (WeightedPriority) */
    f32 startTime;      /* 0x2C: GetAudioTime() when it started; -1 before */
    f32 vol;            /* 0x30: 0 to 1 */
    f32 lrpan;          /* 0x34: -1 (left) to 1 (right) */
    f32 fbpan;          /* 0x38: -1 (front) to 1 (back) */
    f32 pitch;          /* 0x3C: 1 is the sound's own pitch */
    s32 fxid;           /* 0x40: the effect (an index of gEffectLoop and the MusyX sfx table) */
    s32 subId;          /* 0x44: the caller's sub id (a car's slot ...): with fxid, what replace compares */
    u32 sysId;          /* 0x48: the sound server's handle (AudioPlayEffect) while playing */
    void *data;         /* 0x50: the emitter (kFxEmitter) or the sequence (kFxSequence) */
} tFxSequence;

/* A node of the priority pass's tree (MAT3's, unnamed): two tree links (tBSTE), then the tag. */
typedef struct anon_te_tag_c {
    void *left;         /* 0x0 */
    void *right;        /* 0x8 */
    tFxSequence *tag;   /* 0x10 */
} tSortedFxTag;

/* A 3D sound source (MAT3 tSfxEmitter, 0x38 bytes on the PC): AudEmitEffect's; each retrace ProcessEmitters gives
 * its tag a volume, pans and pitch from the listeners. */
typedef struct tSfxEmitter {
    tDLLE ll;           /* 0x0: on gFreeEmitters or gActiveEmitters */
    s8 free;            /* 0x10: on gFreeEmitters */
    s8 active;          /* 0x11: on gActiveEmitters */
    f32 pos[3];         /* 0x14 */
    f32 fadeDistance;   /* 0x20: the distance at which the volume has fallen to minVol (0: no fall-off) */
    f32 maxVol;         /* 0x24 */
    f32 minVol;         /* 0x28 */
    f32 basePitch;      /* 0x2C */
    u32 sndTag;         /* 0x30: the fx tag's handle (MAT3: int; the N64 code treats it as unsigned) */
} tSfxEmitter;

/* A listener (MAT3's, unnamed; 0x48 bytes on the PC): one per player's view (StartListeners); its pos, look and up
 * place the emitters' sounds. */
typedef struct tSfxListener {
    tDLLE ll;           /* 0x0: on gFreeListeners or gActiveListeners */
    s8 free;            /* 0x10 */
    s8 active;          /* 0x11 */
    f32 pos[3];         /* 0x14 */
    f32 dir[3];         /* 0x20: the velocity (not used for doppler on the N64) */
    f32 look[3];        /* 0x2C: the forward axis */
    f32 up[3];          /* 0x38 */
} tSfxListener;

/* A timed sequence of up to four effects played one after another (MAT3's, unnamed; 0x48 bytes on the PC):
 * AudPlaySequence's; ProcessSequences starts the next when the last has ended. */
typedef struct tAudioSequence {
    tDLLE ll;           /* 0x0: on gFreeSequences or gActiveSequences */
    u32 id;             /* 0x10: the handle: (generation << gTagShift) | index */
    s8 free;            /* 0x14 */
    s8 active;          /* 0x15 */
    s8 stop;            /* 0x16: set by the stop-all command: freed at the next pass */
    f32 startTime;      /* 0x18 */
    f32 timeout;        /* 0x1C: seconds to wait for the first effect to start */
    u8 next;            /* 0x20: the index in ids of the next effect */
    s32 ids[4];         /* 0x24: the effects; -1 for none */
    s32 subId;          /* 0x34 */
    s32 replace;        /* 0x38 */
    u8 priority;        /* 0x3C */
    s32 sndTag;         /* 0x40: the playing effect's fx tag handle, -1 for none */
} tAudioSequence;

/* A message to the audio manager thread (MAT3 tAudioMsg, 0x20 bytes on the PC): GetCmd takes one from the pool
 * (pc_snd_msgs on the PC), the poster fills it and sends it on audio_command_queue, the thread runs it
 * (ProcessCommand) and gives it back (used = 0). */
typedef struct tAudioMsgHeader {
    s16 type;           /* 0x0: -1 (GetCmd) */
    u8 cmd;             /* 0x2: kAudioCmd* */
    s8 used;            /* 0x3: taken from the pool */
} tAudioMsgHeader;

enum {
    kAudioCmdPlaySong = 0,
    kAudioCmdKillSong = 1,
    kAudioCmdPlayEffect = 2,        /* a new tag: replace a like one, then wait for the priority pass */
    kAudioCmdStartEffect = 3,       /* the priority pass: give the tag a voice */
    kAudioCmdChangeEffect = 4,      /* volume, pans, pitch (AUDIO_NO_CHANGE: keep) */
    kAudioCmdInterruptEffect = 5,   /* the priority pass: take the tag's voice (a loop waits to restart) */
    kAudioCmdStopEffect = 6,
    kAudioCmdStopAllEffects = 7,
    kAudioCmdPauseAllEffects = 8,
    kAudioCmdResumeAllEffects = 9,
    kAudioCmdSetMasterVol = 10,
    kAudioCmdMono = 11
};

typedef union tAudioMsg {
    tAudioMsgHeader hdr;
    struct { tAudioMsgHeader hdr; s32 song; s8 loop; } playSong;                                /* song 0x4, loop 0x8 */
    struct { tAudioMsgHeader hdr; s32 replace; f32 vol, lrpan, fbpan; tFxSequence *tag; } playEffect; /* tag 0x18 */
    struct { tAudioMsgHeader hdr; tFxSequence *tag; } startEffect;                              /* tag 0x8 */
    struct { tAudioMsgHeader hdr; f32 vol, lrpan, fbpan, pitch; tFxSequence *tag; } changeEffect;  /* tag 0x18 */
    struct { tAudioMsgHeader hdr; tFxSequence *tag; } intEffect;
    struct { tAudioMsgHeader hdr; tFxSequence *tag; } stopEffect;
    struct { tAudioMsgHeader hdr; f32 vol, time; s8 music, sfx; } setMasterVol;                 /* time in seconds */
    struct { tAudioMsgHeader hdr; s8 mono; } mono;
} tAudioMsg;

/* An effect instance of the sound server (MAT3 tSfxVoice without its PS2 and GameCube fields; 0x18 bytes). Its
 * handle (index) is (generation << 8) | its index in gSfxInstances. */
typedef struct tSfxVoice {
    s8 used;            /* 0x0 */
    s8 started;         /* 0x1: has a MusyX voice (AudioPlayEffectInternal) */
    s8 stop;            /* 0x2: a stop is queued */
    s8 stopped;         /* 0x3: the voice has been keyed off */
    s8 stopAll;         /* 0x4: AudioStopAllEffects marked it */
    s8 fail;            /* 0x5: a command for it failed this pass: its later commands wait for the next */
    u8 pad6[2];
    u32 index;          /* 0x8: the handle, 0 when free */
    s32 soundId;        /* 0xC: the effect (the MusyX sfx id is its low half) */
    s32 sound;          /* 0x10: the MusyX voice handle (sound_start_sfx), -1 for none */
    s32 unused14;       /* 0x14 */
} tSfxVoice;

/* A sound server command (MAT3 tSfxCmd, 0x30 bytes on the PC), on gCmdQ until AudioSfxServer carries it out. */
enum {
    kSfxCmdPlay = 0,
    kSfxCmdStop = 2,
    kSfxCmdChange = 3,  /* vol, lrpan, fbpan, pitch: AUDIO_NO_CHANGE keeps one */
    kSfxCmdMono = 4     /* vol holds the mono flag */
};

typedef struct tSfxCmd {
    tDLLE dlle;         /* 0x0 */
    u8 cmd;             /* 0x10: kSfxCmd* */
    u8 failCounter;     /* 0x11: passes it has failed; dropped after 10, a play never (MAT3: any after 20) */
    tSfxVoice *sound;   /* 0x18: NULL for the mono command */
    f32 vol;            /* 0x20 */
    f32 lrpan;          /* 0x24 */
    f32 fbpan;          /* 0x28 */
    f32 pitch;          /* 0x2C */
} tSfxCmd;

#endif /* GAME_AUDIOMGR_H */
