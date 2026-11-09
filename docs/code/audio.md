# Audio: the sound manager, the sound server and the MusyX library

How San Francisco Rush 2049 makes its sound, for a programmer new to it: the layers from a game call such as
`PlayGameSound` down to the samples the PC plays, the files of each, the main data, and what is still shaped by
the N64 (for wave 3, [PCONLY.md](../port/PCONLY.md) 4). Names are the tree's (wave 2b). The PC side of the output
(port/audio's interpreter of the subframe descriptors, the mixer, the voice and budget settings) is in
[AUDIO.md](../port/AUDIO.md); the sound data's formats are in [MUSYX_DATA.md](../port/MUSYX_DATA.md).

## 1. The layers

```
 game code (cars, HUD, menus, targets)
   PlayGameSound / EmitGameSound / PlayTargetSound / Play*Sound      MAT3 audio.c        src/blob/Play*.c, car_sounds/
   UpdateCarSounds, StartListeners / UpdateListeners
        |
 the sound manager (fx tags, messages, emitters, listeners, sequences)  MAT3 audiomgr.c  src/blob/Aud*.c, groups/audio*/
   AudPlayEffect, AudEmitEffect, AudChangeEffect, AudStopEffect ...  --message-->  audio manager thread
   each retrace: ProcessEmitters, ProcessSequences, the priority pass (WeightedPriority, OrderFx, UpdateFx)
        |
 the sound server (effect instances, a command queue)               MAT3 audiosfx.c    groups/audiosfx*/
   AudioPlayEffect, AudioSetEffect*, AudioStopEffect --> gCmdQ --> AudioSfxServer (each retrace)
        |
 the MusyX sound library (boot segment)                             N64 only           src/rom/sound/
   sound_start_sfx, sound_set_volume, sound_start_song ... the synth, the macros, the sequencer
   sound_hw_service_frame: subframe descriptors (SFDs) --> the RSP task --> on the PC, port/audio
```

**The game side** asks for a sound by an effect id (`fxid`, an index of `gEffectLoop` and of the MusyX project's
effect table) with a sub id (which instance: a car's slot ...), a replace rule (`kFxIndividual`, `kFxReplace`,
`kFxNoReplace`) and a priority. It gets back an **fx tag handle**, `(generation << gTagShift) | index`, which it keeps
to change or stop the sound; a stale handle is ignored (`ConvertTag`).

**The sound manager** (MAT3 audiomgr.c) owns the tags (`gFxTags`, `tFxSequence`) and the 3D sound. Every request
takes the audio lock (`audio_lock_queue`, a one-message queue: `audio_lock`/`audio_unlock`), takes a message from the
pool (`GetCmd`; `tAudioMsg`, on the PC `pc_snd_msgs`), fills it and posts it to `audio_command_queue`. The **audio
manager thread** (`audio_manager_thread_entry`, src/blob/groups/audiomgr/audiomgr_thread.c, the system area) runs the
messages and, on each retrace message, `PeriodicMaintenance`:

- `ProcessEmitters`: for each 3D source (`tSfxEmitter`, from `AudEmitEffect`) a volume, pans and pitch from the
  listeners (`tSfxListener`, one per player's view: `StartListeners`, moved each frame by `UpdateListeners`), sent as
  a change (`AudChangeEffect`).
- `ProcessSequences`: the timed sequences of `AudPlaySequence` (`tAudioSequence`), one effect after another.
- the **priority pass**: the tags playing or waiting go into a tree (`gSortedFxTagTree`, nodes `tSortedFxTag`)
  ordered by `WeightedPriority` (the caller's priority, plus up to 80 the quieter the sound, plus 8 for a loop, plus
  a point a quarter second for an old one-shot); `WalkBST` calls `UpdateFx` on each in order, which takes the voice of
  the tags ranked past three quarters of the effect quota (`gMaxSfx`) and gives one to the audible waiting ones.
  The resulting interrupt and start messages are run after the walk.
- `AudioSfxServer`, the sound server's pass.

A tag's states (`kFxPending`, `kFxPlaying`, `kFxInterrupted`, `kFxInvalid`) and lists (`gInterruptedFxTags` for
waiting ones, `gPlayingFxTags`, `gFreeFxTags`) follow from this: a new sound waits for the next pass; a looped sound
whose voice was taken waits interrupted and restarts when it ranks again.

**The sound server** (MAT3 audiosfx.c, N64 form) turns starts, changes and stops into MusyX calls. An **effect
instance** (`gSfxInstances`, `tSfxVoice`, `gNumInstances` of them: the effect quota) has a handle
`(gNextIndex << 8) | index`; its commands (`tSfxCmd`, three per instance, `audiosfx_gCmds`) wait on `gCmdQ`.
Each retrace `AudioSfxServer` frees the instances whose MusyX voice has ended, then runs the queue from its oldest:
plays (`AudioPlayEffectInternal`: `sound_start_sfx`, `sound_set_volume`, `sound_set_pan`), changes (volume, pans,
doppler), stops (key off, then free when silent), mono. A command MusyX refuses is kept for the next retrace (up to
ten times, a play forever), and the instance's later commands wait behind it. The free channels
(`gNumChannels`) limit the starts per retrace.

**The songs** go another way: `AudPlaySong`'s message has the thread call `AudioSongPlay`, which loads the song's
wave (asset file `10 + song`), pushes its MusyX sound group and starts it (`sound_start_song`, `song_handle`);
`AudKillSong` and `AudioSongStop` undo it. The music and effect volumes are MusyX's volume groups 21 and 22
(`AudSetMasterVol`, `AudioSetMasterVolume`). `StartGameMusic` (`StartGameMusicInternal`) picks the track's song.

**The MusyX library** (src/rom/sound, the boot segment) is the N64's sound library: registries of the project's
sound groups, a synth of up to 32 voices (96 on the PC: `[audio] voices`), each voice running a SoundMacro program,
a song sequencer, streams and its own 3D emitters, and a hardware layer that describes each stretch of output as
subframe descriptors for the RSP's MusyX microcode. [boot.md](boot.md) 3 has its layers; section 3 below its files.

**Speech** (src/rom/speech): GSM 06.10 speech streamed from the cartridge. Nothing in the game starts it
(`speech_init` is never called): it runs only in the port's tests.

## 2. The game side's files

The sound manager's own records are in `include/game/audiomgr.h` (`tFxSequence`, `tAudioMsg`, `tSfxEmitter`,
`tSfxListener`, `tAudioSequence`, `tSortedFxTag`, `tSfxVoice`, `tSfxCmd`, the state, type and command enums), in the
PC's layout and with MAT3's field names. It is not in game.h because audiomgr_thread.c (the system area) still defines
its own copies of `tFxSequence` and `tAudioMsg`; when it includes the header instead, the header can join types.h.
The car sounds' records (`tSoundState`, `tRoadNoises`, `Definition32` the engine layers, `Impact24` the bump peaks)
are in types.h; the menu sounds' ids (`kSfxMenu*`) and button edges (`MENU_EDGE_*`) in audio.h.

| File | What (MAT3 module) |
|---|---|
| `src/blob/AudPlaySong.c`, `AudKillSong.c`, `AudMono.c`, `AudStopAllEffects.c`, `AudPauseAllEffects.c`, `AudResumeAllEffects.c` | the requests that are one message each (audiomgr.c) |
| `groups/audio_emitters/group.c` | `GetFxTag`, `AudPlayEffect`, `AudPlayEffectVP`, `AudEmitEffect` (audiomgr.c) |
| `groups/transform/group.c` | `AudChangeEffect` and its volume, pitch and pan setters (audiomgr.c) |
| `groups/aud_stop_effect/group.c` | `ConvertTag`, `AudStopEffect` (audiomgr.c) |
| `groups/audiomgr_emitters/group.c` | `AudStopEmitter`, `AudEmitterPlaying`, `AudUpdateEmitter` (audiomgr.c) |
| `AudEffectPlaying.c`, `AudPlaySequence.c`, `AudInit3D.c`, `AudAddListener.c`, `groups/listeners/*` | tests, sequences, the 3D start-up, the listeners (audiomgr.c) |
| `groups/audiomgr/audio_effect_setup_b132.c` | `FreeFxTag`, `ProcessSequences` (audiomgr.c) |
| `groups/update_fx/updatefx.c`, `groups/weighted_priority/group.c`, `OrderFx.c`, `groups/interrupt_fx_tag/ap.c` | the priority pass (audiomgr.c) |
| `groups/abort_game/GetCmd.c`, `groups/heap_release/audio_lock.c`, `audio_unlock.c`, `AudioShutdown.c` | the message pool, the lock, the shut-down |
| `NewEmitter.c`, `NewListener.c`, `ValidateEmitter.c` | the free lists' takers; the empty debug check |
| `groups/audiosfx/group.c`, `FindChangeCmd.c`, `AudioSfxInit.c`, `AudioEffectPlaying.c`, `AudioStopAllEffects.c`, `groups/audiosfx_play/group.c` | the sound server (audiosfx.c, audiosfxps2.c) |
| `groups/AudioInitSys/AudioInitSys.c`, `AudioShutdownSys.c`, `AudioSetMasterVolume.c`, `AudioTimeout*.c`, `GetAudioTime.c` | the system under the manager: MusyX's start-up, the clock (audiosysps2.c) |
| `groups/slot_sound/AudioSongPlay.c`, `AudioSongStop.c`, `AudioSongInit.c` | the songs (audiosongps2.c) |
| `PlayGameSound.c`, `EmitGameSound.c`, `PlayTargetSound.c`, `Play{Exit,LR,UD,StartAB}Sound.c`, `groups/game_sounds/*`, `groups/mode_init/group.c`, `UpdateListeners.c` | the game's helpers: race sounds, menu sounds, volumes, music, listeners (audio.c) |
| `groups/car_sounds/d5e64.c`, `mode.c`, `msh.c`, `ded78.c`, `StopCarSounds.c`, `InitSoundState.c`, `StopSound.c` | a car's engine, screech, road noises, bumps, scrapes, thumps, horn, bog (audio.c) |
| `groups/audioscr_update/*`, `groups/hud_text_anims/*` | the audio options screen and its panes (audioscr.c) |
| `musyx_submit_audio_task.c` | MusyX's service that sends the audio task to the scheduler |

Also in the area's list, not sound: `FillFreeMem.c` (the heap's freed-memory fill), `heap_lock.c`/`heap_unlock.c`
(empty), `update_player_visibility.c` (MAT3 visuals.c's UpdateArrowVis), `groups/slot_sound/AdjustTextWavePointers.c`
(the text bank), `groups/state_waves/slots.c` (the front end's waves). The audio manager thread itself, AudioInit,
ProcessEmitters and the MusyX services other than the task are the system area's.

**The cars** (MAT3 audio.c): `StartCarSounds` starts a car's two engine layers (`gEngineSpecs[type]`): plain effects
for a human's car (`drone_type` 2), 3D emitters for a drone's. Every other frame per car (`pc_engine_phase`)
`UpdateCarSounds` sets the layers' pitch from the rpm and their volume through the layer's low, mid and high rpm, then
for a human's car the tire screech from the skids, the road noise of the loudest surface under it (dirt, water;
pavement has none), the bumps and scrapes (`UpdateBumps`: the body forces' peaks, `GetForceAndPeak`), the thumps of
a landing, the horn and the bog. `tSoundState` keeps each sound's handle and last values so that only changes are
sent (`SetSoundVP`).

## 3. The MusyX library's files

| File | What |
|---|---|
| `src/rom/sound/hardware.c` | the hardware layer: `SoundHwVoice` per voice (sample, 4.12 rate, gains, ADSR envelope); each retrace `sound_hw_service_frame` renders one SFD (0x50 bytes a voice, 192 frames) per AI buffer, running the 60 Hz tick within (`sound_hw_render_subframe`); two caches (0x600-byte sample blocks, 256-byte codebooks) filled by one DMA list a frame; last frame's SFDs go as the audio task; the output thread; the lock; an unused delay effect |
| `dma_queue.c` | up to four queued cartridge reads for the speech streams |
| `init.c` | `sound_lib_init` (and the restart over a running output, `sound_lib_init_shared_output`), the exits |
| `api.c` | the locked entry points: start an effect, controllers, key off, fades, stop everything, idle, the services |
| `groups.c`, `registry.c` | `sound_push_group`/`sound_pop_group` walk the project (wave 6) and register a group's macros, samples, tables, keymaps, layers and effect table from the pool (wave 7) and the sample directory (wave 8): sorted, reference-counted registries searched by id |
| `voices.c` | handles (sorted nodes naming a chain's first voice), voice allocation and stealing within the music and effect quotas, per-priority lists, stream voices |
| `synth.c` | starts effects, notes, keymaps and layers; `sound_synth_tick` runs each voice's macro and computes pitch (16.16 note to 4.12 rate) and volumes through the faders and controllers; the volume groups' fades |
| `macros.c` | the SoundMacro interpreter (`sound_macro_run`: up to 32 eight-byte steps per voice per tick, `MacroOpcode`'s 62 handlers); the voice's byte map is at its top |
| `controllers.c` | MIDI controllers per song player and channel, per effect voice; each voice's nine inputs, each mixing up to four controllers or LFOs |
| `sequencer.c` | eight song slots (`SequenceContext`, include/sequence_context.h): 64 tracks of pattern lists, note events, pitch bend and modulation streams, a tempo track, loops, fades, crossfades (deferrable through controller 104) |
| `streams_3d.c` | streams (a voice playing a CPU-filled PCM ring) and the library's own emitters and listeners, which the game doesn't use (it has its own) |
| `gains.c`, `vector.c`, `util.c`, `rate.c` | gains (volume, span, pan, aux to four Q15 gains), the 3D maths, allocation, random numbers, the sine table, time conversions, sample rate to pitch |
| `src/rom/speech/streams.c`, `decode.c`, `gsm.c` | the speech bank on two streams: 1 KiB async reads into a 4 KiB ring, the GSM decoder into a PCM ring, played on a stream voice (the decoder's kernels are in port/ultra/gsm.c) |

## 4. The main data

| Data | What |
|---|---|
| `gFxTags`, `gNumFxTags`, `gTagShift`, `gTagMask`, `gNextFxId` | the fx tags and their handles' fields (AudioInit sizes them: `max(min(2 gMaxSfx, 64), gMaxSfx + emitters)`) |
| `gFreeFxTags`, `gInterruptedFxTags`, `gPlayingFxTags` | the tags' lists (`AudioList`; `gPlayingFxTags` is `FxTagList`, the same header with MAT3's field names) |
| `pc_snd_msgs` (N64 `audiomgr_gCmds`), `audio_command_queue`, `audio_lock_queue` | the message pool, the thread's queue, the lock |
| `gEmitters`, `gListeners`, `gSequences` and their free and active lists | the 3D sources, listeners and sequences (heap blocks) |
| `gSortedFxTags`, `gSortedFxTagTree`, `pc_snd_start_cmds`, `pc_snd_stop_cmds` | the priority pass's tree and its start and stop messages |
| `gSfxInstances`, `audiosfx_gCmds`, `gCmdQ`, `gFreeQ`, `gNumChannels`, `gStoppedChannels` | the sound server's instances, commands, queues, free channels |
| `song_handle`, `song_number`, `song_wave`, `song_groups`, `musyx_project`, `musyx_pool`, `musyx_sdir`, `musyx_samples` | the playing song and MusyX's data |
| `gAudioSeconds` | the sound clock (wraps every four hours), advanced by the audio retrace thread |
| `pc_engine_sounds`, `pc_screech_sounds`, `gRoadNoises`, `gBump`, `gScrapes` ... | the cars' sounds |
| `sound_*` (boot segment BSS) | MusyX's state: registries, voices (`pc_mx_voice` on the PC), controllers, the sequencer's slots, the hardware layer's records and caches |

## 5. Still N64-shaped (for wave 3)

- **The threads and the lock.** On the N64 the sound manager runs on its own thread with a message queue and a
  lock; MAT3 calls the same functions directly. The PC keeps the threads (host threads) and adds guards for a pool or
  queue that runs out on a loaded host (`PC_CMD_OR_UNLOCK` ..., docs/port/QA.md "Integration soak").
- **Window addresses in records.** A player record keeps its listener as a 32-bit window address (`PC_N64A`,
  `PC_N64P`: StartListeners, UpdateListeners, StopListeners). `musyx_samples` is a cartridge address
  (`D_00039B40`). The heap blocks are given to `PurgeableMemory` as window addresses.
- **The model record** (`tModelData`, 0x808 bytes) is read in the N64's layout by the car sounds' views.
- **MusyX's voices** keep the N64's 0x1A0-byte layout, read through about 30 per-function views in macros.c,
  synth.c, voices.c, api.c and controllers.c; their macro pointers (+0, +4, +8, +0xC, +0x1C, +0x20) are 32-bit
  window addresses (`N64P`/`N64W`) and +0x18 holds a handle node as an N64 address (`pc_voice_node`). One shared
  `SynthVoice` type would replace the views (macros.c's byte map is its spec); the voice flags, controller numbers and
  input indexes are defined in each of those files.
- **MusyX's data** (project, pool, sample directory) stays in the N64's layout, byte-swapped in place; samples and
  codebooks are addressed by 32-bit cartridge or window addresses; the DMA list's sources are 32-bit; the cache
  bookkeeping counts the N64's record sizes (12-byte DMA entries, 0x50-byte SFD records).
- **The sequencer** adds song offsets to a 32-bit song address; `sound_seq_studio_groups` is a label inside slot 0;
  `sound_volume_fader_types` is the type byte inside `sound_volume_faders`; `sound_macro_get_variable` reaches
  `sound_global_variables` 32 bytes past `sound_seq_current_index`, by the data's N64 order.
- **The RSP task** (`sound_task`, `musyx_submit_audio_task`'s `OSScTask`) and the microcode pointers; the AI output
  thread, the D-cache calls and the cartridge DMA are port/ultra's on the PC.
- **The speech** streams' cartridge reads (async DMA) and its KSEG0 test in `speech_set_bank`.
- **Dead or misnamed code** a later cleanup can take: `NewListener` (nothing calls it), `other_user` (a stand-in
  caller), the empty `ValidateEmitter`/`pc_ValidateEmitter`/`ValidateListener` with their N64-build leftovers,
  `update_player_visibility` (MAT3's UpdateArrowVis), the library's own emitters and streams (unused by the game), the
  `AudPauseAllEffects` that stops (as MAT3's).
