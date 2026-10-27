# Record layouts along the code: layout_check

Every shared record has one PC layout ([POINTERS.md](POINTERS.md) 9.1). `view_check.py` and `view_audit.py` check
it for the views declared on a named storage, from the PC build's DWARF. A review of the race-time records found bugs
they could not see, because the code reached the record through a pointer whose struct has another name: a walk of
the level-object nodes through an 8-byte link, a dead-reckoning record seen as `Reckon`, overlay B's power-ups,
`* 952` car strides. `tools/port/layout_check.py` follows the code instead: each access goes to the storage it
reaches, through pointers, parameters, returns, lists, trees, slots and function pointers, and the views that meet
there are compared. 2026-10-27. Related: [ENDIAN.md](ENDIAN.md) 2 (endian_check, whose access model this is),
[CARS.md](CARS.md), [SYNTH.md](SYNTH.md) (the synthetic cartridge, whose boot found some of the same bugs).

```
python3 tools/port/layout_check.py [--build build/pc] [-j 2] [--reuse]    # build/pc/endian/layout.txt
python3 tools/port/layout_check.py --reuse --storage D_8014D280           # every access placed on a storage
python3 tools/port/layout_check.py --selftest                             # ctest layout_check
```

It shares endian_check's compile cache (`build/pc/endian`, the units compiled again with
`-O1 -fno-inline -fdump-tree-forwprop1-lineno`); with `--reuse` and 16 jobs a run takes about 6 s on the x86-64 build
host, and the analysis alone about 10 s on the Pi 5.

## 1. Method

1. **Accesses** are endian_check's: every memory access in GCC's GIMPLE of each live game unit, with its width, its
   base and its host offset in the base (the object's DWARF). For each, layout_check keeps the **view** (the struct
   its code names: the base's declared type, or the struct a pointer was cast to), the field, and the **N64
   position the view gives it** (its N64 layout at that host offset). A view has no opinion where it is bytes or
   void, where a scalar pointer is read past its scalar, or past the struct's end: those accesses (m2c's raw
   offsets) only have their host offset.
2. **Pointers** are followed further than endian_check does: `p + i * K` keeps the offset in the element and records
   the stride K; `(T *)x` of an integer loaded from memory (a u32 window address, `N64P`) or of a loaded value plus a
   constant is a pointer into what the slot points to; a PHI whose other arguments come from itself is a list walk
   (`n = n->next`, a link of the storage to itself) or a pointer step (`p += K`, a stride), and one that steps down
   through the slots it points to (`pp = &(*pp)->right`, a tree insert) makes the record stored through it
   (`*pp = n`) a node linked at those slots; stores of a pointer into a slot are recorded, and so are stores of a
   function's address, calls through a loaded pointer and a function's returned pointers.
3. **Roots.** Each access is put on the root storages it reaches, with host offsets (what the PC code really
   addresses) and N64 positions carried along:
   - a data symbol (aliases at one address are one; a name inside another symbol's storage, `in X+0x18` in
     `data_symbols.txt`, is its container at that offset);
   - what parameter k of F points to is each caller's argument (the offsets added). A parameter given more than 24
     storages is a generic helper's and stays a root of its own;
   - what F returns, at one call, is what it returns from a symbol or from that call's arguments; anything else it
     made (an allocator's block) is that call's own storage, so heap blocks are told apart by their allocation site;
   - what a slot points to is what was stored there, when that is up to four storages; stores made through a
     pointer that reaches several storages (a list or pool function's own stores) are not followed, and neither are
     values that are any of several. `LINKS` names the functions whose calls say it instead: `InsertLLE`
     (`func_80091FBC`: the list's head points to the object's storage) and the StatLL set-up (`pool_init`,
     `struct_fields_init`: the pool's element, active and free slots point to its memory);
   - a list's link is the list's own storage (taken before any store is followed, which would give a list
     function's own list every list), and so are a tree node's links (with the stores: the insert takes the node
     as a parameter);
   - a function whose address is stored into a slot is called by every call through a pointer loaded from that
     slot: its parameters take those calls' arguments (the sorted-tag tree's order `draw_sprites`, through
     `AudioInit`'s tree; the linker's `aliases.ld` names one function by its two names).
   A function name defined in several units (static helpers) is qualified by its unit.
4. **References.** A symbol's reference layout is the declaration gen_data chooses (`data_symbols.txt`'s type; else
   gen_data's ranking), with its unnamed bytes named from the symbol's other declarations of the same size (gen_data
   may choose a padded view). A storage reached only through pointers takes the layout of a view `REFERENCE` names
   (by unit and struct name, or a view that names the root and another unit's struct that gives the layout);
   `SYMBOL_REFERENCE` overrides gen_data's choice where that choice is one of the views in question. A reference
   keeps every scalar at each host offset, so a union's members all count. Positions are taken in the reference's
   element.
5. **Findings**, ranked by score:
   1. *Layouts.* With a reference: a view whose accesses are at host offsets other than where the reference has their
      N64 positions (5). If each such access still lands on the start of a reference scalar of its own width and
      class, the view is either a PC view padded in host bytes, declared for the PC build only (`#ifdef
      TARGET_PC` around its declaration line in its unit, by DWARF), which scores 2, or the N64 build's view too,
      which scores 4: its N64 positions are the N64's fields, and on the PC it reaches another field of the same
      width (func_8008BEA4's `In`, section 4.4). Without a reference: a view that puts an N64 position at another
      host offset than most views do, or a host offset at another N64 position (4, both sides when tied).
   2. *Raw offsets* (1b): an access with no view opinion, or a pointer passed to a function, at a host offset inside a
      reference scalar wider than a byte (5 in a pointer, 4 otherwise); an access with no view opinion that reads the
      first bytes of a wider field (3).
   3. *Strides:* an index (`* K`, a typed index, `p += K`) over a storage whose K is not a multiple of the PC element
      or of an array's around its base: 5 if K is the N64 size (`* 952`, 0x48-byte views of 0x58-byte tags), 2
      otherwise (only past the element's N64 size).
   4. *Widths:* with a reference, an 8-byte access where it has a 4-byte non-pointer (a u32 window address: 5 for a
      pointer, 4 for `intptr_t`), and a 4-byte access of a pointer (4 if written, 3 if used, 1 if only tested);
      without one, or where the reference has only bytes, a host offset accessed as 8 bytes and as 4 (4).
6. **Review.** `REVIEWED` lists findings that were read and judged by design, as (unit regex, regex on the
   function, view, field, access or storage, reason); a finding whose accesses all match one rule (or one side's, for
   a pair) is listed last with the reason. The table is section 7.

## 2. Re-finding the known bugs

Each fix of the review was reverted in a scratch copy of the tree (`git show` of the fix, reversed) and the check run
over the whole game. The run on the fixed tree has none of these.

| Fix reverted | Found (score 5) |
|---|---|
| `5d6334d` `PauseTargets` (RaceStateMachine_Update) | `D_8014D280`: `Target` off the reference `Node` at 2 N64 positions; an 8-byte pointer where `Node` has the u32 `next` |
| `383402f` `random_float`'s `Reckon` | the dead-reckoning records' heap block: `Reckon` off the reference `Matrix64` at 6 positions; an 8-byte pointer where it has the u32 `record` |
| `99f3646` overlay B's `Object` (runtime_b_resource_callbacks) | `D_8014D280`: `Object` off `Node` at 3 positions |
| `99f3646` the walks of `func_8039133C` and `func_800BEA6C` | `D_8014D280`: two 8-byte pointers over `next`; `func_8039133C`'s `Object` off `Node` |
| `1dfe3f6` `func_800FBE60`'s `* 952` | `D_80152818`: an index `* 952`, the N64 size of its element (PC 0x468) |

The nodes are reached through the pool's set-up (`pool_init(&D_80143FC8, &D_8014D280, ...)`, `LINKS`), the active
list `D_80143FD8` (inside `D_80143FC8` at +0x18), the walks, func_8039133C's slots (a pointer store) and race_setup_2's
side array; level_objects' `Node` is their reference.

The synthetic cartridge's boot ([SYNTH.md](SYNTH.md) 3) fixed bugs of the same kind on main at the same time. After
the merge, each of those fixes, and section 4.4's, was reverted the same way:

| Fix reverted | Found |
|---|---|
| the audio manager's records (SYNTH.md 3, item 2: the fx tag and sorted-tag allocations, the command table's step, the views) | 40 open findings, 27 of them 4 or 5: e.g. `Record` (record_score), `Entity` (entity_lookup, entity_helpers, transform) and `ResEntry` off `tFxSequence`; `Event` and `Node16` off the sequence tag, and an index `* 60` over its pool; `Msg` off `tAudioMsg`; `Slot18[]`, `Slot24[]` and `ShutdownMsg[]` indexes of the command's N64 size; `CamSlot` off `Source` |
| camera_victory's `VCar` | `D_8014A250`: host +0x3A0 read as 8 bytes and as 4 (score 4: the reference has bytes there; `VCar` against the model's other views) |
| `func_800E2D18`, `func_800E2C70`, `func_800E1500` | `D_8014A250`: three 8-byte pointers where the reference has the u32 `car` and `parameters` (5); `EngineState` off the reference at 3 positions (5) |
| `draw_sprites`, the sorted-tag order | the sorted-tag pool: a 4-byte read inside a node's link (5). The order is only called through the tree's function pointer; it read the tag at the node's +8, which is its right link on the PC, so the tag's priority was read from another node |
| func_8008BEA4's `In` (section 4.4) | `D_80152818`: `In` gives other N64 positions than the car (4) |

camera_victory's and draw_sprites' needed the function-pointer and tree flow and the width check where the
reference has bytes; the first version of this check (section 3) found neither.

## 3. Over the game

1,300 units, 47,740 accesses, 56,496 placed on 3,856 storages, 443 with a reference layout. On the tree before this
work (af0cc04): **80 open findings** (30 layouts, 28 raw offsets, 8 strides, 21 widths; 59 scored 4 or 5), all of
them real and fixed below; **0 after**, with 59 reviewed (section 7). endian_check's results are unchanged.

After the merge with main (the synthetic cartridge): 1,308 units, 48,040 accesses, 56,913 placed on 3,869
storages, 444 with a reference layout. Following function pointers found **one more** (section 4.4), which the
PC-only rule scores 4; **0 open after**, 60 reviewed. endian_check: 2 open value conflicts in one storage, as
before.

## 4. What was fixed

All edits are N64-neutral (`n64_neutral.py --base 09da2f0`: 0 FAIL): PC views under `#ifdef TARGET_PC` at file scope,
and macros on existing lines whose N64 expansion is the original tokens. The synthetic cartridge fixed the audio
manager's records and the model record's readers on main at the same time (SYNTH.md 3); where both changed a file
(18 files: the audio views, the three physics readers, hud_setup's pools), the merge keeps main's version. The
tests of section 5 and this check pass on them.

### 4.1 The audio manager's records

MAT3 audiomgr.c's records are linked by `InsertLLE` through two pointer-sized links, and the MAT3-typed audio
manager (`engine_sound_sync/audiomgr_thread.c`) has their PC layouts; the other views kept the N64 offsets. On the PC:

| Record | N64 | PC | Fields after the links |
|---|---:|---:|---|
| FX tag (`tFxSequence`), the pool `D_80110244` | 0x44 | **0x58** | list 0x10, id 0x18, state 0x1C, paused/loop/numCmds/priority 0x24–0x27, startTime 0x2C, vol..pitch 0x30–0x3C, sysId 0x48, data 0x50 |
| sequence tag (`anon_ll_id_3c`), the pool `D_80110270` | 0x3C | **0x48** | id 0x10, free/active/stop 0x14–0x16, startTime 0x18, timeout 0x1C, next 0x20, ids 0x24, subId 0x34, replace 0x38, priority 0x3C, sndTag 0x40 |
| message (`tAudioMsg`), the pool `D_80142DD8[128]` | 0x18 | **0x20** | changeEffect's vol..pitch at 0x04–0x10, its tag 0x18; a stop's or an interrupt's tag at 0x08 |
| 3D-sound source (`Source`; `Owner`), a tag's data | 0x2C | **0x38** | active/queued 0x10, position 0x14, radius 0x20, nearVolume 0x24, parameter 0x2C |

- FX tags: `_InterruptFxTag` (`audio_pitch_adjust`, ap.c: it read the tag's list from its `prev` link and wrote
  `&gInterruptedFxTags` over it; its list header `D_80144C50` gets a view of its own), `_CalcPriority`
  (`func_80098710`, record_score), the 3D update's `Effect` (engine_sound_sync.c), `_GetFxTag` (`func_80091BA8`,
  entity_lookup: the id at +0xC and a 0x48-byte stride) and `scheduler_recv`, the tag views of entity_helpers
  (`cam`, the data) and transform (`entity_transform_calc`'s command count and values), `results_time_display`, and
  the pool's size (hud_setup's `Voice` ended 8 bytes short: its padding covered the data pointer).
- Messages: transform's `Msg` kept its payload in a union with a pointer, so on the PC its four words were 4 bytes
  late and the manager read vol from lrpan's place (`MSG_TARGET` for its weak `scheduler_recv`); the pool's
  allocator `func_80091B00`, hud_setup's clear and `func_800C8918` stepped 24 bytes.
- Sequence tags: a sequence's start `car_stats_display` (also: it built `va_list` as a pointer past the last named
  parameter, which is not where the host ABI passes the variadic arguments: the compiler's on the PC),
  `game_timer_pause` (the lookup, a 0x3C stride), `func_800D18D8` (the free list), `entity_flag_check` (the free, at
  raw offsets: `SEQ_ID`, `SEQ_FREE`, `SEQ_ACTIVE`).
- Sources: `func_800AED20` (`_GetSource`) cleared the active byte at +8 (`SOURCE_ACTIVE`); entity_helpers' `CamSlot`
  (the position) had the N64 layout.
- The fades' lookup `func_800956BC` had `s32 word0` over the node's link and read the key at +0x14, where
  audio_fade_control keeps the entry pointer at +0x18: it never matched. The key is `intptr_t` now.
- `pointer_offset8_call` inserted into an ordered list's `count` (+8); the root is at +0x10 (`OL_ROOT`).

None of this ran on the PC yet: the ROM-less boot stops in AudioSfxInit, before the first sound.

### 4.2 The model record, the cars and their visuals

- The model record keeps its N64 layout ([CARS.md](CARS.md) 4), and three readers took its u32 window addresses as
  host pointers: the torque `func_800E2D18` (`EngineState`: the tune at +4 read 8 bytes wide from +8, and the indices
  after it 8 bytes late; `TUNE`), `func_800E1500` (the descriptor through `void **`; `MODEL_CAR`) and
  `func_800E2C70` (`*(s8 **)(arg0 + 0)` and `+ 4`; `MODEL_P`).
- The car's scene-node handle (+0xF4, an s32): `race_init_helper` and `func_800E92C8`'s `Cr` read its low half as the
  s16 at +0xF6, the high half on a little-endian host (ENDIAN.md 3.1; endian_check missed them, as one reaches the car
  through the name `D_80152BD0` inside player_array).
- `func_800EA3F4` read +0x35C/+0x35D raw, past the visuals: `CAR_OFS`.
- `model_bounds_calc` and the five visual animators of `func_800AC8D4`'s table (func_8008B640's group: `ModelObj`)
  saw a visual on the N64 layout, so they took its index as the model and its objnum as the player.

### 4.3 Other records

- The heap's resize in place `menu_item_select` (the save code shrinks a ghost's header block with it) viewed the
  block headers with host pointers; they keep their N64 layout (alloc_at.c's `Block`): `enum n64p_MBlock`.
- The controller/pak records: `func_8008A6A4` strided by 0x304 (`PAK_RECORD`, 0x310 on the PC; POINTERS.md 9.6).
- The result panes `D_80154350` are `intptr_t`, and `records_screen` cleared them through an `s32 *`.
- The profile's name: 17 sites in eight files add the N64 offset 20 to the profile record (`*input_rec0[p].name + 20`,
  `owner->body + 20`, `NameData`'s 20 bytes), and controls.c reads the rumble byte at 43; the PC profile has them at
  `PROFILE_OFS(20)` = 0x20 and 0x37 (POINTERS.md 9.10). `func_8010B5D0` also read the profile handle as an `s32`.
- Blits ([POINTERS.md](POINTERS.md) 9.3): `func_80108DA8`'s `MenuNode` is the 0x40-byte N64 blit (the PC's is 0x68),
  and the hide setter `input_new_data_wrapper` used `arg0[0x1A]` (`BLIT_HIDE`, 0x26).
- The audio DMA list: lib_11050.c's `func_80010450` wrote its entries through a 24-byte view with a pointer-sized
  source; the entries are lib_11640.c's 16 bytes (`DMA_ENTRY_SIZE`), a u32 cartridge source (`DMA_SOURCE`).

### 4.4 Found after the merge: the brake lights

`func_8008BEA4` (AnimateBrakeLights, a car visual's animator: StartCar stores it in the visual, and the visuals'
update calls it through that pointer) read its car slot through `In { u8 pad0[8]; s16 idx; }`, the N64 visual's +8.
On the PC the visual (`struct pc_visual`) has its slot at +0xC, and +8 is its index: the brake lights took the
visual's index as the car, read that car's flags and wrote the colours through that car's palette pointer (outside
both arrays for an index that is no car slot, such as -1). `In` has the PC layout on the PC.

## 5. Tests

- `assets_audiomgr` (`test_audiomgr.c`, new): the FX tag lookup, the interrupt, the priority, the status query, the 3D
  position through the tag's data, the parameter change and the stop messages from the pool, the sequence tags'
  free list, start, pause and free, `_GetSource`, the fades' lookup and the ordered-list insertion, on records with the
  PC layouts of section 4.1. Reverting section 4.1's views makes it fail (and fault); reverting car_stats_display
  fails 6 checks.
- `assets_heap` (`test_heap.c`, new): the game's heap and `menu_item_select` shrinking a block before a used block
  (a split), shrinking and growing one before the free rest. The old view faults.
- `assets_cars`: the torque through the tune pointer, `func_800E1500` and `func_800E2C70`, `race_init_helper`
  moving two cars' nodes by their handles' low halves, and the brake lights of car 1 from a visual whose index is
  3. The old readers fault; the old `In` fails 17 checks.
- `assets_input`: the pause screen's title (`func_8010B5D0`) names the player from the profile's name at +0x20. The
  old view fails.
- `layout_check` (ctest, not on Windows): `--selftest` builds nine synthetic cases (a walk through an 8-byte link of
  32-bit-link nodes, a record seen through a parameter with another struct, nodes read back from slots through a
  view with a host link, an N64 stride, a typed index of the N64 element, a raw N64 offset into a moved field, a u32
  read of a pointer, a sorted tree whose order is only called through the tree's function pointer and reads at the
  N64 offsets, and two views padded in bytes up to a field, one PC-only and one shared), checks the 13 findings it
  must make, and that the correct counterparts give none. Taking out the tree links, the indirect calls or the
  PC-only rule each misses one.

## 6. The two review items

- **AddToEnvList** (`func_8008FFB8.c`) stores the environment visuals' list head, a host pointer, through an `s32 *`.
  No C calls it, and the N64 code doesn't reference its address (only its definition in
  `asm/us/blob/blob_8008d0c0.s`); gen_data's function table has it (`{0x8008FFB8, 1, pc_f_755, "func_8008FFB8"}`), so
  the loader would turn a pointer-typed field of initialized data holding 0x8008FFB8 into it. Whether the ROM has such
  a word can't be told without the ROM (the loader's report, with one, lists every function pointer it converts). It
  takes `intptr_t *` now, so the link is pointer-sized like the visual's `next` either way.
- **`transmission_ratio_get`'s place record.** With an info record's flag 0x400 it keeps `rec` as the node's 32-bit
  state (`N64W`), and `shots.c:788` passes the literal `"WPR_MINE"` as `rec`, a host string. That stores a host
  pointer if `WPR_MINE`'s info record (ROM data) has flag 0x400. The code says it can't on the N64 either: a mine
  writes its state record (`m->base`, `m->timer`, `m->phase` in `func_8010C974`), which would then be the string; the
  record is the category's. On the PC a place record that isn't a window address is not kept: the state stays 0
  (`PLACE_STATE`), so a wrong flag faults on a NULL rather than on half a pointer.

## 7. The review table (`REVIEWED`), references and links

| Rule | Why |
|---|---|
| `pc_statll_link`, `pc_statll_set_link` | the StatLL helpers take a link at the width the list's n64 flag gives (POINTERS.md 10.2); the other width never runs on that list |
| dma_wait_complete's `Sched`, lib_1050_sdk's `PcSched` | a PC view of the native scheduler padded in host bytes to curRSPTask (ULTRA.md); both reach +0x2B0 |
| engine_sound_sync's `Effect` | a PC view of the FX tag padded in host bytes; each access is the tag's field |
| lib_17dc0.c's `Seq_*`, `SequenceContext*` views | PC views of a sequence slot padded in host bytes (`SEQ_PC_OFF`, include/sequence_context.h) |
| updatefx's `AudioMsg`, entity_lookup's `Msg` | a stop's or an interrupt's tag after the header (tAudioMsg's startEffect, +8); the union's N64 positions are its largest member's (updatefx's is shared with the N64 build, so it scores 4) |
| ctx_slot.c's `DecodeGhost` | a heap handle slot is 8 bytes holding a zero-extended window address; a u32 reader takes its low half (POINTERS.md 9.10) |
| camera_lerp_position's `Rec20` | it clears the mine slots' count and its padding with an `intptr_t` 0 |
| race_init_helper | the s16 at +0xF4 is the node handle's low half |
| blast.c's `save_write_data` | StartBlast's data is an s16 slot or an f32 position (endian_check's NOT_FOLLOWED) |
| src/rom's `__osSiDmaBuffer` | libultra builds the PIF commands byte by byte in its word buffer |
| `func_800AED20` | the source's active byte; engine_sound_sync's `Source` names it and `queued` as a u32 word nothing reads whole |
| `func_800A1910` | a byte compare (memcmp) |
| ap.c's `caller_a`, `caller_b` | dead stand-in callers kept for matching; their walk through the tag's list field is not the tags' link |

`REFERENCE`: level_objects' `Node` (the level-object nodes) and `Matrix64` (the dead-reckoning records),
audiomgr_thread's `tFxSequence`, `anon_ll_id_3c` and the sorted-tag node `anon_te_tag_c` (also for hud_setup's
pools, through its `Voice` and `Chan`),
engine_sound_sync's `Source` (also for entity_helpers' `CamSlot`), sound_control's `Blit` (for any view named
`Blit`). `SYMBOL_REFERENCE`: `D_80142DD8` takes audiomgr_thread's `tAudioMsg`, since gen_data chose transform's
`Msg`. `LINKS`: `func_80091FBC`, `pool_init`, `struct_fields_init`.

## 8. Limits

- Indirect calls are followed only from a slot the code stores a function's address into. A function whose
  address is in initialized data (the ROM's tables: the target start functions, blit callbacks), is passed as an
  argument, or is stored into a slot reached through a generic helper has its parameters as roots of their own,
  which only meet other views through what they pass on. Message queues and other `void *` hand-overs aren't
  followed either (hence the FX tags' second root).
- Tree links come only from a walk that steps `pp = &(*pp)->f`; a tree walked through a stack or by index isn't
  seen as one.
- PC-only is the declaration's line in the unit's own source; a view from a header counts as the N64 build's too
  (a padded view there scores 4).
- No call contexts beyond returns: a parameter that takes several records projects its accesses into all of them.
- An access with no view opinion is only checked against a reference's scalars: a raw N64 offset that lands exactly
  on another field of the same width (or in a byte array such as a name) is not seen; that is how controls.c's
  rumble byte had to be found by hand next to the name.
- Positions are host offsets; a field reached at an unknown offset (pointer arithmetic on a non-constant byte count)
  is left out. Pointers built as integers are followed only as `(T *)(load + c)`.
- References are as good as their views: gen_data's choice is merged with the symbol's other declarations of the
  same size, and a storage without one only compares its views with each other (a tie reports both).
- Weak context copies are skipped, as endian_check skips them; units that aren't in the build aren't seen.
