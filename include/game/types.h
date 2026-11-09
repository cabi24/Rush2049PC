/* include/game/types.h: the game's types: the basic ones, libultra's, and the structures its data and functions share */
#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include "game/basic.h"

/* libultra's scalar types */
typedef void * OSMesg;
typedef u64 OSTime;
typedef s32 OSPri;
typedef s32 OSId;
typedef u32 OSEvent;
typedef u32 OSIntMask;
typedef u32 OSYieldResult;

/* Structures the header names only through pointers: each file that reads one defines it, as its own view. */
struct AltPath;
struct AudioCacheNode;
struct AudioModel2056;
struct AudioNode;
struct Basis;
struct Blit;
struct Block;
struct BSTNode;
struct Box104;
struct C96Handle;
struct CCar;
struct CCol;
struct CallbackWords;
struct CamNode;
struct CamSlot;
struct Camera;
struct Car;
struct CarA;
struct CarB;
struct CarE;
struct CarState;
struct Cell;
struct Chan;
struct CollisionObject;
struct CollisionReckon;
struct Command;
struct Config64;
struct Cr;
struct Defense;
struct Descriptor;
struct Descriptor72;
struct Dims;
struct DmaReq;
struct Dynamic;
struct EUCar;
struct Effect;
struct EffectObj;
struct EngineState;
struct Entity;
struct Entry;
struct FiveParts;
struct Ghost;
struct GhostHeader;
struct GsmState;
struct GsmStream;
struct H12;
struct Handle;
struct Header;
struct Header88;
struct Heap;
struct In;
struct InputRecord;
struct LayerState;
struct Lights2;
struct Link;
struct LinkNode;
struct List;
struct ListHead;
struct ListNode;
struct LookAt;
struct MODELDAT;
struct MacroCommand;
struct MacroCommandAddAge;
struct MacroCommandKeyOff;
struct MacroCommandSelectVolume;
struct MacroCommandSendFlag;
struct MacroCommandSetAge;
struct MacroCommandSetAgeByVelocity;
struct MacroCommandSetAgeSpeed;
struct MacroCommandSetPitch;
struct MacroCommandSetPitchWheelRange;
struct MacroCommandSetReverbSend;
struct MacroCommandSetupLfo;
struct MacroCommandTrackPitchWheel;
struct MacroCommandUntrapKeyOff;
struct MacroInput;
struct MacroState;
struct MacroStateAddAge;
struct MacroStateAddNote;
struct MacroStateAddPriority;
struct MacroStateCompare;
struct MacroStateEnvelope;
struct MacroStateGosub;
struct MacroStateKeyOff;
struct MacroStateLastNote;
struct MacroStateLoop;
struct MacroStatePitchSweep;
struct MacroStatePlayMacro;
struct MacroStatePortamento;
struct MacroStateReturn;
struct MacroStateScaleVolume;
struct MacroStateSelectVolume;
struct MacroStateSendFlag;
struct MacroStateSendKeyOff;
struct MacroStateSetAdsr;
struct MacroStateSetAge;
struct MacroStateSetAgeByVelocity;
struct MacroStateSetAgeSpeed;
struct MacroStateSetNote;
struct MacroStateSetPitch;
struct MacroStateSetPitchWheelRange;
struct MacroStateSetReverbSend;
struct MacroStateSetupLfo;
struct MacroStateSplitKey;
struct MacroStateSplitModulation;
struct MacroStateSplitRandom;
struct MacroStateSplitVelocity;
struct MacroStateStartSample;
struct MacroStateStopSample;
struct MacroStateTrackPitchWheel;
struct MacroStateTrapKeyOff;
struct MacroStateUntrapKeyOff;
struct MacroStateVibrato;
struct MapRecord;
struct MenuData;
struct MenuItem;
struct MenuNode;
struct MessageQueue;
struct Metadata48;
struct MidiSetup;
struct Model;
struct Model2056;
struct ModelObj;
struct ModelRecord;
struct ModelSnapshot;
struct ModelView;
struct MultiBlit;
struct NameEntry;
struct Nav;
struct Node;
struct Node104;
struct Node16;
struct Node68;
struct OSContPad;
struct OSPfs;
struct OSPfsState;
struct OSScClient_s;
struct OSScTask_s;
struct OSThread_s;
struct Obj;
struct Object;
struct Outer;
struct Owner;
struct P3;
struct PackedKey;
struct PackedLeadingKey;
struct Packet;
struct PadConfig;
struct PakHandle;
struct Palette;
struct PathPoint;
struct PathPt;
struct Pending;
struct Place;
struct Player;
struct PlayerData;
struct PlayerRec;
struct PointList;
struct Poly;
struct Pool;
struct Profile;
struct QNode;
struct RampState;
struct Rec32;
struct RecB;
struct Reckon;
struct Record;
struct Record88;
struct Region;
struct RelativeEntry;
struct ReplaySlot;
struct ResSlot;
struct Resource24;
struct Resource32;
struct ResourceGroup;
struct ResourceOffsets;
struct Respawn;
struct Row20;
struct SalMatrix;
struct SalVector;
struct SampleLookup;
struct SampleRecord;
struct SaveData;
struct Sequence20;
struct SequencePageEntry;
struct SequencePlayParams;
struct SequenceHead;
struct SequenceTimedValue;
struct SfdSampleRange;
struct Shot;
struct ShotObj;
struct Slot;
struct Slot18;
struct SongHeader;
struct SoundEmitterUpdateView;
struct SoundHwSampleInfo;
struct SoundHwVoice;
struct SoundListenerAddView;
struct SoundListenerUpdateView;
struct SoundState;
struct SpatialState;
struct SpeechStreamDecodeView;
struct Sprite;
struct State;
struct StateNode;
struct StreamState;
struct Surface;
struct SynthVoiceMacroView;
struct SynthVoicePanView;
struct SynthVoicePriorityView;
struct SynthVoiceVariablesView;
struct Table;
struct Target;
struct TexDef;
struct TexRect;
struct Texture;
struct Tire;
struct TireDesc;
struct TrackNode;
struct Tree;
struct surftyp;
struct V3s;
struct VCar;
struct Vec3;
struct Vehicle2056;
struct VehicleModel;
struct View;
struct Visual;
struct Voice;
struct VoiceControls;
struct VoicePrefix;
struct VoiceState;
struct WheelSlot;
struct X;
struct XForm;
struct __OSInodeCache;
struct __OSPackId;
struct __OSThreadprofile_s;
struct _tBlit_2;
struct _tSusp;
struct anon_kerns_value_c;
struct anon_te_tag_c;
struct internal_state;
struct tCircuit;
struct tCarData;
struct tFxSequence;     /* the sound manager's records: game/audiomgr.h defines them */
struct tMiniModelData;
struct tModelData;
struct tModelPart;
struct tNewSkid;
struct tPS_File;
struct tPlayerProfile;
struct tSfxCmd;
struct tSfxEmitter;
struct tSfxListener;
struct tSfxVoice;
struct tTexture;
struct tUVMatrix;
struct tVisual;
struct tree_desc_s;
struct z_stream_s;
union Color;
union InputSequence;
union Vtx;
union tRGBA;

/* ---- ultra: libultra and the RSP microcode: the N64 OS interface port/ultra implements */

typedef struct Gwords { /* 0x10 bytes */
    u32 w0;       /* 0x0 */
    uintptr_t w1; /* 0x8 */
} Gwords;

typedef union Gfx { /* 0x10 bytes */
    Gwords words;
    s64 force_structure_alignment;
} Gfx;

typedef struct OSContStatus { /* 0x4 bytes */
    u16 type;  /* 0x0 */
    u8 status; /* 0x2 */
    u8 errno;  /* 0x3 */
} OSContStatus;

typedef struct OSIoMesgHdr { /* 0x10 bytes */
    u16 type;                       /* 0x0 */
    u8 pri;                         /* 0x2 */
    u8 status;                      /* 0x3 */
    struct OSMesgQueue_s *retQueue; /* 0x8 */
} OSIoMesgHdr;

typedef struct OSIoMesg { /* 0x28 bytes */
    OSIoMesgHdr hdr; /* 0x0 */
    void *dramAddr;  /* 0x10 */
    u32 devAddr;     /* 0x18 */
    u32 size;        /* 0x1C */
    void *piHandle;  /* 0x20 */
} OSIoMesg;

typedef struct OSMesgQueue_s { /* 0x28 bytes */
    struct OSThread_s *mtqueue;   /* 0x0 */
    struct OSThread_s *fullqueue; /* 0x8 */
    s32 validCount;               /* 0x10 */
    s32 first;                    /* 0x14 */
    s32 msgCount;                 /* 0x18 */
    OSMesg *msg;                  /* 0x20 */
} OSMesgQueue;

typedef struct OSMesgQueueBytes { /* 0x18 bytes */
    u8 opaque[24]; /* 0x0 */
} OSMesgQueueBytes;

typedef struct OSMesgQueueWords { /* 0x18 bytes */
    s32 w[6]; /* 0x0 */
} OSMesgQueueWords;

typedef struct OSPifRam { /* 0x40 bytes */
    u32 ramarray[15]; /* 0x0 */
    u32 pifstatus;    /* 0x3C */
} OSPifRam;

typedef struct OSTask_t { /* 0x78 bytes */
    u32 type;              /* 0x0 */
    u32 flags;             /* 0x4 */
    u64 *ucode_boot;       /* 0x8 */
    u32 ucode_boot_size;   /* 0x10 */
    u64 *ucode;            /* 0x18 */
    u32 ucode_size;        /* 0x20 */
    u64 *ucode_data;       /* 0x28 */
    u32 ucode_data_size;   /* 0x30 */
    u64 *dram_stack;       /* 0x38 */
    u32 dram_stack_size;   /* 0x40 */
    u64 *output_buff;      /* 0x48 */
    u64 *output_buff_size; /* 0x50 */
    u64 *data_ptr;         /* 0x58 */
    u32 data_size;         /* 0x60 */
    u64 *yield_data_ptr;   /* 0x68 */
    u32 yield_data_size;   /* 0x70 */
} OSTask_t;

typedef struct OSTask { /* 0x78 bytes */
    OSTask_t t; /* 0x0 */
} OSTask;

typedef struct OSThreadBytes { /* 0x1B0 bytes */
    u8 opaque[432]; /* 0x0 */
} OSThreadBytes;

typedef struct OSViCommonRegs { /* 0x24 bytes */
    u32 ctrl;     /* 0x0 */
    u32 width;    /* 0x4 */
    u32 burst;    /* 0x8 */
    u32 vSync;    /* 0xC */
    u32 hSync;    /* 0x10 */
    u32 leap;     /* 0x14 */
    u32 hStart;   /* 0x18 */
    u32 xScale;   /* 0x1C */
    u32 vCurrent; /* 0x20 */
} OSViCommonRegs;

typedef struct OSViFieldRegs { /* 0x14 bytes */
    u32 origin; /* 0x0 */
    u32 yScale; /* 0x4 */
    u32 vStart; /* 0x8 */
    u32 vBurst; /* 0xC */
    u32 vIntr;  /* 0x10 */
} OSViFieldRegs;

typedef struct OSViMode { /* 0x50 bytes */
    u8 type;                  /* 0x0 */
    OSViCommonRegs comRegs;   /* 0x4 */
    OSViFieldRegs fldRegs[2]; /* 0x28 */
} OSViMode;

typedef struct Vtx_t { /* 0x10 bytes */
    s16 ob[3]; /* 0x0 */
    u16 flag;  /* 0x6 */
    s16 tc[2]; /* 0x8 */
    u8 cn[4];  /* 0xC */
} Vtx_t;

typedef union __OSInodeUnit { /* 0x2 bytes */
    struct { u8 bank; u8 page; } inode_t;
    u16 ipage;
} __OSInodeUnit;

typedef struct __OSInode { /* 0x100 bytes */
    __OSInodeUnit inode_page[128]; /* 0x0 */
} __OSInode;

typedef struct __OSThreadContext { /* 0x190 bytes */
    u64 at;       /* 0x0 */
    u64 v0;       /* 0x8 */
    u64 v1;       /* 0x10 */
    u64 a0;       /* 0x18 */
    u64 a1;       /* 0x20 */
    u64 a2;       /* 0x28 */
    u64 a3;       /* 0x30 */
    u64 t0;       /* 0x38 */
    u64 t1;       /* 0x40 */
    u64 t2;       /* 0x48 */
    u64 t3;       /* 0x50 */
    u64 t4;       /* 0x58 */
    u64 t5;       /* 0x60 */
    u64 t6;       /* 0x68 */
    u64 t7;       /* 0x70 */
    u64 s0;       /* 0x78 */
    u64 s1;       /* 0x80 */
    u64 s2;       /* 0x88 */
    u64 s3;       /* 0x90 */
    u64 s4;       /* 0x98 */
    u64 s5;       /* 0xA0 */
    u64 s6;       /* 0xA8 */
    u64 s7;       /* 0xB0 */
    u64 t8;       /* 0xB8 */
    u64 t9;       /* 0xC0 */
    u64 gp;       /* 0xC8 */
    u64 sp;       /* 0xD0 */
    u64 s8;       /* 0xD8 */
    u64 ra;       /* 0xE0 */
    u64 lo;       /* 0xE8 */
    u64 hi;       /* 0xF0 */
    u32 sr;       /* 0xF8 */
    u32 pc;       /* 0xFC */
    u32 cause;    /* 0x100 */
    u32 badvaddr; /* 0x104 */
    u32 rcp;      /* 0x108 */
    u32 fpcsr;    /* 0x10C */
    u64 fp0;      /* 0x110 */
    u64 fp2;      /* 0x118 */
    u64 fp4;      /* 0x120 */
    u64 fp6;      /* 0x128 */
    u64 fp8;      /* 0x130 */
    u64 fp10;     /* 0x138 */
    u64 fp12;     /* 0x140 */
    u64 fp14;     /* 0x148 */
    u64 fp16;     /* 0x150 */
    u64 fp18;     /* 0x158 */
    u64 fp20;     /* 0x160 */
    u64 fp22;     /* 0x168 */
    u64 fp24;     /* 0x170 */
    u64 fp26;     /* 0x178 */
    u64 fp28;     /* 0x180 */
    u64 fp30;     /* 0x188 */
} __OSThreadContext;

typedef struct OSThread { /* 0x1C8 bytes */
    struct OSThread *next;              /* 0x0 */
    s32 priority;                       /* 0x8 */
    struct OSThread **queue;            /* 0x10 */
    struct OSThread *tlnext;            /* 0x18 */
    u16 state;                          /* 0x20 */
    u16 flags;                          /* 0x22 */
    s32 id;                             /* 0x24 */
    s32 fp;                             /* 0x28 */
    struct __OSThreadprofile_s *thprof; /* 0x30 */
    __OSThreadContext context;          /* 0x38 */
} OSThread;

/* ---- system: start-up, threads, the scheduler, timers, the frame pipeline, video, decompression, the overlays */

typedef struct AudioList { /* 0x18 bytes */
    u8 indirect;       /* 0x0 */
    u8 doubly;         /* 0x1 */
    u8 pad[2];         /* 0x2 */
    u32 count;         /* 0x4 */
    struct Link *head; /* 0x8 */
    struct Link *tail; /* 0x10 */
} AudioList;

typedef struct AudioMenuRect { /* 0x10 bytes */
    s32 x;  /* 0x0 */
    s32 w;  /* 0x4 */
    s32 h;  /* 0x8 */
    s32 cx; /* 0xC */
} AudioMenuRect;

typedef struct BackgroundMotion { /* 0x38 bytes */
    f32 bounds[4]; /* 0x0 */
    f32 dx;        /* 0x10 */
    f32 dy;        /* 0x14 */
    f32 xs[4];     /* 0x18 */
    f32 ys[4];     /* 0x28 */
} BackgroundMotion;

typedef struct CoinSlot { /* 0x34 bytes */
    s16 id;      /* 0x0 */
    u8 pad02[2]; /* 0x2 */
    f32 pos[9];  /* 0x4 */
    u8 obj[12];  /* 0x28 */
} CoinSlot;

typedef struct ComponentLight { /* 0x10 bytes */
    u8 col[3];  /* 0x0 */
    u8 pad0;    /* 0x3 */
    u8 colc[3]; /* 0x4 */
    u8 pad1;    /* 0x7 */
    s8 dir[3];  /* 0x8 */
    s8 pad2;    /* 0xB */
    u8 pad3[4]; /* 0xC */
} ComponentLight;

/* An engine sound layer of a car type (MAT3's, unnamed: gEngineSpecs): its effect and how the engine's rpm sets
 * its pitch and volume (the engine update, UpdateEngineSound). */
typedef struct Definition32 { /* 0x20 bytes */
    s32 id;             /* 0x0: the effect, -1 for none */
    u16 basePitchRpm;   /* 0x4: the rpm at which the sound plays at its own pitch */
    u16 pitchRpmRange;  /* 0x6 */
    u16 lowVolRpm;      /* 0x8: the rpm of lowVol, midVol and hiVol */
    u16 midVolRpm;      /* 0xA */
    u16 hiVolRpm;       /* 0xC */
    u8 padE[2];         /* 0xE */
    f32 lowVol;         /* 0x10 */
    f32 midVol;         /* 0x14 */
    f32 hiVol;          /* 0x18 */
    u8 pad1C[4];        /* 0x1C */
} Definition32;

/* A car type's two engine layers (gEngineSpecs[type]). */
typedef struct Definitions64 { /* 0x40 bytes */
    Definition32 item[2]; /* 0x0 */
} Definitions64;

/* A force's peak on one part of a car (MAT3's, unnamed: gBump[car][part], parts 0-3 the body's corners, 4 its
 * centre): GetForceAndPeak keeps the peak of a hit, UpdateBumps plays a bump when it has settled (bump). */
typedef struct Impact24 { /* 0x18 bytes */
    f32 peak;       /* 0x0: the force's peak magnitude */
    f32 peakVec[3]; /* 0x4: the force at its peak */
    f32 time;       /* 0x10: gSeconds when the hit began or peaked, 0 for none */
    s32 bump;       /* 0x14: a settled hit for UpdateBumps to play */
} Impact24;

typedef struct LayerRecord { /* 0xC bytes */
    u32 payload;    /* 0x0 */
    u16 id;         /* 0x4 */
    u16 type;       /* 0x6 */
    u16 references; /* 0x8 */
    u16 metadata;   /* 0xA */
} LayerRecord;

typedef struct PaneColors { /* 0x8 bytes */
    u8 x[4]; /* 0x0 */
    u8 y[4]; /* 0x4 */
} PaneColors;

typedef struct PcPfs { /* 0x70 bytes */
    s32 status;           /* 0x0 */
    void *queue;          /* 0x8 */
    s32 channel;          /* 0x10 */
    u8 id[32];            /* 0x14 */
    u8 label[32];         /* 0x34 */
    s32 version;          /* 0x54 */
    s32 dir_size;         /* 0x58 */
    s32 inode_table;      /* 0x5C */
    s32 minode_table;     /* 0x60 */
    s32 dir_table;        /* 0x64 */
    s32 inode_start_page; /* 0x68 */
    u8 banks;             /* 0x6C */
    u8 activebank;        /* 0x6D */
} PcPfs;

typedef struct PcRespawn { /* 0x10 bytes */
    s8 flag;    /* 0x0 */
    u8 pad1[3]; /* 0x1 */
    f32 pos[3]; /* 0x4 */
} PcRespawn;

typedef struct PcScTaskShape { /* 0xA0 bytes */
    void *next;     /* 0x0 */
    u32 state;      /* 0x8 */
    u32 flags;      /* 0xC */
    void *fb;       /* 0x10 */
    u32 type;       /* 0x18 */
    u32 tflags;     /* 0x1C */
    void *boot;     /* 0x20 */
    u32 boot_size;  /* 0x28 */
    void *ucode;    /* 0x30 */
    u32 ucode_size; /* 0x38 */
    void *udata;    /* 0x40 */
    u32 udata_size; /* 0x48 */
    void *stack;    /* 0x50 */
    u32 stack_size; /* 0x58 */
    void *out;      /* 0x60 */
    void *out_size; /* 0x68 */
    void *data;     /* 0x70 */
    u32 data_size;  /* 0x78 */
    void *yield;    /* 0x80 */
    u32 yield_size; /* 0x88 */
    void *msgQ;     /* 0x90 */
    void *msg;      /* 0x98 */
} PcScTaskShape;

typedef struct FrameBuffer { /* 0xD0 bytes */
    void *buf;          /* 0x0 */
    PcScTaskShape task; /* 0x8 */
    u8 *commands;       /* 0xA8 */
    s16 n;              /* 0xB0 */
    u8 pad[30];         /* 0xB2 */
} FrameBuffer;

typedef struct PcSched { /* 0x2C0 bytes */
    s16 state;                       /* 0x0 */
    u8 pad02[30];                    /* 0x2 */
    s16 priority;                    /* 0x20 */
    u8 pad22[30];                    /* 0x22 */
    u8 cmdQueue[24];                 /* 0x40 */
    u8 cmdMsgs[32];                  /* 0x58 */
    u8 retQueue[24];                 /* 0x78 */
    u8 retMsgs[32];                  /* 0x90 */
    u8 padB0[432];                   /* 0xB0 */
    u8 pad260[28];                   /* 0x260 */
    volatile s32 retraceCount;       /* 0x27C: the retraces since start-up (the scheduler's thread counts them) */
    s32 doAudio;                     /* 0x280 */
    struct OSScClient_s *clientList; /* 0x288 */
    struct OSScTask_s *audioListHead; /* 0x290 */
    struct OSScTask_s *gfxListHead;  /* 0x298 */
    struct OSScTask_s *audioListTail; /* 0x2A0 */
    struct OSScTask_s *gfxListTail;  /* 0x2A8 */
    struct OSScTask_s *volatile curRSPTask; /* 0x2B0: the task on the RSP (scheduler_current_rsp_task) */
    struct OSScTask_s *curRDPTask;   /* 0x2B8 */
} PcSched;

/* An input record (gPlayers, 80 bytes) from its profile handle on (MAT3 tPadState.profile, at +0x48): what
 * gPlayers_save, a label at gPlayers + 0x48, reads per player. */
typedef struct PlayerTail { /* 0x50 bytes */
    struct tProfile **profile; /* 0x0 the player's profile (a handle) */
    u8 pad[72];                /* 0x8 the next player's record up to its profile */
} PlayerTail;

typedef struct __attribute__((packed)) RecordFields { /* 0xC bytes */
    u16 id;                         /* 0x0 */
    u16 count;                      /* 0x2 */
    struct Descriptor *descriptors; /* 0x4 */
} RecordFields;

typedef struct ResourceRange { /* 0x4 bytes */
    u16 count; /* 0x0 */
    u16 first; /* 0x2 */
} ResourceRange;

typedef struct ScreenButton { /* 0x40 bytes */
    s32 type;      /* 0x0 */
    s32 handle;    /* 0x4 */
    f32 ang;       /* 0x8 */
    f32 mat[3][3]; /* 0xC */
    f32 pos[3];    /* 0x30 */
    u8 pad3C[4];   /* 0x3C */
} ScreenButton;

typedef struct ScreenPos { /* 0x8 bytes */
    s32 x; /* 0x0 */
    s32 y; /* 0x4 */
} ScreenPos;

/* How TB_DrawText and the text boxes place a string against its position (gJust, TB_SetJust; MAT3 anon_h_v_8):
 * across against x (kTB_JustLeft, kTB_JustCenter, kTB_JustRight), down against y (kTB_JustTop ...). */
typedef struct TextJust { /* 0x8 bytes */
    s32 h; /* 0x0 */
    s32 v; /* 0x4 */
} TextJust;

/* A frame's record (frame_records[2]: the frame being built and the one being drawn, frame_record_index). The PC's
 * layout: the scheduler's task is the PC's OSScTask. */
typedef struct FrameRecord { /* 0xD0 bytes */
    PcScTaskShape task;     /* 0x0: the frame's RSP task (StartGraphics, EndGraphics) */
    u8 *commands;           /* 0xA0: its command buffer (pc_frame_dl): dynamic data, matrices from +1728, the
                             * display list from PC_GLIST_OFFSET */
    s16 doneMsgType;        /* 0xA8: the message the task sends the main thread when done: OS_SC_DONE_MSG */
    u8 doneMsgRest[30];     /* 0xAA */
    uintptr_t colorBuffer;  /* 0xC8: the frame buffer it draws into (a window address) */
} FrameRecord;

typedef struct SequenceSlotTrack { /* 0x18 bytes */
    void *start;  /* 0x0 */
    void *cursor; /* 0x8 */
    u32 fraction; /* 0x10 */
    u32 time;     /* 0x14 */
} SequenceSlotTrack;

typedef struct SequenceTrack { /* 0x38 bytes */
    u32 fraction;     /* 0x0 */
    s32 whole;        /* 0x4 */
    u32 unknown08;    /* 0x8 */
    u8 *active0C;     /* 0x10 */
    u8 *active10;     /* 0x18 */
    u8 *active14;     /* 0x20 */
    u8 unknown18[16]; /* 0x28 */
} SequenceTrack;

typedef struct TPos { /* 0x18 bytes */
    s32 pad0[4]; /* 0x0 */
    s32 x;       /* 0x10 */
    s32 y;       /* 0x14 */
} TPos;

typedef struct TableRecord { /* 0x8 bytes */
    u32 payload;    /* 0x0 */
    u16 id;         /* 0x4 */
    u16 references; /* 0x6 */
} TableRecord;

typedef struct TrackLoadState { /* 0x8 bytes */
    s8 loading; /* 0x0 */
    s8 loaded;  /* 0x1 */
    s8 inited;  /* 0x2 */
    s8 ready;   /* 0x3 */
    s32 idx;    /* 0x4 */
} TrackLoadState;

typedef struct UVMat { /* 0x30 bytes */
    f32 uvs[3][3]; /* 0x0 */
    f32 pos[3];    /* 0x24 */
} UVMat;

typedef struct MuzzleMat { /* 0x30 bytes */
    UVMat mat; /* 0x0 */
} MuzzleMat;

typedef struct UVMatrixRows { /* 0x30 bytes */
    f32 uvs[4][3]; /* 0x0 */
} UVMatrixRows;

typedef struct ct_data_s { /* 0x4 bytes */
    union { u16 freq; u16 code; } fc; /* 0x0 */
    union { u16 dad; u16 len; } dl;   /* 0x2 */
} ct_data;

/* A checkpoint (MAT3 anon_pos_normal_50, gCheckpoints.points[]; it belongs with the track's types below): a gate
 * across the road, read from the track's path file (ReadCheckpointData). A car passes it when it crosses the plane
 * through pos along normal within the radius (CheckCPs). */
typedef struct tCheckpoint { /* 0x50 bytes */
    f32 pos[3];      /* 0x0 the gate's centre */
    f32 normal[3];   /* 0xC the direction a car crosses it in */
    s32 radius;      /* 0x18 the gate's radius, squared (compared with dx * dx + dz * dz) */
    s16 flags;       /* 0x1C CP_FLAG_FINISH, CP_FLAG_START, CP_FLAG_LOOP */
    s16 time[2];     /* 0x1E seconds the drone paths take from here to the next checkpoint: [0] on the first lap
                      * (from the start), [1] on the others (CalcCheckpointTimes) */
    union {
        s16 pathPts[20];         /* 0x22 where each path crosses this gate (SyncCheckpointsToPaths): */
        struct {
            s16 resPathPt;       /* 0x22 pathPts[0]: the point of the resurrection path gResPath */
            s16 dronePathPt[4];  /* 0x24 pathPts[1 + n]: the point of drone path n (gDronePaths[n]) */
            s16 altPathPt[15];   /* 0x2C pathPts[5 + n]: the point of alternative path n (gResPath.alt[n]), -1 none */
        };
    };
    f32 distToNext;  /* 0x4C the resurrection path's length from here to the next checkpoint (FindPathDistances) */
} tCheckpoint;

/* tCheckpoint.flags */
#define CP_FLAG_FINISH 1 /* the finish line: gCheckpoints.finishPt */
#define CP_FLAG_START 2  /* the starting grid's checkpoint: gCheckpoints.startPt */
#define CP_FLAG_LOOP 4   /* where a lap begins once the first one is done: gCheckpoints.loopPt */

/* ---- memory: the heap, the asset wads and waves, lists and trees
 *
 * The heap's own headers are in game/heap.h, which only the heap's files include. */

/* A wave instance (MAT3 waves.c's, an anonymous struct there): one of the 64 slots of gWaveInstance, each holding one
 * loaded asset file (a "wave": a model WAD, a font, a sound bank ...) in a heap block. Host layout on the PC. */
typedef struct anon_loaded_ready_14 { /* 0x20 bytes */
    s8 loaded;   /* 0x0: the file has been read into the block */
    s8 ready;    /* 0x1: and set up (SetupWaveModel) */
    s8 locked;   /* 0x2: InitWaves keeps it (LockWaveInstance) */
    s8 model;    /* 0x3: the wave is a model WAD (wave_is_model) */
    u8 mirrored; /* 0x4: its display lists are mirrored (MirrorWaveInstance) */
    s8 handle;   /* 0x5: ptr is a heap handle (a master slot) rather than the block */
    u8 wave;     /* 0x6: which wave (an index into gWaveSizes, wave_rom_offsets ...) */
    u32 size;    /* 0x8: the block's size */
    u8 *ptr;     /* 0x10: the block, or its handle; 0 for a free slot */
    u8 *lastPtr; /* 0x18: a handle wave's block when it was last set up: the heap may move it */
} anon_loaded_ready_14;

/* A WAD's table of contents entry (MAT3 anon_tag_address_c): a four-character tag ('TXHD', 'OBHD' ...), where the
 * data is and how long. address is the file's offset until MBSetupWad rebases it to the data's 32-bit window
 * address. The WAD keeps its N64 layout in the window (port/assets swaps it to host byte order). */
typedef struct WadEntry { /* 0xC bytes */
    u32 tag;     /* 0x0 */
    u32 address; /* 0x4 */
    u32 length;  /* 0x8: in bytes, or a count of records */
} WadEntry;

/* A WAD file's first two words: its table of contents (an offset, a window address once rebased) and how many
 * entries it has. */
typedef struct WadFileHeader { /* 0x8 bytes */
    u32 toc;        /* 0x0 */
    s32 TOCentries; /* 0x4 */
} WadFileHeader;

/* A WAD's handle (MAT3 anon_base_TOCentries_c), filled by MBSetupWad or MBOffsetWad; host layout. */
typedef struct WadHandle { /* 0x18 bytes */
    WadFileHeader *base; /* 0x0: the file */
    s32 TOCentries;      /* 0x8 */
    WadEntry *TOC;       /* 0x10 */
} WadHandle;

typedef struct config_s { /* 0x8 bytes */
    u16 good_length; /* 0x0 */
    u16 max_lazy;    /* 0x2 */
    u16 nice_length; /* 0x4 */
    u16 max_chain;   /* 0x6 */
} config;

typedef struct static_tree_desc_s { /* 0x20 bytes */
    struct ct_data_s *static_tree; /* 0x0 */
    s32 *extra_bits;               /* 0x8 */
    s32 extra_base;                /* 0x10 */
    s32 elems;                     /* 0x14 */
    s32 max_length;                /* 0x18 */
} static_tree_desc_s;

/* ---- maths: vectors, matrices, angles, random numbers */

typedef union CullVertex { /* 0x10 bytes */
    Vtx_t v;
    s64 force_structure_alignment;
} CullVertex;

typedef struct UnitMatrix { /* 0x24 bytes */
    f32 values[9]; /* 0x0 */
} UnitMatrix;

/* ---- audio: the sound manager, effects, music and the MusyX sound library
 *
 * The MusyX library's records here (ChannelState the volume faders, LargeNode and SmallNode the 3D emitter lists,
 * SequenceContext and SequenceNode the song slots and their notes, SpeechStream, the registries' storage) only lay
 * out the library's data for the data loader: no game file reads their fields. The library's own definitions, with
 * the fields named by what the code does, are in src/rom/sound and include/sequence_context.h,
 * include/sound_hw_voice.h, include/sound_sample_record.h (docs/code/audio.md). The sound manager's records are in
 * game/audiomgr.h. */

typedef struct AudioConfiguration { /* 0x208 bytes */
    u32 frequency;     /* 0x0 */
    u8 mode;           /* 0x4 */
    u8 sample_bits;    /* 0x5 */
    u8 unknown06[256]; /* 0x6 */
    s8 text[256];      /* 0x106 */
} AudioConfiguration;

typedef struct ChannelState { /* 0x28 bytes */
    u32 word00;       /* 0x0 */
    u32 word04;       /* 0x4 */
    u32 flags08;      /* 0x8 */
    u32 pending0C;    /* 0xC */
    u32 unknown10;    /* 0x10 */
    u8 kind14;        /* 0x14 */
    u8 unknown15[3];  /* 0x15 */
    u32 value18;      /* 0x18 */
    u8 unknown1C[12]; /* 0x1C */
} ChannelState;

typedef struct EmitGroup { /* 0x18 bytes */
    u32 identifier;          /* 0x0 */
    struct LargeNode *large; /* 0x8 */
    struct SmallNode *small; /* 0x10 */
} EmitGroup;

typedef struct FxTagList { /* 0x18 bytes */
    char handle;     /* 0x0 */
    char dbl;        /* 0x1 */
    s32 numElements; /* 0x4 */
    void *head;      /* 0x8 */
    void *tail;      /* 0x10 */
} FxTagList;

typedef struct Impact120 { /* 0x78 bytes */
    Impact24 point[5]; /* 0x0 */
} Impact120;

typedef struct LargeNode { /* 0x28 bytes */
    struct LargeNode *next;   /* 0x0 */
    f32 distance;             /* 0x8 */
    f32 first;                /* 0xC */
    f32 second;               /* 0x10 */
    f32 third;                /* 0x14 */
    f32 fourth;               /* 0x18 */
    struct StateNode *object; /* 0x20 */
} LargeNode;

typedef union LayerRecordStorage { /* 0xC bytes */
    LayerRecord fields;
    u32 words[3];
} LayerRecordStorage;

typedef union RecordStorage { /* 0xC bytes */
    RecordFields fields;
    u32 words[2];
} RecordStorage;

typedef struct RegisteredSamples { /* 0x18 bytes */
    struct SampleRecord *records; /* 0x0 */
    u8 *base;                     /* 0x8 */
    u16 count;                    /* 0x10 */
    u16 unknown0A;                /* 0x12 */
} RegisteredSamples;

typedef struct SequenceContext { /* 0x1628 bytes */
    u32 identifier;                            /* 0x0 */
    void *normtab004;                          /* 0x8 */
    u8 normTrans008[128];                      /* 0x10 */
    void *drumtab088;                          /* 0x90 */
    u8 drumTrans08C[128];                      /* 0x98 */
    void *song10C;                             /* 0x118 */
    u32 first110;                              /* 0x120 */
    u32 second114;                             /* 0x124 */
    u32 fraction118;                           /* 0x128 */
    s32 whole11C;                              /* 0x12C */
    u32 half120;                               /* 0x130 */
    u32 rate124;                               /* 0x134 */
    SequenceSlotTrack unknown128[64];          /* 0x138 */
    u8 channels528[64];                        /* 0x738 */
    SequenceTrack tracks568[64];               /* 0x778 */
    struct SequenceTimedValue *timingStartF68; /* 0x1578 */
    struct SequenceTimedValue *currentF6C;     /* 0x1580 */
    u32 lowF70;                                /* 0x1588 */
    u32 highF74;                               /* 0x158C */
    struct SequenceNode *active;               /* 0x1590 */
    struct SequenceNode *pending;              /* 0x1598 */
    u8 unknownF80[64];                         /* 0x15A0 */
    u8 activeFC0;                              /* 0x15E0 */
    u8 inactiveFC1;                            /* 0x15E1 */
    u16 valueFC2;                              /* 0x15E2 */
    u8 channelFC4;                             /* 0x15E4 */
    u8 disabledFC5;                            /* 0x15E5 */
    u16 valueFC6;                              /* 0x15E6 */
    u8 unknownFC8[16];                         /* 0x15E8 */
    void *streamFD8;                           /* 0x15F8 */
    u8 unknownFDC[4];                          /* 0x1600 */
    u8 valueFE0;                               /* 0x1604 */
    u8 unknownFE1[3];                          /* 0x1605 */
    u32 firstFE4;                              /* 0x1608 */
    u32 secondFE8;                             /* 0x160C */
    u16 valueFEC;                              /* 0x1610 */
    u8 flagsFEE;                               /* 0x1612 */
    u8 unknownFEF;                             /* 0x1613 */
    uintptr_t pendingFF0;                      /* 0x1618 */
    u8 unknownFF4[4];                          /* 0x1620 */
} SequenceContext;

typedef struct SequenceNode { /* 0x20 bytes */
    struct SequenceNode *next;     /* 0x0 */
    struct SequenceNode *previous; /* 0x8 */
    u32 identifier;                /* 0x10 */
    s32 deadline0C;                /* 0x14 */
    u32 fraction10;                /* 0x18 */
    s32 whole14;                   /* 0x1C */
} SequenceNode;

typedef struct SfxQueue { /* 0x18 bytes */
    u32 pad0[2];       /* 0x0 */
    struct Node *tail; /* 0x8 */
    struct Node *head; /* 0x10 */
} SfxQueue;

typedef struct SmallNode { /* 0x18 bytes */
    struct SmallNode *next;   /* 0x0 */
    f32 distance;             /* 0x8 */
    struct StateNode *object; /* 0x10 */
} SmallNode;

typedef struct SpeechServiceHooks { /* 0x40 bytes */
    void (*unknown_00[6])(void); /* 0x0 */
    void *(*allocate)(u32, s32); /* 0x30 */
    void (*release)(void *);     /* 0x38 */
} SpeechServiceHooks;

typedef struct SpeechStream { /* 0x1268 bytes */
    u8 unknown_0000[358];                                        /* 0x0 */
    u16 block_count;                                             /* 0x166 */
    u8 unknown_0168[40];                                         /* 0x168 */
    void (*callback)(void *, void *, u32, struct OSMesgQueue_s *); /* 0x190 */
    void *data;                                                  /* 0x198 */
    void *argument2;                                             /* 0x1A0 */
    u32 argument3;                                               /* 0x1A8 */
    u8 unknown_01A0[4096];                                       /* 0x1AC */
    u16 read_count;                                              /* 0x11AC */
    u16 write_count;                                             /* 0x11AE */
    u32 buffered;                                                /* 0x11B0 */
    s32 remaining;                                               /* 0x11B4 */
    OSMesgQueue queue;                                           /* 0x11B8 */
    OSMesg message;                                              /* 0x11E0 */
    u32 field_11C8;                                              /* 0x11E8 */
    u32 field_11CC;                                              /* 0x11EC */
    u32 field_11D0;                                              /* 0x11F0 */
    u32 consumed;                                                /* 0x11F4 */
    u32 available;                                               /* 0x11F8 */
    s8 field_11DC;                                               /* 0x11FC */
    s8 state;                                                    /* 0x11FD */
    s8 scale;                                                    /* 0x11FE */
    u8 unknown_11DF;                                             /* 0x11FF */
    OSMesgQueue request_queue;                                   /* 0x1200 */
    OSMesg request_messages[2];                                  /* 0x1228 */
    volatile u8 busy;                                            /* 0x1238 */
    u8 value1;                                                   /* 0x1239 */
    u8 value2;                                                   /* 0x123A */
    u8 value3;                                                   /* 0x123B */
    u8 option;                                                   /* 0x123C */
    u8 unknown_1205[3];                                          /* 0x123D */
    u32 rate;                                                    /* 0x1240 */
    s32 handle;                                                  /* 0x1244 */
    s16 *buffer;                                                 /* 0x1248 */
    u32 buffer_count;                                            /* 0x1250 */
    u8 request_state;                                            /* 0x1254 */
    u8 mode;                                                     /* 0x1255 */
    u8 unknown_121A[2];                                          /* 0x1256 */
    u32 processed;                                               /* 0x1258 */
    f32 field_1220;                                              /* 0x125C */
    u32 token;                                                   /* 0x1260 */
} SpeechStream;

/* A car's sound (MAT3's, unnamed; 0x14 bytes): its fx tag handle and the values last sent to it, so that a change
 * is sent only when a value moves (SetSoundVP). InitSoundState empties it: sound -1, the values -2 (none). */
typedef struct tSoundState { /* 0x14 bytes */
    s32 sound;  /* 0x0: the sound manager's handle, -1 for none */
    f32 vol;    /* 0x4 */
    f32 pitch;  /* 0x8 */
    f32 lrpan;  /* 0xC */
    f32 fbpan;  /* 0x10 */
} tSoundState;

/* A car's road noises (MAT3's, unnamed; 0x3C bytes): the surface sounds under it, one at a time. */
typedef struct tRoadNoises { /* 0x3C bytes */
    tSoundState dirt;  /* 0x0 */
    tSoundState water; /* 0x14 */
    tSoundState pave;  /* 0x28 */
} tRoadNoises;

typedef union TableRecordStorage { /* 0x8 bytes */
    TableRecord fields;
    u32 words[2];
} TableRecordStorage;

typedef struct TransferRequest { /* 0x18 bytes */
    void *destination; /* 0x0 */
    void *source;      /* 0x8 */
    u32 size;          /* 0x10 */
} TransferRequest;

typedef struct Vector { /* 0xC bytes */
    f32 x; /* 0x0 */
    f32 y; /* 0x4 */
    f32 z; /* 0x8 */
} Vector;

typedef struct SoundEmitterAddView { /* 0x50 bytes */
    struct StateNode *next;     /* 0x0 */
    struct StateNode *previous; /* 0x8 */
    u32 flags;                  /* 0x10 */
    Vector position;            /* 0x14 */
    Vector velocity;            /* 0x20 */
    f32 range;                  /* 0x2C */
    f32 gain;                   /* 0x30 */
    f32 minimum;                /* 0x34 */
    f32 curve;                  /* 0x38 */
    u32 handle;                 /* 0x3C */
    u32 context;                /* 0x40 */
    u16 identifier;             /* 0x44 */
    u16 counter;                /* 0x46 */
    f32 fade;                   /* 0x48 */
} SoundEmitterAddView;

typedef struct tOrderedList { /* 0x18 bytes */
    s32 (*OrderFunc)(void *, void *); /* 0x0 */
    s32 numElements;                  /* 0x8 */
    void *root;                       /* 0x10 */
} tOrderedList;

/* ---- scene: the scene objects (mbox and zoid), models, textures, windows, the sky and the world */

typedef struct ArbEntry { /* 0x50 bytes */
    void *f0;    /* 0x0 */
    void *f4;    /* 0x8 */
    u8 unk8[64]; /* 0x10 */
} ArbEntry;

typedef struct DmaList { /* 0x18 bytes */
    u16 pad0;                   /* 0x0 */
    u16 count;                  /* 0x2 */
    u16 next;                   /* 0x4 */
    u16 pad6;                   /* 0x6 */
    struct DmaReq *reqs;        /* 0x8 */
    struct OSMesgQueue_s *done; /* 0x10 */
} DmaList;

/* A flipbook frame of texture animation (MAT3 tTextureBase): the texture's name, the texture once found, the image
 * to show (its texBase, or the address its display list loads). */
typedef struct Item12 { /* 0x18 bytes */
    char *name;                  /* 0x0 */
    struct Resource32 *tex;      /* 0x8 */
    void *texBase;               /* 0x10 */
} Item12;

typedef struct ModelList { /* 0x10 bytes */
    struct Model *models; /* 0x0 */
    s32 count;            /* 0x8 */
} ModelList;

/* A scene node, an entry of ObjInstanceList (MAT3 ObjInstance; the N64's 0x44 bytes with the pointers widened):
 * its flags (OBJ_*, scene.h), its culling word, its matrix (a tUVMatrix, relative to its parent), the object handle
 * ((model << 10) | definition; 0xFFFF a free node), the first child and the next sibling (-1 none), its lights, a
 * texture for each level of detail, its tile, tile size and palette, and two colours. */
typedef struct ObjectInstance { /* 0x68 bytes */
    u32 flags;           /* 0x0 */
    u32 culling;         /* 0x4 */
    void *mat;           /* 0x8 */
    f32 scale;           /* 0x10 */
    f32 w;               /* 0x14 */
    u16 index;           /* 0x18 */
    s16 child;           /* 0x1A */
    s16 next;            /* 0x1C */
    s16 numLights;       /* 0x1E */
    void *lights;        /* 0x20 */
    void *texture[4];    /* 0x28 */
    void *tile;          /* 0x48 */
    void *tileSize;      /* 0x50 */
    void *palette;       /* 0x58 */
    u32 color;           /* 0x60 */
    u32 envcolor;        /* 0x64 */
} ObjectInstance;

typedef struct PcViewport { /* 0x10 bytes */
    s16 scale[4];     /* 0x0 */
    s16 translate[4]; /* 0x8 */
} PcViewport;

typedef struct PolyInstance { /* 0x58 bytes */
    s16 nverts;   /* 0x0 */
    u16 flags;    /* 0x2 */
    u16 texidx;   /* 0x4 */
    s16 z;        /* 0x6 */
    u8 verts[80]; /* 0x8 */
} PolyInstance;

/* ---- blit: the 2D blitter and panes */

typedef struct BlitColor { /* 0x4 bytes */
    u8 r; /* 0x0 */
    u8 g; /* 0x1 */
    u8 b; /* 0x2 */
    u8 a; /* 0x3 */
} BlitColor;

/* A blit: one 2D sprite of the HUD or a menu (MAT3 blit.c's _tBlit), in the PC's layout (pointers 8 bytes; the N64's
 * record was 0x40 bytes). The pool is InitBlits' 200 records, BlitPtrs points to each, the first NumBlits of them in
 * use. Each owns one entry of the zoid blit list (tBlitInstance, by blidx) that the renderer draws; UpdateBlit copies
 * the blit's fields into it. A blit's children (child) are the rest of a multiblit, removed with it. */
typedef struct tBlit { /* 0x68 bytes */
    const char *name;               /* 0x0  texture name; NULL: a plain colour (default_blit_image), BLIT_NO_TEXTURE:
                                     *      no texture (a text blit: texture is then its draw function) */
    void *palette;                  /* 0x8  the colours: an RGBA colour (a pane's) or the texture's palette */
    struct tTexture *texture;       /* 0x10 the texture's definition (MBOX_FindTexture_WORK) */
    u16 texIndex;                   /* 0x18 the texture's index in the model data */
    s16 x;                          /* 0x1A screen position of the top left corner */
    s16 y;                          /* 0x1C */
    u16 z;                          /* 0x1E depth (draw order) */
    s16 width;                      /* 0x20 size on screen */
    s16 height;                     /* 0x22 */
    u8 alpha;                       /* 0x24 */
    u8 flip;                        /* 0x25 mirrored left to right */
    u8 hide;                        /* 0x26 */
    u8 init;                        /* 0x27 set by NewBlit: the animation function's first call is yet to come */
    s16 top;                        /* 0x28 the part of the texture shown: rows top..bot, columns left..right */
    s16 bot;                        /* 0x2A   (-1: the whole texture) */
    s16 left;                       /* 0x2C */
    s16 right;                      /* 0x2E */
    s16 color;                      /* 0x30 */
    u16 reserved32;                 /* 0x32 */
    s32 (*animFunc)(struct tBlit *); /* 0x38 called each frame by DrawBlits; returning 0 removes the blit */
    u32 animID;                     /* 0x40 the animation function's argument (which player, which item) */
    intptr_t animDTA;               /* 0x48 the animation function's own data (a pane, a counter) */
    u16 blidx;                      /* 0x50 index of the blit's entry in gBlitInstanceList */
    u16 reserved52;                 /* 0x52 */
    void *data;                     /* 0x58 */
    struct tBlit *child;            /* 0x60 the next blit of the same multiblit */
} tBlit;

/* The texname of a blit without a texture (MAT3 `(const char *)-1`): DrawBlits skips its animation function, and
 * the renderer draws it by the function in its texture field (ZOID_TextBlit). */
#define BLIT_NO_TEXTURE ((const char *)-1)

/* One blit of a multiblit's table (MAT3 tBlitDef): NewMultiBlit makes a chain of blits from an array of these. */
typedef struct tBlitDef { /* 0x30 bytes */
    const char *texname;             /* 0x0  the texture, or BLIT_NO_TEXTURE */
    s16 dulx;                        /* 0x8  position relative to the multiblit's */
    s16 duly;                        /* 0xA */
    s16 width;                       /* 0xC  size, or -1 for the texture's */
    s16 height;                      /* 0xE */
    s16 top;                         /* 0x10 the part of the texture shown (tBlit.top ...) */
    s16 bot;                         /* 0x12 */
    s16 left;                        /* 0x14 */
    s16 right;                       /* 0x16 */
    u32 zdepth;                      /* 0x18 */
    u32 alpha;                       /* 0x1C */
    s32 (*animfunc)(struct tBlit *); /* 0x20 the blit's animation function (tBlit.animFunc) */
    u32 animid;                      /* 0x28 its argument; bit 31 is NewBlit's flags argument */
} tBlitDef;
typedef tBlitDef HudBlitDef; /* frontend.h's name for tBlitDef (its data), to be folded into tBlitDef */

/* A pane (MAT3 pane.c's tBlitSet): a bordered rectangle made of five blits, the four one-pixel edges in the border
 * colour and the inside in the pane colour. gPaneBlits holds the 16 panes. */
typedef struct tBlitSet { /* 0x38 bytes */
    s8 used;                   /* 0x0 */
    s8 perm;                   /* 0x1 permanent: RemovePanes(0) keeps it */
    u8 blackColor[4];          /* 0x2 the inside's RGBA (blits[2]) */
    u8 grayColor[4];           /* 0x6 the border's RGBA (the other four) */
    u8 pad10[2];               /* 0xA */
    struct tBlit *blits[5];    /* 0x10 top edge, left edge, inside, right edge, bottom edge */
} tBlitSet;

/* One entry of the zoid blit list gBlitInstanceList (MAT3 zoidblit.c's anon_palette_texture_20), in the PC's layout:
 * what the renderer draws for one blit. ZOID_AddBlit hands out a free entry, the ZOID_*Blit setters fill it. */
typedef struct tBlitInstance { /* 0x28 bytes */
    void *palette;             /* 0x0  the blit's colours (tBlit.palette) */
    void *texture;             /* 0x8  its texture's definition, or a text blit's draw function */
    u16 texIndex;              /* 0x10 */
    s16 x;                     /* 0x12 */
    s16 y;                     /* 0x14 */
    u16 z;                     /* 0x16 */
    s16 width;                 /* 0x18 */
    s16 height;                /* 0x1A */
    u8 alpha;                  /* 0x1C */
    u8 flags;                  /* 0x1D BLITINST_FLIP ... */
    s8 hide;                   /* 0x1E 0 shown, 1 hidden, BLITINST_FREE the entry is unused */
    u8 pad1F;                  /* 0x1F */
    s16 top;                   /* 0x20 the part of the texture drawn */
    s16 left;                  /* 0x22 */
    s16 bot;                   /* 0x24 */
    s16 right;                 /* 0x26 */
} tBlitInstance;

/* ---- text: fonts and text */

/* A font's header (MAT3 HAL_text.c's anon_version_type_10), the start of the font's data wave; numChars letters
 * (FontLetter) follow it, then, when kern is set, one row of numChars kerning amounts per letter. Sizes in pixels. */
typedef struct FontHeader { /* 0x10 bytes */
    u8 version;     /* 0x0 */
    u8 type;        /* 0x1 */
    u8 ascent;      /* 0x2 */
    u8 descent;     /* 0x3 */
    u8 leading;     /* 0x4 the default space between lines */
    u8 totalHeight; /* 0x5 */
    u8 maxWidth;    /* 0x6 the widest letter: every letter's width when nonProp */
    u8 spaceWidth;  /* 0x7 */
    u8 spacing;     /* 0x8 the default space between letters */
    u8 nonProp;     /* 0x9 fixed width (HAL_SetFontFixedWidth) */
    u8 kern;        /* 0xA has a kerning table */
    u8 numTextures; /* 0xB the font's texture pages (gFontTextures) */
    u8 numChars;    /* 0xC */
    u8 pad13[3];    /* 0xD */
} FontHeader;

/* One letter of a font (MAT3 anon_kerns_value_c), 12 bytes as in the font's file: where it is in which texture page.
 * kerns is the window address of its row of the kerning table (port/assets/font.c), read through PC_N64P. */
typedef struct FontLetter { /* 0xC bytes */
    u32 kerns;   /* 0x0 */
    u8 value;    /* 0x4 the character it draws */
    u8 texIdx;   /* 0x5 its texture page */
    u8 uls;      /* 0x6 its texels: columns uls..lrs, rows ult..lrt */
    u8 ult;      /* 0x7 */
    u8 lrs;      /* 0x8 */
    u8 lrt;      /* 0x9 */
    s16 pad12;   /* 0xA */
} FontLetter;

typedef struct Color8 { /* 0x8 bytes */
    BlitColor color; /* 0x0 */
    u8 extra[4];     /* 0x4 */
} Color8;

typedef struct MenuDepthPoint { /* 0x8 bytes */
    f32 x; /* 0x0 */
    f32 y; /* 0x4 */
} MenuDepthPoint;

typedef struct PcText4 { /* 0x4 bytes */
    char c[4]; /* 0x0 */
} PcText4;

typedef union TextColor { /* 0x4 bytes */
    u8 c[4];
    u32 rgba;
} TextColor;

typedef struct TextTables { /* 0x48 bytes */
    u16 numSingle; /* 0x0 */
    u8 **single;   /* 0x8 */
    u16 numTab;    /* 0x10 */
    u16 numTabA;   /* 0x12 */
    u16 *tab;      /* 0x18 */
    u8 **taba;     /* 0x20 */
    u16 numTab2;   /* 0x28 */
    u16 numTab2a;  /* 0x2A */
    u16 numTab2b;  /* 0x2C */
    u16 *tab2;     /* 0x30 */
    u16 *tab2a;    /* 0x38 */
    u8 **tab2b;    /* 0x40 */
} TextTables;

/* ---- input: controllers and the Rumble Pak */

/* A controller as ControllerUpdate leaves it for ControlIRQ: gControllerData[4] (MAT3 tPadInfo, which also has the
 * buttons the pad offers and moves the sticks to 0x0C). */
typedef struct ControllerState { /* 0x10 bytes */
    s8 attached;   /* 0x0: plugged in */
    u8 pad1[3];    /* 0x1 */
    u32 buttons;   /* 0x4: held, in the game's button bits (n64_button_map) */
    s8 stick[2];   /* 0x8: x, y, about -80..80 */
    s8 stick2[2];  /* 0xA: a second stick: the N64 has none (0) */
    u8 analog[2];  /* 0xC: analog triggers, 0-255 (the controls 0x15 and 0x16): the N64 has none (0) */
    u8 padE[2];    /* 0xE */
} ControllerState;

/* A player's input record (MAT3 tPadState), one of gPlayers[4], in the PC's layout (the save pointer is 8 bytes).
 * Each frame UpdateRushControls copies its controller's state here; UpdateModelControls turns it into the car's
 * steering, throttle, brake and gear. The record's type is still each file's own (gPlayers is declared by
 * InitOptions.c); this is the input files' view of it. */
typedef struct tPadState { /* 0x50 bytes */
    u8 slot;                        /* 0x0: the car the player drives (model[], gGameCar[]) */
    u8 controller;                  /* 0x1: the controller port, 5 for none */
    u32 edges;                      /* 0x4: buttons newly pressed this frame */
    u32 levels;                     /* 0x8: buttons held */
    u32 autoEdges;                  /* 0xC: pressed, and repeated while held (UpdateControls) */
    f32 analog[2];                  /* 0x10: the stick, x and y, -1..1 */
    u32 controls[10];               /* 0x18: each control's button bits (gControlTable[gNewControls[...]]) */
    u16 numCrashes;                 /* 0x40 */
    s8 trans;                       /* 0x42 */
    u8 pad43;
    s32 listener;                   /* 0x44: the player's sound listener, -1 for none */
    struct tPlayerProfile **save;   /* 0x48: the player's profile on a Controller Pak (a handle), or 0 */
} tPadState;

/* The controls the car reads, by their index in gNewControls[player][] and tPadState.controls[]
 * (UpdateModelControls; 7 and 8 are the front end's). */
#define CONTROL_GAS 0
#define CONTROL_BRAKE 1
#define CONTROL_STEER 2         /* how the stick steers (CONTROL_VALUE_STICK_DEAD or not) */
#define CONTROL_SHIFT_UP 3
#define CONTROL_SHIFT_DOWN 4
#define CONTROL_REVERSE 5       /* held: reverse gear and full throttle */
#define CONTROL_ABORT 6         /* pressed: abort the car (respawn) */
#define CONTROL_WINGS 9         /* held: the wings */
/* The values in tPadState.controls[] that aren't button bits (ReadGasAndBrake, ReadSteeringWheel). */
#define CONTROL_VALUE_STICK_Y 0x13      /* the stick's y axis: up the gas, down the brake */
#define CONTROL_VALUE_ANALOG_1 0x15     /* the first analog trigger (none on the N64) */
#define CONTROL_VALUE_ANALOG_2 0x16     /* the second analog trigger (none on the N64) */
#define CONTROL_VALUE_STICK_DEAD 0x19   /* the stick, with a dead zone: steering, or the y axis as above */
/* The controller port of a player who has none. */
#define CONTROLLER_NONE 5

/* ---- save: the Controller Pak, profiles, records, options and passwords */

/* libultra's OSPfsState: a file's entry in a pak's directory (the same layout on the PC). */
typedef struct PakFileState { /* 0x20 bytes */
    u32 file_size;     /* 0x0 0: no file */
    u32 game_code;     /* 0x4 */
    u16 company_code;  /* 0x8 */
    u8 ext_name[4];    /* 0xA in the pak's character set (pak_charset) */
    u8 game_name[16];  /* 0xE */
} PakFileState;

/* One of the 16 files of a pak, as the pak system keeps it (tPak.files, by the file's number on the pak). */
typedef struct tPakFile { /* 0x28 bytes */
    s8 locked;           /* 0x0 in use (PS_LockFile): the pak's files stay while it is out */
    s8 dirty;            /* 0x1 holds data PS_FlushFile is yet to write */
    s8 flag2;            /* 0x2 cleared with the others; PS_WriteFile tests it and does nothing */
    u8 pad3;             /* 0x3 */
    u16 mapBytes;        /* 0x4 the size of its dirty map: one bit for each 32 bytes of its data, after the data */
    u16 pad6;            /* 0x6 */
    PakFileState state;  /* 0x8 its directory entry (osPfsFileState) */
} tPakFile;

/* One controller's pak, as the pak system keeps it (gPaks; MAT3 paksysdc.c's anon_changed_inserted_25c, whose flags
 * are these but formatted and in another order), in the PC's layout (pointers 8 bytes; 0x304 bytes on the N64). The
 * rumble thread keeps the motor's state in it too. */
typedef struct tPak { /* 0x310 bytes */
    s8 changed;          /* 0x0 PS_PakServer found it put in, taken out or another pak (PS_PakChanged) */
    s8 inserted;         /* 0x1 a pak the pak system has read is in (PS_PakInserted) */
    s8 plugged;          /* 0x2 something is plugged into the controller (pak_poll_inserted) */
    s8 wasRemoved;       /* 0x3 the pak was taken out while its files were kept (another port has it now) */
    s8 inited;           /* 0x4 pfs holds a pak that was read (its id tells whether the next pak is the same) */
    s8 damaged;          /* 0x5 the pak can't be read (PS_PakDamaged) */
    s8 rumble;           /* 0x6 the pak is a Rumble Pak (PS_PakRumble) */
    s8 flushing;         /* 0x7 PS_FlushPak is writing it */
    s8 flushed;          /* 0x8 PS_FlushPak wrote something */
    s8 original;         /* 0x9 the pak in is the one the files came from */
    s8 locked;           /* 0xA one of its files is locked (tPakFile.locked) */
    s8 flush;            /* 0xB one of its files is dirty */
    PcPfs pfs;           /* 0x10 the pak's libultra handle */
    s32 error;           /* 0x80 the last error (kPS_Err*) */
    s32 FreeSpace;       /* 0x84 free bytes (osPfsFreeBlocks) */
    u8 motorOn;          /* 0x88 the rumble thread's: the motor is running */
    u8 request;          /* 0x89 the rumble wanted, 0..40 (PS_SetRumblePercent) */
    u8 level;            /* 0x8A the rumble now: falls one a retrace towards request */
    u8 pad8B;            /* 0x8B */
    s16 phase;           /* 0x8C the retrace in the rumble's 10-retrace duty cycle (rumble_duty_masks) */
    u8 pad8E[2];         /* 0x8E */
    tPakFile files[16];  /* 0x90 */
} tPak;

#define NUM_PAKS 4          /* one a controller */
#define PAK_MAX_FILES 16    /* the files a pak holds (tPak.files) */
#define PAK_BLOCK_SIZE 256  /* a pak's page: files are sized in them */

/* A pak file's handle: what the pak system's callers hold. */
typedef struct tPakHandle {
    struct PakFileRecord *file;
} tPakHandle;

typedef struct Ent19 { /* 0x13 bytes */
    s8 b[19]; /* 0x0 */
} Ent19;

typedef struct PakErrorState { /* 0x2C bytes */
    s8 busy;         /* 0x0 */
    s8 enabled;      /* 0x1 */
    s8 unk2;         /* 0x2 */
    s8 player;       /* 0x3 */
    u8 primary[4];   /* 0x4 */
    u8 secondary[4]; /* 0x8 */
    u8 previous[4];  /* 0xC */
    s8 present;      /* 0x10 */
    s32 mode;        /* 0x14 */
    s16 arg[4];      /* 0x18 */
    u8 pad20[8];     /* 0x20 */
    u32 deadline;    /* 0x28 */
} PakErrorState;

/* An open pak file (MAT3 tPS_File), in the PC's layout: pak_file_records holds 64, their handles pak_file_handles;
 * each pak's open files are a list (gPS_Files, linked through next). */
typedef struct PakFileRecord { /* 0x60 bytes */
    struct tPakHandle *next;                                   /* 0x0 */
    struct tPakHandle *prev;                                   /* 0x8 */
    void (*RemoveFile)(struct tPakHandle *);                   /* 0x10 called when the file goes (the pak removed) */
    void (*MoveFile)(struct tPakHandle *, struct tPakHandle *); /* 0x18 */
    u8 pak;                                                   /* 0x20 the controller it is on */
    u8 fileNo;                                                /* 0x21 its number on the pak (tPak.files) */
    u8 name[35];                                              /* 0x22 its name and extension, in the game's */
    u8 ext[11];                                               /* 0x45   characters (pak_decode_name) */
    u32 size;                                                 /* 0x50 bytes */
    u32 blocks;                                               /* 0x54 pages of PAK_BLOCK_SIZE */
    u8 **data;                                                /* 0x58 its data in the heap (a handle), then its
                                                               *      dirty map; NULL until read */
} PakFileRecord;

typedef struct PlayerFileList { /* 0x18 bytes */
    u32 unknown0;             /* 0x0 */
    u32 unknown4;             /* 0x4 */
    struct PlayerData **head; /* 0x8 */
    struct PlayerData **tail; /* 0x10 */
} PlayerFileList;

/* The parts of a profile file (tSaveData). Each starts with the checksum of the rest (CalcChecksumInternal), which
 * CheckAndRepairPlayer checks. MAT3's layouts and names (save.c's anon_checksum_*). */
typedef struct tRaceSettings { /* 0x14 bytes */
    u32 checksum;   /* 0x0 */
    u8 track;       /* 0x4 */
    u8 car;         /* 0x5 */
    u8 trans;       /* 0x6 */
    u8 carView;     /* 0x7 */
    s8 laps;        /* 0x8 */
    s8 drones;      /* 0x9 */
    s8 gravity;     /* 0xA */
    s8 difficulty;  /* 0xB */
    s8 catchup;     /* 0xC */
    s8 backwards;   /* 0xD */
    s8 mirror;      /* 0xE */
    s8 fog;         /* 0xF */
    s8 wind;        /* 0x10 */
    s8 deaths;      /* 0x11 */
    u8 pad12[2];    /* 0x12 */
} tRaceSettings;

typedef struct tBattleSettings { /* 0x10 bytes */
    u32 checksum;  /* 0x0 */
    u8 track;      /* 0x4 */
    u8 car;        /* 0x5 */
    u8 trans;      /* 0x6 */
    u8 carView;    /* 0x7 */
    s8 gravity;    /* 0x8 */
    s8 mirror;     /* 0x9 */
    s8 fog;        /* 0xA */
    s8 wind;       /* 0xB */
    s8 mode;       /* 0xC */
    s8 points;     /* 0xD */
    s8 minutes;    /* 0xE */
    s8 neg;        /* 0xF */
} tBattleSettings;

typedef struct tStuntSettings { /* 0x14 bytes */
    u32 checksum;  /* 0x0 */
    u8 track;      /* 0x4 */
    u8 car;        /* 0x5 */
    u8 trans;      /* 0x6 */
    u8 carView;    /* 0x7 */
    s8 gravity;    /* 0x8 */
    s8 mirror;     /* 0x9 */
    s8 fog;        /* 0xA */
    s8 wind;       /* 0xB */
    s8 mode;       /* 0xC */
    s8 flow;       /* 0xD */
    s8 points;     /* 0xE */
    s8 minutes;    /* 0xF */
    s8 deaths;     /* 0x10 */
    u8 pad11[3];   /* 0x11 */
} tStuntSettings;

typedef struct tObstacleSettings { /* 0xC bytes */
    u32 checksum;  /* 0x0 */
    u8 track;      /* 0x4 */
    u8 car;        /* 0x5 */
    u8 trans;      /* 0x6 */
    u8 carView;    /* 0x7 */
    s8 gravity;    /* 0x8 */
    s8 mirror;     /* 0x9 */
    s8 fog;        /* 0xA */
    s8 wind;       /* 0xB */
} tObstacleSettings;

/* A track's race records and totals (one per track; gTotalRaceStats sums them over the profiles). */
typedef struct tRaceStats { /* 0x60 bytes */
    u32 checksum;              /* 0x0 */
    f32 scores[3][5];          /* 0x4 the five best laps, average laps and ghost race times (RecordRace,
                                *      RecordGhost; gTotalNames: their owners, gNewTotalScore: this race's) */
    f32 totalTime;             /* 0x40 */
    u16 totalLaps;             /* 0x44 */
    u16 totalRaces;            /* 0x46 */
    u16 total1st;              /* 0x48 */
    u16 total2nd;              /* 0x4A */
    u16 total3rd;              /* 0x4C */
    u16 totalCrashes;          /* 0x4E */
    u16 totalDeathAttempted;   /* 0x50 */
    u16 totalDeathCompleted;   /* 0x52 */
    u16 totalGhosts;           /* 0x54 */
    u8 pad56[2];               /* 0x56 */
    u32 totalMiles;            /* 0x58 */
    u16 coinsFound;            /* 0x5C a bit for each coin found on the track */
    u8 pad5E[2];               /* 0x5E */
} tRaceStats;

/* A stunt arena's records and totals. */
typedef struct tStuntStats { /* 0x40 bytes */
    u32 checksum;          /* 0x0 */
    u32 bestStunt;         /* 0x4 */
    u32 bestAvg;           /* 0x8 */
    u32 totalPoints;       /* 0xC */
    u16 totalTime;         /* 0x10 */
    u16 totalCrashes;      /* 0x12 */
    u32 totalStunts[10];   /* 0x14 each kind of stunt */
    u16 coinsFound;        /* 0x3C */
    u8 pad3E[2];           /* 0x3E */
} tStuntStats;

/* A battle arena's totals. */
typedef struct tBattleStats { /* 0xC bytes */
    u32 checksum;       /* 0x0 */
    u16 totalMatches;   /* 0x4 */
    u16 totalWins;      /* 0x6 */
    u16 totalKills;     /* 0x8 */
    u16 totalDeaths;    /* 0xA */
} tBattleStats;

/* The obstacle course's records and totals. */
typedef struct tObstacleStats { /* 0x18 bytes */
    u32 checksum;            /* 0x0 */
    f32 bestTime;            /* 0x4 */
    f32 totalTime;           /* 0x8 */
    u16 totalRaces;          /* 0xC */
    u16 totalFinished;       /* 0xE */
    u16 totalCrashes;        /* 0x10 */
    u16 numDeathAttempted;   /* 0x12 */
    u16 numDeathCompleted;   /* 0x14 */
    u8 pad16[2];             /* 0x16 */
} tObstacleStats;

/* A circuit's records and totals (one per circuit). */
typedef struct tCircuitStats { /* 0x1C bytes */
    u32 checksum;            /* 0x0 */
    u16 numCircuits;         /* 0x4 */
    u16 num1st;              /* 0x6 */
    u16 num2nd;              /* 0x8 */
    u16 num3rd;              /* 0xA */
    u16 numDeathAttempted;   /* 0xC */
    u16 numDeathCompleted;   /* 0xE */
    u16 bestPts;             /* 0x10 */
    u8 pad12[2];             /* 0x12 */
    f32 bestTime;            /* 0x14 */
    u32 totalPts;            /* 0x18 */
} tCircuitStats;

/* A car's settings (the car setup screen's sliders). */
typedef struct tCarSettings { /* 0x10 bytes */
    u32 checksum;      /* 0x0 */
    s8 settings[10];   /* 0x4 */
    u8 pad0E[2];       /* 0xE */
} tCarSettings;

/* The circuit in progress (MAT3 tCircuit; circuitscr.c has its own view of that name). */
typedef struct tCircuitRecord { /* 0x4C bytes */
    u32 checksum;      /* 0x0 */
    u8 active;         /* 0x4 */
    u8 death;          /* 0x5 */
    u8 temp;           /* 0x6 */
    u8 difficulty;     /* 0x7 */
    s8 type;           /* 0x8 the circuit (circuitStats' index) */
    s8 track;          /* 0x9 */
    u8 padA[2];        /* 0xA */
    u32 seed;          /* 0xC */
    f32 totalTime;     /* 0x10 */
    u8 ranks[6][9];    /* 0x14 */
    u8 pad4A[2];       /* 0x4A */
} tCircuitRecord;

/* A profile's file on the pak (MAT3 save.c's tSaveData, the N64's revision 16 of it: controls 10 bytes, then rumble
 * and the race type), 2064 bytes. A file format: its layout is the pak file's, byte for byte. */
typedef struct tSaveData { /* 0x810 bytes */
    u32 nameChecksum;                     /* 0x0 over id, revision and name */
    u32 id[2];                            /* 0x4 the profile's id: the creation time, the record's address and pak */
    u8 revision;                          /* 0xC PROFILE_REVISION */
    u8 name[13];                          /* 0xD */
    u8 pad1A[2];                          /* 0x1A */
    u32 optChecksum;                      /* 0x1C */
    s8 options[21];                       /* 0x20 the game options (gOptions' first 21) */
    u8 pad35[3];                          /* 0x35 */
    u32 contChecksum;                     /* 0x38 over the controls and rumble */
    s8 controls[10];                      /* 0x3C */
    s8 rumble;                            /* 0x46 */
    u8 raceType;                          /* 0x47 the race type last played (gRace) */
    tRaceSettings raceSettings;           /* 0x48 */
    tBattleSettings battleSettings;       /* 0x5C */
    tStuntSettings stuntSettings;         /* 0x6C */
    tObstacleSettings obstacleSettings;   /* 0x80 */
    tRaceStats raceStats[12];             /* 0x8C */
    tStuntStats stuntStats[4];            /* 0x50C */
    tBattleStats battleStats[8];          /* 0x60C */
    tObstacleStats obstacleStats[1];      /* 0x66C */
    tCircuitStats circuitStats[4];        /* 0x684 */
    tCircuitRecord circuit;               /* 0x6F4 the circuit in progress */
    tCarSettings carSettings[13];         /* 0x740 */
} tSaveData;

#define PROFILE_REVISION 16   /* tSaveData.revision (MAT3's is 19) */

/* A ghost file's header (MAT3 save.c's tSaveHeader, the N64's 0x58 bytes): file data, so in the file's layout; the
 * compressed recording follows it. */
typedef struct tGhostHeader { /* 0x58 bytes */
    u32 headerChecksum;     /* 0x0 over revision .. unzippedSize */
    u8 revision;            /* 0x4 GHOST_REVISION */
    s8 flag;                /* 0x5 */
    s8 track;               /* 0x6 */
    u8 backward;            /* 0x7 */
    u8 pad08[0x16 - 0x08];  /* 0x8 */
    u8 name[13];            /* 0x16 the driver's */
    u8 pad23[0x28 - 0x23];  /* 0x23 */
    u32 playerId[2];        /* 0x28 the driver's profile id */
    u8 pad30[0x38 - 0x30];  /* 0x30 */
    f32 finish;             /* 0x38 the race time */
    u32 zippedSize;         /* 0x3C */
    u32 unzippedSize;       /* 0x40 */
    u32 zippedChecksum;     /* 0x44 over the compressed recording */
    u32 unzippedChecksum;   /* 0x48 */
    u32 start;              /* 0x4C the recording's heap block once read: a window address, 32 bits as in the file */
    u32 offset;             /* 0x50 */
    u32 size;               /* 0x54 */
} tGhostHeader;

#define GHOST_REVISION 17

/* A ghost on a pak, as the ghost list keeps it (MAT3 tGhostFile), in the PC's layout (the N64's is 0x2C bytes:
 * GHOST_OFS). gGhostFiles lists them, fastest first. */
typedef struct tGhostFile { /* 0x40 bytes */
    void *ll;                 /* 0x0 the list's link (the next ghost's handle) */
    struct tPakHandle *fileh;  /* 0x8 */
    s8 track;                 /* 0x10 */
    u8 backward;              /* 0x11 */
    u8 name[13];              /* 0x12 */
    u8 pad1F;                 /* 0x1F */
    u32 playerId[2];          /* 0x20 */
    f32 finish;               /* 0x28 */
    u8 **datah;               /* 0x30 the recording once read */
    tGhostHeader **header;    /* 0x38 the file's data (its header first) once read */
} tGhostFile;
#define PROFILE_FILE_SIZE 2064

/* A profile in memory (MAT3 save.c's tPlayerProfile), in the PC's layout (pointers 8 bytes; the N64's is 0x30
 * bytes): PROFILE_OFS(n) is the PC offset of the N64's offset n, for the code that still reaches it so. Each pak's
 * profiles are player_profile_records, reached through handles (player_profile_handles); gPlayerFiles lists the
 * profiles read, gRealJustPlayers are the four without a pak ("just play"). */
typedef struct tProfile { /* 0x40 bytes */
    void *next;                /* 0x0 the list of profiles (handles) */
    void *prev;                /* 0x8 */
    struct tPakHandle *fileh;   /* 0x10 its pak file, NULL for a profile without one */
    u32 id[2];                 /* 0x18 tSaveData.id */
    u8 name[13];               /* 0x20 */
    s8 controls[10];           /* 0x2D */
    s8 rumble;                 /* 0x37 */
    tSaveData **datah;         /* 0x38 its file's data once read (the pak file's data handle), else NULL */
} tProfile;

typedef struct Rec164A4 { /* 0x2A bytes */
    s16 unk00[3]; /* 0x0 */
    s16 unk06;    /* 0x6 */
    s16 unk08[9]; /* 0x8 */
    s16 unk1A;    /* 0x1A */
    s16 unk1C;    /* 0x1C */
    s16 unk1E;    /* 0x1E */
    s16 unk20;    /* 0x20 */
    s16 unk22[4]; /* 0x22 */
} Rec164A4;

typedef struct RecordOwner { /* 0x78 bytes */
    intptr_t names[2][5];    /* 0x0 */
    struct Profile **who[5]; /* 0x50 */
} RecordOwner;


typedef struct tBlastMat { /* 0x8 bytes */
    struct tUVMatrix *mat; /* 0x0 */
} tBlastMat;

typedef struct tStatList { /* 0x28 bytes */
    u8 flag;     /* 0x0 */
    s32 count;   /* 0x4 */
    s32 size;    /* 0x8 */
    void *mem;   /* 0x10 */
    void *head;  /* 0x18 */
    void *store; /* 0x20 */
} tStatList;

/* ---- track: tracks, the road tree, targets, paths and checkpoints */

/* A point of a drone path (MAT3 anon_pos_mph_8): where the computer cars drive, and how fast. */
typedef struct DronePathPt { /* 0x8 bytes */
    s16 pos[3]; /* 0x0 */
    u8 mph;     /* 0x6 the speed to drive at here (CalcCheckpointTimes: mph * 1.4666667 is feet a second) */
    s8 hints;   /* 0x7 driving hints for the drones here */
} DronePathPt;

/* A drone path (MAT3 anon_numPts_pts_8, gDronePaths[4]): a loop of points round the track that the computer cars
 * follow (maxpath.c), read from the track's path file (ReadPathData). Host layout: pts is a host pointer. */
typedef struct DronePath { /* 0x10 bytes */
    u16 numPts;       /* 0x0 */
    u16 pad02;        /* 0x2 the path file's padding, copied with numPts (ReadPathData) */
    DronePathPt *pts; /* 0x8 */
} DronePath;

/* An alternative path (MAT3 anon_resurrect_firstPath_10, gResPath.alt[] = gAltPaths): a branch of the resurrection
 * path, a shortcut or a second route, whose ends join the main path or another branch (SyncAltPaths links them). On
 * the PC ReadPathData keeps them in a host array of its own (pts is a host pointer). */
typedef struct AltPath { /* 0x18 bytes */
    u8 resurrect;    /* 0x0 0 a branch no car is put back on (a respawn point on it is traced on to the path it
                      * joins: FindResPt); ALT_PATH_RESURRECT one cars may be put back on (its points count in the
                      * track's bounds, CalcResPathBounds); ALT_PATH_ALONE one joined to nothing (SyncAltPaths) */
    s8 firstPath;    /* 0x1 the path its first point joins: an alternative path, or -1 the main path */
    u16 firstIdx;    /* 0x2 the point of firstPath it joins there */
    s8 lastPath;     /* 0x4 the path its last point joins */
    u16 lastIdx;     /* 0x6 the point of lastPath it joins there */
    s8 startSection; /* 0x8 the checkpoint section it starts in (TraceToCheckpoint), -1 none */
    u16 numPts;      /* 0xA */
    s16 (*pts)[3];   /* 0x10 */
} AltPath;

/* AltPath.resurrect */
#define ALT_PATH_RESURRECT 1
#define ALT_PATH_ALONE 2

typedef struct MineSlots { /* 0x28 bytes */
    struct Target *target[4]; /* 0x0 */
    s32 count;                /* 0x20 */
} MineSlots;

/* The resurrection path (MAT3 anon_numPts_pts_10, gResPath): the line down the middle of the road that a crashed car
 * is put back on (resurrect.c) and that measures the cars' progress (maxpath.c), with its branches. Host layout. */
typedef struct ResurrectPath { /* 0x20 bytes */
    u16 numPts;    /* 0x0 the main path's points */
    s16 (*pts)[3]; /* 0x8 = gResPathPtList */
    u8 numAlt;     /* 0x10 */
    AltPath *alt;  /* 0x18 = gAltPaths, NULL when there are none */
} ResurrectPath;

/* A kind of target (MAT3 tTargetInfo, targetInfo[122], ROM data): a name the track's objects are matched by, its
 * model, its functions and its collision. Host layout, 0x40 bytes (N64 0x30): its first four members are pointers
 * (TINFO_OFS). MAT3's names but visFunc and sound (MAT3 animFunc, startSnd: the names other files use). */
typedef struct TargInfo { /* 0x40 bytes */
    char *name;         /* 0x0 the start of the track objects' names it matches (SetupTarget) */
    char *mboxname;     /* 0x8 its model's name */
    void *startFunc;    /* 0x10 the function that starts it (StartCactus ...) */
    void *visFunc;      /* 0x18 its visual's animator (AnimateCoin ...) */
    s16 collFuncIdx;    /* 0x20 its collision test (gTargetCollFuncPtrs; 4 a bounding box, 6 none) */
    u16 Flags;          /* 0x22 2 an environment visual animates it, 4 its start function runs then too, 8 a record
                         * of its own kind (gTargetOwnStructPtrs), 0x20 kept without the Expansion Pak, 0x100 one
                         * player only, 0x400 its data is the track's object record, 0x800 in no world group,
                         * 0x4000 started when defined, 0x8000 a bounding box model (gBBoxModelStructs) */
    s16 objListIdx;     /* 0x24 its object definition (gObjList) */
    s8 structureID;     /* 0x26 the kind of record its data is (gTargetStructPtrs[]): 4 a path, 5 a weapon, 6 a coin */
    s8 structureIdx;    /* 0x27 its record among its kind's static ones (gTargetStructInfoPtrs) */
    f32 collVal;        /* 0x28 its collision radius, or for a box its row of BBoxCollInfo; -1 the model's radius */
    s32 sound;          /* 0x2C the sounds: starting (MAT3 startSnd), moving and stopping, -1 none */
    s32 moveSnd;        /* 0x30 */
    s32 stopSnd;        /* 0x34 */
    s32 replace;        /* 0x38 the sound's replace mode */
    f32 fadeDist;       /* 0x3C the sound's fade distance */
} TargInfo;

/* gTargetList, the targets' node pool (MAT3 tStatList): 130 nodes of 0x70 bytes in gTargetArray. Host layout; the
 * nodes themselves keep the N64's (tTarget). */
typedef struct TargetNodePool { /* 0x28 bytes */
    u8 dbl;          /* 0x0 doubly linked (not this one) */
    u8 n64;          /* 0x1 PC: the nodes link by 32-bit window addresses (pc_statll_link) */
    u8 pad2[2];      /* 0x2 */
    s32 numElements; /* 0x4 */
    s32 elementSize; /* 0x8 */
    u8 *list;        /* 0x10 the nodes */
    intptr_t head;   /* 0x18 the first target in use (a tTarget *), 0 none */
    u8 *store;       /* 0x20 the first free node */
} TargetNodePool;

/* A target (MAT3 tTarget): one of the track's objects that take part in the race: a cone, a cactus, a fence, a
 * window, a coin, a mine, a weapon, a moving object on a path. Each is a node of gTargetList (SetupTarget), with a
 * kind (targetInfo[infoIdx]) and maybe a record of its kind's (data). In the N64's layout on the PC: its link and
 * its data are 32-bit window addresses (PC_N64P; docs/port/POINTERS.md 10). */
typedef struct tTarget { /* 0x70 bytes */
    u32 next;      /* 0x00 ll.next: the next target in its list, a window address; 0 the last */
    u8 Flags;      /* 0x04 TARGET_* */
    s32 mbIdx;     /* 0x08 its model (MBOX_FindObject_WORK); a path target's or a world object's key, once set up */
    s32 dlIdx;     /* 0x0C its scene object (ZOID_AddObject), -1 none */
    s16 infoIdx;   /* 0x10 its kind: its index in targetInfo */
    UVMat mat;     /* 0x14 its orientation and position, which its scene object is drawn at */
    f32 pos[3];    /* 0x44 its position in the world (MBOX_GetPosition) */
    s16 type;      /* 0x50 the object definition it shows now (gObjList) */
    f32 radius;    /* 0x54 its collision radius */
    s16 otype;     /* 0x58 the object definition it started with */
    s16 frames;    /* 0x5A its animation's frames */
    s8 slot;       /* 0x5C the car that hit it, -1 none */
    s32 sound;     /* 0x60 its sound emitter, -1 none */
    s8 sndState;   /* 0x64 its sound: 0 starting, 1 moving, 2 stopped */
    s8 owner;      /* 0x65 the car that owns it (a mine); a path target's path; a box's mini model */
    f32 timeStamp; /* 0x68 */
    u32 data;      /* 0x6C its kind's record (gTargetStructPtrs[structureID]), a window address; 0 none */
} tTarget;

/* tTarget.Flags */
#define TARGET_SET_UP 0x1    /* its first frame is done (a path target's StartPathTarget) */
#define TARGET_ACTIVE 0x2    /* still in play: CheckTargetCollision tests only these (with TARGET_COLLIDES); a hit
                              * object's start function clears it, and a path target without it is removed
                              * (AnimatePathTarget) */
#define TARGET_COLLIDES 0x8  /* it has a collision test (collFuncIdx not 6) */
#define TARGET_IN_WORLD 0x10 /* a path target the world placed (not from a PTHD record) */
#define TARGET_GOLD 0x40     /* a gold coin */

/* A point of a target path (MAT3 anon_pos_norm_44), the track WAD's PTHD records' points. N64 layout, 0x44 bytes. */
typedef struct TargetPathPt {
    f32 pos[3];     /* 0x00 */
    f32 norm[3];    /* 0x0C the direction on to the next point */
    f32 scale[3];   /* 0x18 the target's scale here */
    f32 quat[4];    /* 0x24 its orientation here */
    f32 distToNext; /* 0x34 the distance to the next point (a stretch whose flags have 0x10000008: moved evenly) */
    f32 timeToNext; /* 0x38 the time to the next point */
    f32 speed;      /* 0x3C the speed here (between points it changes evenly) */
    s32 flags;      /* 0x40 */
} TargetPathPt;

/* A path target's record (MAT3 tTargetStructPath, tTarget.data of a target whose structureID is 4): where it is on
 * its path. A heap record in the N64's layout: pathData is a 32-bit window address. */
typedef struct tTargetStructPath { /* 0x48 bytes */
    u32 pathData;       /* 0x00 its path (a PTHD record, tBasicTargetPath), a window address */
    f32 timeElapsed;    /* 0x04 the time since it left point ptOn (or, going backwards, to reach it) */
    f32 curDistToNext;  /* 0x08 */
    s16 ptOn;           /* 0x0C the point it is at or last passed */
    u16 flags;          /* 0x0E TPATH_FORWARD or TPATH_BACKWARD */
    f32 curVel;         /* 0x10 its speed, feet a second */
    f32 maxVel;         /* 0x14 the path's top speed */
    f32 lastPos[3];     /* 0x18 its position the frame before */
    f32 lastUVS[3][3];  /* 0x24 its orientation at its last point, transposed (for its collision object) */
} tTargetStructPath;

/* tTargetStructPath.flags */
#define TPATH_FORWARD 4
#define TPATH_BACKWARD 8

/* A node of the targets' quadtree (MAT3 tTargetGroup, the track WAD's GDAT records, gTargetGroups): a rectangle of
 * the ground; a node split in four (flags 1) has four children, a leaf a run of gTargetGroupList. */
typedef struct tTargetGroup { /* 0x1C bytes */
    s16 parent;   /* 0x00 */
    u8 unused;    /* 0x02 */
    u8 flags;     /* 0x03 1 split in four */
    f32 minh;     /* 0x04 x from */
    f32 maxh;     /* 0x08 x to */
    f32 minv;     /* 0x0C z from */
    f32 maxv;     /* 0x10 z to */
    u16 child[4]; /* 0x14 split: its children (south-west, south-east ...: CheckTargetCollision picks by the
                   * middle, child[2]'s top corner); a leaf: [0] how many targets, [1] the first in
                   * gTargetGroupList */
} tTargetGroup;

/* A bounding box model (MAT3 tMiniModelData, gBBoxModelStructs[]): a moving target the cars collide with as a box.
 * A heap record in the N64's layout: bboxInfo is a 32-bit window address. */
typedef struct tMiniModelData { /* 0x40 bytes */
    u32 bboxInfo;  /* 0x00 its box, a row of BBoxCollInfo (a window address) */
    f32 uvs[3][3]; /* 0x04 its orientation */
    f32 pos[3];    /* 0x28 */
    f32 vel[3];    /* 0x34 */
} tMiniModelData;

/* The surface tree (MAT3 stree.c): the track's collision surfaces, from its collision WAD (InitSTree). No pointers,
 * the same layout on both; the files read stree_save, stree_svert and stree_nodes (declared u8 *) through views of
 * these. A vertex position is in 1/32 feet: whole feet in pos, the three axes' 5-bit fractions in overflow. */
/* The collision WAD's header (MAT3 stheadertyp, gRoadHead): how many of each record follow it, in this order: the
 * road path (gRoadPath, MAT3 pathtyp, 0x84 bytes each), the tree's nodes, the surfaces, the vertices, the saved
 * surfaces, the vertex lists, the surface lists (InitSTree). */
typedef struct stheadertyp { /* 0x10 bytes */
    u16 pnum;     /* 0x0 road path points */
    u16 tnum;     /* 0x2 tree nodes */
    u16 snum;     /* 0x4 surfaces */
    u16 vnum;     /* 0x6 vertices */
    u16 savenum;  /* 0x8 saved surfaces */
    u16 vlnum;    /* 0xA bytes of vertex lists */
    u32 slnum;    /* 0xC bytes of surface lists */
} stheadertyp;

typedef struct sverttyp { /* 0x8 bytes, stree_svert[] */
    s16 pos[3];   /* 0x0 whole feet */
    u16 overflow; /* 0x6 the fractions: x << 10 | y << 5 | z, 32nds */
} sverttyp;

/* A saved surface of a collision object (MAT3 savetyp, stree_save[stree_savenum]): a surface and one of its
 * vertices as the track had them, which moving targets move and turn (MoveCollisionObject, RotateCollisionObject)
 * and ResetCollisionObject puts back. */
typedef struct savetyp { /* 0x20 bytes */
    u16 object;     /* 0x00 the collision object it belongs to (a target path's collIdx) */
    u16 snum;       /* 0x02 the surface (stree_surf[]) */
    s16 uvs[3][3];  /* 0x04 the surface's orientation as the track had it, 1.0 = 0x4000 */
    u16 vnum;       /* 0x16 the vertex (stree_svert[]) */
    s16 pos[3];     /* 0x18 the vertex as the track had it (sverttyp) */
    u16 overflow;   /* 0x1E */
} savetyp;

/* A node of the surface tree (MAT3 streetyp, stree_nodes[]): a rectangle of the ground, in feet; split in four
 * (flags 1), or a leaf with a run of surfaces. */
typedef struct streetyp { /* 0x14 bytes */
    s16 parent;   /* 0x0 */
    u8 unused;    /* 0x2 */
    u8 flags;     /* 0x3 */
    s16 minh;     /* 0x4 */
    s16 maxh;     /* 0x6 */
    s16 minv;     /* 0x8 */
    s16 maxv;     /* 0xA */
    u16 child[4]; /* 0xC */
} streetyp;

/* An element of gAlwaysColList (MAT3 tAlwaysCol): a target every car is tested against, wherever it is (battle's
 * weapons). Host layout: both are host pointers (16 bytes). */
typedef struct tAlwaysCol {
    struct tAlwaysCol *next; /* ll.next */
    tTarget *t;
} tAlwaysCol;

typedef struct TrackLoadInfo { /* 0x2C bytes */
    s8 trackInited;           /* 0x0 */
    TrackLoadState track;     /* 0x4 */
    TrackLoadState world;     /* 0xC */
    TrackLoadState collision; /* 0x14 */
    TrackLoadState path;      /* 0x1C */
    TrackLoadState other;     /* 0x24 */
} TrackLoadInfo;

typedef struct WorldPoint { /* 0x6 bytes */
    s16 x; /* 0x0 */
    s16 y; /* 0x2 */
    s16 z; /* 0x4 */
} WorldPoint;

typedef struct tCheckpoints { /* 0x32C bytes */
    s16 startTime;          /* 0x0 */
    s16 loopPt;             /* 0x2 */
    s16 finishPt;           /* 0x4 */
    s16 startPt;            /* 0x6 */
    s16 numPoints;          /* 0x8 */
    tCheckpoint points[10]; /* 0xC */
} tCheckpoints;

/* ---- cars: the cars, their visuals, skids and smoke */

typedef union ArrowColor { /* 0x4 bytes */
    struct { u8 r; u8 g; u8 b; u8 a; } c;
    u32 rgba;
} ArrowColor;

typedef struct CarPalette { /* 0x18 bytes */
    u8 flag;       /* 0x0 */
    u8 other1[15]; /* 0x1 */
    u8 active;     /* 0x10 */
    u8 marker;     /* 0x11 */
    u16 value;     /* 0x12 */
    u32 data;      /* 0x14 */
} CarPalette;

typedef struct CarPartSet { /* 0x40 bytes */
    s32 frame;        /* 0x0 */
    u8 unknown04[20]; /* 0x4 */
    s32 weap;         /* 0x18 */
    u8 unknown1c[36]; /* 0x1C */
} CarPartSet;

typedef struct Collision16 { /* 0x10 bytes */
    f32 front;  /* 0x0 */
    f32 rear;   /* 0x4 */
    f32 side;   /* 0x8 */
    f32 height; /* 0xC */
} Collision16;

/* An environment visual, an entry of gEnvirons (a tVisual: the next on its list, its index, scene node and car
 * slot, a data word, a time and its animator). */
typedef struct EnvironNode { /* 0x20 bytes */
    struct Node *next; /* 0x0 */
    s16 index;         /* 0x8 */
    s16 objnum;        /* 0xA */
    s16 slot;          /* 0xC */
    u8 padA[2];        /* 0xE */
    u32 data;          /* 0x10 */
    f32 timeStamp;     /* 0x14 */
    void *func;        /* 0x18 */
} EnvironNode;

/* A skid mark, an entry of gSkidArray (MAT3 tSkid): its list links, when it last grew, its polygon, its middle, its
 * colour, and whether it has stopped (and so fades). */
typedef struct PcEffectObj { /* 0x38 bytes */
    void *next;      /* 0x0 */
    void *prev;      /* 0x8 */
    f32 lastTime;    /* 0x10 */
    void *poly;      /* 0x18 */
    f32 pos[3];      /* 0x20 */
    u8 color[4];     /* 0x2C */
    s32 active;      /* 0x30 */
} PcEffectObj;

/* A segment of a light streak's ribbon, an entry of gStreakPolyArray (MAT3 tStreakPoly): its polygon, the seconds
 * it has left, the time to its next fade step, its colour and its streak. */
typedef struct PcFade { /* 0x30 bytes */
    void *next;    /* 0x0 */
    s32 prev;      /* 0x8 */
    void *poly;    /* 0x10 */
    f32 lifeTime;  /* 0x18 */
    f32 incTime;   /* 0x1C */
    u8 clr[4];     /* 0x20 */
    void *s;       /* 0x28 */
} PcFade;

/* A smoke or spray puff, an entry of gSmokeObjs (MAT3 smoke.c's puff): its orientation and size, its position, its
 * scene node, the seconds it has left, the time to its next animation frame, its drift, its data word (PUFF_*),
 * its car and its animation frame. */
typedef struct PcPuff { /* 0x60 bytes */
    void *next;       /* 0x0 */
    f32 uvs[3][3];    /* 0x8 */
    f32 pos[3];       /* 0x2C */
    s32 mbIdx;        /* 0x38 */
    u8 pad3C[4];      /* 0x3C MAT3 tlifeTime, a float the code reads through its own views */
    f32 lifeTime;     /* 0x40 */
    f32 swTime;       /* 0x44 */
    f32 vel[3];       /* 0x48 */
    u32 data;         /* 0x54 */
    s16 slot;         /* 0x58 */
    s16 tIdx;         /* 0x5A */
} PcPuff;

/* A light streak behind a battle shot, an entry of gStreakArray (MAT3 tStreak): its flags (STREAK_*), the time to
 * its next segment, its shot's new and last points, and its ribbon's back edge ([0] the lower corner, [1] the
 * upper). */
typedef struct PcTrail { /* 0x48 bytes */
    void *next;          /* 0x0 */
    s32 prev;            /* 0x8 */
    s32 flags;           /* 0xC */
    f32 delay;           /* 0x10 */
    f32 newpos[3];       /* 0x14 */
    f32 lastpos[3];      /* 0x20 */
    f32 backVerts[2][3]; /* 0x2C */
} PcTrail;

/* A handling tune, an entry of gTune (MAT3's anonymous tune record, through tModelData.tune): the factors carsetup
 * applies to the car's descriptor (MAT3's field names). */
typedef struct Tune44 { /* 0x2C bytes */
    f32 diffbl;          /* 0x0 */
    f32 weight_scale;    /* 0x4 the mass (CalcMass) */
    f32 torque_scale;    /* 0x8 the engine's torque */
    f32 trans_scale;     /* 0xC the gear ratios */
    f32 clutchmaxtk;     /* 0x10 */
    f32 shiftk;          /* 0x14 */
    f32 fwdfk;           /* 0x18 */
    f32 sidefk;          /* 0x1C */
    f32 skidk;           /* 0x20 */
    f32 antispin;        /* 0x24 */
    f32 drift;           /* 0x28 */
} Tune44;

typedef struct tUVPair { /* 0x10 bytes */
    u16 uv[8]; /* 0x0 */
} tUVPair;

/* ---- physics: the car simulation: collision, the road, the drivetrain, tyres, respawning */

typedef struct PlayerInput { /* 0x50 bytes */
    u8 b0;       /* 0x0 */
    u8 control;  /* 0x1 */
    u8 pad2[70]; /* 0x2 */
    void *ref;   /* 0x48 */
} PlayerInput;

typedef struct Row3 { /* 0x6 bytes */
    s16 v[3]; /* 0x0 */
} Row3;

typedef struct tSkidData { /* 0x1C bytes */
    s32 forceTable[5];  /* 0x0 */
    f32 skidThreshold;  /* 0x14 */
    f32 smokeThreshold; /* 0x18 */
} tSkidData;

typedef struct tStuntPlayer { /* 0x78 bytes */
    s16 show;            /* 0x0 */
    s16 crash;           /* 0x2 */
    f32 time;            /* 0x4 */
    s32 maxStuntPoints;  /* 0x8 */
    s32 points;          /* 0xC */
    s32 pointsHud;       /* 0x10 */
    s32 totalPoints;     /* 0x14 */
    s32 numstunts;       /* 0x18 */
    s32 lastStuntType;   /* 0x1C */
    s16 stunts[10];      /* 0x20 */
    s16 stuntsHud[10];   /* 0x34 */
    s16 totalStunts[10]; /* 0x48 */
    s16 unused;          /* 0x5C */
    f32 wingTime;        /* 0x60 */
    s8 wingBonus;        /* 0x64 */
    s8 wingBonusHud;     /* 0x65 */
    s8 doneBonus;        /* 0x66 */
    s8 doneBonusHud;     /* 0x67 */
    s32 bonus;           /* 0x68 */
    s32 bonusHud;        /* 0x6C */
    s32 showBonus;       /* 0x70 */
    s32 done;            /* 0x74 */
} tStuntPlayer;

/* ---- race: the game loop and the race: state, countdown, drones, stunts, battle, ghosts */

/* A race of a circuit (MAT3 tCircuitTrack, gCircuitTracks[]: the arena holds them all; ContinueCircuit). */
typedef struct CircuitTrackSlot { /* 0x5 bytes */
    u8 trackNum; /* 0x0 */
    u8 backward; /* 0x1 */
    u8 mirror;   /* 0x2 */
    u8 fog;      /* 0x3 0 none to 3 thick */
    u8 wind;     /* 0x4 */
} CircuitTrackSlot;

typedef struct GhostFile { /* 0x40 bytes */
    void *ll;                    /* 0x0 */
    void **fileh;                /* 0x8 */
    s8 track;                    /* 0x10 */
    u8 backward;                 /* 0x11 */
    u8 name[13];                 /* 0x12 */
    u8 pad17;                    /* 0x1F */
    u32 playerId[2];             /* 0x20 */
    f32 finish;                  /* 0x28 */
    u8 **datah;                  /* 0x30 */
    struct GhostHeader **header; /* 0x38 */
} GhostFile;

typedef struct GhostPlayer { /* 0x40 bytes */
    void *next;      /* 0x0 */
    void *prev;      /* 0x8 */
    void *fileh;     /* 0x10 */
    u32 playerId[2]; /* 0x18 */
    u8 name[13];     /* 0x20 */
    u8 pad21[11];    /* 0x2D */
    u8 **datah;      /* 0x38 */
} GhostPlayer;

typedef struct GhostSaveHeader { /* 0x58 bytes */
    u32 headerChecksum;   /* 0x0 */
    u8 revision;          /* 0x4 */
    s8 flag;              /* 0x5 */
    s8 track;             /* 0x6 */
    u8 backward;          /* 0x7 */
    u8 car;               /* 0x8 */
    u8 carTrans;          /* 0x9 */
    s8 settings[12];      /* 0xA */
    u8 name[13];          /* 0x16 */
    u8 pad23[5];          /* 0x23 */
    u32 playerId[2];      /* 0x28 */
    u32 bestTime;         /* 0x30 */
    f32 frameTime;        /* 0x34 */
    f32 finish;           /* 0x38 */
    u32 zippedSize;       /* 0x3C */
    u32 unzippedSize;     /* 0x40 */
    u32 zippedChecksum;   /* 0x44 */
    u32 unzippedChecksum; /* 0x48 */
    u32 start;            /* 0x4C */
    u32 offset;           /* 0x50 */
    s32 size;             /* 0x54 */
} GhostSaveHeader;

typedef struct MuzzleObj { /* 0x10C bytes */
    s32 mdls[5];       /* 0x0 */
    MuzzleMat mats[5]; /* 0x14 */
    s32 on;            /* 0x104 */
    f32 onTime;        /* 0x108 */
} MuzzleObj;

typedef struct PcHitPoly { /* 0x10 bytes */
    void *next; /* 0x0 */
    void *poly; /* 0x8 */
} PcHitPoly;

typedef struct PcShot { /* 0x78 bytes */
    void *next;     /* 0x0 */
    s8 slot;        /* 0x8 */
    s8 slotDamaged; /* 0x9 */
    s8 type;        /* 0xA */
    s8 flags;       /* 0xB */
    f32 lifeTime;   /* 0xC */
    f32 incTime;    /* 0x10 */
    f32 radius;     /* 0x14 */
    f32 vel[3];     /* 0x18 */
    f32 pos[3];     /* 0x24 */
    f32 old[3];     /* 0x30 */
    f32 uvs[3][3];  /* 0x3C */
    void *streak;   /* 0x60 */
    void *obj;      /* 0x68 */
    void *obj2;     /* 0x70 */
} PcShot;

typedef struct PcShotCase { /* 0x50 bytes */
    void *next;  /* 0x0 */
    s32 mbIdx;   /* 0x8 */
    f32 mat[12]; /* 0xC */
    f32 time;    /* 0x3C */
    f32 vel[3];  /* 0x40 */
} PcShotCase;

typedef struct PcShotObj { /* 0x48 bytes */
    void *next;     /* 0x0 */
    s32 mbIdx;      /* 0x8 */
    f32 mat[12];    /* 0xC */
    void *shotInfo; /* 0x40 */
} PcShotObj;

typedef struct RGB { /* 0x3 bytes */
    u8 r; /* 0x0 */
    u8 g; /* 0x1 */
    u8 b; /* 0x2 */
} RGB;

/* The scorch marks on the track (MAT3 worlddam.c's DamagePolys, anon_full_oldest_68): a ring of 25 polygons, the
 * oldest replaced once full. Host layout. */
typedef struct RingState { /* 0xD0 bytes */
    s8 full;           /* 0x0 all 25 used */
    u8 unknown01;      /* 0x1 */
    u16 oldest;        /* 0x2 the next to replace */
    void *polys[25];   /* 0x8 */
} RingState;

typedef struct Shield { /* 0x34 bytes */
    s32 dlIdx; /* 0x0 */
    UVMat mat; /* 0x4 */
} Shield;

typedef struct StuntInfo { /* 0x7C bytes */
    s32 active;  /* 0x0 */
    u8 pad[120]; /* 0x4 */
} StuntInfo;

/* A power-up on the battle arena (MAT3 tBattlePowerUpT, gBattlePowerUps.tS[]): its target. Host layout. */
typedef struct tBattlePowerUpT { /* 0x10 bytes */
    tTarget *t;  /* 0x0 */
    s16 last;    /* 0x8 */
    s16 type;    /* 0xA */
} tBattlePowerUpT;

/* The battle arena's power-ups (MAT3 tBattlePowerUps, gBattlePowerUps): how many, and the time left until each kind
 * appears again (health, invisibility, shield). */
typedef struct tBattlePowerUps { /* 0x18 bytes */
    s8 num;               /* 0x0 */
    u8 idx[3];            /* 0x1 MAT3 hIdx, sIdx, iIdx: the power-up each kind last came at */
    f32 healthTLeft;      /* 0x4 */
    f32 invsTLeft;        /* 0x8 */
    f32 shieldTLeft;      /* 0xC */
    tBattlePowerUpT *tS;  /* 0x10 the power-ups (a heap block) */
} tBattlePowerUps;

/* A car's cannon flash (MAT3 tCMuzzleObj, gCannMuzzleObjs[4]). The data loader lays it out by this declaration, so
 * its other members stay bytes; weapons.c reads it whole as CMuzzleObj. */
typedef struct tCMuzzleObj { /* 0x148 bytes */
    u8 mdls_mats[260];  /* 0x0 mdls[5] (s32), mats[5] (UVMat) */
    s32 ripdl;          /* 0x104 the ripple's scene object, -1 none */
    u8 mat[48];         /* 0x108 the ripple's UVMat */
    s8 ripCnt;          /* 0x138 -1 none */
    u8 ripTIdx_ripTime[11]; /* 0x139 ripTIdx, on, ripScale, ripTime */
    f32 onTime;         /* 0x144 */
} tCMuzzleObj;

/* A car of a circuit (MAT3 tCircuitCar, gCircuitCars[6]: the arena holds six; the data loader knows the first):
 * its driver, its standing and its results race by race. */
typedef struct tCircuitCar { /* 0x4C bytes */
    u8 name;          /* 0x0 its driver (the roster's car) */
    u8 totalPlace;    /* 0x1 its place in the circuit so far */
    u16 totalPoints;  /* 0x2 its points so far */
    u8 places[24];    /* 0x4 its place in each race */
    u8 points[24];    /* 0x1C its points from each race */
    s16 skill[12];    /* 0x34 a drone's skill in each race */
} tCircuitCar;

/* ---- camera: the cameras */

/* A camera (MAT3 anon_mat_n64_98, gCamera[4], one per window): where it looks from. The data loader lays gCamera
 * out by this declaration, its bytes from 0x30 as bytes; the camera files read it whole as tCamera (below). */
typedef struct CameraView { /* 0x98 bytes */
    f32 uvs[3][3];     /* 0x0 its orientation */
    f32 pos[3];        /* 0x24 its position */
    u8 rest[104];      /* 0x30 n64, fcam, view2spring, slot */
} CameraView;

/* A camera whole (MAT3 anon_mat_n64_98): the camera files' view of gCamera (VIEW(tCamera[], gCamera)). */
typedef struct tCamera { /* 0x98 bytes */
    UVMat mat;        /* 0x00 its orientation and position, as the cameras set them */
    UVMat n64;        /* 0x30 what the N64 draws by: mat with four elements negated (UpdateCameras) */
    UVMat fcam;       /* 0x60 the fixed camera's: its pos is where the crash and finish views look from */
    f32 view2spring;  /* 0x90 */
    u8 slot;          /* 0x94 the car it shows (ZeroCamera starts camera i on car i) */
    u8 pad95[3];
} tCamera;

typedef struct PcFscls { /* 0x1EC bytes */
    f32 fscls[3];          /* 0x0 */
    PcRespawn respawn[30]; /* 0xC */
} PcFscls;

/* ---- hud: the HUD */

typedef struct BattlePointEvent { /* 0x18 bytes */
    s8 active;       /* 0x0 */
    s8 age;          /* 0x1 */
    s8 column;       /* 0x2 */
    s8 row;          /* 0x3 */
    f32 from_x;      /* 0x4 */
    f32 from_y;      /* 0x8 */
    f32 to_x;        /* 0xC */
    f32 to_y;        /* 0x10 */
    s8 delta;        /* 0x14 */
    u8 unknown15[3]; /* 0x15 */
} BattlePointEvent;

typedef struct CoinRow { /* 0x68 bytes */
    CoinSlot slot[2]; /* 0x0 */
} CoinRow;

typedef struct CreditPage { /* 0x58 bytes */
    char *names[11]; /* 0x0 */
} CreditPage;

typedef union HudColor { /* 0x4 bytes */
    u32 word;
    struct { u8 r; u8 g; u8 b; u8 a; } rgba;
} HudColor;

typedef struct NameQuad { /* 0x20 bytes */
    char *name[4]; /* 0x0 */
} NameQuad;

typedef struct tHudIcon { /* 0x34 bytes */
    s16 type;         /* 0x0 */
    s16 idx;          /* 0x2 */
    UVMatrixRows mat; /* 0x4 */
} tHudIcon;

typedef struct tPalette { /* 0x18 bytes */
    char name[16]; /* 0x0 */
    u8 start;      /* 0x10 */
    u8 end;        /* 0x11 */
    u16 format;    /* 0x12 */
    u32 palBase;   /* 0x14 */
} tPalette;

typedef struct tStuntCoin { /* 0x48 bytes */
    s16 type;         /* 0x0 */
    s16 idx;          /* 0x2 */
    UVMatrixRows mat; /* 0x4 */
    f32 alpha;        /* 0x34 */
    s16 screenPos[2]; /* 0x38 */
    s16 startPos[2];  /* 0x3C */
    s16 destPos[2];   /* 0x40 */
    f32 scrollTime;   /* 0x44 */
} tStuntCoin;

/* ---- frontend: the front end's screens (runtime image A) */

typedef union ButtonColor { /* 0x4 bytes */
    u32 word;
    u8 channel[4];
} ButtonColor;

typedef struct CarComponentSlot { /* 0x34 bytes */
    s16 idx;               /* 0x0 */
    s16 level;             /* 0x2 */
    u8 matrix_and_pos[48]; /* 0x4 */
} CarComponentSlot;

typedef struct CarScreenCar { /* 0xC bytes */
    f32 selection;   /* 0x0 */
    f32 angle;       /* 0x4 */
    s8 position;     /* 0x8 */
    s8 car;          /* 0x9 */
    u8 unknown0A[2]; /* 0xA */
} CarScreenCar;

typedef struct CarScreenMsgPos { /* 0xC bytes */
    f32 v[3]; /* 0x0 */
} CarScreenMsgPos;

typedef struct CircuitScaleView { /* 0x44 bytes */
    u8 other[32]; /* 0x0 */
    f32 scale;    /* 0x20 */
    u8 tail[32];  /* 0x24 */
} CircuitScaleView;

typedef struct ColorWord { /* 0x4 bytes */
    u32 word; /* 0x0 */
} ColorWord;

typedef struct ComponentLights { /* 0x28 bytes */
    u8 col[3];           /* 0x0 */
    u8 pad0;             /* 0x3 */
    u8 colc[3];          /* 0x4 */
    u8 pad1;             /* 0x7 */
    ComponentLight l[2]; /* 0x8 */
} ComponentLights;

typedef struct Egg { /* 0xA4 bytes */
    u8 size;        /* 0x0 */
    u8 pad[3];      /* 0x1 */
    u32 levels[20]; /* 0x4 */
    u32 edges[20];  /* 0x54 */
} Egg;

typedef struct HighScoreRect { /* 0xC bytes: a player's box on the high-score screen */
    s16 x;          /* 0x0 */
    s16 y;          /* 0x2 */
    s16 w;          /* 0x4 */
    s16 h;          /* 0x6 */
    s16 cx;         /* 0x8 its centre, where the text goes */
    s16 cy;         /* 0xA */
} HighScoreRect;

typedef struct MapLod { /* 0x10 bytes */
    u16 texidx;      /* 0x0 */
    u16 flags;       /* 0x2 */
    f32 lodRad;      /* 0x4 */
    u32 displayList; /* 0x8 */
    u32 verts;       /* 0xC */
} MapLod;

typedef struct MenuButton { /* 0x40 bytes */
    s32 type;         /* 0x0 */
    s32 handle;       /* 0x4 */
    f32 angle;        /* 0x8 */
    f32 matrix[3][3]; /* 0xC */
    f32 position[3];  /* 0x30 */
    u8 alpha;         /* 0x3C */
    u8 pad[3];        /* 0x3D */
} MenuButton;

typedef struct ModeScreenButton { /* 0x40 bytes */
    s32 unknown0;     /* 0x0 */
    s32 id;           /* 0x4 */
    f32 angle;        /* 0x8 */
    f32 matrix[3][3]; /* 0xC */
    f32 position[3];  /* 0x30 */
    s32 unknown3C;    /* 0x3C */
} ModeScreenButton;

typedef struct MotionFrame { /* 0xE0 bytes */
    BackgroundMotion slot[4]; /* 0x0 */
} MotionFrame;

typedef struct ObscenityList { /* 0xB0 bytes */
    u8 *p[22]; /* 0x0 */
} ObscenityList;

typedef struct PlayerRect { /* 0x18 bytes */
    s32 x;  /* 0x0 */
    s32 y;  /* 0x4 */
    s32 w;  /* 0x8 */
    s32 h;  /* 0xC */
    s32 cx; /* 0x10 */
    s32 cy; /* 0x14 */
} PlayerRect;

typedef struct SelectCarState { /* 0x18 bytes: a car on a car select screen (MAT3 anon_created_hidden_18, gSelectCar) */
    s8 created;          /* 0x0 its scene objects exist */
    s8 hidden;           /* 0x1 */
    s8 ready;            /* 0x2 its parts are set up */
    s8 loading;          /* 0x3 its model's load has started (LoadSelectCar) */
    s32 waveInst;        /* 0x4 its model's wave instance, -1 none */
    s8 color[3];         /* 0x8 the colours it was last drawn in (N64; MAT3 PaintJob, unused, pjModelNum) */
    u8 pad11;            /* 0xB */
    s32 rims;            /* 0xC its rim texture, -1 none */
    f32 frontTireScale;  /* 0x10 the tyre scales it was last drawn with, -1 none */
    f32 rearTireScale;   /* 0x14 */
} SelectCarState;

typedef struct SelectCarWaveSlot { /* 0xC bytes: a car model load (MAT3 anon_wave_loading_c, gSelectCarWavePtrs) */
    s32 wave;  /* 0x0 its wave instance, -1 none */
    s8 loading; /* 0x4 a load was started */
    s8 slot;   /* 0x5 the car slot it is for, -1 free */
    u8 pad[6]; /* 0x6 */
} SelectCarWaveSlot;

typedef struct SoundClearRecord { /* 0x30 bytes */
    struct pc_multiblit m; /* 0x0 */
} SoundClearRecord;

typedef struct StartScreenButton { /* 0x3C bytes */
    f32 x;            /* 0x0 */
    f32 y;            /* 0x4 */
    f32 angle;        /* 0x8 */
    u8 unknown0C[36]; /* 0xC */
    f32 position[3];  /* 0x30 */
} StartScreenButton;

typedef struct TrackMapEntry { /* 0x48 bytes */
    s32 handle;       /* 0x0 */
    f32 offset_x;     /* 0x4 */
    f32 offset_y;     /* 0x8 */
    f32 angle;        /* 0xC */
    f32 scale_from;   /* 0x10 */
    f32 scale_to;     /* 0x14 */
    f32 matrix[3][3]; /* 0x18 */
    f32 position[3];  /* 0x3C */
} TrackMapEntry;

typedef struct TrackScreenButton { /* 0x40 bytes */
    f32 x;            /* 0x0 */
    f32 y;            /* 0x4 */
    f32 angle;        /* 0x8 */
    u8 unknown0C[36]; /* 0xC */
    f32 position[3];  /* 0x30 */
    u8 alpha;         /* 0x3C */
    u8 unknown3D[3];  /* 0x3D */
} TrackScreenButton;

typedef struct TrackSlot { /* 0x18 bytes */
    s8 gTrackNum;  /* 0x0 */
    f32 pos[3];    /* 0x4 */
    f32 targetPos; /* 0x10 */
    s8 slot;       /* 0x14 */
} TrackSlot;


#endif /* GAME_TYPES_H */
