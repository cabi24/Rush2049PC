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

Agent C4: the sound manager's records (MAT3 audiomgr.c, audiosfx.c), the MusyX library's synth voice, and the records
the library and the game share.

**The sound manager**: [audiomgr.h](../../include/game/audiomgr.h), which the files that read the records include
after game.h.

| Record | What it holds | Its other tags (files) |
|---|---|---|
| `tFxSequence` | an fx tag: one sound the game asked for, on one of the three tag lists | `Voice` (save/AudioInit.c); `Effect` (audiomgr/ProcessEmitters.c, whose `kind`, `state`, `delay`, `persistent`, `voice` are the tag's `state`, `paused`, `loop`, `numCmds`, `sysId`); audiomgr_thread.c's own copy |
| `tAudioSequence` | a timed sequence of up to four effects (MAT3's unnamed anon_ll_id_3c) | `Chan` (AudioInit.c), `Event` (stop_sequence.c), `Sequence` (NewSequence.c), `SequenceHead` (free_sequence/group.c), `anon_ll_id_3c` (audiomgr_thread.c) |
| `tSortedFxTag` | a node of the priority pass's tree (MAT3's anon_te_tag_c) | `anon_te_tag_c` (audiomgr_thread.c); AudioInit sized its pool with a literal 24 |
| `tAudioMsg`, `tAudioMsgHeader` | a message to the audio manager thread | audiomgr_thread.c's copy, `Slot24` (AudioInit.c), `MessageNode` (abort_game/group.c), `ShutdownMsg` (heap_release/audio_shutdown_posts.c), `SceneMsg` (gameover/gameover.c) |
| `tSfxEmitter` | a 3D sound source | `Source` (ProcessEmitters.c, whose `radius`, `nearVolume`, `farVolume`, `parameter`, `handle` are `fadeDistance`, `maxVol`, `minVol`, `basePitch`, `sndTag`) |
| `tSfxListener` | a listener | `Listener` (ProcessEmitters.c, whose `right` and `forward` are MAT3's `look` and `up`) |
| `tSfxVoice` | an effect instance of the sound server | `SfxInstance` (are_sound_voices_idle.c, whose `active` is `used`) |

In [audio.h](../../include/game/audio.h): `SampleDmaList`, the sample DMA list in progress (`pc_sample_dma_list`, the
PC's own storage; was sample_dma_thread/group.c's `SampleDmaList`, sample_dma_next.c's and types.h's `DmaList` and
musyx_start_dma_list.c's `DmaListView`), and `SoundGroupEntry`, an entry of `song_groups` and `boot_sfx_groups` (a
group of the MusyX project to push; was AudioSongPlay.c's `SongGroup` and AudioInitSys.c's `Instrument4`, and the
data were declared as the library's `ResourceRange`, two u16s alike).

Settled: `tAudioMsgHeader.cmd` is MAT3's `char`, `u8` under the game's `-funsigned-char`. `tFxSequence.list` was a
`tDLList *` of audiomgr_thread.c's own (MAT3's char/int list header): it is `AudioList *`, and `gPlayingFxTags`,
declared `FxTagList` (the same header under MAT3's names), is an `AudioList` like the other two tag lists
(`FxTagList` is gone; the thread reads its `count`). Its field program now takes the two bytes after `doubly`, padding
in `FxTagList` and `pad[2]` in `AudioList`, as bytes too: BSS, which nothing reads there, and the state hash takes only
a datum's pointer runs. It is the one difference in the generated data beyond names. `tSfxEmitter.sndTag` stays u32 (ProcessEmitters read
an s32; every use passes it to an int). `tAudioSequence.id` is MAT3's unsigned (FreeSequence: `id &= gTagMask`). The
message pool `pc_snd_msgs` is port data whose declaration (`struct pc_snd_msg`) the game's types don't see: its readers
cast it to `tAudioMsg`, as AudioShutdown.c did. `gFreeQ` keeps its type `SfxQueue` (an `AudioList` whose first two words
are declared as words): as an `AudioList` its field program would change. The OS records some of these files declare
for themselves (`OSMesgQueue`, `OSThread`, `OSScClient`) are D2's and stay.

**The MusyX synth voice**: [src/rom/sound/musyx.h](../../src/rom/sound/musyx.h)'s `SynthVoice`, with its head
`SynthVoiceHead`, its controller inputs `SynthInputMix` and `SynthInputSource`, its LFOs `SynthLfo` and the
`SYNTH_INPUT_*` order (which macros.c and controllers.c each had). The library's five files read a voice as it
(`sound_synth_voices`, `pc_mx_voice` as voices). It replaces 56 views whose fields were named after their N64 offsets
(`flags24`, `id60`, `lfo17C` ...): `VoiceState` (api.c, controllers.c, macros.c, synth.c, voices.c), `VoicePrefix`
(controllers.c, synth.c), `MacroState` and 40 `MacroState*` handler views and `SynthVoice*View` (macros.c), synth.c's
`RampState`, `SynthVoiceLegatoView` and its own `SynthVoice`, voices.c's `SynthVoicePriorityView` and
`SynthVoiceAllocView`, controllers.c's `VoiceControls`; with them the inputs' and LFOs' two copies each
(`ControlInput`/`MacroInput`, `ControlSource`/`MacroInputSource`, `SynthLfo`/`MacroLfoControl`) and macros.c's 13
copies of the macro step (`MacroCommandAddAge` ... are `MacroCommand`). It is the library's header, not include/game:
only the library reads the voice (the game names it in prototypes only), and the library's files can't include the
game's headers (their libultra types are the boot code's).

- The names are macros.c's byte map, the most complete view (synth.c's `cFlags24`, `midi4A`, `midiSet4B`,
  `identifier60`, `note50`, `field68` are the same fields); the map is now the record's comments.
- `volume`, `panning` and their targets are u32 (16.16, as macros.c's 17 views read them); `panDelta`, `sweepOff` and
  `sweepAdd` s32. synth.c's update clamps a sweep that went below 0, which its view read signed: `(s32)sv->volume < 0`.
- Packed (`#pragma pack(1)`), as 56 of the 58 views were. Every field is at its natural alignment either way, but GCC
  makes other code for an aligned record's fields (sound_macro_loop's flag tests). For the same reason voices.c's
  allocator reads `blocked` and `allocId` through the voices as a pointer (`VOICE_AT`) and PLAY_MACRO links the chain
  through the voices' addresses, as their views did.

**Shared by the library and the game**: [soundlib.h](../../include/game/soundlib.h), which the library's files
include after rom_tu.h and audio.h after types.h (it needs the basic types and libultra's `OSMesgQueue`, `OSMesg` and
`OSTask`, which each side declares alike).

| Record | What it holds | Its other declarations |
|---|---|---|
| `SpeechStream` | a speech stream (`speech_streams[2]`): its GSM decoder state, input ring, PCM ring and voice | types.h (the data's layout), decode.c, streams.c; gsm.c's `GsmStream` view |
| `SoundServices` | the game's services to the library | types.h, src/rom/sound/sound_services.h (which now includes the header) |
| `SampleDmaRequest` | an entry of the sample DMA list (`sound_dma_list`, now typed by it) | hardware.c's `SoundDmaEntryCodebook` and `SoundDmaEntrySamples`, dma_queue.c's `SoundDmaEntry`, sample_dma_next.c's `DmaReq` |
| `TransferRequest` | a cartridge read a speech stream queued | types.h, dma_queue.c |

`SpeechStream`'s first 400 bytes are gsm.c's `GsmState`, which the data loader lays out as types.h had them (bytes, the
last lag's u16, bytes): gsm.c reads them as its `GsmState` through a commented cast. Its input ring is a union of the
bytes the reads fill and the words the decoder takes; the loader copies a union that bytes cover as bytes, so the
field program is the old `u8[4096]`'s. `SampleDmaRequest.length` is the library's int (the game's view had a u32).

**VIEW lines**: the eight are gone.

| Datum | Read as (file) | Now |
|---|---|---|
| `gFxTags` | `Voice *` (AudioInit.c) | its type, `tFxSequence *` |
| `gSortedFxTags` | `void *` (AudioInit.c) | `tSortedFxTag *` (audio.h named the struct `anon_te_tag_c`) |
| `gSequences` | `Event *` (stop_sequence.c) | `tAudioSequence *` (audio.h declared it `struct Chan *`) |
| `gSfxInstances` | `SfxInstance *` (are_sound_voices_idle.c) | its type, `tSfxVoice *` |
| `audio_manager_thread` | AudioInit.c's own 0x1B0-byte `OSThread` | its type, `OSThreadBytes` (the N64's 0x1B0 bytes, as the loader lays it out): AudioInit passes `(OSThread *)` to osCreateThread and osStartThread, which on the PC keep only the address, as the thread's key |
| `boot_sfx_groups` | `Instrument4[4]` (AudioInitSys.c) | `SoundGroupEntry[4]` |
| `song_groups` | `SongGroup[]` (AudioSongPlay.c) | `SoundGroupEntry[12]` |
| `musyx_retrace_callback` | `intptr_t` (musyx_set_retrace_callback.c) | its type, `void (*)()`, declared in audio.h (was frontend.h); `musyx_set_retrace_callback` takes `void (*)(void)` and `musyx_start_dma_list` its list and queue as pointers |

**Not `same` under objsame** (against wave 4's reference): RelWithDebInfo 959 of 961. `gameover.c`: its scratch message
for a full pool is a whole `tAudioMsg` (32 bytes, was 24, `.bss` only). `synth.c`: `sound_synth_update_voices`
allocates registers otherwise around the signed clamps of the volume and pan sweeps (the same values; the functions
after it in `.text` pad their loops otherwise). Debug 950 of 961: those two; `ProcessEmitters.c`, `audiomgr_thread.c`,
`free_sequence/group.c`, gsm.c, dma_queue.c and sample_dma_next.c convert between an s32 and a u32 of the same width
at -O0 (a register move, or the order of two loads); `sample_dma_thread/group.c` reads the done queue as a field (one
instruction where `M2C_FIELD`'s address took two); `macros.c` and synth.c's `sound_synth_init`: -O0 addresses a packed
record's field from its offset as GCC's layout splits it, which depends on the fields before it. The generated data
(`data.c`, the symbol and layout lists) differ in type names and in `gPlayingFxTags`' first run (above).

**Numbers** (the sound area: the audio manager's and sound server's files, the car sounds', src/rom/sound and
src/rom/speech, 120 files): local struct and union declarations 215 to 101, pad arrays 228 to 46, VIEW lines 8 to 0,
tags declared in more than one file 8 (19 declarations) to 5 (10): the five left are other agents' records
(`MODELDAT`, `OSMesgQueue`, `PlayerRec`, `Vec3`) and the library's own vector (`Vector` in streams_3d.c and vector.c,
with `SalVector` and `SoundVector` the same three floats). layout_check: open findings 6 to 5 (the DMA list's views,
which agreed in the end on one host layout). The views ctests take the records' names; `views_sound_commands` went
(the messages are a union, which audiomgr.h asserts at the pool's 32-byte element).

**Left**, and why:

- Not sound, though the brief's tags named them: the circuit code's `Resource` (SetCircuitCode.c, UpdateCircuit.c,
  circuitscr.c: the circuit's bytes; modescr.c's: a pak file), new_streak's and hudextra_icons' `Effect` (trails, HUD
  icons), AddBattlePointInternal.c's `Event` (a battle point's marker), gameover.c's `Voice` (a blit:
  `gameover_blit`, frontend.h's `struct Voice *`).
- The library's records that types.h copies for the data loader (`SequenceContext`, `SequenceNode`, `ChannelState`,
  `LargeNode`, `SmallNode`, `EmitGroup`, `ResourceRecord`, `LayerRecord`, `AudioConfiguration` ...) beside the
  library's own definitions (include/sequence_context.h, synth.c, streams_3d.c, registry.c, hardware.c): each needs
  the soundlib.h treatment, and some its readers' types made to agree first (synth.c reads the volume faders through
  three views, `ChannelState`, `SynthFader`, `MasterFader`, with signed and unsigned fields; sequencer.c's slot views,
  two of them layout_check's open findings).
- `gFreeQ` (`SfxQueue`, above), and the port's storage `pc_snd_msgs` and `pc_mx_voice` (port/include/pc_game_types.h
  declares them for the game without the game's types: the readers cast).

## 5. The OS records

(Agent D: the message queues, the pak file system, the scheduler's clients, zlib's state.)

## 6. The proof

(The lead's, after the merge.)
