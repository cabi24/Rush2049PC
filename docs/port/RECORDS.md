# One type per record (wave 4)

Every record the game shares between files has one declaration, in `include/game`, with every field the game uses
named and typed. A file includes `game/game.h` and uses that declaration. It doesn't declare its own copy that names
only the fields it touches and skips the rest with byte arrays.

The headers before this wave declared each datum once (PCONLY.md 2). Many records still had a different struct in each
file that reads them: the physics model, for example, had 73 declarations across the files that use it. A file that
read a datum with a different type said so with a `VIEW()` line ([view.h](../../include/game/view.h)). Those copies
had to agree on the layout, and `view_check.py`, `view_audit.py` and `layout_check.py`
([LAYOUT.md](LAYOUT.md)) checked that they did. With one declaration there is nothing to disagree.

## 1. The rules

- A record shared by two or more files is declared once, in the header of its subsystem. A record that only one file
  uses may stay in that file, declared whole.
- MAT3's names are preferred for records and fields. A field that no code reads keeps its place under a name that
  says so.
- Comments describe the record as the PC has it. Offsets are given only where they belong to a file format: a save,
  a ROM file the loader converts, or a network message.
- A file that reads a field as another type does so in its code, through a union member or a commented conversion,
  not through a second struct.

## 2. The car's physics

### 2.1 The model record

(Agent A1: the model record, MAT3's MODELDAT.)

### 2.2 The car's descriptor, its tune, its tires and the paths

(Agent A2: the physics descriptor, the tune, the tires, the reckoning, resurrection, the drones' paths.)

## 3. The game car, the scene and drawing

### 3.1 The game car

(Agent B1: the game car, MAT3's CAR_DATA.)

### 3.2 The scene

(Agent B2: visuals, matrices and vectors, windows, scene objects, polygons, skids, blasts, smoke, texture swaps.)

### 3.3 Blits and colours

(Agent B3: the blits and the one colour type.)

## 4. Players, saves, menus and sound

### 4.1 Players and their input

(Agent C1: the players, their pads and input records.)

### 4.2 Profiles, records and ghosts

(Agent C2: profiles, records, ghosts, the pak's files.)

### 4.3 Menus, battles and circuits

(Agent C3: the menus' buttons and items, battles, circuits.)

### 4.4 Sound

(Agent C4: voices, channels, speech, sequences, sound resources.)

## 5. The OS records

(Agent D: the message queues, the pak file system, the scheduler's clients, zlib's state.)

### 5.2 Threads, queues and the scheduler (agent D2)

The game's threads, queues, locks and events are `game/threads.h`'s API now, and its OS records are declared once
([ULTRA.md](ULTRA.md) 11).

| Record | Declared in | What it holds | Its other tags and copies (gone) |
|---|---|---|---|
| `OSMesgQueue` | types.h (libultra's section; other records embed it) | a message queue: port/ultra keeps its messages and waiting threads under its address; libultra's fields, unused | 44 files' own: `struct OSMesgQueue` in 13 (InitControlSys.c, init_controllers.c, PS_RumbleStop.c, InitWaves.c, ControllerUpdate.c, waves.c, loadwave.c, audiomgr_thread.c, AudioInit.c, modelthread.c, paksysdc/group.c, PS_ReadPartialFile.c, pak_file_unchanged.c), anonymous 24-byte stand-ins (`s32 pad[6]`, `u8 opaque[0x18]`, `u32 opaque[6]`) or incomplete tags in 31 more; types.h's `OSMesgQueueBytes`, `OSMesgQueueWords` |
| `OSThread` | types.h | a thread: port/ultra runs it on a host thread, keyed by its address; libultra's fields, unused | 13 files' own (`OSThreadView`, `OSThread_s`, `u8 opaque[0x1B0]`, a 0x1B0-byte word array), sample_dma_thread's `__OSThreadContext` copy; types.h's `OSThreadBytes`; `u8[432]` for three threads |
| `OSScMsg` | threads.h | a scheduler message: its type (`OS_SC_RETRACE_MSG`, `_DONE_MSG`, `_PRE_NMI_MSG`, `GAME_MSG_END_FRAME`) | modelthread.c's; UpdateFrame.c's `SchedMsg`; boot.c's `MainThreadSchedulerMessage`; PcSched's `s16 state`/`priority` and pads, FrameRecord's `doneMsgType` + `doneMsgRest[30]`, EndGraphics.c's `doneMsg[0x20]` bytes |
| `OSScClient` | threads.h | a scheduler client: its queue, in the scheduler's list | `struct OSScClient` in audiomgr_thread.c and modelthread.c, anonymous copies in rumble/group.c and audio_retrace_thread_entry.c (all with `msgQ`), m2c_types.h's |
| `OSScTask` | threads.h | a task for the RSP and RDP: its list link, state and flags, frame buffer, the RSP task in place (`OSTask_t list`), whom to tell when done | m2c_types.h's (the RSP task flattened into `pad18`... fields), types.h's `PcScTaskShape`, EndGraphics.c's `Frame` head, musyx_submit_audio_task.c's |
| `PcSched` | threads.h | the scheduler (`gScheduler`): libultra's `OSSched` with the PC's types | types.h's and scheduler.c's (N64 offsets: `pad02`, `pad22`, the queues as `u8[24]`, `padB0[432]`, `pad260[28]`), m2c_types.h's `OSSched`, boot.c's `MainThreadScheduler`; the asm aliases `scheduler_interrupt_queue`, `scheduler_task_queue`, `scheduler_thread`, `scheduler_retrace_count`, `scheduler_current_rsp_task` |
| `FrameRecord` | system.h | a frame: its task, its command buffer, its done message, its frame buffer | EndGraphics.c's `Frame` view (`VIEW(Frame[], frame_records)`) |

**Fields that disagreed.** The scheduler's `curRSPTask` was at 0x274 in m2c_types.h's copy and at 0x2B0 in the PC's;
the clients' copies named the queue `msgQ`, the scheduler `msgQueue`; the task's done queue sat at `msgQ` (N64 0x50)
in the game's copies and m2c's flattened copy, with its `output_buff_size` an `s32 *` read as `*(u64 *)`. One
declaration settles them as libultra has them: `OSScTask.list.output_buff_size` is a `u64 *`, the queue `msgQueue`.
`PcSched` now has libultra's real order (the two messages, the event queue, the task queue, the thread, the lists, the
retrace count), 648 bytes on the N64 and 792 on the PC, where it kept N64 offsets the game no longer reads.

**Data typed.** Twelve data were byte stand-ins for an OS record: the queues `model_thread_queue`,
`audio_retrace_queue`, `rumble_queue`, `bg_load_request_queue`; the threads `audio_manager_thread`,
`audio_retrace_thread`, `sample_dma_thread`, `rumble_thread`; the message slots `heap_lock_msg`,
`bg_load_request_messages`, `sample_dma_pi_msg`, `musyx_task_done_msg`. Each is its record now, and moved from
N64-layout storage to own storage: the one change to the data's layout in this part, harmless because nothing reads
or writes these bytes (port/ultra keys queues and threads by address and keeps the messages itself).

**VIEW lines removed.** `EndGraphics.c`'s `VIEW(Frame[], frame_records)` (it builds the frame's task in
`frame_records[n].task`) and `AudioInit.c`'s `VIEW(OSThread, audio_manager_thread)` (the datum is an `OSThread`).

**Numbers, in the game's files (src/blob, src/ovl_*, nonmatching, src/rom's boot.c and scheduler.c).** Local
declarations of the OS records: 72 (44 queues, 14 threads, 4 clients, 3 messages, 2 tasks, scheduler.c's and
boot.c's schedulers, m2c_types.h's 3) to 0; duplicated tags (`struct OSMesgQueue` 13, `struct OSScClient` 2)
to 0; VIEW lines 2 to 0; pads and byte stand-ins in the scheduler's, tasks' and frames' copies 35 to 0; local
prototypes of libultra's thread and queue calls 176 lines in 86 files to 0; lines calling them 474 to 0 (the
libraries' own C keeps them).

**Units not `same` under objsame**: the 90 that call the API (Debug: the `static inline` functions are local copies
in the unit; RelWithDebInfo: inlined into the same libultra calls), the scheduler's and the boot's, and the 6 that
read `gScheduler.retraceCount` (its offset moved): get_input_idle_seconds.c, animDot.c, animCountdownTimer.c,
pakerr/group.c, save/group.c, SetPakError.c. ULTRA.md 11.4 has the proof.

## 6. The proof

(The lead's, after the merge.)
