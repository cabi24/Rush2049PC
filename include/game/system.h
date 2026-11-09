/* include/game/system.h: start-up, threads, the scheduler, timers, the frame pipeline, video, decompression, the
 * overlays, and the game state machine's states (docs/code/system.md). */
#ifndef GAME_SYSTEM_H
#define GAME_SYSTEM_H

#include "game/types.h"

/* The game's threads (src/rom/boot.c): libultra thread ids, and priorities (the higher runs first). On the PC each is
 * a host thread and port/ultra keeps their priority order (docs/port/ULTRA.md). */
#define THREAD_ID_IDLE 1        /* main's first thread: starts the PI manager and the main thread, then idles */
#define THREAD_ID_MODEL 5       /* the car simulation (ModelThread) */
#define THREAD_ID_MAIN 6        /* start-up, then the scheduler's client that starts and ends frames */
#define THREAD_ID_GAME 7        /* GameLoop forever */
#define THREAD_ID_BG_LOAD 8     /* the background wave loader (BGLoadWaveMain) */
#define THREAD_PRI_IDLE_START 2 /* the idle thread until it has started the main thread; then 0 */
#define THREAD_PRI_BG_LOAD 3
#define THREAD_PRI_MAIN 4
#define THREAD_PRI_GAME 5
#define THREAD_PRI_MODEL 7
#define THREAD_PRI_SCHEDULER 12
#define THREAD_PRI_PI_MANAGER 150

/* The messages on the main thread's queue (main_thread_queue): the scheduler's (libultra's sched.h names), and the
 * game thread's request to end a frame (UpdateFrame); the main thread sends frame_start_msg to the game thread. */
#define OS_SC_RETRACE_MSG 1
#define OS_SC_DONE_MSG 2
#define OS_SC_PRE_NMI_MSG 4
#define GAME_MSG_END_FRAME 0xABE    /* 2750 */
#define GAME_MSG_START_FRAME 0xABF

/* The game state machine's states (gGameState, gNewGameState: setGameState, updateGameState in state.c). One bit
 * each; MAT3 has the same bits, unnamed. The front end's states are the screens of runtime image A. */
#define GAME_STATE_BOOT 0x1             /* start-up: options, records, then the licence screen */
#define GAME_STATE_LICENSE 0x2
#define GAME_STATE_TITLE 0x4
#define GAME_STATE_DEMO 0x8             /* the attract mode's demo race */
#define GAME_STATE_START 0x10           /* the start menu */
#define GAME_STATE_PLAYERS 0x20
#define GAME_STATE_MODE 0x40
#define GAME_STATE_TRACK 0x80
#define GAME_STATE_CAR 0x100
#define GAME_STATE_OPTIONS 0x200
#define GAME_STATE_CONTROLS 0x400
#define GAME_STATE_AUDIO 0x800
#define GAME_STATE_VIDEO 0x1000
#define GAME_STATE_RECORDS 0x2000
#define GAME_STATE_CIRCUIT 0x4000
#define GAME_STATE_RACE_LOAD 0x40000    /* a race chosen: load the track, the cars, the race image */
#define GAME_STATE_PREPLAY 0x80000      /* PrePlay until the track and cars are in */
#define GAME_STATE_GO 0x100000          /* the countdown has run out: start the race clock and the music */
#define GAME_STATE_COUNTDOWN 0x200000
#define GAME_STATE_PLAY 0x400000        /* the race */
#define GAME_STATE_GAME_OVER 0x800000
#define GAME_STATE_RACE_END 0x1000000   /* EndGame: results, then high scores or game over */
#define GAME_STATE_HIGH_SCORE 0x2000000
#define GAME_STATE_PAK 0x4000000        /* the Controller Pak manager */
#define GAME_STATE_CHEATS 0x8000000
/* 0x10000000 (the start menu's item 7 and the pak manager's exit lead there; no screen updates it) and 0x20000000
 * (initGameState loads wave 55 for it) have no name: what they were for isn't known. */
#define GAME_STATE_WARNING 0x40000000   /* the Controller Pak warning after the licence */
/* The front end's states (the screens of image A, and the boot); the race's (race set-up to high scores); the race
 * from its loading to its play; the race on the track. */
#define GAME_STATES_FRONT_END 0x7C03FFFE
#define GAME_STATES_RACE 0x03FC0000
#define GAME_STATES_RACE_START 0x007C0000
#define GAME_STATES_ON_TRACK 0x00600000

/* Data */
extern void *scheduler_audio_task;
extern s32 __osScPendingSwap;
extern u32 __osScSwapCount;
extern s32 gTimerPaused;
extern f32 gRetraceRate;
extern f32 gRetraceTime;
extern s32 screen_width;
extern s32 screen_height;
extern s16 video_offset_bounds[EXTENT(2)][8];
extern s16 video_default_extra_width[EXTENT(1)];
extern s16 video_default_extra_height[EXTENT(1)];
extern s16 video_default_x[EXTENT(9)];
extern s16 video_default_y[EXTENT(9)];
extern s16 video_pending_frames;
extern struct OSViMode *video_adjusted_mode;
extern s8 gInitFlag;
extern s8 decompress_lock_ready;
extern OSViMode osViModeTable[EXTENT(5)];
extern s32 __osContPifBuf;
extern s32 __osSiInitialized;
extern s32 __osContLastChannel;
extern s32 __osSiLastChannel;
extern u8 __osSiChannelMask;
extern f32 video_x_scale_factor;
extern f32 video_y_scale_factor;
extern f32 retrace_time_pal;
extern f32 retrace_time_ntsc;
extern const f64 gSinCoeffs[EXTENT(5)];
extern const f64 gTwoOverPi;
extern const f64 gPiOver2Hi;
extern const f64 gPiOver2Lo;
extern const f32 gNaN;
extern const f64 gCosCoeffs[EXTENT(5)];
extern const f64 gCosAngleScale;
extern const f64 gCosPiOver2Hi;
extern const f64 gCosPiOver2Lo;
extern const f32 gCosOne;
extern const f64 gOrthoScale;
extern const f32 gNaNf;
extern volatile OSTime __osScRetraceTime;
extern PcSched gScheduler;
extern s32 scheduler_spare_word;
extern s16 gRenderInProgress;
extern volatile OSTime __osScAudioStart;
extern volatile OSTime __osScFrameTime;
extern u64 scheduler_spare_time;
extern f32 gSeconds;
extern f32 gFrameSeconds;
extern s32 gLastFrameCount;
extern s32 gCountdownEnd;
extern s32 gCountdownStart;
extern volatile s32 gLastFrameCounter;
extern void *zbuffer;
extern s16 video_tv_type;
extern s16 video_mode_index;
extern OSViMode *video_base_mode;
extern OSViMode video_adjusted_modes[EXTENT(2)];
extern s16 video_h_start;
extern s16 video_v_start;
extern s16 video_h_end;
extern s16 video_v_end;
extern s16 video_extra_width;
extern s16 video_extra_height;
extern s16 video_width;
extern s16 video_height;
extern s16 video_offset_x;
extern s16 video_offset_y;
extern OSMesgQueue game_thread_queue;
extern OSMesg gEventMesgBuf[EXTENT(8)];
extern OSMesgQueue main_thread_queue;
extern OSMesg gRetraceMesgBuf[EXTENT(62)];
extern OSMesgQueue gViModeTable;
extern OSMesg gViModeLan1[EXTENT(20)];
extern OSMesg gDmaMesgBuf[EXTENT(8)];
extern OSMesg gInflateMesgBuf[EXTENT(8)];
extern OSMesg gSyncMesgBuf[EXTENT(4)];
extern OSMesgQueue decompress_lock_queue;
extern OSMesg decompress_lock_message;
extern u8 gStackIdle[EXTENT(32)];
extern u8 bg_load_thread_stack[EXTENT(2400)];
extern u8 main_thread_stack[EXTENT(8192)];
extern u8 game_thread_stack[EXTENT(4800)];
extern u8 model_thread_stack[EXTENT(3200)];
extern u8 gSchedulerStack[EXTENT(1600)];
extern OSThread game_thread;
extern OSThread bg_load_thread;
extern OSThread model_thread;
extern OSThread gIdleThread;
extern OSThread main_thread;
extern OSMesgQueue rom_dma_queue;
extern OSMesgQueue unzip_dma_queue;
extern volatile u8 frame_start_pending;
extern volatile u8 game_frame_building;
extern volatile s8 frame_end_requested;
extern OSPifRam __osSiDmaBuffer;
extern u8 __osContLastCmd;
extern u8 __osMaxControllers;
extern OSMesgQueue __osContQueryMsgQ;
extern OSMesg __osContQueryMesg;
extern u8 __osMotorPifBuf[EXTENT(256)];
extern void *__osSiMesgQueue;
extern OSMesgQueue __osSiMesg;
extern __OSInode __osContPifInode;
extern u8 __osPfsPifRam[EXTENT(64)];
extern Definitions64 gDecompressedDataEnd1[EXTENT(9)];
extern AudioMenuRect audio_menuRect;
extern f32 rear_traction_by_surface[EXTENT(7)][6];
extern f32 rear_traction_by_tune[EXTENT(9)][3];
extern s8 si_lock_inited;
extern s8 gInited_HALTime;
extern u8 highscore_inited;
extern ScreenPos no_rumble_text_pos[EXTENT(4)][4];
extern PaneColors hud_playerPaneColors[EXTENT(4)];
extern char *options_buttonNames[EXTENT(9)];
extern TPos pause_gPlayerRects[EXTENT(4)][4];
extern ResourceRange pause_titlePos;
extern void *pause_DimBlit;
extern void *pause_TitleBlit;
extern void *pause_PauseBlit;
extern void *pause_SureBlit;
extern void *pause_RumbleBlit;
extern s8 pause_gControllerRemoved;
extern s8 pause_gYes;
extern ScreenButton video_buttonInfo[EXTENT(12)];
extern s32 video_buttonsInited;
extern s8 video_optionValid[EXTENT(4)];
extern s8 video_gControllerRemoved;
extern AudioMenuRect video_menuRect;
extern char pak_error_title_format[EXTENT(16)];
extern char pakerr_newlines_no_controller[EXTENT(4)];
extern char pakerr_newlines_new_pak[EXTENT(4)];
extern char pakerr_newlines_inconsistent[EXTENT(4)];
extern char pakerr_newlines_full[EXTENT(4)];
extern char pakerr_newlines_bad_data[EXTENT(8)];
extern char pakerr_newlines_unknown[EXTENT(40)];
extern f32 dotireforce_idt_coef;
extern f32 dotireforce_idt_offset;
extern f32 dotireforce_side_drop;
extern f32 dotireforce_side_scale_mode2;
extern f32 dotireforce_side_scale_slope;
extern f32 dotireforce_side_scale_base;
extern ct_data gBssStart[EXTENT(288)];
extern volatile u32 frame_cycle_count;
extern s16 frame_start_msg;
extern AudioList pak_free_handles;
extern OSMesg text_lock_msg[EXTENT(1)];
extern Gfx *frame_dl;
extern OSMesgQueue si_lock_queue;
extern u8 *frame_dl_start;
extern PlayerTail gPlayers_save[EXTENT(3)];
extern s32 retraces_since_frame;
extern OSMesg si_lock_msg;
/* gOPPFuncs: declared by its files (src/blob/groups/hal_time/group.c: defines it) */
extern FrameRecord frame_records[EXTENT(2)];
extern FrameBuffer frame_color_buffers[EXTENT(1)];
extern s32 framebuffer_a;
extern s32 framebuffer_b;
extern struct Dynamic *frame_dynamic;
extern u8 *frame_matrix_cursor;
extern s32 video_texture_filter;
extern s32 video_color_dither;
extern s32 video_dither_filter;
extern s32 video_divot;
/* pc_audio_pool_fail_every: declared by its files (src/blob/groups/abort_game/GetCmd.c: defines it) */
/* pc_frame_dl: declared by its files (src/blob/groups/SetupCarColor/ctx_initsystem.c: defines it) */

/* Functions */
s64 __d_to_ll(f64 d);
u64 __d_to_ull(f64 d);
s64 __f_to_ll(f32 f);
u64 __f_to_ull(f32 f);
s64 __ll_div(s64 a0, s64 a1);
u64 __ll_lshift(u64 a0, u64 a1);
s64 __ll_mod(s64 a0, s64 a1);
u64 __ll_mul(u64 a0, u64 a1);
s64 __ll_rem(u64 a0, s64 a1);
s64 __ll_rshift(s64 a0, s64 a1);
f64 __ll_to_d(s64 s);
f32 __ll_to_f(s64 s);
s32 __osCheckId(struct OSPfs *pfs);
s32 __osCheckPackId(struct OSPfs *pfs, struct __OSPackId *temp);
u8 __osContAddressCrc(u16 addr);
u8 __osContDataCrc(u8 *data);
void __osContGetInitData(u8 *pattern, OSContStatus *data);
s32 __osContRamRead(OSMesgQueue *mq, s32 channel, u16 address, u8 *buffer);
s32 __osContRamWrite(OSMesgQueue *mq, s32 channel, u16 address, u8 *buffer, s32 force);
s32 __osGetId(struct OSPfs *pfs);
s32 __osIdCheckSum(u16 *ptr, u16 *csum, u16 *icsum);
void __osMakeMotorData(s32 channel, OSPifRam *mdata);
s32 __osMotorAccess(struct OSPfs *pfs, s32 flag);
void __osPackReadData(void);
void __osPackRequestData(u8 cmd);
s32 __osPfsCheckPages(struct OSPfs *pfs, struct __OSInodeCache *cache);
s32 __osPfsCheckRamArea(struct OSPfs *pfs);
s32 __osPfsDeclearPage(struct OSPfs *pfs, __OSInode *inode, s32 file_size_in_pages, s32 *first_page, u8 bank, s32 *decleared, s32 *last_page);
void __osPfsGetInitData(u8 *pattern, OSContStatus *data);
s32 __osPfsGetNextPage(struct OSPfs *pfs, u8 *bank, __OSInode *inode, __OSInodeUnit *page);
void __osPfsGetOneChannelData(s32 channel, OSContStatus *data);
s32 __osPfsGetStatus(OSMesgQueue *queue, s32 channel);
s32 __osPfsReleasePages(struct OSPfs *pfs, __OSInode *inode, u8 start_page, u8 bank, __OSInodeUnit *last_page);
void __osPfsRequestData(u8 cmd);
void __osPfsRequestOneChannel(s32 channel, u8 cmd);
s32 __osPfsRWInode(struct OSPfs *pfs, __OSInode *inode, u8 flag, u8 bank);
s32 __osPfsSelectBank(struct OSPfs *pfs, u8 bank);
s32 __osRepairPackId(struct OSPfs *pfs, struct __OSPackId *badid, struct __OSPackId *newid);
void __osSiCreateAccessQueue(void);
void __osSiGetAccess(void);
void __osSiRelAccess(void);
u16 __osSumcalc(u8 *ptr, s32 length);
void __scAppendList(PcSched *scheduler, struct OSScTask_s *task);
void __scExec(PcSched *scheduler, struct OSScTask_s *sp, struct OSScTask_s *dp);
s32 __scExecTask(PcSched *scheduler, struct OSScTask_s *task);
void __scHandlePreNMI(PcSched *scheduler);
void __scHandleRDP(PcSched *scheduler);
void __scHandleRetrace(PcSched *scheduler);
void __scHandleRSP(PcSched *scheduler);
void __scMain(void *arg);
void __scSchedule(PcSched *scheduler);
s32 __scScheduleCore(PcSched *scheduler, struct OSScTask_s **sp, struct OSScTask_s **dp, s32 availRCP);
void __standin2_highscore_UninitScreen(void);
u64 __ull_div(u64 a0, u64 a1);
void __ull_divremi(u64 *div, u64 *rem, u64 a2, u16 a3);
u64 __ull_rem(u64 a0, u64 a1);
u64 __ull_rshift(u64 a0, u64 a1);
f64 __ull_to_d(u64 u);
f32 __ull_to_f(u64 u);
void _BGLoadWave(s32 idx);
s32 _FileChanged(struct tPS_File *psfile);
void _StopRumble(s32 pak);
s32 acquire_decompress_lock(s32 block);
void AddCountdownTime(f32 seconds);
s32 ar_font_set(s32 font);
void ar_SetFont(s32 font);
void au_SetFont(s32 font);
void au_SetJust(s32 h, s32 v);
s8 au_validOption(s32 i);
s32 audio_user(s32 x, s32 y);
void AudPlaySong_TTL(s32 song, s32 loop);
void cart_free(void **address);
void cart_free_direct(void **address);
void ClearScreen(u16 color);
void cs_ctl10_DC(struct Cell *c);
void cs_draw1_DC(s32 row, s32 col);
void cs_draw3_DC(s32 row, s32 col);
void cs_drawT0_DC(s32 row, s32 col);
void cs_drawT1_DC(s32 row, s32 col);
void cs_drawT2_DC(s32 row, s32 col);
void cs_drawT3_DC(s32 row, s32 col);
void cs_wait11_DC(s32 row, s32 col);
void cs_wait12_DC(s32 row, s32 col);
void cs_wait13_DC(s32 row, s32 col);
void ct_DrawEllipseText(s16 x, s16 y, s16 w, void *text);
void DeallocateMemory_Init(u32 address);
void deflate_init(struct z_stream_s *strm, s32 level, s32 windowBits, s32 memLevel);
s32 font_set_8AB0(s32 font);
s32 font_set_slp(s32 font);
void game_thread_entry(s32 arg);
s32 getB6_6874(s32 ctl);
s32 getB6_6B3C(s32 ctl);
s32 getB6_BC84(s32 ctl);
f32 GetCountdownTimeRemaining(void);
void gh_SetFont(s32 font);
s32 ghost_blocks_from_size(s32 size);
void ghost_free_start(struct GhostHeader *header);
void ghost_heap_lock(void);
void ghost_heap_unlock(void);
char *GhostDirName(struct MenuData *header);
void guLookAt(u32 (*m)[4][4], f32 xEye, f32 yEye, f32 zEye, f32 xAt, f32 yAt, f32 zAt, f32 xUp, f32 yUp, f32 zUp);
void guLookAtF(f32 (*mf)[4], f32 xEye, f32 yEye, f32 zEye, f32 xAt, f32 yAt, f32 zAt, f32 xUp, f32 yUp, f32 zUp);
void guMtxF2L(f32 (*mf)[4], u32 (*m)[4][4]);
void guMtxIdent(u16 *mtx);
void guMtxIdentF(f32 (*mtx)[4]);
void guMtxL2F(f32 (*mf)[4], u32 (*m)[4][4]);
void guOrtho(u32 (*m)[4][4], f32 l, f32 r, f32 b, f32 t, f32 n, f32 f, f32 scale);
void guOrthoF(f32 (*mf)[4], f32 l, f32 r, f32 b, f32 t, f32 n, f32 f, f32 scale);
void guPerspective(u32 (*m)[4][4], u16 *perspNorm, f32 fovy, f32 aspect, f32 near, f32 far, f32 scale);
void guPerspectiveF(f32 (*mf)[4], u16 *perspNorm, f32 fovy, f32 aspect, f32 near, f32 far, f32 scale);
void HAL_CallOncePerFrameFuncs(void);
u8 HAL_GetFontAscent_Speed(void);
u8 HAL_GetFontDescent_Speed(void);
void HAL_InitText_Init(void);
void HAL_InitTime(void);
void HAL_InstallOncePerFrameFunc(intptr_t func);
s32 HAL_IntLog2(s32 value);
void HAL_SetFont_8AB0(s32 font);
void heap_free_locked(u32 address);
s32 Hidden_9F54(struct Blit *blt, s32 hide);
void hs_SetFont(s32 font);
void idle_thread_entry(void *argument);
void init_decompress_lock(void);
void InitGfx(void);
void InitGraphics(void);
void InitializeTimer(void);
void InitSystem(void);
s32 item_disable_B9C8(struct MenuItem *item, s32 off);
s32 item_refresh_B5D0(struct MenuItem *item);
s32 item_refresh_B7FC(struct MenuItem *item);
void leEndLoadingEffect(void);
void leStartLoadingEffect(void);
f32 lsq(f32 *v);
void main_thread_entry(void *arg);
void MainEndFrame(void);
void MainPreNMI(void);
void mem_free(u32 address);
void op_calcOptionMenuMaxSize(void);
void op_initButtons(void);
void osContGetQuery(OSContStatus *data);
void osContGetReadData(struct OSContPad *data);
s32 osContInit(OSMesgQueue *mq, u8 *bitpattern, OSContStatus *data);
s32 osContStartQuery(OSMesgQueue *mq);
s32 osContStartReadData(OSMesgQueue *mq);
void osCreateScheduler(PcSched *scheduler, void *stack, OSPri priority, u8 mode, u8 numFields);
s32 osMotorInit(OSMesgQueue *mq, struct OSPfs *pfs, s32 channel);
s32 osPfsAllocateFile(struct OSPfs *pfs, u16 company_code, u32 game_code, u8 *game_name, u8 *ext_name, s32 file_size_in_bytes, s32 *file_no);
s32 osPfsChecker(struct OSPfs *pfs);
s32 osPfsDeleteFile(struct OSPfs *pfs, u16 company_code, u32 game_code, u8 *game_name, u8 *ext_name);
s32 osPfsFileState(struct OSPfs *pfs, s32 file_no, struct OSPfsState *state);
s32 osPfsFindFile(struct OSPfs *pfs, u16 company_code, u32 game_code, u8 *game_name, u8 *ext_name, s32 *file_no);
s32 osPfsFreeBlocks(struct OSPfs *pfs, s32 *bytes_not_used);
s32 osPfsInitPak(OSMesgQueue *queue, struct OSPfs *pfs, s32 channel);
s32 osPfsIsPlug(OSMesgQueue *mq, u8 *pattern);
s32 osPfsReadWriteFile(struct OSPfs *pfs, s32 file_no, u8 flag, s32 offset, s32 size_in_bytes, u8 *data_buffer);
s32 osPfsRepairId(struct OSPfs *pfs);
void osScAddClient(PcSched *scheduler, struct OSScClient_s *client, OSMesgQueue *queue);
void osSyncPrintf(s32 fmt, s32 value1, s32 value2, s32 value3);
void other_user(s32 x);
s32 pak_slot_b1(s32 pak);
s32 pak_slot_b5(s32 pak);
s32 pak_slot_b6(s32 pak);
void paksys_lock(void);
void paksys_queue_init(void);
void PauseTimer(void);
void pc_Check2WheelSide(s32 idx);
void pc_CheckAirTime(s32 idx);
void pc_CheckMulti(s32 idx);
void pc_ContinueSkid(struct WheelSlot *ns, s16 slot, u32 tire, u8 *color);
f32 *pc_grid_start(struct tModelData *m, s32 what);
f32 *pc_pole_row(s32 place);
void pc_ReadShifter(struct CarState *car, struct InputRecord *player);
s32 pc_scenery_unplaced(s32 idx, void *node_obj, s32 absolute);
void pc_ValidateEmitter(struct CamSlot *c);
void pdf_dealloc(u32 address);
void pdf_free_blocks(s32 pak);
void pdf_heap_lock(void);
void pdf_heap_unlock(void);
void pdf_release(struct PakHandle *fileh);
void pfs_lock(void);
void pfs_queue_init(void);
s32 PlayGameSound_Countdown(s32 fxid, s32 subId, s32 replace, u8 priority);
void PlayPauseSfx(void);
void PlaySelectSfx(void);
void post_type1(void);
void post_type7(void);
void prpf_nop(void);
void ps_SetFont(s32 font);
void ps_updatePauseStateCreate(void);
void release_decompress_lock(void);
void ResumeTimer(void);
void rmonPrintf(const char *fmt, ...);
void rs_DrawEllipseText(s16 x, s16 y, s16 w, char *text);
void SaveFlush_TTL(s32 x);
void SetCountdownTimer(f32 seconds);
void SetModelPalette(s32 id, struct Resource24 *descriptor);
void SetPausePlayer(s32 player);
void slot_forget(struct Obj **node);
void sound_list_init(struct List *list, s8 handle, s8 dbl);
f32 TB_GetFlashTextCycle_8AB0(void);
s16 TB_GetFontHeight_Speed(void);
void TB_SetJust_6B3C(u32 first, u32 second);
void tire_extend(struct Visual *v, struct Model2056 *model);
void tire_sound_off(struct Model2056 *model);
void tire_sound_on(struct Model2056 *model);
void update_drone_models(void);
void update_model(struct ModelRecord *car);
void UpdateFrameTimer(void);
void UpdateGraphics(s32 drawBlits, s32 updateEnvirons);
void us_uninitButtons(void);
void vi_SetFont(s32 font);
void video_apply_pending_mode(void);
void video_build_adjusted_mode(void);
void video_get_default_offset(s32 *x, s32 *y);
void video_get_offset(s32 *x, s32 *y);
s32 video_mode_base_index(void);
void video_restore_base_mode(void);
void video_set_mode(s32 tv, s32 mode, s32 width, s32 height);
void video_set_offset(s32 x, s32 y);
ArbEntry *view_bind(s32 k, void *vp, s16 *r);
void vs_drawColorBars(void);
void vs_SetFont(s32 font);
void vs_setWidth(s16 *width);
s8 vs_validOption(s32 item);
void zcfree(void *ptr);
void zfree_locked(void *ptr);
void ZOID_ShowPoly_bg(struct Poly *poly);

#endif /* GAME_SYSTEM_H */
