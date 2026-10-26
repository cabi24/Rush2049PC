# The matching ceiling

The ROM is byte-identical, but not every function in it is built from C. This page lists the functions that are
not, why, and where their best C drafts are. Every function here is built from its retail assembly
(`asm/us/blob`, `asm/us/ovl_a`, `asm/us/ovl_b`) and is **not counted as matched**; the drafts in
[`nonmatching/`](../nonmatching/) are not compiled into the ROM.

State, 2026-10-26:

| Population | From C | Not from C | Bytes left |
|---|---:|---:|---:|
| Static code | 634 / 677 functions, 154,372 / 162,284 B (95.12%) | 43 original hand-written assembly routines (7,816 B) | none: static code is complete |
| Game code | 1,197 / 1,216 functions, 542,284 / 561,860 code B (96.52%) | 19 functions | 19,576 |
| Runtime image A | 166 / 193, 135,556 / 152,952 B (88.63%) | 7 functions + 20 stubs | 17,396 |
| Runtime image B | 44 / 49, 36,296 / 38,708 B (93.77%) | 1 function + 4 stubs | 2,412 |

## Static code

The 43 static routines that are not C were assembly in the original build too, so they have no C form:

* 27 libultra assembly routines, each byte-identical to the object that decompals' ultralib (`e24c836`,
  `VERSION_J`) builds from its `.s` file with ultralib's own IDO recipe: `osSetIntMask`, `osWritebackDCacheAll`,
  `osWritebackDCache`, `osInvalICache`, `osInvalDCache`, `bzero`, `bcopy`, `bcmp`, `__osDisableInt`,
  `__osRestoreInt`, `osGetCount`, the exception handler (`__osExceptionPreamble`, `__osException`, `send_mesg`,
  `__osEnqueueAndYield`, `__osEnqueueThread`, `__osDispatchThread`, `__osCleanupThread`), `__osSetSR`,
  `__osGetSR`, `__osSetFpcCsr`, `osUnmapTLBAll`, `osMapTLBRdb`, `__osGetCause`, `sqrtf`, `__osSetCompare`,
  `__osProbeTLB`.
* 6 other hand-written routines: `lzss_decode` and `inflate_io_wait` (non-ABI: they return in `t1` / read `t1`
  after each call and spill `s0` themselves), `inflate_entry` and `inflate_entry_alt` (a call hand-edited after
  compilation: `li a1,12000` + `addiu a0,zero,0` after a one-argument compile; no C order reaches it), the boot
  `entrypoint` (clears .bss from 0x8002E8E0, sets `sp`, jumps to `main`), and `__setfpcsr` (`ctc1 a0,$31` and
  return; not ultralib's `__osSetFpcCsr`, which returns the old value).
* 10 routines in the boot segment tail (`asm/us/boot_data/`): nine libgsm decoder kernels that address inline
  tables PC-relatively after a never-taken `bltzal $zero`, call each other with frameless `bal`, and reach
  coefficient routines through computed `ra + imm` pointers; and a D-cache create-dirty-exclusive loop placed
  among the segment's data.

## Why the remaining game and runtime-image functions do not match

Every function below compiles to the right length and the right calls from source that follows the GameCube /
Dreamcast ports' text. What differs is register choice, spill-slot numbering or instruction order decided inside
IDO's `uopt`, `ugen` and `as1`. Each residual was traced to a mechanism:

- **Codeless block boundaries.** `uopt` decides register conflicts per basic block; retail's source had a block
  boundary that emits no instruction (an empty statement, a compiled-out test, a dead line from the original
  source). A positional sweep of `do {} while (0);` / `if (x) {}` closed several functions; where every position
  was tried without effect (ghost_race_setup, 48 positions), the boundary's source form is unknown.
- **Phantom values.** Retail carries a register-only value across a call that our source never creates (the
  font-height pair, TB_DrawText): the address web lands in `t4` instead of `t2`.
- **`as1` cross-block hoists and line ties.** The assembler moves an instruction across a block and
  copy-propagates with its own liveness (both weapon-icon functions, func_803A44FC, func_803AD57C); ties are
  broken by source line.
- **Loop forms.** `uopt` never unrolls a 3-trip loop and folds a `-1` argument back to a constant
  (video_animText, TB_EllipseText); a constant placed by partial-redundancy elimination has the same insertion
  points as a load (updatePauseScreen).
- **Layout.** `ugen`'s temporary ring (createBG) and a runtime image's `.data` residue modulo 16 (animTrackMaps,
  which would need the image's data padding to be adjustable).

"Distance" is the last measured difference: differing 32-bit words, or differing rows after alignment. "Runtime"
records, where one exists, a comparison of the draft's behaviour with retail in an emulator; it earns no
coverage.

## Game image (19 functions, 19,576 B)

| Function | Bytes | Identity | Distance | Mechanism | Runtime | Draft |
|---|---:|---|---|---|---|---|
| video_animText | 2,444 | animText@videoscr.c | 423/611 words | 3-trip loop never unrolls; retail's bottom-row blocks are straight-line | caller of video_animButtonText, whose match is provisional on this one | [`game/video_animText.c`](../nonmatching/game/video_animText.c) |
| display_list_flush | 2,356 | PrePlay@game.c | 58 rows, frame exact | caller-saved colour ties after the callee-saved spills were fixed | – | [`game/display_list_flush.c`](../nonmatching/game/display_list_flush.c) |
| camera_transform | 2,228 | AudioSfxServer@audiosfx.c | 189 rows, frame exact | gSfxInstances load not hoisted out of the command-scan loop; structural | – | [`game/camera_transform.c`](../nonmatching/game/camera_transform.c) |
| RaceStateMachine_Update | 2,148 | updatePauseScreen@pausescr.c | 47 rows | 13 `li t0,2` at the PRE insertion points of the gPauseState load | behaves as retail in 7 of 9 traced states | [`game/RaceStateMachine_Update.c`](../nonmatching/game/RaceStateMachine_Update.c) |
| highscore_animText | 1,780 | animText@highscorescr.c | 2 of 445 words | uopt emits hoisted preheader constants in reverse colouring order | – | [`game/highscore_animText.c`](../nonmatching/game/highscore_animText.c) |
| animSpeedText | 1,460 | animSpeedText@hud.c | 31 words, length exact | four split address pieces coloured differently (not a tie) | – | [`game/animSpeedText.c`](../nonmatching/game/animSpeedText.c) |
| physics_float_calc | 1,176 | WorldCullObjects@world.c | 32 words | a web keeping `a1` busy through the loop; induction-variable numbering; latch order | – | [`game/physics_float_calc.c`](../nonmatching/game/physics_float_calc.c) |
| CreatePlayer | 1,088 | CreatePlayer@save.c (N64 head) | 120 rows | structure of the pak-name head; retail register map known | behaves as retail over 4 traced creates | [`game/CreatePlayer.c`](../nonmatching/game/CreatePlayer.c) |
| drone_set_catchup | 852 | PS_ReadPartialFile@paksysdc.c | 21 rows | one ugen temporary pop; size/file `v0`-`v1` order | – | [`game/drone_set_catchup.c`](../nonmatching/game/drone_set_catchup.c) |
| createBG | 732 | createBG@front.c | 51 words, operations equal | two extra ugen ring pops before the gBackCamera load; `lui t1; addu; lw t3` occurs nowhere else | – | [`game/createBG.c`](../nonmatching/game/createBG.c) |
| StartWeaponIcon | 704 | _StartWeaponIcon@targets.c | 23 rows | as1 hoist of the default case into the jump-table dispatch | – | [`game/StartWeaponIcon.c`](../nonmatching/game/StartWeaponIcon.c) |
| TB_EllipseText | 484 | TB_EllipseText@TB_text.c | 60 rows | `-1` argument kept as a narrowed register value; phantom `s5` save | – | [`game/TB_EllipseText.c`](../nonmatching/game/TB_EllipseText.c) |
| func_8010D680 | 476 | _AnimateWeaponIcon@targets.c | 22 rows | two as1 cross-block hoists | – | [`game/func_8010D680.c`](../nonmatching/game/func_8010D680.c) |
| ghost_race_setup | 440 | GhostEnd@ghost.c | 3 of 110 words | a codeless uopt block boundary in the main block (48 positions tried) | – | [`game/ghost_race_setup.c`](../nonmatching/game/ghost_race_setup.c) |
| state_utility | 396 | TB_DrawText@TB_text.c | 10 rows | two phantom values across `_SetupFontData` | – | [`game/state_utility.c`](../nonmatching/game/state_utility.c) |
| func_8010E8B4 | 340 | animPanes@videoscr.c | 3 of 85 words | `y` unpropagated and menuRect loads live with the line-height return | – | [`game/func_8010E8B4.c`](../nonmatching/game/func_8010E8B4.c) |
| display_enable | 316 | LoadFrontEndOverlay@state.c | 55 of 79 words | flag-address webs; PrevMaxPath argument order | **behaves as retail**: 136 retail calls and 11 draft calls (images A and B) traced identical | context in [`src/blob/groups/state/state.c`](../src/blob/groups/state/state.c) |
| object_bytes_sum_global | 84 | TB_GetFontLineHeight | 4 of 21 words | phantom values (as TB_DrawText) | – | [`game/object_bytes_sum_global.c`](../nonmatching/game/object_bytes_sum_global.c) |
| object_bytes23_sum | 72 | TB_GetFontHeight | 4 of 18 words | phantom values | – | [`game/object_bytes23_sum.c`](../nonmatching/game/object_bytes23_sum.c) |

## Runtime image A (7 functions + 20 stubs, 17,396 B)

| Function | Bytes | Identity | Distance | Mechanism | Draft |
|---|---:|---|---|---|---|
| func_8038CD14 | 4,824 | track loader | 354/1206 words | ugen generates it twice (FP spills); structural rows first | [`ovl_a/func_8038CD14.c`](../nonmatching/ovl_a/func_8038CD14.c) |
| func_803A9088 | 3,668 | drawDeletePlayer@playerscr.c | 10/917 words | one more coloured web | [`ovl_a/func_803A9088.c`](../nonmatching/ovl_a/func_803A9088.c) |
| func_803A44FC | 3,668 | – | 2/917 words | as1 cross-block scheduling (`bne`/`sll` swapped) | [`ovl_a/func_803A44FC.c`](../nonmatching/ovl_a/func_803A44FC.c) |
| func_803AD57C | 2,092 | – | 497/523 words | same as1 family as func_803A44FC | context in [`src/ovl_a/groups/runtime_a_stats_menu/group.c`](../src/ovl_a/groups/runtime_a_stats_menu/group.c) |
| func_803A9EEC | 1,260 | – | 102/315 words | needs a noalias region open at the end of case 6 | [`ovl_a/func_803A9EEC.c`](../nonmatching/ovl_a/func_803A9EEC.c) |
| func_8038A820 | 1,168 | animTrackMaps@trackscr.c | 292/292 with .data at 8 mod 16 | the image's `.data` layout | [`ovl_a/func_8038A820.c`](../nonmatching/ovl_a/func_8038A820.c) |
| func_8038A400 | 556 | updateWarningScreen@warningscr.c | 118/139 words | its own .data cannot be placed; Dreamcast port | [`ovl_a/func_8038A400.c`](../nonmatching/ovl_a/func_8038A400.c) |

Stubs (8-byte `jr ra; nop` bodies of helpers that were deleted after inlining; each can only land with its
caller): func_8038CCE4, func_8038CCEC, func_8038CCF4, func_8038CCFC, func_8038CD04, func_8038CD0C
(func_8038CD14); func_803A44EC, func_803A44F4 (func_803A44FC); func_803AD574 (func_803AD57C); func_803A9EDC,
func_803A9EE4 (func_803A9EEC); and func_8039A440, func_803A3D9C, func_803A8B6C, func_803ACA8C, func_803AE62C,
func_803AE634, func_803AECC4, func_803AECCC, func_803AECD4.

## Runtime image B (1 function + 4 stubs, 2,412 B)

| Function | Bytes | Identity | Distance | Mechanism | Draft |
|---|---:|---|---|---|---|
| func_80391B00 | 2,380 | – | 83/595 with a holder; 407/595 clean | one code-free web numbered before the `(u8)alpha` conversion | [`ovl_b/func_80391B00.c`](../nonmatching/ovl_b/func_80391B00.c) |

Stubs: func_80391640, func_80391648 (func_80391B00); func_8038CB10, func_8038CB18.

## Landing a match

A function here that someone matches lands like any other: add its C under `src/blob/` (or `src/ovl_<x>/`, or as
a group under `groups/`), add it to `manifest/<image>.json` with its flags, freeze its own-data placement
(`python3 tools/build_image.py <image> --freeze-placements ...`), and the image gate plus the ROM SHA-1 decide.
