# MusyX sound data on the PC (port/assets/musyx.c)

Phase 4 loaders, part 3 ([ASSETS.md](ASSETS.md) §3.8, §5). Written 2026-10-27 from the decompilation's C. No ROM
bytes were read: the formats below are what the game's code reads, and the tests build synthetic files from them.

## 1. Summary

- **Four file kinds** feed the sound library in the boot segment: the project (wave 6), the pool (7), the sample
  directory (8) and twelve songs (10–21). `AudioInitSys` (`car_shadow_render`) loads 6–8 at boot and pushes the
  sfx groups 12–15; the song start (`func_800979A0`) loads wave `song + 10`. The samples stay in the ROM at
  0x39B40 and are DMA'd on demand.
- **Strategy: swap in place.** `pc_assets_musyx` swaps every field to host order at the width the library reads
  it, before anything parses the file. The files keep their N64 size and layout, so the record strides and
  offsets the library hard-codes still hold, and the 32-bit addresses it stores into and about them (all in the
  game heap, inside the RDRAM window) still fit.
- **Why not big-endian accessors** in the library: the readers are spread over five units (about 90 functions
  in `lib_15a10.c`, `lib_16320.c`, `lib_17dc0.c`, `lib_1a660.c`, `lib_22300.c`), two of them reserved by other
  work, and N64 neutrality allows no line changes in function bodies. Almost every reader uses whole-field
  loads; section 4 lists the few that don't, and each is handled by how the converter swaps or by one
  file-scope `TARGET_PC` change.
- **Game edits** (all `#ifdef TARGET_PC` at file scope, `n64_neutral.py`: 0 FAIL against `HEAD`, 'shifted' for the
  three units): the sample record keeps 0x1C bytes; the keymap and macro registries keep 8-byte records; the
  sample-info records agree with `func_800146B4`'s view; the WAITMS step sets its flag on the word, not byte 6.
- **Boot.** With a ROM, sndPushGroup can now read the converted files, but the boot stops first at the MusyX
  service callbacks (section 8.1), which are next. `--no-rom` now gets past `LoadWave(6..8)` to that same point.

## 2. Load path and pointers

| Step | Where | What |
|---|---|---|
| load | `_ReadWave` (`waves.c`) → `pc_wave_loaded` → `pc_assets_musyx` | the file, in a locked heap block, converted once per read; the read's big-endian registration is dropped |
| addresses kept | `car_shadow_render` | `D_80152464` project, `D_801526D8` pool, `D_80152690` sample directory, `D_801525FC` sample base (`(s32)D_00039B40`) |
| push | `func_8001536C` (sndPushGroup) | walks the project's groups; registers the group's macros, tables, keymaps, layers (pool entries), samples (directory records) and sfx table by id |
| registries | `lib_16320.c` BSS | `{payload, id, refs}`: macros `D_8003E228` (in 512 buckets by `id >> 6`), tables `D_80038610`, keymaps `D_8003C618`, layers `D_8003CE20` (+ count), sfx tables `D_80042230`, sample directories `D_800385A8` (`{records, base, count}`); groups pushed `D_80038398` |
| look up | `func_80016C20` macro, `func_80016E68` table, `func_80016EE0` keymap, `func_80016F80` layer, `func_80017040` sfx entry, `func_80016CF0` sample | bsearch (`func_8001E864`) by id |
| play | macro interpreter `func_80023E9C` (voice `D_8004BEB8`, macro pointers), sequencer `func_800178B0` (slot `D_80043EB8`, pointers into the song) | |
| samples | `func_800162AC`: record `data = base + offset`; `func_80016CF0` → macro 0x10 → `func_800146B4` → voice; `func_80012D18` builds DMA requests `{length, source, destination}` from `data` | cartridge address 0xB0039B40 + offset; codebook 0x100 bytes at `data`, 40-byte ADPCM blocks from `data + 0x100`; port/ultra copies from the ROM into the big-endian caches, port/audio reads them |

Every pointer into a file is an address in the game heap, which is the RDRAM window at host 0x80000000 on the
PC, so a 32-bit field holds it exactly. The sample addresses are cartridge addresses (0xB0000000 | offset), also
32-bit.

## 3. Formats

Offsets are from the start of the record. All multi-byte fields are big-endian in the ROM. "Swapped" means at
that width; bytes are never moved.

### 3.1 Project (wave 6)

Groups follow each other until a size of 0xFFFFFFFF (`func_8001536C`, `func_800154A4`, `func_8001558C`).

| Off | Field | Readers | Swapped |
|---|---|---|---|
| +0 | u32 size (next group at +size); 0xFFFFFFFF ends | sndPushGroup (also read as the end test) | u32 |
| +4 | u16 id | sndPushGroup, `func_8001558C` | u16 |
| +6 | u16 type: 1 sfx, 0 songs | same | u16 |
| +8 | u32 offsets[], from the group + 8: [0] macro ids, [1] sample ids, [2] table ids, [3] keymap ids, [4] layer ids; type 1: [5] sfx table; type 0: [5] normal page, [6] drum page, [7] MIDI setups | same (`ResourceGroup`, `ResourceHeader`) | 6 or 8 × u32 (5 for other types) |

| Structure | Layout | Readers | Swapped |
|---|---|---|---|
| id list | u16 ids ended by 0xFFFF | `func_80014F14`/`F80`/`FEC`, `func_80015058`, `func_80015228`; pop: `func_800150C8` … `func_800152A8` (walks the sample list backwards) | u16 each |
| sfx table | u16 count, u16, then count × 12: u16 id, u16 macro/keymap/layer id, u8 max voices, u8 priority, u8 volume, u8 pan, u8 key, u8 group (byte 9, **written**: 31 by `func_800164D0`, the channel by `func_80020370`), 2 bytes | `func_80015318`, `func_800164D0`, `func_80017040` (bsearch on the u16 id, stride 12), `func_8001B1D0` | u16 at +0, +2 per entry |
| page (normal, drum) | 8-byte entries {u16 macro; u8 priority; u8 max voices; u8 slot; u8 program; 2 bytes} until program (byte 5) = 0xFF | `func_8001785C` (byte 5 only, for the end too), `func_800178B0`, `func_80017720` | u16 at +0, not in the end entry |
| MIDI setup | 132-byte records {u16 song id; u16; 16 × {program, volume, pan, reverb, chorus, 3 bytes}} until id 0xFFFF | `func_8001558C` (by id), `func_800178B0` | u16 at +0, +2 |

### 3.2 Pool (wave 7)

`+0 u32 offsets[4]` from the pool start (`func_80014E64` … `func_80014EE8`) to four lists. An entry is
`{u32 size; u16 id; u16; payload}`, the next one at +size, and a size of 0xFFFFFFFF ends the list
(`func_80014E1C`).

| List | Payload | Readers | Swapped |
|---|---|---|---|
| 0 macros | at +8: 8-byte steps, two u32 words; opcode = word 0 & 0x7F, parameters in bit fields of both words | `func_80023E9C` copies each step word by word into `cstep`; every handler reads the copy (`lib_22300.c`) | u32 × 2 per step |
| 1 tables | at +8: bytes (volume curves, 128 entries: `func_8002321C`), or an envelope of four u16 (attack, decay, sustain, release) stored little-endian, which `func_80022F24` (macro 0x0C) reads as u16 and byte-swaps | as listed | envelopes (the ids that a step 0x0C names, bits 8–23): u16 × 4; curves: none |
| 2 keymaps | at +8: 128 × {u16 macro/layer id; s8 transpose; u8 pan; s16 priority; 2 bytes} | `func_8001A270` | u16 at +0, +4 per entry |
| 3 layers | header +8 u16, **+0xA u16 count** (`func_80015058`), payload at +12: count × {u16 id; u8 low key; u8 high key; s8 transpose; u8 volume; s16 priority; u8 pan; 3 bytes} | `func_80015A0C`, `func_80016F80`, `func_80019F48` | u16 at +8, +0xA; per entry u16 at +0, +6 |

### 3.3 Sample directory (wave 8)

0x1C-byte records until id 0xFFFF (`func_8001605C`, `func_800162AC`, `func_800163A8`, `func_80016CF0`; bsearch on
the u16 id, stride 0x1C):

| Off | Field | Swapped |
|---|---|---|
| +0 | u16 id | u16 |
| +2 | u16 reference count, **written** | u16 |
| +4 | u32 offset from the sample base | u32 |
| +8 | u32 address, **written**: base + offset (`void *` in the N64 C, u32 on the PC) | u32 |
| +0xC | u32 info: key << 24 \| rate (macro 0x1F, `func_80022CD4`) | u32 |
| +0x10 | u32 length: format << 24 \| samples (format 0 PCM16, 3 ADPCM) | u32 |
| +0x14, +0x18 | u32 loop start, u32 loop length | u32 |

### 3.4 Songs (waves 10–21)

Offsets are from the song start; the sequencer adds them with 32-bit arithmetic (`offset + (u32)song`).

| Structure | Layout | Readers | Swapped |
|---|---|---|---|
| header | +0 u32 track table, +4 u32 pattern table, +8 u32 channel map, +0xC u32 tempo track (0 none), +0x10 u32 tempo, +0x14 u32 loop time | `func_800178B0` (`Arr`), `func_80018634` (`SequenceHeader`) | u32 × 6 |
| track table | 64 × u32 (0 = no track) | `func_800178B0` | u32 |
| track events | 12 bytes {u32 time; u8 program; u8 controller; u16; u16 code; u16 index **or** s8, s8}: code 0xFFFF ends, 0xFFFE loops to event *index*, else a pattern index with the two bytes (transpose, velocity offset) | `func_80018634` (reads the end event's time and code too) | u32, u16 at +6, +8; +0xA as u16 only after 0xFFFE |
| pattern table | u32 offsets, indexed by event codes | `func_80018634` | u32 (the entries some event uses) |
| pattern | {u32; u32 pitch-bend stream; u32 modulation stream (0 none)} then note events | `func_80018634`; the streams are bytes (`func_8001824C`, `func_80018448`) | u32 × 3 |
| note events | u16 delta time, u8 key, u8 velocity, then u16 duration for notes: key bit 7 with velocity 0 (program), 1 (volume) or bit 7 (controller) is 4 bytes, key = velocity = 0 is 4 bytes, anything else 6; key = velocity = 0xFF ends (its delta is read) | `func_80017D38` (`*(u16 *)events`, `*(u16 *)(events + 4)`) | u16 at +0, and +4 for notes |
| channel map | 64 bytes | `func_80018634` | none |
| tempo track | {u32 time; u32 tempo} until time 0xFFFFFFFF | `func_80018A30` | u32 × 2 (time only for the end) |

## 4. What the audit found beyond whole-field loads

| Reader | What it does | Handled by |
|---|---|---|
| `func_80021B9C` (macro 0x07, WAITMS) | `((u8 *)command)[6] = 1`: byte 6 of the step copy is bits 15..8 of word 1 on the N64, the milliseconds flag `func_8002193C` tests | `lib_22300.c`: a PC version stores the same byte by value, `word[1] = (word[1] & ~0xFF00) \| 0x100` |
| `func_80022F24` (macro 0x0C) | reads the envelope as u16 and swaps each: the table is little-endian in the file | the converter swaps those u16 (found from the steps 0x0C name), so the native read returns the N64's value and the code's swap gives the same result |
| `func_80015058` | layer count as a u16 at +0xA (the low half of a u32 at +8, if the tool wrote one) | swapped as two u16 |
| `func_8001785C` | page end by byte 5, before the u16 at +0 | the end entry's u16 is left alone |
| `func_80018634` | event +0xA as a u16 for 0xFFFE, as two bytes otherwise | swapped by event code |
| `func_80017D38` | variable-length note events | walked with the game's rule |
| `func_800164D0`, `func_80020370` | write byte 9 of sfx entries | a byte |
| `func_8001605C`, `func_800162AC` | write the sample record's count and address | swapped fields, written natively afterwards |
| `func_8001A270` | `keymap[key].panning` with the unmasked key: for sfx (`func_8001B1D0` passes key \| 0x80) it reads byte 3 of entry 128 + key, past the keymap | identical where that byte is a byte field; it differs only if entry 128 lands on the next entry's u32 size (key 0 after a keymap of exactly 128 entries) |

**Pointers into the files**, and how they fit on LP64:
- macro and keymap registries (`RecordFields_8003E228`, `RecordFields_8003C618`): `void *payload` made the
  records 12 bytes natively, while `func_80016C20` searches with a literal stride of 8 and the comparators read
  the id at +4. On the PC the payload is a u32 window address (`lib_16320.c`), as the table and layer
  registries already had it. The keymap search only worked by accident with a single keymap.
- sample record `data`: a u32 cartridge address (`boot_tail_sample_contract.h`, and the copy in
  `lib_15a10.c`), so the record stays 0x1C bytes for the walks and the bsearch. With `void *`, the second
  record was read 4 bytes off.
- sample info: `func_80016CF0` fills, `func_80022874` passes and `func_800146B4` reads one record through three
  views. `func_800146B4`'s (and `func_8001C580`'s, whose `data` is a stream's host buffer) has a packed 8-byte
  pointer at +4; the other two now match it (`lib_16320.c`, `lib_22300.c`).
- sfx tables, sample directories, pushed groups, and the sequencer's and voices' pointers are native pointers in
  their own storage (gen_data gives pointer-bearing tables host-sized storage).

The sample base is `(s32)D_00039B40` in `car_shadow_render`: sign-extended to 0xFFFFFFFFB0039B40 in the
pointer-sized `D_801525FC`, then truncated back to 0xB0039B40 + offset in the u32 record. `func_80014CAC` takes
the 64-bit value for a host pointer and returns it unchanged, which is what the N64 does for a 0xB0 address too.

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
3. The game's own readers on the converted files, placed below 4 GiB as the heap is: `lib_15a10.c`,
   `lib_16320.c`, `lib_17dc0.c`, `lib_1f340.c` and `lib_22300.c` built with the game's flags
   (`test_musyx_game`), with stubs for the rest of the library. sndPushGroup of an sfx group and a song group;
   every lookup (macros, tables, two keymaps, layers, sfx entries, samples, with the sign-extended sample base);
   the song start through `func_800178B0` (pages, MIDI setup, track table, tempo track); macro steps 0x0C
   (envelope values), 0x10 (sample address, length, format, loop, offset as `func_800146B4` receives them) and
   0x07 (250 ms = 64000 clock units, against 7812 for ticks); sndPopGroup twice, with the shared macro kept
   until the second; a blank project.

A mutation check broke the converter at 12 points (layer count, keymap priority, envelopes, loop end, note
length rule, directory stride, sfx header length, page end, tempo values, event +6, the BE registration, song
header length) and reverted each of the 6 game edits in turn: the test caught all 18.

## 7. To check with the ROM

1. `RUSH_TRACE_WAVES=1`: waves 6, 7, 8 and each song should convert with no "doesn't parse" line. Expect 4 or more
   groups (sfx groups 12–15 and the song groups of `D_8011F070`), several hundred macros and samples.
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

## 8. Next walls in the sound library (found during the audit)

### 8.1 The service callbacks (boot, before sndPushGroup)

`car_shadow_render` passes `&D_8011EAAC`, a table of 8 N64 function addresses in game .data, to
`func_80020598` (`lib_207b0.c`), which copies it as `u32[8]` into `D_80038000`. The library calls the slots as
8 function pointers `D_80038000` … `D_8003801C`: task start (`func_8010FBE0`), ?, register and clear the retrace
callback (`func_8010FC60`, `func_8010FC70`), DMA (`struct_init_and_call`), address translation (`func_8010FD60`),
alloc (`resource_alloc_init`) and free. On LP64:
- `D_8011EAAC` is declared `s8`/`s32` (1 byte in gen_data), so the loader never makes its words host pointers;
- the u32 copy fills 4-byte slots while gen_data lays out `D_80038000` as `ServiceHooks_8002574C`
  (`lib_25bb0.c`: 24 bytes, then `allocate` at +0x18 and `release` at +0x20) and aliases the other slots at
  +4, +8, … as 8-byte pointers that overlap;
- the first allocation (`func_80010C68`, `lib_11640.c:95`, from sndInit `func_80010840`) calls a bad
  `D_80038018`. `--no-rom` stops there now (the slots are zero: a call to 0, whose fault report has no frames).

Changes needed: `D_8011EAAC` as `void (*[8])(void)` in `car_shadow_render.c` (the loader then resolves the
function addresses); `CallbackWords` as 8 pointers (`lib_207b0.c`); `ServiceHooks_8002574C` with 6 pointer slots
before `allocate` (`lib_25bb0.c`), so gen_data aliases the slots at 8-byte steps. Then the callbacks' own
prototypes: `func_8010FC60(s32)` stores the registered function pointer in `D_8011EAA8` (called from
`dma_wait_complete.c`, reserved) and must take `intptr_t`; `struct_init_and_call(s32, s32, s32)` gets the DMA
request list and a pointer as `s32`, and its `CallSlot` view of `D_80153F10` puts `s32 arg` where
`func_8008ABE4`'s `DmaList` reads the 8-byte `reqs`.

### 8.2 Reserved files

- `lib_11640.c`: the DMA request list is allocated as `D_800382A8 * 2 * 12U` bytes (`func_800114C0`, line 344),
  but the entries (`DmaEntry_80012234`, `DmaEntry_80012730`, `func_8008ABE4`'s `DmaReq`) are 16 bytes on LP64:
  `sizeof(DmaEntry_80012234)` under `TARGET_PC`. Also `bzero`'s size parameter should be `__SIZE_TYPE__`
  (PROTOTYPES.md §6). `func_800146B4`'s sample-info view needs nothing: the others were made to match it.
- `lib_17dc0.c`: the sequencer's views of a slot disagree natively. `Seq_800178B0` (pointers at +4, +0x88,
  +0x10C, in the tracks, …) is 0x1218 bytes on LP64 and overruns the 0x1008-byte `SequenceContext` slot;
  `func_80017720`'s `prgState` is at 0xF8C and `func_800178B0`'s at 0x11A0; `func_80018634` reads the song pointer
  at +0x110 where `func_800178B0` stores it at +0x118, and its `TRACK(i)` array (64 × 24 bytes from +0x128) runs
  into its `PLAY(i)` array (from +0x568). In the test, a song start returns identifier 1
  instead of 0 because `func_800178B0`'s stores land on `SequenceContext.inactiveFC1`. The views need one
  native layout (or u32 window addresses for the pointers into the song) before music can play.

### 8.3 The voice state

`D_8004BEB8` (32 voices of 0x1A0 bytes) is reached through about 90 struct views in `lib_22300.c`,
`lib_1a660.c`, `lib_1f5b0.c` and `lib_21210.c`. Fifteen of them declare pointers (the macro pointers at +0,
+4, +8, +0xC, the trap pointers at +0x1C and +0x20, a sequence node at +0x18), which moves every later field on
LP64, while the others reach `flags24`, `+0x30` and the rest through byte arrays at N64 offsets (and
`func_80021BF0`-style views hold +0 … +0xC as u32). The pointers all point into the pool, in the window, so
u32 slots would do, but the dereferences (`voice->curAddr04->word[0]`) need a typed PC version of the
interpreter's entry points. Until then, starting a voice on the PC is unsafe.
