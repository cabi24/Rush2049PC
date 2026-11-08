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
struct Bank;
struct Basis;
struct Blit;
struct Block;
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
struct D_8012E6E0_Node;
struct D_80143FD8_Record;
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
struct Msg;
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
struct PathNode;
struct PathPoint;
struct PathPt;
struct Pending;
struct Place;
struct Player;
struct PlayerRec;
struct PointList;
struct Poly;
struct Pool;
struct PositionEntry;
struct Profile;
struct Puff;
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
struct SearchPoint;
struct Sequence20;
struct SequencePageEntry;
struct SequencePlayParams;
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
struct Source;
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
struct TblHdr;
struct TexDef;
struct TexRect;
struct Texture;
struct Tire;
struct TrackNode;
struct Tree;
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
struct tModelData;
struct tPS_File;
struct tPlayerProfile;
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

typedef struct BlitColor { /* 0x4 bytes */
    u8 r; /* 0x0 */
    u8 g; /* 0x1 */
    u8 b; /* 0x2 */
    u8 a; /* 0x3 */
} BlitColor;

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

typedef struct Definition32 { /* 0x20 bytes */
    s32 object;  /* 0x0 */
    u16 r0;      /* 0x4 */
    u16 r1;      /* 0x6 */
    u16 b0;      /* 0x8 */
    u16 b1;      /* 0xA */
    u16 b2;      /* 0xC */
    u8 padE[2];  /* 0xE */
    f32 p0;      /* 0x10 */
    f32 p1;      /* 0x14 */
    f32 p2;      /* 0x18 */
    u8 pad1C[4]; /* 0x1C */
} Definition32;

typedef struct Definitions64 { /* 0x40 bytes */
    Definition32 item[2]; /* 0x0 */
} Definitions64;

typedef struct HighScoreRect { /* 0xC bytes */
    s16 x;          /* 0x0 */
    s16 y;          /* 0x2 */
    s16 w;          /* 0x4 */
    s16 h;          /* 0x6 */
    s16 unknown[2]; /* 0x8 */
} HighScoreRect;

typedef struct HudBlitDef { /* 0x30 bytes */
    const char *texname;            /* 0x0 */
    s16 dulx;                       /* 0x8 */
    s16 duly;                       /* 0xA */
    s16 width;                      /* 0xC */
    s16 height;                     /* 0xE */
    s16 top;                        /* 0x10 */
    s16 bot;                        /* 0x12 */
    s16 left;                       /* 0x14 */
    s16 right;                      /* 0x16 */
    u32 zdepth;                     /* 0x18 */
    u32 alpha;                      /* 0x1C */
    s32 (*animfunc)(struct Blit *); /* 0x20 */
    u32 animid;                     /* 0x28 */
} HudBlitDef;

typedef struct Impact24 { /* 0x18 bytes */
    f32 force;     /* 0x0 */
    f32 vector[3]; /* 0x4 */
    f32 timestamp; /* 0x10 */
    s32 flag;      /* 0x14 */
} Impact24;

typedef struct LayerRecord { /* 0xC bytes */
    u32 payload;    /* 0x0 */
    u16 id;         /* 0x4 */
    u16 type;       /* 0x6 */
    u16 references; /* 0x8 */
    u16 metadata;   /* 0xA */
} LayerRecord;

typedef struct MenuButton { /* 0x40 bytes */
    s32 type;         /* 0x0 */
    s32 handle;       /* 0x4 */
    f32 angle;        /* 0x8 */
    f32 matrix[3][3]; /* 0xC */
    f32 position[3];  /* 0x30 */
    u8 alpha;         /* 0x3C */
    u8 pad[3];        /* 0x3D */
} MenuButton;

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
    s32 retraceCount;                /* 0x27C */
    s32 audioListPending;            /* 0x280 */
    struct OSScClient_s *clientList; /* 0x288 */
    struct OSScTask_s *rspTaskHead;  /* 0x290 */
    struct OSScTask_s *rspTaskTail;  /* 0x298 */
    struct OSScTask_s *rdpTaskHead;  /* 0x2A0 */
    struct OSScTask_s *rdpTaskTail;  /* 0x2A8 */
    struct OSScTask_s *curRSPTask;   /* 0x2B0 */
    struct OSScTask_s *curRDPTask;   /* 0x2B8 */
} PcSched;

typedef struct PlayerRect { /* 0x18 bytes */
    s32 x;  /* 0x0 */
    s32 y;  /* 0x4 */
    s32 w;  /* 0x8 */
    s32 h;  /* 0xC */
    s32 cx; /* 0x10 */
    s32 cy; /* 0x14 */
} PlayerRect;

typedef struct PlayerTail { /* 0x50 bytes */
    struct PlayerRec **rec; /* 0x0 */
    u8 pad[72];             /* 0x8 */
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

typedef struct Segs { /* 0x1A0 bytes */
    PcScTaskShape task0; /* 0x0 */
    void *buf88;         /* 0xA0 */
    s16 n92;             /* 0xA8 */
    u8 other94[30];      /* 0xAA */
    uintptr_t f124;      /* 0xC8 */
    PcScTaskShape task1; /* 0xD0 */
    void *buf216;        /* 0x170 */
    s16 n220;            /* 0x178 */
    u8 other222[30];     /* 0x17A */
    uintptr_t f252;      /* 0x198 */
} Segs;

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

typedef struct State20 { /* 0x14 bytes */
    s32 handle; /* 0x0 */
    f32 a;      /* 0x4 */
    f32 b;      /* 0x8 */
    f32 c;      /* 0xC */
    f32 d;      /* 0x10 */
} State20;

typedef struct StuntPlayer { /* 0x78 bytes */
    s32 score;   /* 0x0 */
    u8 pad[116]; /* 0x4 */
} StuntPlayer;

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

typedef struct tCheckpoint { /* 0x50 bytes */
    f32 pos[3];      /* 0x0 */
    f32 normal[3];   /* 0xC */
    s32 radius;      /* 0x18 */
    s16 flags;       /* 0x1C */
    s16 time[2];     /* 0x1E */
    s16 pathPts[20]; /* 0x22 */
    f32 distToNext;  /* 0x4C */
} tCheckpoint;

/* ---- memory: the heap, the asset wads and waves, lists and trees */

typedef struct DeflateConfig { /* 0x8 bytes */
    u16 good_length; /* 0x0 */
    u16 max_lazy;    /* 0x2 */
    u16 nice_length; /* 0x4 */
    u16 max_chain;   /* 0x6 */
} DeflateConfig;

typedef struct anon_loaded_ready_14 { /* 0x20 bytes */
    s8 loaded;   /* 0x0 */
    s8 ready;    /* 0x1 */
    s8 locked;   /* 0x2 */
    s8 model;    /* 0x3 */
    u8 mirrored; /* 0x4 */
    s8 handle;   /* 0x5 */
    u8 wave;     /* 0x6 */
    u32 size;    /* 0x8 */
    u8 *ptr;     /* 0x10 */
    u8 *lastPtr; /* 0x18 */
} anon_loaded_ready_14;

typedef struct static_tree_desc_s { /* 0x20 bytes */
    struct ct_data_s *static_tree; /* 0x0 */
    s32 *extra_bits;               /* 0x8 */
    s32 extra_base;                /* 0x10 */
    s32 elems;                     /* 0x14 */
    s32 max_length;                /* 0x18 */
} static_tree_desc_s;

typedef struct tCarModel { /* 0x18 bytes */
    u32 data;   /* 0x0 */
    u8 pad[20]; /* 0x4 */
} tCarModel;

/* ---- maths: vectors, matrices, angles, random numbers */

typedef union CullVertex { /* 0x10 bytes */
    Vtx_t v;
    s64 force_structure_alignment;
} CullVertex;

typedef struct UnitMatrix { /* 0x24 bytes */
    f32 values[9]; /* 0x0 */
} UnitMatrix;

/* ---- audio: the sound manager, effects, music and the MusyX sound library */

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

typedef struct States60 { /* 0x3C bytes */
    State20 values[3]; /* 0x0 */
} States60;

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

typedef struct Item12 { /* 0x18 bytes */
    char *name;                  /* 0x0 */
    struct Resource32 *resource; /* 0x8 */
    void *output;                /* 0x10 */
} Item12;

typedef struct ModelList { /* 0x10 bytes */
    struct Model *models; /* 0x0 */
    s32 count;            /* 0x8 */
} ModelList;

typedef struct ObjectInstance { /* 0x68 bytes */
    u32 flags;    /* 0x0 */
    u32 w4;       /* 0x4 */
    void *mat;    /* 0x8 */
    f32 scale;    /* 0x10 */
    f32 w;        /* 0x14 */
    u16 index;    /* 0x18 */
    s16 child;    /* 0x1A */
    s16 next;     /* 0x1C */
    s16 mode;     /* 0x1E */
    void *light;  /* 0x20 */
    void *tex[4]; /* 0x28 */
    void *rect;   /* 0x48 */
    void *clip;   /* 0x50 */
    void *pal;    /* 0x58 */
    u32 color;    /* 0x60 */
    u32 envcolor; /* 0x64 */
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

typedef struct PaneBlits { /* 0x38 bytes */
    s8 active;                 /* 0x0 */
    u8 own;                    /* 0x1 */
    u8 a;                      /* 0x2 */
    u8 b;                      /* 0x3 */
    u8 c;                      /* 0x4 */
    u8 d;                      /* 0x5 */
    u8 e;                      /* 0x6 */
    u8 f;                      /* 0x7 */
    u8 g;                      /* 0x8 */
    u8 h;                      /* 0x9 */
    u8 other10[2];             /* 0xA */
    struct PadConfig *part[5]; /* 0x10 */
} PaneBlits;

typedef struct tBlitInstance { /* 0x28 bytes */
    struct tPalette *palette; /* 0x0 */
    void *texture;            /* 0x8 */
    u16 texIndex;             /* 0x10 */
    s16 x;                    /* 0x12 */
    s16 y;                    /* 0x14 */
    u16 z;                    /* 0x16 */
    s16 width;                /* 0x18 */
    s16 height;               /* 0x1A */
    u8 alpha;                 /* 0x1C */
    u8 flags;                 /* 0x1D */
    u8 pad16[10];             /* 0x1E */
} tBlitInstance;

/* ---- text: fonts and text */

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

typedef struct ControllerState { /* 0x10 bytes */
    s8 connected; /* 0x0 */
    u8 pad1[3];   /* 0x1 */
    u32 buttons;  /* 0x4 */
    s8 stick[2];  /* 0x8 */
    s8 unkA[4];   /* 0xA */
    u8 padE[2];   /* 0xE */
} ControllerState;

/* ---- save: the Controller Pak, profiles, records, options and passwords */

typedef struct Control { /* 0x310 bytes */
    u8 pad0[6];    /* 0x0 */
    s8 b6;         /* 0x6 */
    u8 pad7[5];    /* 0x7 */
    PcPfs pfs;     /* 0x10 */
    u8 pad74[656]; /* 0x80 */
} Control;

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

typedef struct PakFileListHead { /* 0x18 bytes */
    void **head; /* 0x0 */
    s32 pad[3];  /* 0x8 */
} PakFileListHead;

typedef struct PakFileRecord { /* 0x60 bytes */
    struct PakHandle *next;                                   /* 0x0 */
    struct PakHandle *prev;                                   /* 0x8 */
    void (*RemoveFile)(struct PakHandle *);                   /* 0x10 */
    void (*MoveFile)(struct PakHandle *, struct PakHandle *); /* 0x18 */
    u8 pak;                                                   /* 0x20 */
    u8 fileNo;                                                /* 0x21 */
    u8 name[35];                                              /* 0x22 */
    u8 ext[11];                                               /* 0x45 */
    u32 size;                                                 /* 0x50 */
    u32 blocks;                                               /* 0x54 */
    u8 **data;                                                /* 0x58 */
} PakFileRecord;

typedef struct PlayerFiles { /* 0x18 bytes */
    u32 unknown0;       /* 0x0 */
    u32 unknown4;       /* 0x4 */
    struct Owner *head; /* 0x8 */
    struct Owner *tail; /* 0x10 */
} PlayerFiles;

typedef struct PlayerSaveData { /* 0x810 bytes */
    char pad0[13];   /* 0x0 */
    char name[2051]; /* 0xD */
} PlayerSaveData;

typedef struct PlayerSaveSlot { /* 0x40 bytes */
    void *next;             /* 0x0 */
    void *prev;             /* 0x8 */
    void *fileh;            /* 0x10 */
    char pad0_C[8];         /* 0x18 */
    char name[24];          /* 0x20 */
    struct SaveData **data; /* 0x38 */
} PlayerSaveSlot;

typedef struct ProfileRec { /* 0x40 bytes */
    void *next;   /* 0x0 */
    void *prev;   /* 0x8 */
    void **fileh; /* 0x10 */
    u8 pad0C[32]; /* 0x18 */
    void **data;  /* 0x38 */
} ProfileRec;

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

typedef struct Settings { /* 0x29 bytes */
    u8 unknown00[28]; /* 0x0 */
    s8 mode;          /* 0x1C */
    u8 unknown1D[3];  /* 0x1D */
    s8 a;             /* 0x20 */
    s8 b;             /* 0x21 */
    s8 c;             /* 0x22 */
    s8 d;             /* 0x23 */
    s8 e;             /* 0x24 */
    s8 f;             /* 0x25 */
    s8 g;             /* 0x26 */
    s8 h;             /* 0x27 */
    s8 i;             /* 0x28 */
} Settings;

typedef struct StatsB { /* 0x40 bytes */
    u32 first;   /* 0x0 */
    u32 max0;    /* 0x4 */
    u32 max1;    /* 0x8 */
    u32 total;   /* 0xC */
    u16 count0;  /* 0x10 */
    u16 count1;  /* 0x12 */
    u32 sum[10]; /* 0x14 */
    u16 flags;   /* 0x3C */
    u8 pad62[2]; /* 0x3E */
} StatsB;

typedef struct StatsC { /* 0xC bytes */
    u32 first;    /* 0x0 */
    u16 count[4]; /* 0x4 */
} StatsC;

typedef struct StatsD { /* 0x18 bytes */
    u32 first;    /* 0x0 */
    f32 minimum;  /* 0x4 */
    f32 total;    /* 0x8 */
    u16 count[5]; /* 0xC */
    u8 pad22[2];  /* 0x16 */
} StatsD;

typedef struct StatsE { /* 0x1C bytes */
    u32 first;    /* 0x0 */
    u16 count[6]; /* 0x4 */
    u16 maximum;  /* 0x10 */
    u8 pad18[6];  /* 0x12 */
    u32 total;    /* 0x18 */
} StatsE;

typedef struct WeightRecord { /* 0x60 bytes */
    u8 prefix[4];     /* 0x0 */
    f32 weight[3][5]; /* 0x4 */
    u8 suffix[32];    /* 0x40 */
} WeightRecord;

typedef struct tBlastMat { /* 0x8 bytes */
    struct tUVMatrix *mat; /* 0x0 */
} tBlastMat;

/* ---- track: tracks, the road tree, targets, paths and checkpoints */

typedef struct CheckpointPoint { /* 0x50 bytes */
    f32 pos[3];    /* 0x0 */
    f32 normal[3]; /* 0xC */
    s32 radius;    /* 0x18 */
    u8 pad1C[52];  /* 0x1C */
} CheckpointPoint;

typedef struct DronePathPoint { /* 0x10 bytes */
    u16 count;                /* 0x0 */
    u16 other2;               /* 0x2 */
    struct PathPoint *points; /* 0x8 */
} DronePathPoint;

typedef struct MineSlots { /* 0x28 bytes */
    struct Target *target[4]; /* 0x0 */
    s32 count;                /* 0x20 */
} MineSlots;

typedef struct PointEntry { /* 0x10 bytes */
    struct SearchPoint *points; /* 0x0 */
    u8 pad[4];                  /* 0x8 */
} PointEntry;

typedef struct ResurrectPath { /* 0x20 bytes */
    u16 numPoints;            /* 0x0 */
    struct PathPoint *points; /* 0x8 */
    u8 numNodes;              /* 0x10 */
    struct PathNode *nodes;   /* 0x18 */
} ResurrectPath;

typedef struct TargInfo { /* 0x40 bytes */
    char *name;    /* 0x0 */
    char *file;    /* 0x8 */
    void *start;   /* 0x10 */
    void *visFunc; /* 0x18 */
    s16 kind;      /* 0x20 */
    u16 flags;     /* 0x22 */
    s16 shape;     /* 0x24 */
    s8 category;   /* 0x26 */
    s8 element;    /* 0x27 */
    f32 value;     /* 0x28 */
    s32 sound;     /* 0x2C */
    s32 w20;       /* 0x30 */
    s32 w24;       /* 0x34 */
    s32 w28;       /* 0x38 */
    f32 f2C;       /* 0x3C */
} TargInfo;

typedef struct TargetPool { /* 0x28 bytes */
    u8 b0;        /* 0x0 */
    u8 n64;       /* 0x1 */
    u8 pad2[2];   /* 0x2 */
    s32 count;    /* 0x4 */
    s32 size;     /* 0x8 */
    u8 *mem;      /* 0x10 */
    intptr_t f16; /* 0x18 */
    u8 *free;     /* 0x20 */
} TargetPool;

typedef struct TrackLoadInfo { /* 0x2C bytes */
    s8 trackInited;           /* 0x0 */
    TrackLoadState track;     /* 0x4 */
    TrackLoadState world;     /* 0xC */
    TrackLoadState collision; /* 0x14 */
    TrackLoadState path;      /* 0x1C */
    TrackLoadState other;     /* 0x24 */
} TrackLoadInfo;

typedef struct Waypoint { /* 0x50 bytes */
    s16 index[40]; /* 0x0 */
} Waypoint;

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

typedef struct tStatList { /* 0x28 bytes */
    u8 flag;     /* 0x0 */
    s32 count;   /* 0x4 */
    s32 size;    /* 0x8 */
    void *mem;   /* 0x10 */
    void *head;  /* 0x18 */
    void *store; /* 0x20 */
} tStatList;

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

typedef struct EnvironNode { /* 0x20 bytes */
    struct Node *next; /* 0x0 */
    s16 field4;        /* 0x8 */
    s16 field6;        /* 0xA */
    s16 field8;        /* 0xC */
    u8 padA[2];        /* 0xE */
    u32 fieldC;        /* 0x10 */
    f32 field10;       /* 0x14 */
    void *field14;     /* 0x18 */
} EnvironNode;

typedef struct PcEffectObj { /* 0x38 bytes */
    void *next;  /* 0x0 */
    void *prev;  /* 0x8 */
    f32 f8;      /* 0x10 */
    void *dl;    /* 0x18 */
    f32 pos[3];  /* 0x20 */
    u8 color[4]; /* 0x2C */
    s32 f20;     /* 0x30 */
} PcEffectObj;

typedef struct PcFade { /* 0x30 bytes */
    void *next;    /* 0x0 */
    s32 unknown04; /* 0x8 */
    void *object;  /* 0x10 */
    f32 lifetime;  /* 0x18 */
    f32 interval;  /* 0x1C */
    u8 color[4];   /* 0x20 */
    void *source;  /* 0x28 */
} PcFade;

typedef struct PcPuff { /* 0x60 bytes */
    void *next;       /* 0x0 */
    f32 matrix[3][3]; /* 0x8 */
    f32 position[3];  /* 0x2C */
    s32 id;           /* 0x38 */
    u8 pad56[4];      /* 0x3C */
    f32 life;         /* 0x40 */
    f32 timer;        /* 0x44 */
    f32 velocity[3];  /* 0x48 */
    u32 flags;        /* 0x54 */
    s16 car;          /* 0x58 */
    s16 frame;        /* 0x5A */
} PcPuff;

typedef struct PcTrail { /* 0x48 bytes */
    void *next;      /* 0x0 */
    s32 unknown04;   /* 0x8 */
    s32 flags;       /* 0xC */
    f32 lifetime;    /* 0x10 */
    f32 origin[3];   /* 0x14 */
    f32 previous[3]; /* 0x20 */
    f32 lower[3];    /* 0x2C */
    f32 upper[3];    /* 0x38 */
} PcTrail;

typedef struct Tune44 { /* 0x2C bytes */
    f32 values[11]; /* 0x0 */
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

typedef struct CircuitTrackSlot { /* 0x5 bytes */
    u8 a; /* 0x0 */
    u8 b; /* 0x1 */
    u8 c; /* 0x2 */
    u8 d; /* 0x3 */
    u8 e; /* 0x4 */
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

typedef struct RingState { /* 0xD0 bytes */
    s8 wrapped;        /* 0x0 */
    u8 unknown01;      /* 0x1 */
    u16 next;          /* 0x2 */
    void *objects[25]; /* 0x8 */
} RingState;

typedef struct Shield { /* 0x34 bytes */
    s32 dlIdx; /* 0x0 */
    UVMat mat; /* 0x4 */
} Shield;

typedef struct StuntInfo { /* 0x7C bytes */
    s32 active;  /* 0x0 */
    u8 pad[120]; /* 0x4 */
} StuntInfo;

typedef struct tBattlePowerUps { /* 0x18 bytes */
    s8 count;           /* 0x0 */
    u8 unknown_1[3];    /* 0x1 */
    f32 delay;          /* 0x4 */
    f32 phase_a;        /* 0x8 */
    f32 phase_b;        /* 0xC */
    struct Slot *slots; /* 0x10 */
} tBattlePowerUps;

typedef struct tCMuzzleObj { /* 0x148 bytes */
    u8 unknown000[260]; /* 0x0 */
    s32 handle;         /* 0x104 */
    u8 unknown108[48];  /* 0x108 */
    s8 selection;       /* 0x138 */
    u8 unknown139[11];  /* 0x139 */
    f32 timer;          /* 0x144 */
} tCMuzzleObj;

typedef struct tCircuitCar { /* 0x4C bytes */
    u8 id;         /* 0x0 */
    u8 b1;         /* 0x1 */
    u16 h2;        /* 0x2 */
    u8 b4[24];     /* 0x4 */
    u8 b28[24];    /* 0x1C */
    s16 skill[12]; /* 0x34 */
} tCircuitCar;

/* ---- camera: the cameras */

typedef struct CameraView { /* 0x98 bytes */
    f32 uv[3][3];      /* 0x0 */
    f32 position[3];   /* 0x24 */
    u8 unknown30[104]; /* 0x30 */
} CameraView;

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
    s16 handle;       /* 0x0 */
    s16 auxiliary;    /* 0x2 */
    u8 unknown04[48]; /* 0x4 */
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

typedef struct CourseFlag { /* 0x5 bytes */
    u8 flag;         /* 0x0 */
    u8 unknown01[4]; /* 0x1 */
} CourseFlag;

typedef struct Egg { /* 0xA4 bytes */
    u8 size;        /* 0x0 */
    u8 pad[3];      /* 0x1 */
    u32 levels[20]; /* 0x4 */
    u32 edges[20];  /* 0x54 */
} Egg;

typedef struct MapLod { /* 0x10 bytes */
    u16 texidx;      /* 0x0 */
    u16 flags;       /* 0x2 */
    f32 lodRad;      /* 0x4 */
    u32 displayList; /* 0x8 */
    u32 verts;       /* 0xC */
} MapLod;

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

typedef struct PcFscls { /* 0x1EC bytes */
    f32 fscls[3];          /* 0x0 */
    PcRespawn respawn[30]; /* 0xC */
} PcFscls;

typedef struct SelectCarState { /* 0x18 bytes */
    u8 b0;    /* 0x0 */
    u8 b1;    /* 0x1 */
    u8 b2;    /* 0x2 */
    u8 b3;    /* 0x3 */
    s32 w4;   /* 0x4 */
    s8 b8;    /* 0x8 */
    s8 b9;    /* 0x9 */
    s8 b10;   /* 0xA */
    u8 pad11; /* 0xB */
    s32 w12;  /* 0xC */
    f32 f16;  /* 0x10 */
    f32 f20;  /* 0x14 */
} SelectCarState;

typedef struct SelectCarWaveSlot { /* 0xC bytes */
    s32 a;     /* 0x0 */
    s8 b;      /* 0x4 */
    s8 c;      /* 0x5 */
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
