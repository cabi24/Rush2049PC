# The system: start-up, threads, the frame loop, game states, memory, loading, input

This page is for a programmer new to the game's lower layers: how the game starts, which threads run what, how a
frame is started, built and shown, how the game moves between its screens and the race, how memory is allocated,
how assets ("waves") are loaded, and how the controllers become a car's steering. It uses the tree's names as of
2026-11-08 (PC-only wave 2b). [boot.md](boot.md) maps the boot segment file by file; [README.md](README.md) has the
whole game's map. Where a function is MAT3's (the user's GameCube decomp of the same game), the MAT3 file is
named; the rest is the N64's own.

The headers: [include/game/system.h](../../include/game/system.h) (the frame's buffers) with
[threads.h](../../include/game/threads.h) (the threads' ids and priorities, the main thread's messages, which the
boot code includes too; the game states are race.h's `GS_*`), [memory.h](../../include/game/memory.h),
[input.h](../../include/game/input.h) (the game's buttons, `BUTTON_*`), [heap.h](../../include/game/heap.h) (the
heap's own headers, for the heap's files only), and these sections of
[types.h](../../include/game/types.h): system (`PcSched`, `FrameRecord`, `PcScTaskShape` ...), memory
(`anon_loaded_ready_14`, the wave instance; `WadHandle`, `WadEntry`, `WadFileHeader`), input (`ControllerState`,
`tPadState`, the `CONTROL_*` indices).

## 1. Start-up and the threads

| Thread | id, priority | Body | What it does |
|---|---|---|---|
| idle | 1, 2 then 0 | `idle_thread_entry` (src/rom/boot.c) | starts the PI manager (150) and the main thread, then idles |
| main | 6, 4 | `main_thread_entry` (boot.c) | start-up, then serves the scheduler (`process_scheduler_message`, mainloop.c) |
| scheduler | 4 (its own id), 12 | `__scMain` (src/rom/scheduler.c) | retraces, RSP and RDP interrupts, task scheduling |
| background loader | 8, 3 | `BGLoadWaveMain` (waves/group.c) | reads a wave when `_BGLoadWave` asks |
| model | 5, 7 | `ModelThread` (modelthread/modelthread.c) | each retrace: the controllers (`ControllerUpdate`), then the car simulation step |
| game | 7, 5 | `game_thread_entry` (boot.c) | `GameLoop` forever, one frame a call |
| audio manager | 10, 10 | `audio_manager_thread_entry` (audiomgr/audiomgr_thread.c) | the sound commands and the periodic sound pass |
| rumble, sample DMA, audio retrace | 2, 3, 9 | the audio and rumble files (other areas) | |

`main` (the N64's entry) starts libultra and the idle thread. `main_thread_entry` sets the FPU mode
(`FPCSR_GAME`), makes the queues (`rom_dma_queue`, `unzip_dma_queue`, `main_thread_queue`, `game_thread_queue`),
the scheduler (`osCreateScheduler`: the video mode from `video_mode_base_index`), inflates the game image from the
cartridge (`unzipdata_from_rom`; on the PC the data loader fills the image's blocks), subscribes to the scheduler, starts
the background loader and the model thread, runs **`InitSystem`** (heap_release/initsystem.c), shows the boot
screen (`BlitInit`), starts the game thread, waits for its first end of frame and then serves the scheduler
forever.

`InitSystem`: the heap (`InitMemory(50, 10)`: 50 master slots, 10 more each time they run out), the saves
(`SaveInit`), the graphics (`_InitGfx_Init`: the video mode, the z-buffer at the heap's bottom and the two frame
buffers at its top, each frame record's command buffer; the scene objects, the blits, the cameras), the sound
(`AudioInit(32, 16, 16, 5, 64, ...)`, and without an Expansion Pak the 3D tables freed again), the controls
(`InitRushControls`), the options, the frame timer, the text.

On the PC every thread is a host thread; port/ultra implements libultra underneath (message queues, the VI, the
PI, the SI with emulated controllers and paks, the RSP tasks as the renderer and the audio interpreter:
docs/port/ULTRA.md). The game's priorities are kept as an order.

## 2. The frame

Three threads take part:

1. **The scheduler** (`__scMain`) gets the VI's retrace (`VIDEO_MSG`), counts it (`gScheduler.retraceCount`, which
   the game reads as `scheduler_retrace_count`: the game's clock), swaps in a finished frame's buffer when enough
   retraces have passed (`PC_SWAP_RETRACES`), and sends every client a retrace message (`OS_SC_RETRACE_MSG`). It runs
   the tasks the game sends on `scheduler_task_queue`: audio first (`doAudio`, `audioListHead`), then graphics
   (`gfxListHead`), asking a graphics task to yield for audio.
2. **The main thread** (`process_scheduler_message`, groups/mainloop/mainloop.c): on a retrace, if the screen shows
   the last frame, nothing is being built or ended, and fewer than two frames are in flight, it starts a frame
   (`StartGraphics`) and wakes the game thread (`frame_start_msg`, `GAME_MSG_START_FRAME`). When the game thread
   asks to end a frame (`GAME_MSG_END_FRAME`, from `UpdateFrame`) it ends it (`MainEndFrame` -> `EndGraphics`, which
   sends the RSP task) and switches frame record (`frame_record_index`). A finished task (`OS_SC_DONE_MSG`) frees a
   slot (`gfx_tasks_pending`). Pre-NMI (`OS_SC_PRE_NMI_MSG`, the reset button): `MainPreNMI` silences and blacks
   everything.
3. **The game thread** (`GameLoop`, src/blob/groups/front_end/GameLoop.c, the race area's) begins each call with
   `UpdateFrame`: it asks the main thread to end the frame it built, waits for the next frame's start, then
   measures the frame (`UpdateFrameTimer`, src/rom/timer.c: `gLastFrameCount` retraces, at most 5, `gFrameSeconds`;
   the game time `gSeconds`, stopped while paused; the countdown), reads the controls (`UpdateRushControls`) and
   runs the once-a-frame functions (`HAL_CallOncePerFrameFuncs`). Then it updates the game state (section 3), the
   screen or the race, and builds the frame's display list into the frame record's command buffer
   (`frame_records[i].commands`, `pc_frame_dl`: dynamic data, matrices from +1728, the display list from
   `PC_GLIST_OFFSET`).

The model thread runs beside them at each retrace: `ControllerUpdate` (section 6), then the car simulation step.

## 3. The game states

`gGameState` is one bit of race.h's `GS_*` (MAT3 rush_flags.h's names); `setGameState` asks for another
(`gNewGameState`), and `updateGameState` (groups/state/state.c), at the start of each `GameLoop`, leaves the old one
(`uninitGameState`: its waves go) and enters the new one (`initGameState`). Entering a state chooses which runtime
image is in (`LoadFrontEndOverlay`: image A, the front end; `load_extra_overlay`: image B, the battle, stunt and
obstacle extras; both share one heap block), which waves are loaded (`InitWaves` frees the unlocked ones, `LoadWave`
loads), the sound set (`AudioInit` with fewer voices for a race without an Expansion Pak), and which controllers count
(`SetControllerActive`, `SetControllerStickEdges`).

- The boot and the front end (`GS_FRONTEND_MASK`): `GS_BOOT` -> `GS_LICENSE` -> `GS_WARNING` -> `GS_TITLE`, the
  attract `GS_DEMO`, then the menus: `GS_START`, `GS_PLAYER`, `GS_MODE`, `GS_TRACK`, `GS_CAR`, `GS_CIRCUIT`,
  `GS_OPTIONS`, `GS_CONTROLS`, `GS_AUDIO`, `GS_VIDEO`, `GS_RECORDS`, `GS_PAK`, `GS_CHEAT`. Each is a screen of image
  A (`updateFrontEnd`).
- The race (`GS_IN_GAME_MASK`): `GS_GAME_INIT` (the track, the cars, image B) -> `GS_PREPLAY` (until loaded) ->
  `GS_COUNTDOWN` -> `GS_GO` (the race clock and the music) -> `GS_RACING` -> `GS_END_GAME` (`EndGame`) ->
  `GS_HIGH_SCORE` or `GS_GAME_OVER`, back to the front end. `GS_RACE_MASK` is the race from `GS_GAME_INIT` to
  `GS_RACING`, `GS_ON_TRACK_MASK` the countdown and the race.
- `GS_DEBUG` (0x10000000): the start menu's debug row leads there; nothing updates it. 0x20000000 has no name
  (initGameState loads wave 55 for it; nothing sets it).

## 4. Memory: the heap

The heap (MAT3 memsys.c; include/game/heap.h) carves the memory the PC gives it (`pc_heap_memory`: on the N64
everything above the game's BSS up to the end of RDRAM, which is still where it lies) into **chunks**, each a
32-byte `tMemoryChunk` header (next, prev, master, size, used, internal, locked, a magic byte) then its data, linked
both ways in address order; a 64-byte `tHeap` header starts each heap. The headers hold host pointers in the N64's
sizes, so what fits is the original game's ([NATIVE.md](../port/NATIVE.md) step 6). `gMainHeap` is the main heap
(`locate_main_heap`); `create_heap` makes a heap inside a chunk of another (FindHeap finds the innermost heap of an
address), `destroy_heap` gives it back. The runtime images' block is the N64's: `PC_RUNTIME_IMAGE_OFFSET` bytes past
the main heap's header (`runtime_image_block`, heap_release/overlay_loader.c).

- `AllocateMemoryHeap(heap, size)` takes the lowest free chunk that fits (size rounded to 32), splitting off the
  rest when it is at least 64 bytes; `AllocateMemoryHeapHigh` takes from the top (the frame buffers);
  `AllocateHandleHeap` allocates behind a **master slot** and returns the slot's address (a handle), so that the
  data may move: `ClearMemoryHeap` (heap_release/ClearMemoryHeap.c, another area's file) frees the purgeable
  chunks and compacts, updating the slots. `DeallocateMemory(ptr)` frees and merges with free neighbours;
  `ReallocateMemory` resizes in place. Every function takes and returns pointers (MAT3's `void *`); one of a
  handle's master slot names the handle's chunk.
- A chunk's lock count (`lock_memory`, `unlock_memory`, `PurgeableMemory`) keeps compaction from moving it.
  `GetPtrSize` is a chunk's size; `LargestAvailableMemoryHeapChunk` the largest free one.
- Every entry point takes the heap lock (`heap_lock_queue`, a one-message queue); `AllocateMemory` and
  `DeallocateMemoryInternal` are the unlocked cores.

Static lists (statll.c: `InitStatLL`, `GetStatLLE`, `FreeStatLLE`, `ClearStatLL`) are pools of fixed-size records
from an array; linked lists (ll.c: `InitLL`, `InsertLLE`, `RemoveLLE`) link records the caller owns, singly or
doubly, directly or through handles (the header is `AudioList`, MAT3's tDLList: `void *head, *tail`, which a walk
takes into its own record pointer; `PlayerFileList` and `SfxQueue` are the same header with words for the data
loader); ordered lists (bstree.c: `InitBST`, `InsertBSTE`, `WalkBST`) are the sound
manager's priority tree.

## 5. Loading: waves and WADs

A **wave** is one of the game's asset files (a model WAD, a font, a sound bank, a track ...), stored on the
cartridge raw, LZSS-compressed or deflated (`wave_storage_kinds`, `wave_rom_offsets`, `gWaveSizes`). It is loaded
into one of 64 **wave instances** (`gWaveInstance`, `anon_loaded_ready_14`: loaded, ready, locked, model, mirrored,
handle, wave, size, ptr, lastPtr):

- `LoadWave(wave, loadAgain, loadBackground, handle, heap)` (waves/loadwave.c) returns the instance that already
  holds the wave, or a new one (`NewWaveInstance`: a heap block, or a handle) read now (`ReadWave`) or by the
  background loader (`_BGLoadWave` -> `BGLoadWaveMain`, which waits on `bg_load_request_queue` and signals
  `bg_load_idle_queue`).
- `ReadWave` (waves/waves.c) DMAs or decompresses the file (`wave_rom_read`, `lzss_decompress`,
  `inflate_decompress`, one at a time under the decompression lock), has the PC layer convert it
  (`pc_wave_loaded`: port/assets), then `SetupWaveModel`: for a model WAD the model, texture and palette tables
  (`gRomObjDefPtrs`, `gTexDefPtrs`, `gPalDefPtrs`; a track's target paths `gTargetPaths`) point at the WAD's records,
  which its converter built beside the file (`pc_wad_records`: OBHD, TXHD, PLHD, PTHD; on the N64 `MBSetupWad` and
  `MBGetFromWad` rebased the file's own records in place), the texture loads (TXLD) are relocated to the texels
  (IMAG), and each model's display lists relocated (`prepare_model_dl`, keeping the sections for the player count
  and the track's direction) and mirrored for a mirrored track (`MirrorWaveInstance`). The converters' records go
  with the block when the heap frees it, and follow it when the heap moves it (the font data, a handle:
  `pc_heap_block_moved`; port/assets/records.c).
- `InitWaves` frees every unlocked wave between states; `LockWaveInstance` keeps one (the fonts); `UnloadWaveAll`
  frees every instance of a wave; `ReplaceWave` reads another wave into an instance's block.

## 6. Input

1. **`ControllerUpdate`** (ControllerUpdate.c), on the model thread each retrace, reads the four pads over the serial
   bus (libultra's `osContStartReadData`; a status query every 30 calls notes paks plugged in or out:
   `pak_poll_inserted`), maps the N64 buttons to the game's button bits (`n64_button_map`) into `gControllerData`
   (`ControllerState`), and calls `ControlIRQ`.
2. **`ControlIRQ`** (controlsys/group.c, MAT3 controlsys.c) keeps the held buttons (`gNewLevels`), the presses since
   the game last looked (`gNewEdges`), the raw sticks with a calibration that grows with what the stick reaches
   (`gContStickRange`), and this retrace's own state for the car (`gRealLevels`, `gRealEdges`, `gRealStick`). Where
   the stick drives the menus (`gStickEdges`) a pushed stick adds a d-pad bit.
3. **`UpdateControls`**, on the game thread each frame, makes the frame's input: held, pressed, auto-repeated
   (every 0.2 s) buttons and the scaled sticks per controller (`gContLevels`, `gContEdges`, `gAutoEdges`,
   `gContStick`) and over the active controllers (`gActiveLevels` ...). `UpdateRushControls` copies each player's
   controller's into their record (`gPlayers`, `tPadState`).
4. **`UpdateModelControls`** (model_controls_input/group.c, MAT3 controls.c) turns each player's record and this
   retrace's state into their car's inputs (`tModelData.mainin`): `ReadGasAndBrake`, `ReadSteeringWheel`,
   `pc_ReadShifter`, the wings and the abort, by the player's control layout (`tPadState.controls`, made by
   `SetControls` from `gNewControls` and `gControlTable`).

The pak error dialogs (pakerr/group.c) and the pause, audio, video and controls screens' text in this area use the
same input.

## 7. Files

| Part | Files |
|---|---|
| Boot, threads, timer, video, scheduler | src/rom/boot.c, timer.c, video_mode.c, scheduler.c, decompress.c |
| libultra, libc | src/rom/libultra/*.c (controllers, the Controller Pak file system, the Rumble Pak, matrices, 64-bit helpers), src/rom/libc/*.c |
| Frame loop, states, start-up | groups/mainloop/mainloop.c, groups/state/state.c, groups/heap_release/initsystem.c, groups/heap_release/initsystem_start.c, groups/initgraphics/*.c (not called on the PC), UpdateGraphics.c (not called), ClearScreen.c, ZOID_SplitScreen.c, groups/hal_time/group.c, HAL_CallOncePerFrameFuncs.c |
| Heap | groups/memsys/group.c, AllocateMemoryHeapHigh.c, FindHeap.c, groups/heap_release/group.c, groups/destroy_heap, groups/largest_heap_chunk, groups/purgeable_memory, groups/pointer_counters (and FindChunk.c, ClearMemoryHeap.c, alloc_at.c, heap_move_block_low.c, memsys_free_total: other areas' files) |
| Lists and trees | InitStatLL.c, GetStatLLE.c, FreeStatLLE.c, ClearStatLL.c, InsertLLE.c, RemoveLLE.c, groups/save/save.c (InitLL, ClearLL), InitBST.c, ClearBST.c, InsertBSTE.c, InsertBSTEInternal.c, WalkBST.c, WalkBSTInternal.c |
| Waves and WADs | InitWaves.c, FindWaveInstance.c, ModelActive.c, groups/waves/*.c, groups/UnloadWaveAll, groups/slot_sound/*.c, groups/state_waves/FindWaveInstanceInternal.c, groups/heap_release/UnloadWaveInstance.c, MB*Wad.c, prepare_model_dl.c, display_list_traverse.c, mirror_vertex_texcoords.c |
| Compression | groups/heap_release/zip.c, groups/zlib_deflate/group.c, groups/heap_release/zcfree.c (zlib 1.0.4's deflate, for the ghosts) |
| Input | ControllerUpdate.c, groups/controlsys/group.c, InitControlSys.c, ClearControls.c, Get/SetController*, InitRushControls.c, SetDefaultControls.c, SetControls.c, UpdateRushControls.c, ClearRushControls.c, FindPlayerNum.c, groups/model_controls_input/group.c, pak_poll_inserted.c, groups/pakerr/group.c |

## 8. What is still N64-shaped (for wave 3)

- **The heap** is native since [NATIVE.md](../port/NATIVE.md) step 6 (its headers, its interface and the image block),
  and its memory a block of PC memory since step 12 (`pc_heap_memory`: osMemSize less the N64's room below the heap,
  its first byte as far past a 64 KiB boundary as on the N64), with the N64's accounting; nothing kept in its blocks
  holds a 32-bit address since step 11. The WAD's records and the
  font's letters are native beside the file since step 7, its display lists since step 10. Every heap file still
  declares its own `osRecvMesg`/`osSendMesg` and views `heap_lock_queue` its own way.
- **The WADs' display lists and texture loads** are native beside the file since NATIVE.md step 10: the WAD converter
  builds them command for command in the PC's form (`Gfx`, w1 a host pointer where it carries an address) and the
  vertices they load in the host's byte order; `prepare_model_dl` keeps the race's sections, and
  `display_list_traverse`, `mirror_vertex_texcoords` and the texture animation read and write them as they are. The
  texels and the palettes' colours stay in the file in the N64's formats.
- **The frame**: `frame_records`' tasks are `PcScTaskShape`, the N64 OSScTask in host form; the frame buffers and
  the z-buffer are pointers into the heap (`framebuffer_a`, `zbuffer`); `frame_color_buffers`
  is a label 0xC8 bytes into `frame_records`.
- **The scheduler's** queues and thread keep N64-sized storage at their N64 offsets in `PcSched`, because the game
  takes their addresses (`scheduler_task_queue` ...) and reads the retrace count at 0x27C.
- **Data declared with the wrong width** in input.h: `gNewLevels` is one s32 to the data loader, MAT3's volatile
  u32[4] in controlsys/group.c's view (the loader's size for the symbol would change). `gNewStick` (volatile
  s8[4][2]), `gNewEdges`, `gContStickRange`, `gAutoEdgeTimes` (u32[4][32]) and the frame's words (`gContLevels` ...
  u32, `gContStick` f32[4][2]) have MAT3's types since wave 2c.
- **gPlayers** has no shared declaration: each file declares it with its own type (InitOptions.c defines it).
  `tPadState` (types.h) is the record; the 67 files that read it could use it once the header declares it.
- Dead code the PC links but never calls: `InitGraphics`, `InitGfx`, `UpdateGraphics`, `unload_start_overlay`,
  `pak_set_error_func`, `pak_shutdown`, `audio_user`, the ordered list's `InitBST`,
  `ClearBST`, `WalkBST` (audiomgr_thread.c has its own clear and walk because a test links it without them).
