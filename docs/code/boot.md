# The boot segment and the low image

On the N64 the cartridge's first megabyte holds a boot segment that the console copies to RAM at 0x80000400 and runs.
It contains everything that stays resident for the whole session: the entry point, the threads, the frame timer,
the scheduler, the video-mode code, the inflater that unpacks the game image, Nintendo's libultra, a small libc,
Factor 5's MusyX sound library and a GSM speech decoder. Its data and BSS (0x800277D0 to 0x80086A50) hold their
globals, the thread stacks, the audio library's tables and the graphics task's buffers. The game image (the
range agents' area) and the two runtime images ([overlays.md](overlays.md)) are loaded after it.

In this tree it is `src/rom/`. Names below are the ones `tools/port/names/rom.tsv` gives; the map has the old
spellings and the evidence. Every file there is listed with its new path at the end of the map.

## 1. What runs when

1. **`main`** (boot.c) is what the boot code jumps to. It initializes libultra (`__osInitialize_common`), reads the
   cartridge header, and starts the **idle thread** (`idle_thread_entry`, priority 2), which creates the PI manager
   and starts **thread 6, `main_thread_entry`**.
2. **`main_thread_entry`** (the main thread's body; its old name called it an init) creates the message queues
   (`rom_dma_queue`, `unzip_dma_queue`, `main_thread_queue`, `game_thread_queue`), the scheduler (`gScheduler`, priority 12),
   inflates the game image from the cartridge into RAM with `unzipdata_from_rom`, registers with the scheduler for
   retraces, starts the other threads (thread 8 `gAudioThread`, thread 5 `model_thread`, thread 7 `game_thread`),
   runs the game's late init and sound init, waits for the scheduler's first message, and then runs the game's main
   loop forever.
3. **`game_thread_entry`** (its old name said audio, but it has nothing to do with audio) calls the game's `game_loop`
   forever.

Further threads (2, 3, 9, 10) are created by the game image; their objects and stacks live here too:
`rumble_thread` (2), `sample_dma_thread` (3), `audio_retrace_thread` (9) and `audio_manager_thread` (10), each
with a `<name>_stack_top`; their bodies are `<name>_entry` in the game image (r1).

## 2. The parts

| Part | File (new path) | What it does |
|---|---|---|
| Frame timer | `timer.c` | MAT3's `timer.c`, function for function: `InitializeTimer`, `UpdateFrameTimer`, `PauseTimer`/`ResumeTimer`, the countdown (`SetCountdownTimer`, `AddCountdownTime`, `GetCountdownTimeRemaining`, `CountdownTimeOut`). `gSeconds` is the time since the countdown started, `gFrameSeconds` the last frame's time (at most 5 retraces), `gRetraceRate`/`gRetraceTime` 60 Hz or 50 Hz. It counts retraces from `scheduler_retrace_count`. |
| Scheduler | `scheduler.c` | The SDK-style scheduler (`osCreateScheduler`, `osScAddClient`, `__scMain` and the `__sc*` handlers): audio and graphics tasks on the RSP, retrace messages to clients. `gScheduler` is its state; `scheduler_task_queue`, `scheduler_interrupt_queue`, `scheduler_thread`, `scheduler_current_rsp_task` and `scheduler_retrace_count` are names inside it. |
| Video mode | `video_mode.c` | Chooses the VI mode for the TV type (`video_set_mode`, which also sets the retrace rate), and moves the picture for the options screen's screen position (`video_set_offset`, `video_get_offset`, `video_get_default_offset`): it builds an adjusted copy of the mode (`video_build_adjusted_mode`) and applies it two retraces later (`video_apply_pending_mode`). N64 only. |
| Decompression | `decompress.c`, `unzip.c` | A lock (`acquire_decompress_lock`, `release_decompress_lock`) around the two decompressors: `lzss_decompress` and the inflater. `unzip.c` is MAT3's `unzip.c` (a gzip-derived inflater: `huft_build`, `inflate_codes`, `inflate_block`, ...), with an N64 input path (`get_buffer`) that DMAs the compressed stream from the cartridge in 4 KB halves (`unzip_input_a`, `unzip_input_b`). |
| libultra | `libultra/*.c` | Nintendo's OS: threads, message queues, timers, VI/PI/SI/AI managers, the controller and Controller Pak file system, Rumble Pak, the RSP task calls, `gu*` matrix helpers. Named after ultralib's files; [SYMBOL_NOTES.md](../SYMBOL_NOTES.md) has the evidence for every name. |
| libc | `libc/*.c` | IDO's `memcpy`, `memset`, `memchr`, `strlen`, `strchr`, `sprintf` and its float formatting, `modf`. |
| Sound library | `sound/*.c` | Factor 5's MusyX for the N64 (section 3). |
| Speech | `speech/*.c` | Two GSM 06.10 speech streams read from the cartridge (section 4). |

## 3. The sound library (MusyX)

The game talks to it through a small API (`sound_lib_init`, `sound_push_group`, `sound_start_sfx`, `sound_start_song`,
`sound_set_volume`, `sound_key_off`, the emitters ...). Every entry takes the library lock (`sound_hw_lock`, a
one-message queue) and tests `sound_initialized`. Its layers, bottom up:

- **Hardware layer** (`sound/hardware.c`, `sound_hw_*`). An **output thread** (`sound_hw_run_output_thread`) keeps a
  ring of 768-byte AI buffers playing (`sound_output_buffers`). Each retrace, **`sound_hw_service_frame`** renders the
  buffers the AI has played since the last call: for each, `sound_hw_render_subframe` runs the library's 60 Hz tick
  when it is due (sequencer and synth), then `sound_hw_build_subframe` writes one **subframe descriptor** (an `SfdBlock`
  of `SfdVoice` records, one per active voice). It issues the frame's DMA list (sample blocks and ADPCM codebooks
  the voices need, fetched into two caches: `sound_hw_fetch_sample_range`, `sound_hw_fetch_codebook`) and submits
  the previous frame's descriptors as an RSP task (`sound_hw_submit_task`) running the MusyX microcode
  (`audio_ucode_text`). Each voice's envelope (attack, decay, sustain, release) is advanced here
  (`sound_hw_advance_envelope`). The hardware voice record is `SoundHwVoice`.
- **Platform services**. The library does not call the OS for tasks, retrace callbacks, DMA, memory or address
  translation; the game installs eight hooks with `sound_set_services` (`sound_services`, `sound_service_alloc`, ...).
- **Registries** (`sound/groups.c`, `sound/registry.c`). `sound_push_group` walks the sound project file and
  registers a group's macros, tables, keymaps, layers, samples and sfx table, each in a sorted, reference-counted
  array (`sound_macros`, `sound_tables`, ...); lookups are binary searches (`sound_find_macro`, `sound_find_sample`).
  The formats are in [MUSYX_DATA.md](../port/MUSYX_DATA.md).
- **Synth** (`sound/synth.c`, `sound/voices.c`, `sound/controllers.c`). `sound_synth_voices` are the synth voices
  (`SynthVoice`, 0x1A0 bytes). A sound is a chain of voices behind one handle (`sound_new_handle`,
  `sound_resolve_handle`). `sound_alloc_voice` finds a free voice or steals the oldest of the lowest priority within
  the music and effect quotas. Each tick `sound_synth_update_voices` runs the voices' macros and computes pitch
  (keys, detune, pitch bend, vibrato, sweeps, portamento) and volume (volume groups `sound_volume_faders`, MIDI
  controllers, curves) into the hardware layer. Controllers are kept per player channel and per voice
  (`sound_midi_controllers`, `sound_voice_controllers`); each voice has nine inputs that mix up to four controllers.
- **Macros** (`sound/macros.c`). Every sound is a SoundMacro program: 8-byte steps that start samples, wait, loop,
  set envelopes, pan, pitch, variables and conditions. `sound_macro_run` executes up to 32 steps per voice per tick;
  each opcode has a handler (`sound_macro_wait`, `sound_macro_start_sample`, `sound_macro_set_adsr`, ...).
- **Sequencer** (`sound/sequencer.c`, `sound_seq_*`). Eight song players (`sound_seq_players`): tracks of pattern
  references, patterns of note events with pitch-bend and modulation streams, a tempo track, loops, fades and
  crossfades (`sound_seq_crossfade`).
- **Streams and 3D** (`sound/streams_3d.c`). A stream is a voice that plays a CPU-filled PCM16 ring
  (`sound_stream_start`), refilled each frame through a callback. Emitters and listeners (`sound_emitter_add`,
  `sound_listener_add`) turn positions and velocities into volume, pan, surround and doppler every frame
  (`sound_emitter_update_all`), with a per-group voice limit that starts the loudest emitters.

The library is portable C: on the PC it compiles as is, with its voice storage widened for more than 32 voices
(`PC_MX_VOICE_SLOTS`). The microcode is not run: `port/audio` interprets the subframe descriptors
([AUDIO.md](../port/AUDIO.md)). N64-only in this layer: the AI output thread, the RSP task, the D-cache line
operations (`dcache_create_dirty_lines`), the cartridge DMA.

## 4. Speech

`speech/streams.c` plays entries of a speech bank (`speech_set_bank`: a table of offsets and rates in the cartridge)
on two streams (`speech_streams`, `SpeechStream`). `speech_play` starts one and returns a token; the frame hook
`speech_service` reads the compressed data ahead with async DMA (`sound_dma_read_async`), decodes it
(`speech_decode`) into the stream's ring, starts the stream voice once primed, and frees it at the end.
`speech/gsm.c` is a GSM 06.10 full-rate decoder (`gsm_decode_stream_frame`, with run-length silence and repeat
codes in the bit stream); its three inner kernels (`gsm_rpe_decode`, `gsm_long_term_synthesis`,
`gsm_short_term_filter`) were hand-written assembly in the boot tail, and `port/ultra/gsm.c` has them in C.

## 5. The low image's data

| Range | What |
|---|---|
| 0x80000000-0x80000400 | exception vectors and libultra's boot globals (`osTvType`, `osMemSize`, ...) |
| 0x80000400-0x800277D0 | the boot segment's code |
| 0x800277D0-0x8002AFA0 | the GSM kernels and the RSP microcode: `rspbootTextStart`, `gspF3DEX2_NoN_fifoTextStart`, `audio_ucode_text` (MusyX, not the SDK's aspMain) |
| 0x8002AFA0-0x8002E8E0 | initialized data: the timer's `gRetraceRate`/`gRetraceTime`, `screen_width`/`screen_height`, the video tables, the unzip tables, libultra's tables, the sound library's curves and ratio tables, `gWaveSizes`, two static display lists, the microcode data segments, the build-info record |
| 0x8002E8E0-0x80086A50 | BSS: the scheduler, the timer state, message queues, thread objects and stacks, the sound library's state (registries, voices, controllers, emitters, the speech streams), the graphics task's buffers (`rsp_dram_stack`, `rsp_yield_buffer`, `rdp_fifo_buffer`), the frame command buffers, the unzip input buffers |

The game image reads several of these directly: the timer values, the framebuffer size, `gScheduler` and its
queues, the threads it creates, the graphics task buffers, `gWaveSizes`. Those names are in the map's `# shared`
section.

## 6. On the PC

`CMakeLists.txt` compiles all of `src/rom` but the units port/ultra replaces (`GAME_REPLACED`): threads, message
queues, events, timers, interrupts, the VI/PI/SI/AI/SP managers and raw device access, cache and TLB operations,
the inflater and LZSS reader (zlib and C in `port/ultra/pi.c`), and the IDO libc (the host's is used). Compiled
natively: the timer, the scheduler, the video-mode code (its VI writes go to port/ultra), the `gu*` helpers and libm,
the Controller Pak file system and controller packing, and the whole sound library. The data stays at its N64
addresses in the RDRAM window for now ([DATA.md](../port/DATA.md)); wave 3 moves it into ordinary PC storage.

Things a reader may trip on:

- Several globals have two names at one address, and the map merges them: `gSeconds` (its other name looked like a
  libultra scheduler variable), `gFrameSeconds`,
 `gLastFrameCount`, `gRetraceRate`, `gRetraceTime`, `scheduler_retrace_count`, `model_thread`,
  the queues. None of the `__osSc*` names is libultra's.
- The boot segment's symbol tables (`asm/us/boot_tail/symbols.json`) still list about 140 invented names that no
  code uses (`gCameraMatrix`, `gHudEnabled`, `gTrackWeather`, `gSpeedometerValue`, ...). They describe nothing in
  this segment and should be dropped rather than renamed.
