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
struct AudioCacheNode;
struct AudioNode;
struct Basis;
struct Block;
struct BSTNode;
struct C96Handle;
struct CCar;
struct CCol;
struct CamNode;
struct CamSlot;
struct Camera;
struct Car;
struct CollisionObject;
struct CollisionReckon;
struct Command;
struct Config64;
struct Descriptor;
struct Descriptor72;
struct Dims;
struct Dynamic;
struct EUCar;
struct Effect;
struct EffectObj;
struct EngineState;
struct Entity;
struct Entry;
struct GsmState;
struct H12;
struct Header;
struct Header88;
struct Heap;
struct In;
struct LayerState;
struct Lights2;
struct Link;
struct LinkNode;
struct List;
struct ListHead;
struct LookAt;
struct MacroCommand;
struct MapRecord;
struct MenuNode;
struct MessageQueue;
struct Metadata48;
struct MidiSetup;
struct Model;
struct ModelObj;
struct ModelSnapshot;
struct Nav;
struct Node;
struct Node16;
struct Node68;
struct OSContPad;
struct Obj;
struct Object;
struct Outer;
struct ObjInstance;
struct P3;
struct PackedLeadingKey;
struct Packet;
struct PadConfig;
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
struct QNode;
struct Rec32;
struct RecB;
struct Reckon;
struct Record88;
struct Region;
struct RelativeEntry;
struct ResSlot;
struct ResourceGroup;
struct ResourceOffsets;
struct Respawn;
struct Row20;
struct SalMatrix;
struct SalVector;
struct SampleRecord;
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
struct SoundSampleInfo;
struct SoundHwVoice;
struct SoundListenerAddView;
struct SoundListenerUpdateView;
struct SoundState;
struct SpatialState;
struct Sprite;
struct State;
struct StateNode;
struct Surface;
struct SynthInputMix;
struct SynthVoice;
struct Table;
struct TexRect;
struct TexTile;
struct Tire;
struct TireDesc;
struct TexTile;
struct TrackNode;
struct Tree;
struct V3s;
struct Vec3;
struct VehicleModel;
struct View;
struct Visual;
struct Voice;
struct WheelSlot;
struct X;
struct XForm;
struct __OSInodeCache;
struct __OSPackId;
struct __OSThreadprofile_s;
struct _tBlit_2;
struct _tSusp;
struct anon_kerns_value_c;
struct internal_state;
struct tAudioSequence;
struct pathtyp;
struct tBasicTargetPath;
struct tCarData;
struct tFxSequence;     /* the sound manager's records: game/audiomgr.h defines them */
struct tMemoryChunk;
struct tMiniModelData;
struct tModelData;
struct tModelDef;
struct tModelPart;
struct tNewSkid;
struct tPoly;
struct tPadState;       /* a player's input record: game/input.h defines it */
struct tPaletteDef;
struct tPlayerProfile;
struct tSfxCmd;
struct tSortedFxTag;
struct tSfxEmitter;
struct tSfxListener;
struct tSfxVoice;
struct tTexture;
struct tUVMatrix;
struct tVisual;
struct tWindowInfo;
struct tree_desc_s;
struct z_stream_s;
union tAudioMsg;

/* ---- ultra: libultra and the RSP microcode: the N64 OS interface port/ultra implements */

/* Display lists: Gfx, a command's record, and GfxWords, a command in the N64's form, as the cartridge's two set-up
 * lists hold them (rsp_init_dl, rdp_init_dl), are the port's (pc_prelude.h brings port/include/pc_gfx_list.h and
 * pc_gfx_n64.h). */

typedef struct OSContStatus { /* 0x4 bytes */
    u16 type;  /* 0x0 */
    u8 status; /* 0x2 */
    u8 errno;  /* 0x3 */
} OSContStatus;

/* A message queue (game/threads.h's queue_* and lock_*). port/ultra keeps a queue's messages and waiting threads in
 * its own table, under the record's address, and never reads or writes the record: these are libultra's fields,
 * unused on the PC. */
typedef struct OSMesgQueue_s {
    struct OSThread *mtqueue;     /* (the threads waiting to receive) */
    struct OSThread *fullqueue;   /* (the threads waiting to send) */
    s32 validCount;               /* (the messages in it) */
    s32 first;                    /* (the first one's slot) */
    s32 msgCount;                 /* (its slots) */
    OSMesg *msg;                  /* (the slots) */
} OSMesgQueue;

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

/* A vertex (libultra Vtx, as the game uses it: no normals). */
typedef union Vtx { /* 0x10 bytes */
    Vtx_t v;
    s64 force_structure_alignment;
} Vtx;

/* The controller pak's file system: OSPfs, OSPfsState, __OSInodeUnit, __OSInode (declared once for the game and the
 * boot segment's libultra). */
#include "game/pfs.h"

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

/* A thread (game/threads.h's thread_*). port/ultra runs it on a host thread and keeps its state in its own table,
 * under the record's address, and never reads or writes the record: these are libultra's fields (the scheduling
 * links and the saved registers), unused on the PC. */
typedef struct OSThread {
    struct OSThread *next;
    s32 priority;
    struct OSThread **queue;
    struct OSThread *tlnext;
    u16 state;
    u16 flags;
    s32 id;
    s32 fp;
    struct __OSThreadprofile_s *thprof;
    __OSThreadContext context;
} OSThread;

/* ---- system: start-up, threads, the scheduler, timers, the frame pipeline, video, decompression, the overlays */

/* A linked list's header (MAT3 ll.c's tDLList): the records (or, when indirect, the handles of the records) are
 * linked through their first words (tDLLE: next, then prev when doubly); InsertLLE and RemoveLLE keep it. */
typedef struct AudioList { /* 0x18 bytes */
    u8 indirect;       /* 0x0 a list of handles */
    u8 doubly;         /* 0x1 */
    u8 pad[2];         /* 0x2 */
    u32 count;         /* 0x4 */
    void *head;        /* 0x8 the first record (handle), NULL when empty */
    void *tail;        /* 0x10 */
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

typedef struct LayerRecord { /* 0x10 bytes */
    void *payload;  /* 0x0 the layers, in the pool */
    u16 id;         /* 0x8 */
    u16 count;      /* 0xA */
    u16 references; /* 0xC */
} LayerRecord;

typedef struct PcRespawn { /* 0x10 bytes */
    s8 flag;    /* 0x0 */
    u8 pad1[3]; /* 0x1 */
    f32 pos[3]; /* 0x4 */
} PcRespawn;

typedef struct __attribute__((packed)) RecordFields { /* 0xC bytes */
    u16 id;                         /* 0x0 */
    u16 count;                      /* 0x2 */
    struct Descriptor *descriptors; /* 0x4 */
} RecordFields;

typedef struct ResourceRange { /* 0x4 bytes */
    u16 count; /* 0x0 */
    u16 first; /* 0x2 */
} ResourceRange;

/* A screen position in 16-bit coordinates (the battle HUD's icon and ammunition places, ammoPos, powerupPos). */
typedef struct ScreenPos16 { /* 0x4 bytes */
    s16 x; /* 0x0 */
    s16 y; /* 0x2 */
} ScreenPos16;

/* A circuit type's random fog or wind (gCircuitFog, gCircuitWind): the draw is irand(range); no code reads the first
 * halfword. */
typedef struct CircuitChance { /* 0x4 bytes */
    u16 unused; /* 0x0 */
    u16 range;  /* 0x2 */
} CircuitChance;

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

typedef struct ResourceRecord { /* 0x10 bytes */
    void *payload;  /* 0x0 the macro's steps, the table's bytes or the keymap's entries, in the pool */
    u16 id;         /* 0x8 */
    u16 references; /* 0xA */
} ResourceRecord;

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
    u8 *lastPtr; /* 0x18: MAT3's handle wave's block when it was last set up, to rebase it after the heap moved it;
                  * unused on the PC, whose WAD records follow their block (docs/port/NATIVE.md step 7) */
} anon_loaded_ready_14;

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

/* ---- audio: the sound manager, effects, music and the MusyX sound library
 *
 * The MusyX library's records here (ChannelState the volume faders, LargeNode and SmallNode the 3D emitter lists,
 * SequenceContext and SequenceNode the song slots and their notes, the registries' storage) only lay out the
 * library's data for the data loader: no game file reads their fields. The library's own definitions, with the fields
 * named by what the code does, are in src/rom/sound and include/sequence_context.h, include/sound_hw_voice.h,
 * include/sound_sample_record.h (docs/code/audio.md). The records the library and the game share (its services, the
 * DMA list's entries, the speech streams) are game/soundlib.h's, the sound manager's game/audiomgr.h's. */

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

typedef union RecordStorage { /* 0xC bytes */
    RecordFields fields;
    u32 words[2];
} RecordStorage;

typedef struct RegisteredSamples { /* 0x10 bytes */
    struct SampleRecord *records; /* 0x0 */
    u32 cart_base;                /* 0x8 the samples' cartridge address */
    u16 count;                    /* 0xC */
    u16 unknown0A;                /* 0xE */
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
    u32 *pendingFF0;                           /* 0x1618 */
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

/* The sound commands' free list (gFreeQ): a list header as AudioList, its first two words read as words. */
typedef struct SfxQueue { /* 0x18 bytes */
    u32 pad0[2];       /* 0x0 indirect, doubly; count */
    void *head;        /* 0x8 */
    void *tail;        /* 0x10 */
} SfxQueue;

typedef struct SmallNode { /* 0x18 bytes */
    struct SmallNode *next;   /* 0x0 */
    f32 distance;             /* 0x8 */
    struct StateNode *object; /* 0x10 */
} SmallNode;

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

/* A window (MAT3 tWindowInfo, ZoidWinInfo[4]; 0x48 bytes on the N64, 0x50 on the PC, whose two pointers are 8 bytes):
 * its viewport and scissor rectangle, then as bytes the flags, the view angles and their tangents, the size and
 * centre, the aspect, the clip planes and the fog (min, max, colour), which the window files read through their own
 * tWindowInfo: the data loader keeps this declaration's field runs for the own-BSS datum (docs/code/race.md 6). */
typedef struct ArbEntry { /* 0x50 bytes */
    void *vp;    /* 0x0 its viewport (vp0 ...) */
    void *s;     /* 0x8 its scissor rectangle (scissor0 ...) */
    u8 rest[64]; /* 0x10 */
} ArbEntry;

/* A flipbook frame of texture animation (MAT3 tTextureBase): the texture's name, the texture once found, the image
 * to show (its texBase, or the address its display list loads). */
typedef struct Item12 { /* 0x18 bytes */
    char *name;                  /* 0x0 */
    struct tTexture *tex;        /* 0x8 */
    void *texBase;               /* 0x10 */
} Item12;

/* A model WAD's records (MAT3's names; pc_game_types.h declares them: the WAD's converter builds them beside the
 * file, port/assets/wad.c). */
typedef struct tLod tLod;                         /* a level of detail of an object definition */
typedef struct tModelDef tModelDef;               /* an object definition (OBHD) */
typedef struct tTexture tTexture;                 /* a texture (TXHD) */
typedef struct tPaletteDef tPaletteDef;           /* a palette (PLHD), a car's paint, a blit's colours */
typedef struct tBasicTargetPath tBasicTargetPath; /* a track's target path (PTHD) */

/* A loaded model file's tables, one entry per wave instance (waves.c's SetupWaveModel fills them from the WAD's
 * records): its object definitions (gRomObjDefPtrs), its textures (gTexDefPtrs), its palettes (gPalDefPtrs), each
 * sorted by name; and the track's target paths (gTargetPaths). A handle to one of them is the wave instance's number
 * times 1024 plus its index here. */
typedef struct ModelDefTable { /* 0x10 bytes (MAT3 anon_romObjDefs_count_8) */
    struct tModelDef *romObjDefs; /* 0x0 */
    s32 count;                    /* 0x8 */
} ModelDefTable;

typedef struct TextureDefTable { /* 0x10 bytes (MAT3 anon_texDefs_count_8) */
    struct tTexture *texDefs; /* 0x0 */
    s32 count;                /* 0x8 */
} TextureDefTable;

typedef struct PaletteDefTable { /* 0x10 bytes (MAT3 anon_palDefs_count_8) */
    struct tPaletteDef *palDefs; /* 0x0 */
    s32 count;                   /* 0x8 */
} PaletteDefTable;

typedef struct TargetPathTable { /* 0x10 bytes (MAT3 anon_paths_count_8) */
    struct tBasicTargetPath *paths; /* 0x0 */
    s32 count;                      /* 0x8 */
} TargetPathTable;

/* A scene node, an entry of ObjInstanceList (MAT3 ObjInstance; the N64's 0x44 bytes with the pointers widened):
 * its flags (OBJ_*, scene.h), its culling word, its matrix (a tUVMatrix, relative to its parent), the object handle
 * ((model << 10) | definition; 0xFFFF a free node), the first child and the next sibling (-1 none), its lights, a
 * texture for each level of detail, its tile, tile size and palette, and two colours. */
typedef struct ObjectInstance { /* 0x68 bytes */
    u32 flags;                    /* 0x0 OBJ_* (scene.h) */
    u32 culling;                  /* 0x4 its culling box: (first cull vertex << 3) | 7 (ZOID_BuildCullingList), 0 none */
    struct tUVMatrix *mat;        /* 0x8 its matrix, relative to its parent */
    f32 scale;                    /* 0x10 */
    f32 w;                        /* 0x14 the matrix's w */
    u16 index;                    /* 0x18 its object, (model << 10) | definition; 0xFFFF a free node */
    s16 child;                    /* 0x1A its first child, -1 none */
    s16 next;                     /* 0x1C its next sibling, -1 none */
    s16 numLights;                /* 0x1E its own lights' count, -1 the scene's */
    struct Lights2 *lights;       /* 0x20 its own lights */
    struct tTexture *texture[4];  /* 0x28 a texture per level of detail in place of the model's, NULL none */
    struct TexTile *tile;         /* 0x48 the part of the texture to load in place of the model's */
    struct TexTile *tileSize;     /* 0x50 the part of it to draw */
    struct tPaletteDef *palette;  /* 0x58 a palette in place of the model's */
    u32 color;                    /* 0x60 the primitive colour, OBJ_COLOR */
    u32 envcolor;                 /* 0x64 the environment colour, OBJ_COLOR */
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

/* A world tree's name and root node (MAT3 anon_woff_wname_14, the world WAD's WHDR entry; WorldTable): declared in
 * pc_game_types.h, where the world's converter builds the table beside the file, the root a host pointer
 * (port/assets/world.c). */
typedef struct NameEntry NameEntry;

/* ---- blit: the 2D blitter and panes */

/* A colour (MAT3 tRGBA), the game's one colour type: its bytes red, green, blue and alpha, c[0] to c[3], in that
 * order in memory. That is how the cartridge holds a colour, how the data loader leaves a tRGBA (its bytes as they
 * are) and how the scene draw (ObjInstanceList[].color), the blitter, the fills, the text and the renderer read one.
 * rgba moves the four bytes at once, as they are: a container to copy or compare, not a number to build or take
 * apart. Every colour the game keeps is one: its data (the HUD's, the menus', the cars'), a blit's or a pane's
 * flat colour, the text's; docs/port/RECORDS.md 3.3. */
typedef union tRGBA { /* 0x4 bytes */
    u8 c[4];
    u32 rgba;
} tRGBA;

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

/* A pane's two colours, as ChangePaneColor takes them: the inside's and the border's (the result panes,
 * hud_playerPaneColors). */
typedef struct PaneColors { /* 0x8 bytes */
    tRGBA pane;   /* 0x0 */
    tRGBA border; /* 0x4 */
} PaneColors;

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

/* One letter of a font (MAT3 anon_kerns_value_c): where it is in which texture page, and its row of the kerning
 * table. pc_game_types.h declares it: the font's converter builds the letters beside the file (port/assets/font.c). */
typedef struct FontLetter FontLetter;

typedef struct MenuDepthPoint { /* 0x8 bytes */
    f32 x; /* 0x0 */
    f32 y; /* 0x4 */
} MenuDepthPoint;

typedef struct PcText4 { /* 0x4 bytes */
    char c[4]; /* 0x0 */
} PcText4;

/* The strings of one language (MAT3 text.c's sections, gText): the text bank's six sections, each a count and a
 * table. Declared in pc_game_types.h, where the text bank's converter builds one for each bank it loads
 * (port/assets/text.c), which AdjustTextWavePointers copies into gText. */
typedef struct TextTables TextTables;

/* ---- save: the Controller Pak, profiles, records, options and passwords. The pak system's and the save code's
 * records are save.h's; their names are here, for the headers before it. */
typedef struct tPakFile tPakFile;
typedef struct tPak tPak;
typedef struct tPakHandle tPakHandle;
typedef struct PakErrorState PakErrorState;
typedef struct PakFileRecord PakFileRecord;
typedef struct tSaveData tSaveData;
typedef struct tGhostHeader tGhostHeader;
typedef struct tGhostFile tGhostFile;
typedef struct tPlayerProfile tProfile;   /* MAT3 tPlayerProfile */

/* A profile's circuit in progress (MAT3 tCircuit), tSaveData.circuit: part of the profile's file, so its layout is the
 * file's (the offsets below are the record's, at 0x6F4 in the file). NewCircuit starts one, ContinueCircuit builds
 * its races and drones from it, UpdateCircuit records each race, and a circuit code (GetCircuitCode, SetCircuitCode)
 * holds all of it but the ranks. The checksum covers the 72 bytes after it (CheckAndRepairPlayer). */
typedef struct tCircuit { /* 0x4C bytes */
    u32 checksum;      /* 0x0 */
    u8 active;         /* 0x4 a circuit is under way */
    u8 death;          /* 0x5 the death option: out of the circuit after a crash */
    u8 temp;           /* 0x6 entered by a circuit code: the points of the races before it are gTempCircuitPoints' */
    u8 difficulty;     /* 0x7 the drones' skill, raised after good races in an extreme circuit */
    s8 type;           /* 0x8 the circuit: 0 beginner to 3 extreme (circuitStats' index) */
    s8 track;          /* 0x9 the races driven */
    u8 unusedA[2];     /* 0xA zero */
    u32 seed;          /* 0xC the random seed its races and drones are drawn from */
    f32 totalTime;     /* 0x10 the time of its races so far */
    u8 ranks[6][9];    /* 0x14 each car's place in each race, 3 bits a race (24 races in a car's 9 bytes) */
    u8 unused4A[2];    /* 0x4A zero */
} tCircuit;


typedef struct tBlastMat { /* 0x8 bytes */
    struct tUVMatrix *mat; /* 0x0 */
} tBlastMat;

/* A pool of fixed-size elements (MAT3 tStatList; InitStatLL, GetStatLLE, FreeStatLLE, ClearStatLL): its elements
 * linked through their first pointer (MAT3 tStatLLE) and, when dbl, back through their second (tDLLE), the ones in
 * use from head, the free ones from store. The PC's layout: the pointers are host pointers (40 bytes), and so are
 * the elements' links, in every list: an element is its host type, links first (docs/port/POINTERS.md 9.5). */
typedef struct tStatList { /* 0x28 bytes */
    u8 dbl;          /* 0x0 doubly linked */
    s32 numElements; /* 0x4 */
    s32 elementSize; /* 0x8 the host size of an element */
    void *list;      /* 0x10 the elements */
    void *head;      /* 0x18 the ones in use */
    void *store;     /* 0x20 the free ones */
} tStatList;

/* A StatLL element's link (MAT3 tStatLLE): the next element, on the list in use or the free list. */
typedef struct tStatLLE {
    void *next; /* 0x0 */
} tStatLLE;

/* A doubly linked element's links (MAT3 ll.c's tDLLE): a doubly linked StatLL's, an AudioList's (InsertLLE,
 * RemoveLLE). */
typedef struct tDLLE {
    void *next; /* 0x0 */
    void *prev; /* 0x8 */
} tDLLE;

/* ---- track: tracks, the road tree, targets, paths and checkpoints */

/* The track's paths (ReadPathData, docs/code/race.md 2): the resurrection path (ResurrectPath, gResPath) with its
 * branches (AltPath, gAltPaths) and the four drone paths (DronePath, gDronePaths), whose points (DronePathPt) are the
 * path file's. Declared in pc_game_types.h, where the path file's converter decodes them beside the file when it
 * loads (port/assets/path.c), with host pointers to the points. */
typedef struct DronePathPt DronePathPt;
typedef struct DronePath DronePath;
typedef struct AltPath AltPath;
typedef struct ResurrectPath ResurrectPath;

/* AltPath.resurrect */
#define ALT_PATH_RESURRECT 1
#define ALT_PATH_ALONE 2

/* A player's mines on the road (MAT3 tPlayerMines, gPlayerMines): at most three at once (AnimateMine). */
typedef struct MineSlots { /* 0x28 bytes */
    struct tTarget *target[4]; /* 0x0 */
    s32 count;                 /* 0x20 */
} MineSlots;

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

/* A target (MAT3 tTarget): one of the track's objects that take part in the race: a cone, a cactus, a fence, a
 * window, a coin, a mine, a weapon, a moving object on a path. Each is a node of gTargetList (SetupTarget), with a
 * kind (targetInfo[infoIdx]) and maybe a record of its kind's (data). The struct is declared with its pool,
 * pc_target_nodes, in pc_game_types.h: PC data in the host layout (0x78 bytes), its link and its data pointers
 * (docs/port/POINTERS.md 10.2). */
typedef struct tTarget tTarget;

/* tTarget.Flags */
#define TARGET_SET_UP 0x1    /* its first frame is done (a path target's StartPathTarget) */
#define TARGET_ACTIVE 0x2    /* still in play: CheckTargetCollision tests only these (with TARGET_COLLIDES); a hit
                              * object's start function clears it, and a path target without it is removed
                              * (AnimatePathTarget) */
#define TARGET_HIT 0x4       /* a car has hit it (CheckTargetCollision sets it before the start function) */
#define TARGET_COLLIDES 0x8  /* it has a collision test (collFuncIdx not 6) */
#define TARGET_IN_WORLD 0x10 /* a path target the world placed (not from a PTHD record) */
#define TARGET_GOLD 0x40     /* a gold coin */
#define TARGET_COIN_GONE 0x80 /* a coin every player has found: AnimateCoin removes it */

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
 * its path. A heap record of the N64's 0x48 bytes (gTargetStructSizes[4]: the heap's accounting keeps the size), so
 * it names its path by the path's index, where MAT3 has a pointer (pathData) and the N64 kept the PTHD record's
 * 32-bit address: TARGET_PATH (track.h) is the path (docs/port/NATIVE.md step 7). */
typedef struct tTargetStructPath { /* 0x48 bytes */
    s32 pathIdx;        /* 0x00 its path, gTargetPaths.paths[pathIdx] (MAT3 pathData) */
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

/* A coin's record (MAT3 tTargetStructCoin, tTarget.data of a stunt or obstacle track's gold or silver coin): the
 * spinning coin AnimateCoin makes and turns, and who has yet to find it (StartCoin). No pointers: 0x4C bytes. */
typedef struct tTargetStructCoin {
    s32 dlIdx;          /* the spinning coin's scene object (ZOID_AddObject's handle; its low half the object) */
    struct {
        f32 uvs[3][3];  /* its orientation */
        f32 pos[3];     /* its position */
    } mat;              /* wave 4: tUVMatrix */
    f32 glowTime;       /* MAT3's; no code reads it */
    f32 sc;             /* MAT3's; no code reads it */
    f32 angV[3];        /* its spin, radians a second about x, y and z */
    u16 cameras;        /* a bit per player who hasn't found it yet */
    u16 whichcoin;      /* its number in the profile's found-coin bits (the gold coins from 8) */
} tTargetStructCoin;

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

/* A box a moving target collides as (MAT3 tBBoxColl, a row of BBoxCollInfo): its sides in its own frame, feet, its
 * radius and its weight. */
typedef struct tBBoxColl { /* 0x20 bytes */
    f32 left;   /* 0x00 x from */
    f32 right;  /* 0x04 x to */
    f32 front;  /* 0x08 z to */
    f32 back;   /* 0x0C z from */
    f32 top;    /* 0x10 y to */
    f32 bot;    /* 0x14 y from */
    f32 rad;    /* 0x18 the radius of the circle round it */
    f32 weight; /* 0x1C */
} tBBoxColl;

/* A bounding box model (MAT3 tMiniModelData, gBBoxModelStructs[]): a moving target the cars collide with as a box.
 * A heap block of the N64's 64 bytes a model (SetTargetDefs), which the game's heap accounts as it did
 * (docs/port/NATIVE.md 4.3), so the box is kept as its row of BBoxCollInfo, where MAT3 kept the row's address
 * (bboxInfo): MINI_MODEL_BOX (track.h) is the row. */
typedef struct tMiniModelData { /* 0x40 bytes */
    s32 bboxRow;   /* 0x00 its box: its row of BBoxCollInfo (its kind's collVal) */
    f32 uvs[3][3]; /* 0x04 its orientation */
    f32 pos[3];    /* 0x28 */
    f32 vel[3];    /* 0x34 */
} tMiniModelData;

/* The surface tree (MAT3 stree.c): the track's collision surfaces, from its collision WAD (InitSTree). No pointers,
 * the same layout on both. A vertex position is in 1/32 feet: whole feet in pos, the three axes' 5-bit fractions in
 * overflow. */
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

/* A surface (MAT3 surftyp, stree_surf[]): a road or wall polygon of the collision WAD. */
typedef struct surftyp { /* 0x18 bytes */
    u16 flags;      /* 0x00 low nibble its kind (5 and 6: walls the cars bounce softly off, PointInSurf), 15 out of
                     * the road (RemoveCollisionObject); 0x2000 soft too, a half pipe (resurrect) */
    u16 snd_flags;  /* 0x02 low nibble its vertex count; 0x10 a push strip, 0x20 moved by a target or a trigger
                     * polygon, 0x30 moving at all; bits 11-15 the push speed (8 mph steps) or the target's number */
    s16 uvs[3][3];  /* 0x04 its orientation, 1.0 = 0x4000: uvs[0] the direction it moves */
    u16 vli;        /* 0x16 its vertex list, a byte offset in stree_vlist */
} surftyp;

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
 * weapons and mines). Host layout: both are host pointers (16 bytes). */
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

typedef struct tCheckpoints { /* 0x32C bytes */
    s16 startTime;          /* 0x0 */
    s16 loopPt;             /* 0x2 */
    s16 finishPt;           /* 0x4 */
    s16 startPt;            /* 0x6 */
    s16 numPoints;          /* 0x8 */
    tCheckpoint points[10]; /* 0xC */
} tCheckpoints;

/* ---- cars: the cars, their visuals, skids and smoke */

/* A car's scene objects (MAT3 CarParts; the N64's is 0x40 bytes, without MAT3's headlights, trim and hood sheens),
 * gCarParts[slot]: each an s32 scene handle whose low half, (s16)handle, is the node (ObjInstanceList's index). */
typedef struct CarParts { /* 0x40 bytes */
    s32 frame;         /* 0x00 the body: the parent of the rest */
    s32 extraparts[3]; /* 0x04 the rocket car's flames (StartFrame) */
    s32 hood;          /* 0x10 shown in the cockpit view */
    s32 sheen;         /* 0x14 the environment-mapped shine over the body */
    s32 weap;          /* 0x18 the battle weapon */
    s32 tires[4];      /* 0x1C */
    s32 lfSpark;       /* 0x2C the sparks: front left, rear left, front right, rear right, under the car */
    s32 lrSpark;       /* 0x30 */
    s32 rfSpark;       /* 0x34 */
    s32 rrSpark;       /* 0x38 */
    s32 bSpark;        /* 0x3C */
} CarParts;

typedef struct Collision16 { /* 0x10 bytes */
    f32 front;  /* 0x0 */
    f32 rear;   /* 0x4 */
    f32 side;   /* 0x8 */
    f32 height; /* 0xC */
} Collision16;

/* An environment visual, an entry of gEnvirons (a tVisual, struct pc_visual's layout: the next on its list, its
 * index, scene node and car slot, a data word, a time and its animator). The data word is pointer-sized: a target's
 * visual keeps its node there (a tTarget *). */
typedef struct EnvironNode { /* 0x28 bytes */
    struct Node *next; /* 0x0 */
    s16 index;         /* 0x8 */
    s16 objnum;        /* 0xA */
    s16 slot;          /* 0xC */
    uintptr_t data;    /* 0x10 */
    f32 timeStamp;     /* 0x18 */
    void *func;        /* 0x20 */
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

typedef struct tUVPair { /* 0x10 bytes */
    u16 uv[8]; /* 0x0 */
} tUVPair;

/* ---- physics: the car simulation: collision, the road, the drivetrain, tyres, respawning */

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

/* A race of a circuit (MAT3 tCircuitTrack), gCircuitTracks[]: ContinueCircuit draws them from the circuit's seed. */
typedef struct tCircuitTrack {
    u8 trackNum;   /* the track */
    u8 backward;   /* driven backward */
    u8 mirror;     /* mirrored */
    u8 fog;        /* 0 none to 3 thick */
    u8 wind;       /* 0 none to 4 */
} tCircuitTrack;

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

/* A tracker's times (MAT3 tStuntTimes). */
typedef struct tStuntTimes { /* 0x8 bytes */
    f32 startTime; /* 0x0 */
    f32 endTime;   /* 0x4 */
} tStuntTimes;

/* A player's stunt in progress (MAT3 tStuntInfo, gStuntInfo[4]). */
typedef struct StuntInfo { /* 0x7C bytes */
    s32 active;          /* 0x00 the trackers running: 0x1 on the two wheels of a side, 0x2 on the front wheels, 0x4
                          * on the back ones, 0x10 a spin, 0x20 a roll, 0x40 a flip, 0x80 the combination, 0x100 air
                          * time; above, each axis's turn direction */
    f32 stuntW[4][3];    /* 0x04 rotation snapshots and sums */
    tStuntTimes time[4]; /* 0x34 each tracker's start and end times */
    f32 endTime;         /* 0x54 */
    f32 initUV[3][3];    /* 0x58 */
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
    s8 hIdx;              /* 0x1 the power-up spot health last came at */
    s8 sIdx;              /* 0x2 the shield's */
    s8 iIdx;              /* 0x3 invisibility's */
    f32 healthTLeft;      /* 0x4 */
    f32 invsTLeft;        /* 0x8 */
    f32 shieldTLeft;      /* 0xC */
    tBattlePowerUpT *tS;  /* 0x10 the power-ups (a heap block) */
} tBattlePowerUps;

/* A car's cannon flash (MAT3 tCMuzzleObj, gCannMuzzleObjs[4]): the five flame objects and their matrices, the
 * ripple and its animation. */
typedef struct tCMuzzleObj { /* 0x148 bytes */
    s32 mdls[5];        /* 0x0 the flames' scene objects, -1 none */
    MuzzleMat mats[5];  /* 0x14 */
    s32 ripdl;          /* 0x104 the ripple's scene object, -1 none */
    UVMat mat;          /* 0x108 the ripple's */
    s8 ripCnt;          /* 0x138 its frame, -1 none */
    s8 ripTIdx;         /* 0x139 its texture (gRipTextures) */
    s16 on;             /* 0x13A the flames show */
    f32 ripScale;       /* 0x13C */
    f32 ripTime;        /* 0x140 to its next frame */
    f32 onTime;         /* 0x144 the flames have shown */
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

/* A window's camera (MAT3 anon_mat_n64_98, gCamera[4], one per window): where it looks from. */
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

typedef struct NameQuad { /* 0x20 bytes */
    char *name[4]; /* 0x0 */
} NameQuad;

typedef struct tHudIcon { /* 0x34 bytes */
    s16 type;         /* 0x0 */
    s16 idx;          /* 0x2 */
    UVMatrixRows mat; /* 0x4 */
} tHudIcon;

/* A window's battle coin (MAT3 anon_idx_mat_34, battleCoin[4]): its 3D object (-1 none) and its matrix. */
typedef struct tBattleCoin { /* 0x34 bytes */
    s16 idx;          /* 0x0 */
    s16 pad2;         /* 0x2 */
    UVMatrixRows mat; /* 0x4 */
} tBattleCoin;

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

typedef struct TrackSlot { /* 0x18 bytes */
    s8 gTrackNum;  /* 0x0 */
    f32 pos[3];    /* 0x4 */
    f32 targetPos; /* 0x10 */
    s8 slot;       /* 0x14 */
} TrackSlot;


#endif /* GAME_TYPES_H */
