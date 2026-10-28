# Audio: the task interpreter and output (port/audio)

Phase 5 of [PORT_PLAN.md](../PORT_PLAN.md). Written 2026-10-26 from the decompilation's C. No ROM bytes were read.

## 1. Findings

**The audio task is not an Acmd command list.** The task's microcode (0x8002A050, which
[memory_map.md](../memory_map.md) calls aspMain) runs Factor 5's **MusyX v1** ABI. Its data is an array of
*subframe descriptors* (SFDs), and `OSTask.t.data_size` is how many there are, not a byte count. The evidence is
all in `src/rom/lib_11640.c`, the MusyX synth driver in the boot segment:

| Fact | Where |
|---|---|
| `t.data_size = count`, and the writeback covers `count * 2576` bytes; 2576 = 0xA10 = a 0x10 header + 32 voices × 0x50 | `func_80011910` |
| two SFD arrays of `D_800382CE * 2576` bytes, a 664-byte (0x298) work block, a 72-byte (0x48) delay block | `func_80011104`, `func_80010E80` |
| a 0x50-byte voice record (`Command_80012D18`) and a 16-byte SFD header (`Block_80012D18`) | `func_80012D18`, `func_800139D4` |
| every SFD renders 192 stereo frames into a 768-byte AI buffer (192 × 4 bytes); `osAiSetNextBuffer(buf, 768)` | `func_80010A40`, `func_80013D70` |
| sample formats 0 (PCM16) and 3 (ADPCM: 40 bytes per 64 samples, a 0x100-byte codebook before the frames) | `func_80012D18` |

So there are no opcodes to implement. The interpreter implements the fixed MusyX v1 pipeline over these records.
Nothing else in `src/` builds an audio task: `func_80011910` is the only writer of `t.type = 2`. The second microcode
in the boot tail (0x80027EA0, 3,616 B) is referenced by no task, and is left alone.

The game asks for 22050 Hz (`func_80010840(22050, ...)` in `audio_init_sys/car_shadow_render.c`). It caps pitch
at 2.0 (`func_80014A0C`), so a voice's source always fits the microcode's 0x200-sample buffer. Rush 2049 never uses
the delay effect: nothing calls its setter (`func_80020528` → `func_80010E80`), and init passes a delay size of 0.
The stage is implemented anyway.

The data flow, from the asset survey ([ASSETS.md](ASSETS.md) §3.8) and the driver (the files themselves, and how
port/assets converts them: [MUSYX_DATA.md](MUSYX_DATA.md)):
- Bank sounds start in `func_80022874`, then `func_80016CF0`. The format is the top byte of the sample descriptor's
  length word. `data` is a cart address, `0xB0000000 | (0x39B40 + offset)`.
- Not being a RAM address, a cart address sets voice flag 1. The builder then fetches through two caches it fills
  by DMA from the ROM: the codebooks go to `D_8003833C` (`count × 256` bytes, `func_80012234`) and the 1,536-byte
  sample blocks to `D_8003834C` (`func_80012730`). The SFD's pointers point into those caches.
- RAM-resident sources are the CPU-filled stream buffers (`func_8001C580`: GSM speech and streams). They are native
  PCM16.
- The tasks are double-buffered. `func_80013DEC` builds this frame's SFDs and issues their DMA list
  (`func_80011D24`), then submits *last* frame's SFDs, whose DMA has already completed (`func_80011CD8`).

## 2. The ABI as the game uses it

`port/audio/musyx_abi.h` has every offset. These tables give each field with its writer in the game and what the
interpreter does with it.

**SFD (0xA10 bytes)**

| Off | Field | Game writer | Meaning |
|---|---|---|---|
| 0x00 | u16 count | `func_80012D18` (count++) | voices written; not read (the output pointer ends the list) |
| 0x02 | u16 delay index | `func_800139D4`: `D_800382EC / 192` | delay-line position, index × 192 |
| 0x04 | u32 release mask | `func_800139D4`: `1 << slot` per voice with `release_pending` | fold slot n's last output into the base volumes |
| 0x08 | u32 state | `D_800382E4` (664 bytes, zeroed once) | carried between tasks |
| 0x0C | u32 delay | `D_800382E8`, or 0 | delay-effect parameters |
| 0x10 | voices[32] × 0x50 | | |

**Voice (0x50 bytes)**

| Off | Field | Game writer (`func_80012D18`) | Meaning |
|---|---|---|---|
| 0x00 | s32 env[4] | `level << 16` | volume per channel (left, right, surround, aux); Q15 in the high half |
| 0x10 | s32 step[4] | `(target - level) << 16 / 192`, or 0 | added after each output frame (a linear ramp over the subframe) |
| 0x20 | u16 pitch frac | `(pos - floor(pos)) * 65536` | Q16 fraction of the first input position |
| 0x22 | u16 pitch | `rate` (Q4.12, ≤ 0x2000) | input samples per output frame |
| 0x24 | cat main: ptr1, ptr2, s16 size1, size2 | the window's data (cache or RAM) | main segment, two ranges concatenated; size1 = 0 in voice 0 means no voices |
| 0x30 | cat loop | | loop segment (when the window crosses the loop end) |
| 0x3C | u8 frames[2] | ADPCM `(padded + rem + 31) >> 5`; PCM 0 | ADPCM frames to decode; main = 0 selects PCM16 |
| 0x3E | u8 skip[2] | `address & 0x3F` (ADPCM), `& 3` (PCM) | first sample; ADPCM bit 5 starts at the block's second frame |
| 0x40 | u32 book / u16 count[2] | ADPCM `data` (or its cache node); PCM `padded` | codebook (8 × 16 s16), or PCM16 samples per segment |
| 0x44 | u32 output | 0, except `func_800139D4` on the last voice | 192 interleaved stereo s16 frames; ends the voice list |
| 0x48 | u16 end | 0x4000 (no end in the window), or `length + rem` | end point, from the main segment's base |
| 0x4A | u16 restart | `(loopRem + mode) \| 0x8000`, `rem + mode`, or `end - 0x20` | bit 15: in the loop segment; else from the main base |
| 0x4C | u16 | `padded * rate * 16 + frac` | not read |
| 0x4E | u16 start | `position - loop_start` (whole-loop window) | added to the first sample's offset |

**Delay effect (0x48, `AudioDelayState_80010E80`)**: line pointer, length (a multiple of 192), tap count ≤ 8,
feedback `feedback << 8`, tap delays (samples, `& ~3`), tap gains `gain << 8`, a u32 v1 doesn't read, and a 4-tap
feedback filter ({0, 0, 0, 32767} when mode ≠ 1).

**State (0x298, private to the interpreter, native byte order)**: last enveloped output per slot and channel
s16[32][4] @0x000, base volumes s32[4] @0x100, the previous subframe's surround mix s16[192] @0x110, and the
feedback filter's history s16[4] @0x290.

**Pipeline per SFD** (`port/audio/musyx.c`):

| Stage | Behavior |
|---|---|
| base | add `last[slot]` for each mask bit, then `base = base * 0xF850 >> 16` (all four channels) |
| init | aux = base aux; left = surround(prev) + base surround; right = −(surround(prev) + base surround); surround = 0 |
| load | PCM16: main at `0x200 − align4(count + skip)`, loop at 0; ADPCM: decode `frames[0]` × 32 at `0x200 − frames × 32`, loop frames at 0 |
| resample | pointer = base + skip (& 31 for ADPCM) + start; per frame: phase row `acc[15:10]`, pointer += `acc >> 16`, `acc = (acc & 0xFFFF) + pitch << 4`, if pointer ≥ end: pointer = restart + overshoot; `x = sat(Σ in[p+j] · lut[row][j] >> 15)` |
| envmix | per channel: `a = x · (env >> 16) >> 15`; `mix = sat(mix + a)`; `env += step`; the last frame's `sat(a)` goes to `last[slot]` |
| delay | wet = Σ taps `sat(wet + line[pos − delay] · gain >> 15)` (circular); left += wet, right += wet; `line[pos..] = sat(aux + FIR(hist, wet) · feedback)` |
| out | `out[2i] = sat(left + base left)`, `out[2i+1] = sat(right + base right)`, native s16 |

ADPCM frame layout: a 40-byte block holds two 32-sample frames. Bytes 0–3 and 4–7 are each frame's first two
samples as big-endian s16. Bytes 8–23 and 24–39 are each frame's header byte (book `(h >> 4) & 7`, shift `h & 15`)
and 15 bytes of nibbles, high nibble first. Residual = `(s16)(nibble << 12) >> shift`. Groups are samples 2–7, 8–15,
16–23 and 24–31. Within a group:
`out[i] = sat((r[i] << 11 + c0[i]·p2 + c1[i]·p1 + Σ_{j<i} c1[i−1−j]·r[j]) >> 11)`, where c0 and c1 are the book's
two 8-entry vectors and p2 and p1 the two samples before the group. Frames are self-contained, with no state
between them.

## 3. Implementation

| File | |
|---|---|
| `musyx_abi.h` | the records: N64-form offsets, native-form structs (`PcMusyxSfd`, `PcMusyxVoice`, `PcMusyxCat`, `PcMusyxDelay`) |
| `musyx.c` | the interpreter; `pc_audio_run_task` |
| `musyx_lut.c` | the resample table |
| `audio_out.c` | the queue: `pc_audio_init/shutdown/set_frequency/push/queued_frames/pull`; the volume |
| `resample.c` | device-rate conversion: polyphase windowed sinc, 32 taps, 256 phases, Kaiser β 6, cutoff 0.45 × the lower rate |
| `out_wav.c` | WAV writer (header rewritten about every second) and reader (tests) |
| `out_sdl.c`, `out_alsa.c` | device backends, compiled only when CMake finds SDL2 or the ALSA headers |
| `pc_audio.h` | port/audio's API beyond `pc_port.h`: form selection, `pc_audio_pull`, stats |

**Two forms.** CMake cache variable `PC_AUDIO_TASK_FORM` (default `NATIVE`) sets the form; `pc_audio_set_task_form`
changes it at run time.
- `NATIVE` is the game's own structs compiled for the host: the same members in the same order, pointers widened
  and naturally aligned. That is 120 bytes per voice and 3,864 per SFD on LP64. On a 32-bit host it is exactly the
  N64 layout.
  - A pointer field holds a host pointer or, below 4 GB, an N64 address resolved with `pc_n64_ptr`: the RDRAM
    window at host 0x80000000, K0, K1 or physical.
  - This relies on the executable being PIE, with no host data below 4 GB outside the window (the game's
    pointer-free data is in it, at its N64 addresses: DATA.md §3). The forms test asserts it.
- `N64` is the 0xA10 and 0x50 layouts with 32-bit addresses. Multi-byte fields are big-endian when the record is in
  a `pc_mem_is_be` range, native otherwise.

The interpreter decodes either form into the same internal record. Nothing writes back into the records.

**Byte order**:
- ADPCM frames are bytes.
- PCM16 sources and codebooks are big-endian inside a `pc_mem_is_be` range (checked per source range) and native
  elsewhere.
- The state block and delay line are native: only the interpreter reads them.
- Output is native interleaved s16, left first.

**Resample table.** At the first task, `musyx_lut_init` scans the player's ROM at the task's microcode data (ROM
0x2EE10, 0x230 bytes) for a table of the right shape. It accepts one whose 64 rows of 4 big-endian taps each sum to
about 1.0, whose tap 1 dominates at phase 0 and falls with the phase, and whose tap 2 rises to dominate at phase 63.
If none is found, it uses this port's own table, a Hann-windowed sinc low-pass at 0.8 × Nyquist. Like the N64's,
that table smooths even at phase 0. Nintendo's table is not in the repository. The log line
`audio: resample table: rom+0x...` or `computed` says which was used.

**Output.** `pc_audio_push` takes the game's AI buffers at the AI rate. It tees them to the dump file, converts them
to the device rate, and appends them to a one-second ring; when the ring is full, the oldest frames go.
`pc_audio_queued_frames` reports game-rate frames, and `pc_audio_target_frames` how many the AI keeps queued
(section 7). The backends:

| Backend | Behavior |
|---|---|
| `null` | drains the ring by the wall clock, so queue lengths behave as with a device |
| `wav` | `null`, plus the dump file (which it requires) |
| `manual` | holds frames until `pc_audio_pull` (tests) |
| `sdl` | `SDL_OpenAudioDevice`; the callback pulls; the frequency may change, and SDL converts format and channels |
| `alsa` | `snd_pcm_set_params` with `soft_resample`; a writer thread pulls 10 ms periods |
| `auto` | SDL2, then ALSA, then null |

Without `pc_audio_init`, the first call initializes from the environment:
- `RUSH2049_AUDIO` sets the backend; `RUSH2049_DUMP_AUDIO` sets the dump path.
- `RUSH2049_AUDIO_RATE` forces a device rate.
- `RUSH2049_ALSA_DEVICE` names the ALSA device (`default` otherwise).

**The volume** (2026-10-27). `pc_audio_set_volume(v)`, v from 0 to 1, is `audio.volume / 100`: main sets it at
start and the settings overlay as it changes ([FRONTEND.md](FRONTEND.md)).
- The gain is v², so the slider's middle is about -12 dB and 10 about -40 dB, in Q16 under the queue's lock.
- It scales what `pc_audio_pull` returns, so it applies to the device backends at once, and the dump file keeps
  what the game played. At 1 the samples are untouched.
- It holds across `pc_audio_init`. The backend itself changes only at start: `pc_audio_init` shuts the output down
  and opens it again, which isn't safe while the game's threads may call into port/audio, so the overlay marks
  `audio.backend` as needing a restart.

## 4. Tests

```
cmake -S . -B build/pc -G Ninja && cmake --build build/pc --target pcaudio_tests -j3
ctest --test-dir build/pc/port/audio          # or standalone: cmake -S port/audio -B build/audio
```

The tests link their own copies of the port/ultra functions (`tests/stubs.c`), including a fake 4 MB RDRAM at
0x80000000 for N64-form records.
- The references in `tests/ref.c` are written from the format, separately from the interpreter: a per-frame ADPCM
  decoder over a flat frame view, a block packer, and a closed-loop ADPCM encoder.
- `tb_voice` copies the game's voice builder (`func_80012D18`) and its position update (`func_80011D74`), so the
  tests drive the interpreter with records built the way the game builds them.

| Test | Checks | What it verifies |
|---|---:|---|
| `audio_adpcm` | 21 | residual nibbles, sign and shift; book selection (an integrator book); block layout and skip ≥ 32 against the reference; zero bytes past the source; encode and decode a sine with an order-2 predictor book: SNR 44.4 dB |
| `audio_pcm` | 14 | closed-form resampling at three pitches; segment placement for each skip; pitch 2.0 and 0.5; end and restart into the loop segment and inside the main segment; start offset; big-endian source; split source ranges |
| `audio_resample` | 17 | the table from a fake ROM (found at +0x20) and rejection of random bytes; computed rows sum to 1.0; DC exact at every phase; output frequency = input × pitch at five pitches; **bit-exact continuity across subframes and tasks** (1, 2 or 3 SFDs per task) for looping PCM16, a loop shorter than the window, and looping ADPCM, against one continuous reference stream |
| `audio_envmix` | 10 | up and down ramps against the closed form; ramps joined across subframes (largest step 86); saturation; last sample per slot and channel; aux silent without the effect |
| `audio_mix` | 16 | empty SFD; the voice list ends at the output voice; release-mask fade (0xF850 per subframe) across SFDs and tasks; an empty SFD keeps slot records; surround one subframe late and antiphase, across tasks; surround in the base; 6 SFDs in one task = 2 + 4 |
| `audio_delay` | 10 | echo at the tap delay and gain; feedback halving per pass; taps wrapping the line; filter history carried between subframes |
| `audio_forms` | 32 | one scene (big-endian PCM, ADPCM with a big-endian book, native PCM, release mask, delay effect) rendered as native structs, native structs with K0 addresses, N64 form native order, and N64 form big-endian: identical outputs, states and lines; LP64 layout; host data above 4 GB |
| `audio_output` | 40 | queue counts, pulls, underruns (the fade out and in, one underrun per dry spell), overflow; the AI's target from pulls, its growth per underrun and 200 ms cap, the `audio.latency` floor; idle and paused silence apart from underruns; the pull hook; buffers played again; 22050 → 48000 frequency, amplitude, ratio, chunked = whole; 48000 → 22050 alias rejection; a forced device rate; WAV round trip; the null backend's clock |
| `audio_e2e` | 14 | four SFDs per task from the game's builder, `pc_audio_run_task`, `pc_audio_push`, the WAV read back. ADPCM 1 kHz at 32 kHz, played at 22050: 999.9480 Hz (expected 999.9481), left 9964.8 (expected about 9960), right/left 0.5, residual 0.19 % rms, worst 0.6 %. PCM16 big-endian 750 Hz at 24 kHz: 749.9611 Hz (expected 749.9611), right inverted at half volume |
| `audio_backends` | 9 | only when SDL2 or ALSA is built: SDL2's `disk` driver gives a 440.000 Hz tone of the right length; ALSA opens alsa-lib's `null` device and drains |
| `audio_volume` | 14 | the master volume through the manual backend's pulls: 1 leaves the samples as they were, 0.5 gives a quarter (the extremes 8191 and -8192), 0 silence, values past either end clamp, and it holds across a re-init |

Result on the Pi 5: 9 of 9 tests pass (159 checks). With SDL2 2.26.5 and ALSA 1.2.8 headers unpacked locally
(not installed, not committed), 10 of 10 pass (168 checks).

A mutation check broke 11 points in `musyx.c`, one at a time: surround sign, decay constant, nibble order, phase
bits, restart segment, in-group predictor index, surround carry, tap direction, filter history, ADPCM skip mask,
and the empty-SFD shortcut. The tests caught all 11.

## 5. What's approximate or unverified

- **No reference output exists here.** There is no ROM, microcode or LLE run. The game's C gives every field's
  meaning, and the record sizes match: 0xA10, 0x50, 0x48 and the 0x298 state block. The DSP behavior the C doesn't
  show follows the MusyX v1 behavior the N64 emulation community reverse-engineered, whose public description is
  mupen64plus-rsp-hle's `musyx.c` (GPL). No code was taken from it; this implementation is written from that
  behavior and the game's records. That covers:
  - the ADPCM frame layout inside a block and the predictor
  - the 4-tap resample table's shape
  - the 0xF850 decay of the base volumes
  - the surround channel delayed a subframe and mixed antiphase
  - the delay effect's mixing
  - the state block's layout
- **Rounding.** The arithmetic is Q15 products, arithmetic shifts and saturation per stage. The RSP's vector
  rounding and its exact clamp points are not reproduced, so outputs may differ from hardware by an LSB or so.
- **Resample table.** The ROM search is tested only against a fake ROM. Without a match, the computed table is close
  to the N64's in character but not equal to it.
- **ADPCM against real data.** The format is self-consistent (44 dB round trip) but untested on the game's samples.
  With a ROM, decode a few samples from 0x39B40 first: a wrong frame layout would be obvious.
- **Output.**
  - Real time is section 7: what is left is the game's own limits there.
  - For the `null` and `manual` backends, a frequency change clears the queue.
  - The WAV dump keeps its first rate.
  - On a device that never blocks (alsa-lib's `null`), the ALSA thread spins.
  - `pc_audio_init` isn't thread-safe: call it from main before the game threads start.

## 6. What the game and port/ultra must do on PC

**Game C (agent D):**
1. **Compile `src/rom/lib_11640.c`.** The top-level CMake lists it in `GAME_REPLACED` as libultra, but it is the
   MusyX synth driver and task builder. It needs only `__unaligned` removed under `TARGET_PC` (`func_800148F8`);
   with that, it compiles.
2. **Native form (the default):**
   - Make `Command_80012D18.unknown44` a pointer (`void *output`).
   - Replace the byte-array views in `func_800139D4` with the real types. `AudioBlock`/`AudioCommand` become
     `Block_80012D18`/`Command_80012D18`: `status` is `main.first_length`, `output` is the 0x44 pointer.
     `AudioLoop` becomes `AudioDelayState_80010E80`, since `length` moves to +8 on LP64.
   - Use `sizeof(Block_80012D18)` for 2576 (`func_80011104` ×2, `func_80011910`) and
     `sizeof(AudioDelayState_80010E80)` for 72. The 664-byte state block stays 664.
   - Alternatively, keep the N64 records with `u32` pointer fields holding window addresses and build with
     `-DPC_AUDIO_TASK_FORM=N64`.
3. **The RAM test in `func_800146B4`**, `(data & 0xF0000000) == 0x80000000`, must also count host pointers
   (≥ 4 GB) as RAM, or a host buffer is fetched as a cart address. `func_80014CAC` has the same top-byte test.
4. The other 64-bit casts in the file can stay as they are or become `uintptr_t`:
   - the D-cache cursors `D_80038030`/`D_80038034` with `func_8002C5E0` (the writebacks are no-ops)
   - `(unsigned int)V->data` on the streamed path, which is a cart address and fine as `u32`
   - the ucode sizes in `func_80011910`
5. The two views of the 0x68 voice record (`AudioState`, `Voice_80012D18`) still agree on LP64: both shift by 4
   at `data`.

**port/ultra (agent A):**
1. `osSpTaskStartGo` with `t.type == 2` calls `pc_audio_run_task(t.data_ptr, t.data_size)`, then signals SP done.
2. `osAiSetFrequency(f)` calls `pc_audio_set_frequency(f)` and returns `f`; the game uses the return value as its
   rate.
3. `osAiSetNextBuffer(buf, len)` calls `pc_audio_push(buf, len / 4)`.
4. `osAiGetLength` returns `pc_audio_queued_frames() * 4`.
5. Post `OS_EVENT_AI` whenever the queue falls below about two buffers (384 frames). The audio thread
   (`func_80010A40`) answers each event with one 192-frame buffer.
6. The MusyX DMA callback (`D_80038010` → `func_8008ABE4`) copies from the ROM. Register its destinations as
   big-endian: the codebook cache `D_8003833C` (`count × 256`) and the sample cache `D_8003834C` (`count × 1536`).
   Do **not** register the CPU-filled stream buffers (`func_8001C580`); they are native PCM16.
7. main calls `pc_audio_init(backend, dump_path)` (`--dump-audio`) before the game threads start and
   `pc_audio_shutdown()` at exit. Link the `pcaudio` target. Weak no-op defaults are needed for the two new
   functions.

**Status (2026-10-27).** The game's side reaches the interpreter: the MusyX service table and the DMA request
list ([MUSYX_DATA.md](MUSYX_DATA.md) §8.1), the voices and sequencer (§8.2, §8.3), and the scheduler's task view
(§8.4). The DMA callback is `D_80038010` = `struct_init_and_call`, which starts `func_8008ABE4`'s PI requests. On a
blank cartridge with the MusyX tuning table planted in a debugger (§8.6), the retrace callback `func_80013DEC`
submits an audio task every retrace (470 in 8 s) through the scheduler to `pc_audio_run_task`.

**pc_port.h changes:**
- added `pc_audio_init(const char *backend, const char *dump_wav_path)` and `pc_audio_shutdown(void)`
- renamed `pc_audio_run_task`'s size argument `data_size`, documented as the SFD count
- unchanged: the `PcGfxWords` comment says "(and Acmd)", but the game has no Acmd

## 7. Real time: pacing, underruns, latency

Written 2026-10-27, after the first play on Windows ("the sound is terrible"). Nobody had listened before, and two
real-time faults showed in runs on the Pi with SDL's `disk` driver (`SDL_AUDIODRIVER=disk`, which consumes in real
time, headless), counted by the gap search in the output and by tracing which ring buffer each push and each task
wrote. Both faults sound as crackle and stutter, worse with a vsync swap:

1. **The queue ran dry.** The AI kept 384 frames (17 ms) queued, and only the main loop sent AI events. A device
   pulls in steps (SDL: 512 frames, 10.7 ms at 48 kHz) from its own thread, and the main loop waits in the swap
   (vsync, up to a refresh) and in graphics tasks, so the queue emptied between its checks: silence in steps of a
   pull, played as clicks.
2. **The AI played buffers twice.** The game keeps 8 AI buffers in a ring (`func_80010A40`); its retrace callback
   (`func_80013DEC`) renders the ones the AI has passed, up to `D_800382CE` SFDs per task, with the task one
   retrace after the SFDs were built. The audio task ran on the main thread, behind graphics tasks and swaps; when
   it was late, the AI pushed a buffer whose task hadn't run yet, and that buffer's sound from the ring's last turn
   (8 buffers, 70 ms back) played again.

**What the port does now:**

| | |
|---|---|
| audio tasks | run at once, in `osSpTaskStartGo`, on the game's scheduler thread (ULTRA.md), as the RSP runs them beside the CPU; the scheduler takes the SP event from its queue after the call |
| AI events | while the queue and the events not yet answered are short of the target; up to 3 outstanding (the game's queue holds 4) |
| who checks | the main loop, and in real time the device's thread after each pull (`pc_audio_set_pull_hook`); with `--fast` the main loop alone, so a run stays the same every time |
| the target | 384 frames until a device pulls; then two pulls, a retrace and a buffer (SDL at 48 kHz: 1029 frames, 47 ms); one buffer more at each underrun, up to 200 ms; `audio.latency` (ms) sets a floor |
| an underrun | the last frame fades to silence over 32 frames, then zeros; the sound after it fades in over 32: no repeated or torn buffer, no click |
| the game held | (`hold()`: the pause key, the F1 overlay) the target is 0: the queue plays out, and its silence isn't an underrun |
| idle | silence while the game pushes nothing for 0.25 s (before its first buffer, a long load) is counted apart |

The latency the port adds is the target: about 45-50 ms with a 10 ms device period, on top of the game's own ring
(up to 6 buffers, 52 ms, as on the N64) and the device's buffer.

**The log.** At exit (`pc_audio_shutdown`), one line:

```
audio: sdl at 48000 Hz: 41.7 s played; 0 underruns (0 ms of silence), 6 buffers played again, 0 ms idle,
0 frames dropped; pulls of 512 frames, queue target 1029 frames (47 ms), at most 55 ms queued
```

and while playing, the first 10 underruns each as `audio: underrun N, T s in: the queue target is now F frames`.
"Played again" counts pushes of a buffer no task had rendered since its last push (`pc_audio_note_rendered`).

**Measured** on the synthetic cartridge (silent, but the AI, the ring and the tasks run as with sound), the Pi 5,
SDL 2.30.9's disk driver at 48 kHz, 40 s of `race.press` in real time: 0 underruns, 6 buffers played again. The
same checks on the retail cartridge, also with the main loop slowed by a sleep after every present (roughly a
swap that blocks for vsync), stay out of the repository.

**What is left: the game's limits.**
- The renderer's rate. It renders at most `D_800382CE` buffers a retrace (from the rate and a tuning value in the
  boot data), little more than the AI plays at 60 Hz (114.8 a second: 22047 Hz over 192): a host that keeps the
  retrace rate low plays some buffers twice. The status lines' `vi=` counts show the retrace rate.
- Loads. The scheduler doesn't preempt a computing thread (ULTRA.md): while a game thread computes for tens of
  milliseconds without a libultra call, the retrace callback can't render, and buffers can play twice at screen
  changes and loads. On the N64 the retrace interrupt would run it.
- Holding AI events back until the next buffer is rendered was tried and dropped: the renderer only renders the
  buffers the AI has passed, and at a re-init (a race's start) the game waits for the AI, so the game hung.
