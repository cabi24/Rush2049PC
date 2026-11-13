# MusyX sound data on the PC (port/assets/musyx.c)

Phase 4 loaders, part 3 ([ASSETS.md](ASSETS.md) §3.8, §5). Written 2026-10-27 from the decompilation's C. No ROM
bytes were read: the formats below are what the game's code reads, and the tests build synthetic files from them.

## 1. Summary

- **Four file kinds** feed the sound library in the boot segment: the project (wave 6), the pool (7), the sample
  directory (8) and twelve songs (10–21). `AudioInitSys` loads 6–8 at boot and pushes the
  sfx groups 12–15; the song start (`AudioSongPlay`) loads wave `song + 10`. The samples stay in the ROM at
  0x39B40 and are DMA'd on demand.
- **Strategy: swap in place.** `pc_assets_musyx` swaps every field to host order at the width the library reads
  it, before anything parses the file. The files keep their N64 size and layout, so the record strides and
  offsets the library hard-codes still hold. What the library keeps about them it keeps in its own records, with
  host pointers (section 4, since [NATIVE.md](NATIVE.md) step 9); the one address it writes into a file, a sample's,
  is the sample's 32-bit cartridge address.
- **Why not big-endian accessors** in the library: the readers are spread over five units (about 90 functions
  in `groups.c`, `registry.c`, `sequencer.c`, `synth.c`, `macros.c`), two of them reserved by other
  work, and N64 neutrality allows no line changes in function bodies. Almost every reader uses whole-field
  loads; section 4 lists the few that don't, and each is handled by how the converter swaps or by one
  file-scope `TARGET_PC` change.
- **Game edits** (all `#ifdef TARGET_PC` at file scope, `n64_neutral.py`: 0 FAIL against `HEAD`, 'shifted' for the
  three units): the sample record keeps 0x1C bytes; the keymap and macro registries keep 8-byte records; the
  sample-info records agree with `sound_hw_set_voice_sample`'s view; the WAITMS step sets its flag on the word, not byte 6.
- **Boot.** The library's own state is LP64-clean (section 8): the service table, the DMA request list, the
  sequencer slots, the voices, the streams, and on the way the audio manager's request list and the scheduler's
  task view. `--no-rom` now runs sndInit and sndPushGroup and stops in AudioSfxInit on a zeroed boot-data table
  (section 8.6); with that table planted in a debugger it runs into the game loop, with an audio task every retrace.

## 2. Load path and pointers

| Step | Where | What |
|---|---|---|
| load | `_ReadWave` (`waves.c`) → `pc_wave_loaded` → `pc_assets_musyx` | the file, in a locked heap block, converted once per read; the read's big-endian registration is dropped |
| addresses kept | `AudioInitSys` | `musyx_project` project, `musyx_pool` pool, `musyx_sdir` sample directory (pointers), `musyx_samples` the sample bank's ROM offset (`SAMPLE_BANK_ROM_OFFSET`, a u32) |
| push | `sound_push_group` (sndPushGroup) | walks the project's groups; registers the group's macros, tables, keymaps, layers (pool entries), samples (directory records) and sfx table by id |
| registries | `registry.c` BSS (own storage) | `ResourceRecord {void *payload; u16 id; u16 references}`: macros `sound_macros` (in 512 buckets by `id >> 6`), tables `sound_tables`, keymaps `sound_keymaps`; `LayerRecord` (+ count): layers `sound_layers`; sfx tables `sound_sfx_tables`, sample directories `sound_sample_directories` (`{records, cart_base, count}`); groups pushed `sound_groups` |
| look up | `sound_find_macro` macro, `sound_find_table` table, `sound_find_keymap` keymap, `sound_find_layer` layer, `sound_find_sfx` sfx entry, `sound_find_sample` sample | bsearch (`sound_bsearch`) by id |
| play | macro interpreter `sound_macro_run` (voice `sound_synth_voices`, macro pointers), sequencer `sound_seq_start` (slot `sound_seq_players`, pointers into the song) | |
| samples | `sound_add_sample`: record `cart_address = cart_base + offset`; `sound_find_sample` → macro 0x10 → `sound_hw_set_voice_sample` (`SoundSampleInfo`) → voice; `sound_hw_build_voice_command` builds DMA requests `{length, source, destination}` from the voice's `cart_address` | cartridge address 0xB0039B40 + offset (`sound_hw_cart_address` makes the bank's ROM offset 0x39B40 the cartridge address, through `musyx_cart_address`); codebook 0x100 bytes at it, 40-byte ADPCM blocks from it + 0x100; port/ultra copies from the ROM into the big-endian caches, port/audio reads them |

Every pointer into a file is a host pointer, held in a pointer field (the registries, the voices' macro pointers,
the sequencer's song pointers: section 4). The sample addresses are cartridge addresses (0xB0000000 | offset), a
number of the cartridge's bus, never memory: 32-bit fields, `cart_address`.

## 3. Formats

Offsets are from the start of the record. All multi-byte fields are big-endian in the ROM. "Swapped" means at
that width; bytes are never moved.

### 3.1 Project (wave 6)

Groups follow each other until a size of 0xFFFFFFFF (`sound_push_group`, `sound_pop_group`, `sound_start_song_in_group`).

| Off | Field | Readers | Swapped |
|---|---|---|---|
| +0 | u32 size (next group at +size); 0xFFFFFFFF ends | sndPushGroup (also read as the end test) | u32 |
| +4 | u16 id | sndPushGroup, `sound_start_song_in_group` | u16 |
| +6 | u16 type: 1 sfx, 0 songs | same | u16 |
| +8 | u32 offsets[], from the group + 8: [0] macro ids, [1] sample ids, [2] table ids, [3] keymap ids, [4] layer ids; type 1: [5] sfx table; type 0: [5] normal page, [6] drum page, [7] MIDI setups | same (`ResourceGroup`, `ResourceHeader`) | 6 or 8 × u32 (5 for other types) |

| Structure | Layout | Readers | Swapped |
|---|---|---|---|
| id list | u16 ids ended by 0xFFFF | `sound_register_macros`/`F80`/`FEC`, `sound_register_layers`, `sound_register_samples`; pop: `sound_unregister_macros` … `sound_unregister_samples` (walks the sample list backwards) | u16 each |
| sfx table | u16 count, u16, then count × 12: u16 id, u16 macro/keymap/layer id, u8 max voices, u8 priority, u8 volume, u8 pan, u8 key, u8 group (byte 9, **written**: 31 by `sound_add_sfx_table`, the channel by `sound_set_sfx_volume_group`), 2 bytes | `sound_register_sfx_table`, `sound_add_sfx_table`, `sound_find_sfx` (bsearch on the u16 id, stride 12), `sound_start_sfx_unlocked` | u16 at +0, +2 per entry |
| page (normal, drum) | 8-byte entries {u16 macro; u8 priority; u8 max voices; u8 slot; u8 program; 2 bytes} until program (byte 5) = 0xFF | `sound_seq_build_program_map` (byte 5 only, for the end too), `sound_seq_start`, `sound_seq_set_program` | u16 at +0, not in the end entry |
| MIDI setup | 132-byte records {u16 song id; u16; 16 × {program, volume, pan, reverb, chorus, 3 bytes}} until id 0xFFFF | `sound_start_song_in_group` (by id), `sound_seq_start` | u16 at +0, +2 |

### 3.2 Pool (wave 7)

`+0 u32 offsets[4]` from the pool start (`sound_find_pool_macro` … `sound_find_pool_layer`) to four lists. An entry is
`{u32 size; u16 id; u16; payload}`, the next one at +size, and a size of 0xFFFFFFFF ends the list
(`sound_find_pool_entry`).

| List | Payload | Readers | Swapped |
|---|---|---|---|
| 0 macros | at +8: 8-byte steps, two u32 words; opcode = word 0 & 0x7F, parameters in bit fields of both words | `sound_macro_run` copies each step word by word into `cstep`; every handler reads the copy (`macros.c`) | u32 × 2 per step |
| 1 tables | at +8: bytes (volume curves, 128 entries: `sound_macro_apply_curve`), or an envelope of four u16 (attack, decay, sustain, release) stored little-endian, which `sound_macro_set_adsr` (macro 0x0C) reads as u16 and byte-swaps | as listed | envelopes (the ids that a step 0x0C names, bits 8–23): u16 × 4; curves: none |
| 2 keymaps | at +8: 128 × {u16 macro/layer id; s8 transpose; u8 pan; s16 priority; 2 bytes} | `sound_synth_start_note` | u16 at +0, +4 per entry |
| 3 layers | header +8 u16, **+0xA u16 count** (`sound_register_layers`), payload at +12: count × {u16 id; u8 low key; u8 high key; s8 transpose; u8 volume; s16 priority; u8 pan; 3 bytes} | `sound_add_layer`, `sound_find_layer`, `sound_synth_start_layer` | u16 at +8, +0xA; per entry u16 at +0, +6 |

### 3.3 Sample directory (wave 8)

0x1C-byte records until id 0xFFFF (`sound_add_sample_directory`, `sound_add_sample`, `sound_remove_sample`, `sound_find_sample`; bsearch on
the u16 id, stride 0x1C):

| Off | Field | Swapped |
|---|---|---|
| +0 | u16 id | u16 |
| +2 | u16 reference count, **written** | u16 |
| +4 | u32 offset from the sample base | u32 |
| +8 | u32 cartridge address, **written**: the directory's cartridge base + offset (`void *` in the N64 C, `cart_address` on the PC) | u32 |
| +0xC | u32 info: key << 24 \| rate (macro 0x1F, `sound_macro_set_pitch`) | u32 |
| +0x10 | u32 length: format << 24 \| samples (format 0 PCM16, 3 ADPCM) | u32 |
| +0x14, +0x18 | u32 loop start, u32 loop length | u32 |

### 3.4 Songs (waves 10–21)

Offsets are from the song start; the sequencer adds them to the song's pointer (`(u8 *)song + offset`).

| Structure | Layout | Readers | Swapped |
|---|---|---|---|
| header | +0 u32 track table, +4 u32 pattern table, +8 u32 channel map, +0xC u32 tempo track (0 none), +0x10 u32 tempo, +0x14 u32 loop time | `sound_seq_start` (`Arr`), `sound_seq_play_tracks` (`SequenceHeader`) | u32 × 6 |
| track table | 64 × u32 (0 = no track) | `sound_seq_start` | u32 |
| track events | 12 bytes {u32 time; u8 program; u8 controller; u16; u16 code; u16 index **or** s8, s8}: code 0xFFFF ends, 0xFFFE loops to event *index*, else a pattern index with the two bytes (transpose, velocity offset) | `sound_seq_play_tracks` (reads the end event's time and code too) | u32, u16 at +6, +8; +0xA as u16 only after 0xFFFE |
| pattern table | u32 offsets, indexed by event codes | `sound_seq_play_tracks` | u32 (the entries some event uses) |
| pattern | {u32; u32 pitch-bend stream; u32 modulation stream (0 none)} then note events | `sound_seq_play_tracks`; the streams are bytes (`sound_seq_play_pitch_bend_streams`, `sound_seq_play_modulation_streams`) | u32 × 3 |
| note events | u16 delta time, u8 key, u8 velocity, then u16 duration for notes: key bit 7 with velocity 0 (program), 1 (volume) or bit 7 (controller) is 4 bytes, key = velocity = 0 is 4 bytes, anything else 6; key = velocity = 0xFF ends (its delta is read) | `sound_seq_play_note_events` (`*(u16 *)events`, `*(u16 *)(events + 4)`) | u16 at +0, and +4 for notes |
| channel map | 64 bytes | `sound_seq_play_tracks` | none |
| tempo track | {u32 time; u32 tempo} until time 0xFFFFFFFF | `sound_seq_play_tempo_track` | u32 × 2 (time only for the end) |

## 4. What the audit found beyond whole-field loads

| Reader | What it does | Handled by |
|---|---|---|
| `sound_macro_wait_ms` (macro 0x07, WAITMS) | `((u8 *)command)[6] = 1`: byte 6 of the step copy is bits 15..8 of word 1 on the N64, the milliseconds flag `sound_macro_wait` tests | `macros.c`: a PC version stores the same byte by value, `word[1] = (word[1] & ~0xFF00) \| 0x100` |
| `sound_macro_set_adsr` (macro 0x0C) | reads the envelope as u16 and swaps each: the table is little-endian in the file | the converter swaps those u16 (found from the steps 0x0C name), so the native read returns the N64's value and the code's swap gives the same result |
| `sound_register_layers` | layer count as a u16 at +0xA (the low half of a u32 at +8, if the tool wrote one) | swapped as two u16 |
| `sound_seq_build_program_map` | page end by byte 5, before the u16 at +0 | the end entry's u16 is left alone |
| `sound_seq_play_tracks` | event +0xA as a u16 for 0xFFFE, as two bytes otherwise | swapped by event code |
| `sound_seq_play_note_events` | variable-length note events | walked with the game's rule |
| `sound_add_sfx_table`, `sound_set_sfx_volume_group` | write byte 9 of sfx entries | a byte |
| `sound_add_sample_directory`, `sound_add_sample` | write the sample record's count and address | swapped fields, written natively afterwards |
| `sound_synth_start_note` | `keymap[key].panning` with the unmasked key: for sfx (`sound_start_sfx_unlocked` passes key \| 0x80) it reads byte 3 of entry 128 + key, past the keymap | identical where that byte is a byte field; it differs only if entry 128 lands on the next entry's u32 size (key 0 after a keymap of exactly 128 entries) |

**Pointers into the files**, and how they fit on LP64 (since [NATIVE.md](NATIVE.md) step 9, section 14 there):
- the registries (`registry.c`): `ResourceRecord {void *payload; u16 id; u16 references}` for macros, tables and
  keymaps, `LayerRecord {void *payload; u16 id; u16 count; u16 references}` for layer lists, 16 bytes each,
  searched as those types (the comparators take the records). The data loader gives the four registries own
  storage of the host layout, each its N64 room (2048, 256, 256 and 2048 records). Before, the payload was a u32
  window address, so the records kept the N64's 8 bytes and the search its literal stride of 8, with the id at +4.
- sample record `cart_address`: a u32 cartridge address (`sound_sample_record.h`, and the copy in `groups.c`), so the
  record stays 0x1C bytes for the walks and the bsearch. With `void *`, the second record was read 4 bytes off.
- sample info: one record, `sound_sample_record.h`'s `SoundSampleInfo`, which `sound_find_sample` fills,
  `sound_macro_start_sample` passes, `sound_stream_start` fills for a stream and `sound_hw_set_voice_sample` reads: a
  sample in the cartridge by its `cart_address`, one in RAM (a stream's buffer) by its `data` pointer. The hardware
  voice keeps both (`SoundHwVoice.cart_address` beside `data`) and fetches a cartridge sample through the caches by
  its cartridge address; before, one pointer-sized field held either, and a range test (below 4 GiB and not KSEG0)
  said which.
- sfx tables, sample directories and pushed groups are native pointers in their own storage (gen_data gives
  pointer-bearing tables host-sized storage); so are the sequencer's slots (section 8.2) and the voices' heads
  (section 8.3).

The sample base: `AudioInitSys` hands `sound_push_group` the sample bank's ROM offset (`SAMPLE_BANK_ROM_OFFSET`,
0x39B40), and `sound_hw_cart_address` makes it the cartridge address 0xB0039B40 through the game's service
`musyx_cart_address` (a cartridge address is taken as it is); the records hold 0xB0039B40 + offset. The N64 passed
the cartridge address itself (the absolute alias `sample_bank_rom`, which the PC took through `PC_N64A`, and before
that through `(s32)`, sign-extended).

## 5. The converter

- One map byte per file byte records what was swapped where. A field reached again (tracks sharing events or
  patterns, groups sharing id lists) is read natively and left; one that overlaps something swapped at another
  width, or a byte read as a byte, is a parse error. Every offset is checked against the block.
- A record that doesn't parse ends that walk and is logged once per file:
  `rush2049pc: musyx: wave N: <record> at 0x... doesn't parse; that walk stops there`.
  `RUSH_TRACE_WAVES=1` adds a line per file with its record counts.
- Pool order: macros first, since their steps say which tables are envelopes.
- The block's big-endian registration (made by the PI DMA or the decompressor) is dropped after conversion.
- Blank cartridge: `--no-rom` gives every wave a 0x200-byte raw file of zeros (`blank_files` in `port/main.c`),
  instead of zero-size blocks that alias the next heap block. An all-zero MusyX file, which no real one is
  (sndPushGroup would loop on it), becomes the empty file of its kind: project `FFFFFFFF`, pool four offsets to
  one `FFFFFFFF`, directory `FFFF`.

## 6. Tests

`assets_musyx` (`port/assets/tests/test_musyx.c`):
1. Synthetic big-endian project, pool, directory and song, built field by field with each field's width and
   value logged. After conversion every logged field reads natively as its value and every other byte is
   unchanged. The song has two tracks sharing their events and a pattern used twice, a loop event followed by
   bytes that are not events, every note-event kind including key bit 7 with a plain velocity, and a tempo track.
2. Damaged files: cut at seven points each, 64 garbage blocks, a group whose list offset points at its own
   header, a zero-size block, and all-zero (blank) files. Nothing is written past a block.
3. The game's own readers on the converted files, in host memory (every record holds pointers): `groups.c`,
   `registry.c`, `sequencer.c`, `util.c` and `macros.c` built with the game's flags
   (`test_musyx_game`), with stubs for the rest of the library. sndPushGroup of an sfx group and a song group;
   every lookup (macros, tables, two keymaps, layers, sfx entries, samples, with the sample bank's ROM offset);
   the song start through `sound_seq_start` (pages, MIDI setup, track table, tempo track); macro steps 0x0C
   (envelope values), 0x10 (sample address, length, format, loop, offset as `sound_hw_set_voice_sample` receives them) and
   0x07 (250 ms = 64000 clock units, against 7812 for ticks); sndPopGroup twice, with the shared macro kept
   until the second; a blank project.

A mutation check broke the converter at 12 points (layer count, keymap priority, envelopes, loop end, note
length rule, directory stride, sfx header length, page end, tempo values, event +6, the BE registration, song
header length) and reverted each of the 6 game edits in turn: the test caught all 18.

Part 4, the library's state on LP64 (section 8.5), also links `voices.c` and the DMA service's two units.

## 7. To check with the ROM

1. `RUSH_TRACE_WAVES=1`: waves 6, 7, 8 and each song should convert with no "doesn't parse" line. Expect 4 or more
   groups (sfx groups 12–15 and the song groups of `song_groups`), several hundred macros and samples.
2. The group header lengths: the converter swaps 8 offsets for type 0 and 6 for type 1. A list offset smaller
   than the header would show as a parse error.
3. The envelope tables: the converter finds them by the steps 0x0C that name them. Check that every table it
   swaps has 8 payload bytes, and that no table id is used both as an envelope and as a curve (a parse error
   would not show that; compare the step scan with the curve ids of steps 0x0D, 0x0F, 0x14).
4. Layer entries: the count at +0xA, payload at +12 (check that entry size = 12 + 12 × count, rounded).
5. Backward offsets: the N64 adds the project's u32 offsets to a 32-bit pointer, so an offset "below" the group
   would wrap; on the PC it would not. The converter reports any offset outside the file.
6. Songs: a pattern's first word (unread) and the event u16 at +6 (unread) are swapped as fields; harmless if
   they are something else. Notes are walked per the sequencer's rule; a wrong rule shows as a parse error at
   the end of the file.
7. Decode a few samples from ROM 0x39B40 through port/audio (AUDIO.md §5): the addresses now arrive as
   0xB0039B40 + offset.

## 8. The library's own state on LP64

Written 2026-10-27, after the converter. The library's in-memory records were written for 4-byte pointers, and
each is reached through several struct views. A record that only holds 32-bit N64 values (the heap is the RDRAM
window, the samples are cartridge addresses) keeps its N64 layout, with its pointer fields 4 bytes wide; a record
that holds host pointers (functions, host storage, a caller's variable) is laid out natively, and every view of
it then has the native field sequence. (Since [NATIVE.md](NATIVE.md) step 9 the records that held 32-bit window
addresses hold pointers too: the registries, the voices' heads; section 4.) All the edits are N64-neutral
(`n64_neutral.py`: 0 FAIL against `09da2f0`).

| Record | Layout on the PC | Why |
|---|---|---|
| service table `sound_services` | native, 8 pointers | function pointers |
| DMA request list `sample_dma_list` | native, 24 bytes, own storage | the done queue is a host address |
| sequencer slot `sound_seq_players[8]` | native, 0x1628 bytes (N64 0xFF8) | sequence nodes in host storage, a caller's result word |
| voice `sound_synth_voices[32]` (`pc_mx_voice`) | the N64's after a head of host pointers: 0x1C0 bytes (N64 0x1A0) | 56 views, each the head (`SynthVoiceHead`) then fields at their N64 offsets + 0x1C (section 8.3) |
| stream `speech_streams[2]` | native, 0x1268 bytes (N64 0x1228) | read callback, host output buffers, message queues |
| sound request node (audio manager) | native, 48 bytes (N64 32) | generic list links (`InsertLLE`) shared with other game code |
| scheduler task `OSScTask` | native, as the game builds it | the OSTask inside has host pointers |

### 8.1 The service callbacks and the DMA request list

`AudioInitSys` hands `&audio_driver_services`, eight function addresses in game data, to
`sound_set_services`, which copies them to `sound_services`. The slots, from how the library calls them:

| Slot | Library's name | Function | Called as |
|---|---|---|---|
| 0 `sound_services` | submit the audio task | `musyx_submit_audio_task` | `(OSTask *)`, from `sound_hw_submit_task` |
| 1 `sound_service_wait_task` | wait for it | `musyx_wait_audio_task` | `(void)`, `sound_hw_wait_task` |
| 2 `sound_service_add_callback` | set the retrace callback | `musyx_set_retrace_callback` | `(void (*)(void))` with `sound_hw_service_frame` |
| 3 `sound_service_remove_callback` | clear it | `musyx_clear_retrace_callback` | same |
| 4 `sound_service_start_dma` | DMA a request list | `musyx_start_dma_list` | `(list, count, done queue)`, `sound_hw_start_dma` |
| 5 `sound_service_translate_address` | cartridge address | `musyx_cart_address` | `void *(*)(void *)` |
| 6 `sound_service_alloc` | allocate | `musyx_alloc` | `(size, mode)` |
| 7 `sound_service_free` | free | `musyx_free` | `(void *)` |

The eight are the functions at the end of the game's code (0x8010FBB4–0x8010FD7C), which nothing calls directly.
The order comes from the call signatures, not from the ROM's table.

- `audio_driver_services` is `void (*[8])(void)` (`AudioInitSys.c`), so the data loader turns the ROM's addresses into host
  functions. `CallbackWords` (`api.c`) copies 8 pointers. `SpeechServiceHooks` (`streams.c`) has six
  pointer slots before `allocate` and `release`, and gen_data now places `sound_service_wait_task` … `sound_service_free` at 8-byte steps
  in `sound_services`'s own storage (`data_symbols.txt`).
- `musyx_set_retrace_callback` takes `intptr_t` (it stores into the pointer-sized `musyx_retrace_callback`, which `audio_retrace_thread_entry` calls
  each retrace), and `musyx_cart_address` takes and returns a u32 (since [NATIVE.md](NATIVE.md) step 9: a ROM offset
  in, a cartridge address out). `musyx_start_dma_list` takes `intptr_t` for the list and the done queue.
  `musyx_alloc` and `musyx_free` deal in heap blocks, as pointers since NATIVE.md step 6.
- The DMA request list: `musyx_start_dma_list`'s `CallSlot` and `sample_dma_next`'s `DmaList` now have one layout
  {u16 × 4; requests; done queue} (24 bytes; `sample_dma_thread_entry` reads the done queue at `DMA_DONE_OFF`). It has
  PC storage of its own, `pc_sample_dma_list` (defined in `sample_dma_next.c`): at 0x80153F10 there are 16 bytes before
  `gPathWave` (the path data pointer), and gen_data places both inside `gGameCar`'s storage. The entries
  (`SoundDmaEntryCodebook`, `SoundDmaEntrySamples`, `DmaReq`) are 16 bytes on LP64, and `sound_hw_alloc_dma` allocates the
  list with `DMA_ENTRY_SIZE` = 16.
- Blank cartridge: the table is ROM data, so `--no-rom` fills it with the eight functions above (`port/main.c`,
  `musyx_services`). With a ROM, `rush2049pc: MusyX service N is ... in the ROM's table, not ...` reports a slot
  that differs from the list.

**For gen_data (not changed here):** `gGameCar`'s extent comes from the gap to the next symbol, 7616 bytes
(8 × 952), and runs over `sample_dma_pi_queue` (the PI done queue), `sample_dma_pi_msg`, `new_sched_to_use`, `sample_dma_list`, `gPathWave`
and `sched_index`. The car array has 6 entries (0x1650 bytes, which ends exactly at `sample_dma_pi_queue`;
`car_checkpoints/path_D169C_path.c` declares it complete, and the link warns that its size changed from 5712).
Placed inside it, `gPathWave` (`void *`, 8 bytes) overlaps `sched_index`. `port/data/extents.txt` should give
`gGameCar 0x1650`.

### 8.2 The sequencer slots

A slot (`sequencer.c`, `include/sequence_context.h`) holds pointers into the song and the project (heap), to
sequence nodes (`sound_seq_note_pool`, host storage) and to a request's result word (anywhere), so on the PC it is laid
out natively: 0x1628 bytes for the N64's 0xFF8. `sequence_context.h`'s PC `SequenceContext` has the slot's whole
field sequence with every pointer typed as one, also where the N64 header has a byte range: the pages at 4 and
0x88, the song at 0x10C, the 64 event tracks at 0x128 (`SequenceSlotTrack`, 24 bytes), the per-track streams in
`SequenceTrack` (56 bytes), the tempo track, the note lists, and the request copy from 0xFC8 with its stream pointer
and result word. gen_data counts pointers as 4 bytes and gets the N64 layout back (32704 bytes for the 8 slots;
slot 0's `track_groups`, until [NATIVE.md](NATIVE.md) 18 the label `sound_seq_studio_groups`, lands at +0x738).

Every view in `sequencer.c` follows the header on the PC: `sound_seq_set_program`'s `SequenceProgramView`, `sound_seq_start`'s
`SequencePlayer` (and its `SequencePatternState` with three pointers), `sound_seq_play_note_events`'s `SequenceContextEvents`,
`sound_seq_play_tracks`'s song pointer (`SequencePlayerSongView`), and the request copy in `sound_seq_crossfade_unlocked`. Their
spans are `SEQ_PC_SPAN(from, to)` of the header, and `SEQ_PC_SAME` checks tie each named field to the header's
offset at compile time. `sound_seq_crossfade_unlocked` keeps the result word's address pointer-sized, and `sound_seq_get_volume_group`
indexes the slots from a `uintptr_t`.

### 8.3 The voices

Since [NATIVE.md](NATIVE.md) step 9 (section 14 there): every view of a voice in `macros.c`, `synth.c`, `voices.c`,
`api.c` and `controllers.c` (56) begins with `src/rom/sound/musyx.h`'s `SynthVoiceHead`, where the N64 had its first
0x24 bytes: `addr`, `curAddr`, `retAddr`, `retCurAddr`, `trapAddr` and `trapCurAddr` are `MacroCommand` pointers into
the pool, `node` the handle node (one of `pc_mx_nodes`), `next` and `previous` the chain's voice ids. The head is
0x40 bytes, so every later field lies 0x1C past the N64 offset its name ends in, and a voice is 0x1C0 bytes
(`PC_MX_VOICE_SIZE`: the N64's 0x1A0, the growth, and 4 bytes that keep the next voice's pointers 8-aligned; the full
views end with `tail[SYNTH_VOICE_TAIL]`). The voice's storage is PC data (`pc_mx_voice`, decoupled). ctest
`views_musyx_voice` (`view_check.py`) checks every view's fields against `macros.c`'s whole view, and
`assets_musyx`, `assets_mxvoices` read the head by its host offsets. The one raw offset left in the files
(`sound_synth_get_tick_rate`'s byte 0x4B) reads its view's field.

Before step 9, `sound_synth_voices` (32 voices of 0x1A0 bytes) was reached through about 90 views, almost all of
them byte arrays and fields at N64 offsets (and raw offsets such as `(u8 *)state + 0xC4` and `+ 0x168`), and the
voice kept that layout on the PC. The 15 views that declared pointers moved every later field, so their pointer
fields were 4 bytes:

| Field | Views | What it points to | On the PC |
|---|---|---|---|
| +0, +4 macro start, current step; +8, +0xC return start and step | the interpreter `sound_macro_run`, `sound_macro_start` (start), the GOSUB/GOTO/RETURN/LOOP handlers | the macros' steps in the pool (wave 7, in the heap), from the macro registry | 32-bit window address (`enum n64p_MacroCommand`) |
| +0x1C, +0x20 trap start, step | `sound_macro_trap_key_off` (SETTRAP), `sound_macro_untrap_key_off`, the interpreter | same | same |
| +0x18 sequence node | `voices.c`'s `VoiceState` | one of `sound_handle_nodes[32]`, host storage | the node's N64 address, 0x80050A50 + 16 × index, translated through the pool |
| +0 (`SynthVoice`) | `synth.c` | the macro start | `u32`: only tested for zero |

In `macros.c`, `N64P(x)` was such a field as a pointer, `N64W(x)` a pointer stored into one and `N64_NEXT(x)` the
interpreter's step increment; in `voices.c`, `pc_voice_node` translated the node. All of them went with step 9.

Checked by `offaudit` (a scratch tool reading the objects' DWARF): in the five units, every member whose name ends in
a hex offset (`flags24`, `id60`, `field2E`, `lfo17C`, …) is at that offset, in every view.

### 8.4 Found on the way

- **The audio manager's sound requests.** `AudioSfxInit` builds a pool of request nodes, which
  `audiosfx/group.c` reads as `Node` {two links, kind, busy, entry pointer, four floats}: 48 bytes natively,
  but the pool's records `{Link; u8[24]}` were 40. Its PC record has the node's sequence. `AudioSfxServer`
  (`audiosfx_play/group.c`, the request walk of `_PeriodicMaintenance`, m2c) read the nodes and the list
  header at N64 offsets; they go through `SFX_NODE_*`/`SFX_LIST_HEAD`, the native offsets on the PC and the
  original tokens on the N64. Before, the audio manager thread faulted on its first retrace.
- **The scheduler's task view.** `scheduler.c` reads tasks through `include/m2c_types.h`'s `OSScTask`, which
  spelled the OSTask as bytes at N64 offsets. The game builds tasks with the OSTask in place
  (`EndGraphics.c`'s `Frame`, `musyx_submit_audio_task`'s audio task), so the scheduler read the done queue and
  message at `output_buff`/`output_buff_size`. After the first graphics task it blocked forever on a queue that
  didn't exist, and the audio thread would have waited forever for its first task's done message. The PC view
  spells out the OSTask.
- **The streams** (`streams.c`, `decode.c`, `gsm.c`). `speech_streams` (two streams: GSM speech and PCM
  streams) has host pointers after its 400-byte decoder state. The full views lay it out natively (0x1268 bytes),
  but four byte views read `state`, `scale` and `busy` at N64 offsets, and the GSM frame decoder's
  `GsmStream` read the input ring and the bit position 12 and 32 bytes early. On the PC each is the full
  layout (compile-time checks in `decode.c`; `ptype /o` of the objects agrees: ring at 428, bit position at
  4584, state at 4605, busy at 4664).

Same audit, not changed: `hardware.c`'s driver records (`SoundHwVoice`, `SoundHwVoiceView`, `SfdVoice`,
`AudioNode`, `AudioCacheNode`, `AudioDelayState`) are native by design and agree (AUDIO.md §6), and `streams_3d.c`'s
emitter views all start with the same two links.

### 8.5 Tests

Part 4 of `assets_musyx` (`test_musyx.c`), with `voices.c` and the DMA service's units (`sample_dma_next.c`,
`musyx_start_dma_list.c`) added to the game's units:
- **The song plays** for 60 frames of 384 samples through `sound_seq_tick`, whose units each read the slot through
  their own view. The 7 note-ons come in order with their programs (from the page, the MIDI setup and a program
  change), transposes, velocity offsets, clamping, channels and tracks, at the frames their ticks fall in (0, 3–4,
  25–28, 49–53); every note ends after it started; the tempo track goes 120, 140 at tick 0x100, and again from the
  loop's time; one loop; controllers, pitch bend and modulation per channel; a track ends. The slot is inspected
  through the game's own PC header.
- **A deferred request** (`sound_seq_crossfade_unlocked`, flags 4) is copied into the slot with its result word's address, and
  the song's controller 104 carries it out.
- **Voices**: `sound_macro_start` starts macros on voices allocated by `voices.c`, and `sound_macro_run` steps them:
  envelope, sample start and end, freeing the voice and its sequence node; a trap and a subroutine whose wait holds
  the voice, then key-off running the trap; a stop by key. The head's pointers are checked as pointers into the pool
  and the node as `&pc_mx_nodes[0]`; the files lie in ordinary host memory (step 9: no mapping below 4 GiB).
- **The DMA service** walks a request list of 16-byte entries and keeps the done queue.

The synthetic song's loop time is now its loop target's time (0x180); 0x300, the loop event's own time, loops
forever on the N64 too. A mutation check reverted 13 of the fixes one at a time: 12 are caught, 6 by the
compile-time checks and 6 by the test (the result-word truncation once the deferred request was added). Not
caught: `DmaList` without the done queue, which only overruns its own 16 bytes of storage.

### 8.6 The ROM-less boot

`rush2049pc --no-rom --headless --no-config --audio null` now runs sndInit through the service table (the audio
thread, the voice and SFD buffers, the DMA list and caches, all from the heap), sndPushGroup of groups 12–15 on the
empty project, and the volume setup, and stops in AudioSfxInit:

```
rush2049pc: Segmentation fault at address 0xc in N64 thread thread 6
  #2  AllocateMemory at src/blob/groups/memsys/group.c:171
  #3  AllocateMemoryHeap at src/blob/groups/memsys/group.c:229
  #4  AudioSfxInit at src/blob/AudioSfxInit.c:75
  #5  AudioInitSys at src/blob/groups/AudioInitSys/AudioInitSys.c:118
```

That is the blank cartridge. `sound_hw_alloc_dma` sizes the sample cache as `sound_sample_blocks_per_voice[mode] × voices`, a table in
the boot segment's data, which is zero without a ROM. `sound_hw_init_sample_cache` then allocates zero-entry caches and writes
the first node's links into the next heap block's header (as the N64 would with that table), which ends the heap's
block list; the next allocation finds no free block. Its MusyX tuning floats (`sound_sfd_margin` … `sound_sample_cache_factor`) are zero
too.

To look past it, plant values in a debugger (this is a diagnosis, not data the port should ship):

```
break sound_hw_alloc_dma
commands
  silent
  set var ((unsigned short *)&sound_sample_blocks_per_voice)[2] = 2
  set var *(float *)&sound_dma_margin = 1.0
  set var *(float *)&sound_sample_cache_factor = 1.0
  continue
end
run --no-rom --headless --no-config --audio null --seconds 8
```

AudioSfxInit and the audio manager then run, and the game loop faults in `DrawNoControllers` (the "controller
removed" scroller) copying a string from the blank text bank: missing data again, outside the sound code. With
that function skipped as well (`break DrawNoControllers` / `return`), 8 seconds give 236 graphics tasks and 470
audio tasks: the retrace callback `sound_hw_service_frame` runs every retrace, submits through slot 0 and the scheduler
to `pc_audio_run_task`, and waits for each through slot 1. The DMA path (slot 4) and the sequencer and voices
do not run there: the blank project has no sounds.

### 8.7 To check with the ROM

1. No `MusyX service N` line at startup (the inferred table, section 8.1).
2. Sample DMA: with sounds playing, `sound_hw_start_dma` hands the request list to `musyx_start_dma_list`, the PI
   copies each request and `sample_dma_thread_entry` sends the done queue; the audio thread must not stall in
   `sound_hw_wait_dma`.
3. Every macro pointer the interpreter stores comes from the macro registry (`sound_find_macro`), so it is in the pool
   (a host pointer since step 9, which nothing truncates).
4. A song from the ROM through the sequencer: its deferred requests (controller 104), loops and tempo tracks.
5. Speech and streams through `speech_streams` (GSM, section 8.4).
6. `gGameCar`'s extent (section 8.1), for whatever gen_data places after the car array.
