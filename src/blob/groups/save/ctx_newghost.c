/* Context copy of src/blob/groups/NewGhost_2/group.c with its SaveServer context draft removed (the real
 * body is in group.c): NewGhost (GC _NewGhost) is internal in the unit, SaveServer calls it.
 * : func_800CC50C (GC CompressGhost) is a MEMBER now; its body and the helpers ghost_blocks_from_size
 * ghost_free_start are at the end of this file (see the comments there). */
/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Real C7200 callsites; only the allocator is claimed. */
/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
#define NULL ((void *)0)
#define TRUE 1
#define FALSE 0
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;
typedef volatile s8 vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile s64 vs64;
typedef union {
    struct {
        u32 w0;
        uintptr_t w1;
    } words;
    u64 force_structure_alignment;
} Gfx;
typedef u32 Mtx[4][4];
typedef f32 F32;
typedef s32 S32;
typedef s16 S16;
typedef s8 S08;
typedef u32 U32;
typedef u16 U16;
typedef u8 U08;
typedef s32 BOOL;
typedef f32 Vec3f[3];
typedef s16 Vec3s[3];
typedef s32 Vec3i[3];
typedef f32 Mat3f[3][3];
typedef f32 Mat4f[4][4];
typedef f32 MtxF[4][4];
struct OSPfs;
extern u8 rspbootTextStart[], rspbootTextEnd[];
extern u8 gspF3DEX2_fifoTextStart[], gspF3DEX2_fifoTextEnd[];
extern u8 gspF3DEX2_fifoDataStart[], gspF3DEX2_fifoDataEnd[];
typedef struct {
    u32 type;
    u32 flags;
    u64 *ucode_boot;
    u32 ucode_boot_size;
    u64 *ucode;
    u32 ucode_size;
    u64 *ucode_data;
    u32 ucode_data_size;
    u64 *dram_stack;
    u32 dram_stack_size;
    u64 *output_buff;
    u64 *output_buff_size;
    u64 *data_ptr;
    u32 data_size;
    u64 *yield_data_ptr;
    u32 yield_data_size;
} OSTask_t;
typedef struct {
    OSTask_t t;
} OSTask;
typedef s32 OSPri;
typedef s32 OSId;
typedef struct __OSThreadContext {
    u64 at, v0, v1, a0, a1, a2, a3;
    u64 t0, t1, t2, t3, t4, t5, t6, t7;
    u64 s0, s1, s2, s3, s4, s5, s6, s7;
    u64 t8, t9, gp, sp, s8, ra;
    u64 lo, hi;
    u32 sr, pc, cause, badvaddr, rcp;
    u32 fpcsr;
    f32 fp0, fp2, fp4, fp6, fp8, fp10, fp12, fp14;
    f32 fp16, fp18, fp20, fp22, fp24, fp26, fp28, fp30;
} __OSThreadContext;
typedef struct OSThread_s {
    struct OSThread_s *next;
    s32 priority;
    struct OSThread_s **queue;
    struct OSThread_s *tlnext;
    u16 state;
    u16 flags;
    s32 id;
    s32 fp;
    struct __OSThreadprofile_s *thprof;
    __OSThreadContext context;
} OSThread;
void osCreateThread(OSThread *thread, OSId id, void (*entry)(void *),
                    void *arg, void *sp, OSPri priority);
void osStartThread(OSThread *thread);
void osStopThread(OSThread *thread);
OSPri osViSetMode(OSThread *thread, OSPri priority);
OSPri osGetThreadPri(OSThread *thread);
OSId osGetThreadId(OSThread *thread);
OSThread *__osPopThread(OSThread **queue);
void __osEnqueueThread(OSThread **queue, OSThread *thread);
extern OSThread *__osRunningThread;
extern OSThread **__osActiveQueue;
typedef void *OSMesg;
typedef struct OSMesgQueue_s {
    OSThread *mtqueue;
    OSThread *fullqueue;
    s32 validCount;
    s32 first;
    s32 msgCount;
    OSMesg *msg;
} OSMesgQueue;
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osJamMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
typedef struct OSIoMesgHdr {
    u16 type;
    u8 pri;
    u8 status;
    OSMesgQueue *retQueue;
} OSIoMesgHdr;
typedef struct OSIoMesg {
    OSIoMesgHdr hdr;
    void *dramAddr;
    u32 devAddr;
    u32 size;
    void *piHandle;
} OSIoMesg;
typedef struct OSPiHandle {
    struct OSPiHandle *next;
    u8 type;
    u8 latency;
    u8 pageSize;
    u8 relDuration;
    u8 pulse;
    u8 domain;
    u32 baseAddress;
    u32 speed;
} OSPiHandle;
u32 osPiGetStatus(void);
s32 osPiWriteIo(u32 devAddr, u32 data);
s32 __osSiRawReadIo(u32 devAddr, u32 *data);
s32 __osPiRawStartDma(OSIoMesg *mb, s32 priority, s32 direction,
                 u32 devAddr, void *dramAddr, u32 size, OSMesgQueue *mq);
void osCreatePiManager(s32 pri, OSMesgQueue *cmdQ, OSMesg *cmdBuf, s32 cmdMsgCnt);
OSPiHandle *osCartRomInit(void);
s32 osAiSetNextBuffer(void *addr, u32 size);
s32 osAiSetFrequency(u32 frequency);
typedef struct OSPfs {
    s32 status;
    OSMesgQueue *queue;
    s32 channel;
    u8 id[32];
    u8 label[32];
    s32 version;
    s32 dir_size;
    s32 inode_table;
    s32 minode_table;
    s32 dir_table;
    s32 inode_start_page;
    u8 banks;
    u8 activebank;
} OSPfs;
typedef struct OSPfsState {
    u32 file_size;
    u32 game_code;
    u16 company_code;
    char ext_name[4];
    char game_name[16];
} OSPfsState;
typedef union __OSInodeUnit {
    struct {
        u8 bank;
        u8 page;
    } inode_t;
    u16 ipage;
} __OSInodeUnit;
typedef struct __OSInode {
    __OSInodeUnit inode_page[128];
} __OSInode;
typedef struct __OSDir {
    u32 game_code;
    u16 company_code;
    __OSInodeUnit start_page;
    u8 status;
    u8 reserved;
    char game_name[16];
    char ext_name[4];
    u16 data_sum;
} __OSDir;
s32 osPfsInitPak(OSMesgQueue *queue, OSPfs *pfs, s32 channel);
s32 osPfsChecker(OSPfs *pfs);
s32 osPfsRepairId(OSPfs *pfs);
s32 osPfsAllocateFile(OSPfs *pfs, u16 companyCode, u32 gameCode,
                      u8 *gameName, u8 *extName, s32 size, s32 *fileNo);
s32 __osPfsReleasePages(OSPfs *pfs, u16 companyCode, u32 gameCode,
                  u8 *gameName, u8 *extName, s32 *fileNo);
s32 osPfsFileState(OSPfs *pfs, s32 fileNo, OSPfsState *state);
s32 osPfsGetLabel(OSPfs *pfs, u8 *label, s32 *length);
s32 osPfsSetLabel(OSPfs *pfs, u8 *label);
s32 osPfsFreeBlocks(OSPfs *pfs, s32 *freeBytes);
s32 osPfsNumFiles(OSPfs *pfs, s32 *maxFiles, s32 *usedFiles);
s32 __osPfsSelectBank(OSPfs *pfs, u8 bank);
s32 __osPfsRWInode(OSPfs *pfs, __OSInode *inode, u8 flag, u8 bank);
s32 osPfsFindFile(OSPfs *pfs, s32 pages);
typedef struct OSContStatus {
    u16 type;
    u8 status;
    u8 errno;
} OSContStatus;
typedef struct OSContPad {
    u16 button;
    s8 stick_x;
    s8 stick_y;
    u8 errno;
} OSContPad;
typedef struct OSContRamIo {
    void *address;
    u8 databuffer[32];
    u8 addressCrc;
    u8 dataCrc;
    u8 errno;
} OSContRamIo;
s32 osContInit(OSMesgQueue *mq, u8 *pattern, OSContStatus *status);
s32 osContReset(OSMesgQueue *mq, OSContStatus *status);
s32 osContStartQuery(OSMesgQueue *mq);
s32 __osPfsGetStatus(OSMesgQueue *mq);
s32 osContSetCh(u8 num);
void osContGetQuery(OSContStatus *status);
void osContGetReadData(OSContPad *pad);
typedef u64 OSTime;
typedef struct OSTimer {
    struct OSTimer *next;
    struct OSTimer *prev;
    OSTime interval;
    OSTime value;
    OSMesgQueue *mq;
    OSMesg msg;
} OSTimer;
OSTime osGetTime(void);
void osSetTime(OSTime time);
s32 osSetTimer(OSTimer *timer, OSTime countdown, OSTime interval,
               OSMesgQueue *mq, OSMesg msg);
s32 osStopTimer(OSTimer *timer);
void osInvalDCache(void *vaddr, s32 nbytes);
void osWritebackDCacheAll(void *vaddr, s32 nbytes);
void osWritebackDCache(void *vaddr, s32 nbytes);
void rmonPrintf(void);
void bzero(void *s, s32 n);
void bcopy(const void *src, void *dst, s32 len);
u32 osVirtualToPhysical(void *vaddr);
void *osPhysicalToVirtual(u32 paddr);
u32 __osDisableInt(void);
void __osRestoreInt(u32 flags);
void osSetTime(u64 time);
u64 osGetTime(void);
s32 osDpIsBusy(void);
void osDpSetNextBuffer(void *dramAddr, u32 size);
typedef struct {
    u32 ctrl, width, burst, vSync, hSync, leap, hStart, xScale, vCurrent;
} OSViCommonRegs;
typedef struct {
    u32 origin, yScale, vStart, vBurst, vIntr;
} OSViFieldRegs;
typedef struct {
    u8 type;
    OSViCommonRegs comRegs;
    OSViFieldRegs fldRegs[2];
} OSViMode;
typedef struct {
    f32 factor;
    u16 offset;
    u32 scale;
} __OSViScale;
typedef struct {
    u16 state;
    u16 retraceCount;
    void *framep;
    OSViMode *modep;
    u32 control;
    OSMesgQueue *msgq;
    OSMesg msg;
    __OSViScale x;
    __OSViScale y;
} __OSViContext;
extern __OSViContext *__osViContext;
void __osDequeueThread(OSThread **queue, OSThread *thread);
extern s32 __osSiRawStartDma(s32 direction, void *dramAddr);
typedef struct {
    u32 ramarray[15];
    u32 pifstatus;
} OSPifRam;
extern OSPifRam __osSiDmaBuffer;
extern u8 __osPfsPifRam[64];
extern s32 __osContRamWrite(OSMesgQueue *mq, s32 channel, u16 address,
                             u8 *buffer, s32 force);
extern s16 video_default_x[32];
extern s16 video_default_y[32];
extern s32 acquire_decompress_lock(s32 blocking);
extern void release_decompress_lock(void);
extern s32 __osSiDmaRetry;
extern void __osEnqueueAndYield(OSThread **queue);
extern s32 __osContRamRead(OSMesgQueue *mq, s32 channel, u16 address,
                            u8 *buffer);
typedef struct OSScTask_s {
    struct OSScTask_s *next;
    s32 state;
    s32 flags;
    void *framebuffer;
    s32 type;
    u8 pad14[0x38 - 0x14];
    void *unk38;
    s32 *unk3C;
    u8 pad40[0x50 - 0x40];
    OSMesgQueue *msgQueue;
    OSMesg msg;
} OSScTask;
typedef struct OSScClient_s {
    struct OSScClient_s *next;
    OSMesgQueue *msgQueue;
} OSScClient;
typedef struct {
    s16 state;
    u8 pad02[0x20 - 0x02];
    s16 priority;
    u8 pad22[0x40 - 0x22];
    OSMesgQueue cmdQueue;
    OSMesg cmdMsgs[8];
    OSMesgQueue retQueue;
    OSMesg retMsgs[8];
    u8 padB0[0x260 - 0xB0];
    OSScClient *clientList;
    OSScTask *rspTaskHead;
    OSScTask *rspTaskTail;
    OSScTask *rdpTaskHead;
    OSScTask *rdpTaskTail;
    OSScTask *curRSPTask;
    OSScTask *curRDPTask;
    s32 retraceCount;
    s32 audioListPending;
} OSSched;
extern void __scAppendList(OSSched *sc, OSScTask *task);
extern void __scExec(OSSched *sc, OSScTask *rspTask, OSScTask *rdpTask);
extern void osViSwapBuffer(void *mode);
extern void video_apply_pending_mode(void);
extern u8 unzip_input_a[0x1000];
extern u8 unzip_input_b[0x1000];
extern s32 osPiStartDma(void *mb, s32 priority, s32 direction,
                              u32 devAddr, void *dramAddr, u32 size,
                              OSMesgQueue *mq);
typedef struct {
    s32 flag;
    u8 pad04[0x8 - 0x4];
    s32 unk8;
    u8 pad0C[0x1C - 0xC];
} __OSPiMgrState;
extern __OSPiMgrState __osPiMgrState;
extern void __osPiCreateAccessQueue(void);
extern void __osPiGetAccess(void);
extern void __osPiRelAccess(void);
extern u32 osRomBase;
extern void __osSetCompare(u32 compare);
extern void __osSpSetStatus(u32 status);
extern void osSpTaskYield(void);
extern void __setfpcsr(u32 value);
extern void GameLoop(void);
extern void __osSiGetAccess(void);
extern void __osSiRelAccess(void);
extern void __osPackReadData(void);
typedef struct __OSTimerNode_s {
    struct __OSTimerNode_s *next;
    struct __OSTimerNode_s *prev;
    s32 reload_hi;
    s32 reload_lo;
    s32 delta_hi;
    s32 delta_lo;
    OSMesgQueue *msgQueue;
    OSMesg msg;
} __OSTimerNode;
extern __OSTimerNode *__osTimerList;
extern void osSetThreadPri(OSThread *thread, OSPri priority);
extern void init_decompress_lock(void);
extern void osViBlack(s32 mask);
extern void osViSetSpecialFeatures(u32 features);
extern u8 main_thread_stack[0x2000];
typedef enum GState {
    ATTRACT, TRKSEL, CARSEL, PLAYGAME, ENDGAME, GAMEOVER, HISCORE,
    PREPLAY, PREPLAY2, COUNTDOWN, NUM_GAME_STATES
} GState;
extern u8 gstate;
extern s32 frame_counter;
extern s32 inactiveBlit;
extern s32 gGameState;
extern s32 gNewGameState;
extern OSMesgQueue *frame_dl;
typedef struct {
    u8 pad00[1];
    u8 active;
    u8 pad02[2];
    s32 unk04;
    s32 unk08;
    s32 unk0C;
    f32 unk10;
    f32 unk14;
    u8 _pad18[0x4D - 0x18];
    u8 unk4D;
} InputRecord;
extern InputRecord gPlayers;
extern InputRecord input_rec1;
extern s32 gContLevels[4];
extern s32 gContEdges[4];
extern s32 gAutoEdges[4];
typedef struct {
    f32 unk0;
    f32 unk4;
} ContStick;
extern ContStick gContStick[4];
typedef struct {
    u8 pad0000[0x9CC0];
    OSMesgQueue unk9CC0;
} SegmentHeader;
typedef struct {
    u8 pad00[0x58];
    SegmentHeader *unk58;
    u8 pad5C[0x7C - 0x5C];
    void *unk7C;
} SegmentTableEntry;
extern SegmentTableEntry frame_records[];
typedef struct {
    u8 pad0[8];
    s32 unk8;
} D_8014A160_Target;
extern D_8014A160_Target **gPlayers_save;
typedef struct D_8012E6E0_Node {
    struct D_8012E6E0_Node *next;
} D_8012E6E0_Node;
extern D_8012E6E0_Node *gPlayerFiles_head;
typedef struct {
    u8 pad000[0xE8];
    s32 unkE8;
    u8 pad0EC[0x110 - 0xEC]; struct pc_visual visuals[21]; u8 pad0EC_308[0x35B - 0x308];
    u8 unk35B;
    u8 pad35C[0x380 - 0x35C];
    void *unk380;
    u8 pad384[0x3A3 - 0x384];
    u8 unk3A3;
    u8 pad3A4[0x3B8 - 0x3A4];
} GameCar;
extern GameCar gGameCar[8];
typedef struct {
    u8 pad00[0x39];
    u8 unk39;
    u8 unk3A;
    u8 unk3B;
} PlaygameSettings;
extern PlaygameSettings gFogColors;
typedef struct {
    u8 pad000[0x1F0];
    s32 unk1F0;
    s32 unk1F4;
    s32 unk1F8;
    s32 unk1FC;
    s32 unk200;
} CountdownObject;
typedef struct {
    u8 pad000[0x19C];
    void *unk19C;
    u8 pad1A0[0x200 - 0x1A0];
    void *unk200;
    void *unk204;
    void *unk208;
} CountdownDetail;
typedef struct {
    void *unk0;
    CountdownDetail *unk4;
    void *unk8;
    void *unkC;
    void **unk10;
} CountdownState;
extern CountdownState gText;
extern CountdownObject *gText_single;
typedef struct PadConfig_s {
    struct PadConfig_s *unk0;
    struct PadConfig_s *unk4;
    s16 unk8;
    s16 unkA;
    s16 unkC;
    s16 unkE;
    s16 unk10;
    s16 unk12;
    u8 unk14;
    u8 unk15;
    u8 unk16;
    u8 pad17;
    s16 unk18;
    s16 unk1A;
    s16 unk1C;
    s16 unk1E;
} PadConfig;
extern PadConfig gBlitInstanceList;
typedef struct {
    PadConfig *unk0;
    s32 unk4;
} D_80138670_Entry;
extern D_80138670_Entry D_80138670[];
extern s32 scheduler_retrace_count;
extern s16 gNumPlayers;
extern s32 gRace;
extern f32 gRetraceRate, gRetraceTime;
extern s32 screen_width, screen_height, zbuffer;
extern u16 gRenderInProgress;
extern u8 frame_start_pending, game_frame_building, frame_end_requested;
extern s32 gLastKeyTime;
extern u8 gRestartGame, gGamePointPlayed, D_801146F0;
extern s32 D_801146F8, gPauseState;
extern u8 pause_gControllerRemoved, pause_gYes;
extern s32 gSkipToGameState;
extern u8 D_8011ED0B;
extern u16 blit_tile_lines[];
extern f32 D_80123FB4, D_80123FB8, D_80123FBC, D_801242A8;
extern u8 frame_lookat_count;
extern s32 gBgColor;
extern u8 gCarTeam, gCoastFlag;
extern s32 D_80140008;
extern u16 gNumLights;
extern s32 gLights, gStuntMode, gStuntModeMins, gStuntModePoints;
extern s32 gStuntModeDeaths, gBattleMode, gBattleModeMins;
extern u8 D_80140C26;
extern s32 D_80140D70, D_80141428, gBattleModePoints;
extern u8 gOutOfTime, gRetireGame, gDeathMode;
extern s32 gAPlayerReallyFinished;
extern f32 save_server_tick_time;
extern u8 gGameStarted, gNumZoidWindows, gSofdecShown, gLicenseShown;
extern s32 blit_dl;
extern u8 gIntroMovieShown, pfsInited, lcdInited;
extern s32 frame_dl_start, pauseMenuSelect;
extern u8 D_8014B240, gCatchup, gUseCatchup;
extern s32 D_80150000;
extern u16 gNumWindows;
extern u8 gUpsideDownTrack, gTeamPoints, gNumActiveCars, D_80152F29;
extern s32 stunt_player_total_points, D_801520C4, D_80153308;
extern f32 gDeathWaitEnd, gModelTime;
extern u16 gNumLaps;
extern s32 playerInPause;
extern u8 expansion_pak_present, gControllerData, gSoftReset, frame_record_index;
extern s32 frame_dynamic, frame_matrix_cursor, video_texture_filter;
extern s32 video_color_dither, video_dither_filter, video_divot, gNumBlitInsts;
extern s32 first_unplugged_controller, zoid_poly_vertices, zoid_current_tlut, zoid_current_texture;
extern s32 zoid_tlut_mode;
typedef struct {
    u16 unk00;
    u16 unk02;
    u8 pad04[0x60 - 0x04];
    s32 unk60;
    s32 unk64;
} D_80143FD8_Record;
extern D_80143FD8_Record *gTargetList_head;
typedef struct {
    u8 pad000[0x7C6];
    u16 unk7C6;
    u8 pad7C8[0x7E8 - 0x7C8];
    u8 unk7E8;
} ModelRecord;
extern ModelRecord D_8014A250;
typedef struct {
    u8 pad00[0x0C];
    u8 unk0C;
    u8 unk0D;
    u8 unk0E;
} D_80146108_Record;
extern D_80146108_Record gOptions;
typedef struct {
    F32 start_time[8];
    F32 end_time[8];
    S16 loop_chkpnt;
    S16 finish_line;
    S16 before_finish;
    S16 number_of_laps;
} Track_Data;
typedef struct SoundClearRecord {
    s32 unk0;
    s16 unk4;
    s16 unk6;
    s16 unk8;
    s16 unkA;
    s16 unkC;
    s16 unkE;
    s16 unk10;
    s16 unk12;
    s32 unk14;
    s32 unk18;
    s32 (*unk1C)(void *);
    s32 unk20;
} SoundClearRecord;
typedef struct SoundState {
    u8 _pad0[0x4];
    struct SoundState *unk4;
    s32 (*unk8)(void *);
    u8 _padC[0x12 - 0xC];
    s16 unk12;
    s16 unk14;
    s16 unk16;
    s8 unk18;
    u8 _pad19[0x1C - 0x19];
    s16 unk1C;
    s16 unk1E;
    s16 unk20;
    s16 unk22;
    u8 _pad24[0x28 - 0x24];
    s32 (*unk28)(void *);
    s32 unk2C;
    u8 _pad30[0x3C - 0x30];
    struct SoundState *unk3C;
} SoundState;
extern SoundState *NewBlit(s32, s32, s32, s32);
extern SoundState *NewMultiBlit(s16 arg0, s16 arg1, SoundClearRecord *arg2, s16 arg3);
extern void GameLoop(void);
extern void game_mode_handler(void);
extern void StartGraphics(void);
extern void UpdateRushControls(void);
extern void updateGameState(void);
extern void updatePauseScreen(void);
extern void UpdateGame(void);
extern void DrawNoControllers(void);
extern void ZOID_EndFrame(s32 pad);
extern void UpdateCameras(void);
   extern s32 UpdateEnvirons(void);
extern void DrawBlits(void);
extern void updateFrontEnd(void);
extern void RemoveBlit(s32 sound_id);
extern s32 HAL_CallOncePerFrameFuncs(void);
extern void UpdateFrameTimer(void);
extern void BlitInit(void);
extern s32 ZOID_SplitMenuScreen(s32, s32, s32, s32);
extern void world_trigger_check(void);
extern void UpdateControls(void);
extern void UpdateBlit(void *);
extern void load_extra_overlay(void);
extern s32 LoadWave(s32, s32, s32, s32, s32);
extern void LoadFrontEndOverlay(s32);
extern void PS_LockFile(s32, s32, s32);
extern void ZOID_SetFog(s32, s32, u8, u8, s32, s32, s32);
extern void func_800c813c(s32, s32);
extern void AudFree3D(void);
extern void InitRecords(void);
extern void AudioInit(s32, s32, s32, s32, s32, f32, f32, s32);
extern void AudInit3D(s32, s32, s32, s32, s32);
extern void SetOptions(void);
extern s32 HAL_SetFont(s32);
extern s32 PurgePlayerData(void **);
extern void HAL_PurgeFonts(void);
extern void SetControllerStickEdges(s32, s32);
extern void SetControllerActive(s32, s32);
extern void LoadFrontEndWaves(void);
extern void scene_cleanup_slots(void);
extern void AudSetMasterVol(void);
extern void uninitGameState(void);
extern void AudPlaySong(s32, s32);
extern void InitWaves(void);
extern void SaveFlush(s32);
extern void updateHighScoreScreen(void);
extern void CarSetup(void);
extern void UpdateVisuals(s8);
extern s32 PrePlay(s32, s32);
extern s32 ClearRushControls(s32);
extern void updateGameOverScreen(void);
extern void WorldFogDensity(s32, s32);
extern void PlayGameSound(s32, s32, s32, s32);
extern void StopListeners(void);
extern void End_MDrive(s16);
extern void StartGameMusicInternal(void);
extern void init_model_task(s32);
extern void func_800f7f3c(void);
extern void CheckButtons(void);
extern void InitCPTime(void);
extern void CountDown(void);
extern void GhostEnd(void);
extern void CalcPolePositions(void);
extern void modelstop(void);
extern void Update_MDrive(void);
extern s32 TextureAnimation(void);
extern void CheckTargetCollision(s16);
extern void RemoveHUD(void);
extern void PlayGame(void);
extern void *memset(void *s, s32 c, u32 n);
extern void SetCountdownTimer(f32);
extern void TB_SetTextColor(s32);
extern void game_strcat(void *, s32);
extern s32 HAL_GetTextWidth(void *, s32);
extern s32 HAL_SetFontInternal(void);
extern void TB_DrawText(s16, s32, void *);
extern void sprintf(s8 *buf, s8 *fmt, ...);
extern void BlitClearAttribute(s32, u8, s32);
extern void BlitDraw(s32, s16, s32, s32, s32, s32);
extern void BlitSetAttribute(s32, u8, s32);
extern void BlitColors(s32, u8, u8, s32);
extern void BlitXLU(u8);
extern void BlitScissor(s16, s16, s32, s32);
extern void BlitRect(s16, s16, s32, s32, void *);
extern void BlitZDist(u16);
extern void zoid_draw_window(s32);
extern void blit_load_texture(s32, u8, u8, u16, s32, s32, s32, s32, s32, s32, s32);

/*
 * This header contains macros emitted by m2c in "valid syntax" mode,
 * which can be enabled by passing `--valid-syntax` on the command line.
 *
 * In this mode, unhandled types and expressions are emitted as macros so
 * that the output is compilable without human intervention.
 */

#ifndef M2C_MACROS_H
#define M2C_MACROS_H

/* Unknown types */
typedef s32 M2C_UNK;
typedef s8  M2C_UNK8;
typedef s16 M2C_UNK16;
typedef s32 M2C_UNK32;
typedef s64 M2C_UNK64;

/* Unknown field access, like `*(type_ptr) &expr->unk_offset` */
#define M2C_FIELD(expr, type_ptr, offset) (*(type_ptr)((s8 *)(expr) + (offset)))

/* Bitwise (reinterpret) cast */
#define M2C_BITWISE(type, expr) ((type)(expr))

/* Unaligned reads */
#define M2C_LWL(expr) (expr)
#define M2C_FIRST3BYTES(expr) (expr)
#define M2C_UNALIGNED32(expr) (expr)

/* Unhandled instructions */
#define M2C_ERROR(desc) (0)
#define M2C_TRAP_IF(cond) (0)
#define M2C_BREAK() (0)
#define M2C_SYNC() (0)

#define GLUE_F64(a, b) (0.0)
#define MULT_HI(a, b) (0)
#define MULTU_HI(a, b) (0)
#define DMULT_HI(a, b) (0)
#define DMULTU_HI(a, b) (0)
#define CLZ(x) (0)
#define REVERSE_BITS(x) (0)
#define ROTATE_RIGHT(x, shift) (0)
#define ARM_RRX(x, carry) (0)
#define BSWAP32(x) (0)
#define BSWAP16(x) (0)
#define BSWAP16X2(x) (0)

/* Carry/overflow bits from partially-implemented instructions */
#define M2C_CARRY 0
#define M2C_OVERFLOW(a) (0)

/* Memcpy patterns */
#define M2C_MEMCPY_ALIGNED memcpy
#define M2C_MEMCPY_UNALIGNED memcpy
#define M2C_STRUCT_COPY memcpy

#endif

float fabsf(float);
float sqrtf(float);
#pragma intrinsic (fabsf)
#pragma intrinsic (sqrtf)

/* referenced data (types guessed from access width) */
extern f32 ghostRaceTimes[];   /* GC ghostRaceTimes */
extern s8 gGlobalsSet;
extern s8 D_8011742C;
extern s8 D_80117430;
extern s8 D_80117434;
extern M2C_UNK save_file_name_prefix;
extern M2C_UNK player_file_ext_prefix;
extern D_8012E6E0_Node *gPlayerFiles;
extern M2C_UNK player_profile_handles;
extern M2C_UNK player_profile_records;
extern M2C_UNK ghost_file_handles;
extern M2C_UNK ghost_file_records;
extern s16 gLang;
extern M2C_UNK gPaks;
extern M2C_UNK gPS_Files_head;
extern s8 gTrackNum;
extern M2C_UNK gGhostFiles;
extern void **gGhostFiles_head;
extern M2C_UNK heap_lock_queue;

/* callees outside the group */
void PS_DeleteFile();
void PS_PurgeFile();
void *GetPtrSize(u32 address);                               /* GC GetPtrSize */
void DeallocateMemoryInternal(u32 address, s32 tag);
void **AllocateHandleHeap(void *heap, u32 size);                   /* GC AllocateHandleHeap */
u32 zip(u8 *next_in, u32 avail_in, u8 *next_out, u32 avail_out,
                               s32 level, s32 windowBits, s32 memLevel);   /* GC zip */
s32 CheckAndRepairPlayer(void *ipa_fp);
void AddPlayerScores();
void PS_ReadPartialFile();
u32 CalcChecksumInternal(u8 *, u32);                                 /* GC _CalcChecksum */
s32 game_strcmp();
void InsertLLE();
s32 game_strncmp();
void *game_memmove(void *dst, const void *src, u32 n);            /* GC memmove */
void LoadTextWave();
s32 GetPlayerOption();
void *AllocateMemoryHeapHigh(void *heap, u32 size);                        /* GC AllocateMemoryHeapHigh */
void PS_PakServer();

/* group functions */
void **NewGhost(void);
void SaveServer(s32 forceUpdate, s32 rumbleOnly);
struct GhostHeader;
void **CompressGhost(struct GhostHeader *header, s8 *blocks);
s32 CheckAndRepairGhost(void **ipa_t0, s32 ipa_t1, s32 ipa_t2);
extern void **ReadPlayer(void **);


/* _NewGhost on the PC. A ghost (Ghost below: 0x2C bytes on the N64, 64 with its four pointers) and its handle (8
 * bytes) don't fit the N64's storage, 64 four-byte handle slots at ghost_file_handles and 64 records 0x2C apart at
 * ghost_file_records in the game arena: the PC keeps both here. Same search order; none free gives NULL
 * (docs/port/CASTS.md). */
static void *pc_ghost_slot[64];
static void *pc_ghost[64][8];
void **NewGhost(void) {
    s32 i;

    for (i = 0; i < 64; i++) {
        if (pc_ghost_slot[i] == NULL) {
            pc_ghost_slot[i] = pc_ghost[i];
            return &pc_ghost_slot[i];
        }
    }
    return NULL;
}


/* SaveServer: defined in group.c */


/* ===== func_800CC50C = GC save.c CompressGhost(header, blocks), N64 form (draft, closed) =====
 * GC transcription: memmove = game_memmove, _CalcChecksum = CalcChecksumInternal, AllocateMemoryHeapHigh =
 * AllocateMemoryHeapHigh, zip, _NewGhost = NewGhost, AllocateHandleHeap =
 * audio_task_complete, GetPtrSize, ghostRaceTimes, gRetraceRate,
 * gTrackNum; PS_GetBlocks inlined as ghost_blocks_from_size; GC `DeallocateMemory(header->start)` as
 * the inlined helper ghost_free_start (defined AFTER this function, see there).
 * Shaping: `gh` (= ghost->header) is a named local assigned twice -- once for track/backward/name, again
 * the data memcpy for playerId/finish/GetPtrSize (retail: first web in a3, second in a0; a single assignment gives
 * the first web v1). No unused locals; no compiled-out reads. */
typedef struct GhostHeader {      /* GC __anon_49776; N64 layout 0x58 (from the retail words) */
    u32 headerChecksum;           /* 0x00 */
    u8 revision;                  /* 0x04 */
    s8 flag;                      /* 0x05 */
    s8 track;                     /* 0x06 */
    u8 backward;                  /* 0x07 */
    u8 pad08[0x16 - 0x08];
    u8 name[13];                  /* 0x16 */
    u8 pad23[0x28 - 0x23];
    u32 playerId[2];              /* 0x28 */
    u8 pad30[0x38 - 0x30];
    f32 finish;                   /* 0x38 */
    u32 zippedSize;               /* 0x3C */
    u32 unzippedSize;             /* 0x40 */
    u32 zippedChecksum;           /* 0x44 */
    u32 unzippedChecksum;         /* 0x48 */
    u32 start;                    /* 0x4C: a heap block (the window), 32 bits as in the file */
    u32 offset;                   /* 0x50 */
    u32 size;                     /* 0x54 */
} GhostHeader;

typedef struct Ghost {            /* GC __anon_49a9c; N64 layout 0x2C */
    void *ll;                     /* 0x00 */
    void **fileh;                 /* 0x04 */
    s8 track;                     /* 0x08 */
    u8 backward;                  /* 0x09 */
    u8 name[13];                  /* 0x0A */
    u8 pad17;
    u32 playerId[2];              /* 0x18 */
    f32 finish;                   /* 0x20 */
    u8 **datah;                   /* 0x24 */
    GhostHeader **header;         /* 0x28 */
} Ghost;

void *memcpy(void *, const void *, u32);
void ghost_free_start(GhostHeader *header);
/* PC: the header keeps its N64 layout, file data (docs/port/POINTERS.md 9.10); a ghost is 64 bytes. The heap's API
 * takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define START(x) ((u8 *)PC_N64P(x))
#define N64A(x) PC_N64A(x)

s32 ghost_blocks_from_size(s32 size) {   /* GC PS_GetBlocks */
    return (size + 255) >> 8;
}

void **CompressGhost(GhostHeader *header, s8 *blocks) {
    Ghost *ghost;
    void **ghosth;
    s32 maxsize;
    u8 *sorted;
    u8 *gdata;
    u32 dataSize;
    u32 num;
    GhostHeader **gh;
    u32 unzippedChecksum;

    sorted = START(header->start);
    maxsize = (f32)(ghostRaceTimes[gTrackNum] + 10.0) * gRetraceRate;
    game_memmove(START(header->start) + header->size, START(header->start) + maxsize, header->size);
    game_memmove(START(header->start) + header->size * 2, START(header->start) + maxsize * 2, header->size);
    num = header->size * 3;
    unzippedChecksum = CalcChecksumInternal(sorted, num);
    *(s16 *)((u8 *)header + 36) = 0;
    gdata = AllocateMemoryHeapHigh(NULL, num);
    dataSize = zip(sorted, num, gdata, num, 9, 10, 4);
    ghost_free_start(header);
    header->start = NULL;
    ghosth = NewGhost();
    ghost = *ghosth;
    memset(ghost, 0, GHOST_OFS(44));
    ghost->header = (GhostHeader **)AllocateHandleHeap(NULL, dataSize + 88);
    memcpy(*ghost->header, header, 88);
    (*ghost->header)->revision = 17;
    (*ghost->header)->start = NULL;
    (*ghost->header)->offset = 0;
    (*ghost->header)->size = 0;
    (*ghost->header)->zippedSize = dataSize;
    (*ghost->header)->unzippedSize = num;
    (*ghost->header)->headerChecksum = CalcChecksumInternal(&(*ghost->header)->revision,
        (u8 *)&(*ghost->header)->zippedChecksum - (u8 *)*ghost->header - 4);
    (*ghost->header)->zippedChecksum = CalcChecksumInternal(gdata, dataSize);
    (*ghost->header)->unzippedChecksum = unzippedChecksum;
    memcpy((u8 *)*ghost->header + 88, gdata, dataSize);
    osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1);
    DeallocateMemoryInternal(N64A(gdata), 0);
    osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
    gh = ghost->header;
    ghost->track = (*gh)->track;
    ghost->backward = (*gh)->backward;
    memcpy(ghost->name, (*gh)->name, 13);
    gh = ghost->header;
    ghost->playerId[0] = (*gh)->playerId[0];
    ghost->playerId[1] = (*gh)->playerId[1];
    ghost->finish = (*gh)->finish;
    *blocks = ghost_blocks_from_size((s32)GetPtrSize(N64A(gh)));
    return ghosth;
}

/* GC DeallocateMemory(header->start) for CompressGhost's first free, written as an inlined helper that reads the
 * pointer itself, on ONE line, defined AFTER func_800CC50C (as1 evidence): as1 breaks scheduling ties
 * source line; retail schedules `lw s2,76(s3)` between the lock's `la a0` and `move a1` with `li a2,1` in the delay
 * slot, which needs the pointer load on the lock's line and that line after the caller's `dataSize = ...` line.
 * Identity hypothesis (disclosed): the GC code calls DeallocateMemory(header->start) directly. Non-static, internal. */
void ghost_free_start(GhostHeader *header) { u8 *p = START(header->start); osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1); DeallocateMemoryInternal(N64A(p), 0); osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0); }
