# The matching ceiling

*History: the N64 build this page describes was dropped on 2026-11-08 ([port/PCONLY.md](port/PCONLY.md)), with the
assembly and the drafts the PC never compiled. The identities below stay as evidence for names, and each table's
last column says where the function's C is now ([Where the C went](#where-the-c-went)).*

The ROM was byte-identical, but not every function in it was built from C. This page lists the functions that were
not, why, and where their best C drafts were. Every function here was built from its retail assembly
(`asm/us/blob`, `asm/us/ovl_a`, `asm/us/ovl_b`) and was **not counted as matched**; the drafts, in a folder of
their own (`nonmatching/`), were not compiled into the ROM.

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
  after each call and spill `s0` themselves), `unzipdata_from_rom` and `unzipdata` (a call hand-edited after
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
  was tried without effect (GhostEnd, 48 positions), the boundary's source form is unknown.
- **Phantom values.** Retail carries a register-only value across a call that our source never creates (the
  font-height pair, TB_DrawText): the address web lands in `t4` instead of `t2`.
- **`as1` cross-block hoists and line ties.** The assembler moves an instruction across a block and
  copy-propagates with its own liveness (both weapon-icon functions, carscr_animText, recordscr_animText); ties are
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

| Function | Bytes | Identity | Distance | Mechanism | Runtime | C now |
|---|---:|---|---|---|---|---|
| video_animText | 2,444 | animText@videoscr.c | 423/611 words | 3-trip loop never unrolls; retail's bottom-row blocks are straight-line | caller of video_animButtonText, whose match is provisional on this one | [`src/blob/groups/videoscr/videoscr.c`](../src/blob/groups/videoscr/videoscr.c) |
| PrePlay | 2,356 | PrePlay@game.c | 58 rows, frame exact | caller-saved colour ties after the callee-saved spills were fixed | – | [`src/blob/groups/update_game/PrePlay.c`](../src/blob/groups/update_game/PrePlay.c) |
| AudioSfxServer | 2,228 | AudioSfxServer@audiosfx.c | 189 rows, frame exact | gSfxInstances load not hoisted out of the command-scan loop; structural | – | [`src/blob/groups/audiosfx_play/group.c`](../src/blob/groups/audiosfx_play/group.c) |
| updatePauseScreen | 2,148 | updatePauseScreen@pausescr.c | 47 rows | 13 `li t0,2` at the PRE insertion points of the gPauseState load | behaves as retail in 7 of 9 traced states | [`src/blob/groups/pausescr_state/updatePauseScreen.c`](../src/blob/groups/pausescr_state/updatePauseScreen.c) |
| highscore_animText | 1,780 | animText@highscorescr.c | 2 of 445 words | uopt emits hoisted preheader constants in reverse colouring order | – | [`src/blob/groups/highscorescr_draw/highscore.c`](../src/blob/groups/highscorescr_draw/highscore.c) |
| animSpeedText | 1,460 | animSpeedText@hud.c | 31 words, length exact | four split address pieces coloured differently (not a tie) | – | [`src/blob/groups/hud_speed/animSpeedText.c`](../src/blob/groups/hud_speed/animSpeedText.c) |
| WorldCullObjects | 1,176 | WorldCullObjects@world.c | 32 words | a web keeping `a1` busy through the loop; induction-variable numbering; latch order | – | [`src/blob/WorldCullObjects.c`](../src/blob/WorldCullObjects.c) |
| CreatePlayer | 1,088 | CreatePlayer@save.c (N64 head) | 120 rows | structure of the pak-name head; retail register map known | behaves as retail over 4 traced creates | [`src/blob/groups/save/group.c`](../src/blob/groups/save/group.c) |
| PS_ReadPartialFile | 852 | PS_ReadPartialFile@paksysdc.c | 21 rows | one ugen temporary pop; size/file `v0`-`v1` order | – | [`src/blob/PS_ReadPartialFile.c`](../src/blob/PS_ReadPartialFile.c) |
| createBG | 732 | createBG@front.c | 51 words, operations equal | two extra ugen ring pops before the gBackCamera load; `lui t1; addu; lw t3` occurs nowhere else | – | [`src/blob/createBG.c`](../src/blob/createBG.c) |
| StartWeaponIcon | 704 | _StartWeaponIcon@targets.c | 23 rows | as1 hoist of the default case into the jump-table dispatch | – | [`src/blob/groups/targets/StartWeaponIcon.c`](../src/blob/groups/targets/StartWeaponIcon.c) |
| TB_EllipseText | 484 | TB_EllipseText@TB_text.c | 60 rows | `-1` argument kept as a narrowed register value; phantom `s5` save | – | [`src/blob/TB_EllipseText.c`](../src/blob/TB_EllipseText.c) |
| AnimateWeaponIcon | 476 | _AnimateWeaponIcon@targets.c | 22 rows | two as1 cross-block hoists | – | [`src/blob/groups/targets/AnimateWeaponIcon.c`](../src/blob/groups/targets/AnimateWeaponIcon.c) |
| GhostEnd | 440 | GhostEnd@ghost.c | 3 of 110 words | a codeless uopt block boundary in the main block (48 positions tried) | – | [`src/blob/GhostEnd.c`](../src/blob/GhostEnd.c) |
| TB_DrawText | 396 | TB_DrawText@TB_text.c | 10 rows | two phantom values across `_SetupFontData` | – | [`src/blob/TB_DrawText.c`](../src/blob/TB_DrawText.c) |
| video_animPanes | 340 | animPanes@videoscr.c | 3 of 85 words | `y` unpropagated and menuRect loads live with the line-height return | – | [`src/blob/groups/videoscr/video_animPanes.c`](../src/blob/groups/videoscr/video_animPanes.c) |
| LoadFrontEndOverlay | 316 | LoadFrontEndOverlay@state.c | 55 of 79 words | flag-address webs; load_overlay argument order | **behaves as retail**: 136 retail calls and 11 draft calls (images A and B) traced identical | [`src/blob/groups/state/state.c`](../src/blob/groups/state/state.c) |
| TB_GetFontLineHeight | 84 | TB_GetFontLineHeight | 4 of 21 words | phantom values (as TB_DrawText) | – | [`src/blob/groups/animgear/TB_GetFontHeight.c`](../src/blob/groups/animgear/TB_GetFontHeight.c) |
| TB_GetFontHeight | 72 | TB_GetFontHeight | 4 of 18 words | phantom values | – | [`src/blob/groups/animgear/TB_GetFontHeight.c`](../src/blob/groups/animgear/TB_GetFontHeight.c) |

## Runtime image A (7 functions + 20 stubs, 17,396 B)

| Function | Bytes | Identity | Distance | Mechanism | C now |
|---|---:|---|---|---|---|
| buildTrackMaps | 4,824 | track loader | 354/1206 words | ugen generates it twice (FP spills); structural rows first | [`src/ovl_a/groups/trackscr/trackscr.c`](../src/ovl_a/groups/trackscr/trackscr.c) |
| drawDeletePlayer | 3,668 | drawDeletePlayer@playerscr.c | 10/917 words | one more coloured web | [`src/ovl_a/groups/playerscr_draw/playerscr_draw.c`](../src/ovl_a/groups/playerscr_draw/playerscr_draw.c) |
| carscr_animText | 3,668 | – | 2/917 words | as1 cross-block scheduling (`bne`/`sll` swapped) | [`src/ovl_a/groups/carscr/carscr_animText.c`](../src/ovl_a/groups/carscr/carscr_animText.c) |
| recordscr_animText | 2,092 | – | 497/523 words | same as1 family as carscr_animText | [`src/ovl_a/groups/recordscr_draw/recordscr_draw.c`](../src/ovl_a/groups/recordscr_draw/recordscr_draw.c) |
| playerscr_animText | 1,260 | – | 102/315 words | needs a noalias region open at the end of case 6 | [`src/ovl_a/groups/playerscr_draw/playerscr_draw.c`](../src/ovl_a/groups/playerscr_draw/playerscr_draw.c) |
| animTrackMaps | 1,168 | animTrackMaps@trackscr.c | 292/292 with .data at 8 mod 16 | the image's `.data` layout | [`src/ovl_a/groups/trackscr/trackscr.c`](../src/ovl_a/groups/trackscr/trackscr.c) |
| findDamageSlot | 556 | updateWarningScreen@warningscr.c | 118/139 words | its own .data cannot be placed; Dreamcast port | [`src/ovl_a/updateWarningScreen.c`](../src/ovl_a/updateWarningScreen.c) (compiled as updateWarningScreen) |

Stubs (8-byte `jr ra; nop` bodies of helpers that were deleted after inlining; each can only land with its
caller): unloadPathData, loadPathData, buildRaceTrackMaps, buildObstacleTrackMaps, buildBattleTrackMaps, buildStuntTrackMaps
(buildTrackMaps); carscr_drawOptions_stub, carscr_animButtonText_stub (carscr_animText); recordscr_drawView_stub (recordscr_animText); drawPakFull,
drawPakDamaged (playerscr_animText); and circuitscr_first_deleted_stub, carscr_slider_inline_stub, drawControls, recordscr_drawStuntTotals_stub, trackscr_slider_stub_first,
trackscr_slider_stub_second, trackscr_button_text_stub_first, trackscr_button_text_stub_second, trackscr_button_text_stub_third.

## Runtime image B (1 function + 4 stubs, 2,412 B)

| Function | Bytes | Identity | Distance | Mechanism | C now |
|---|---:|---|---|---|---|
| UpdateHUDExtraPostCamera | 2,380 | – | 83/595 with a holder; 407/595 clean | one code-free web numbered before the `(u8)alpha` conversion | [`src/ovl_b/groups/hud_icons/hudextra_icons.c`](../src/ovl_b/groups/hud_icons/hudextra_icons.c) |

Stubs: animStuntCoins_stub, animBattleCoins_stub (UpdateHUDExtraPostCamera); NewStreakPoly, AnimateStreakPolys.

## Where the C went

The PC build compiled the drafts in `nonmatching/<image>/` whose functions no file under `src/` defined; the other
functions' C was already in a `src/` file (a draft beside the decompiler's context, or the function whole). When the
N64 build went (2026-11-08), the drafts the PC never compiled went too (they are in the history). In wave 4
(2026-11-18, [port/RECORDS.md](port/RECORDS.md)) the fourteen that were left moved into `src/`, each beside its
subsystem's files and under the same file name (`src/blob/GhostEnd.c` is `src/blob/GhostEnd.c`), and the
folder was deleted: they are the game's C like any other file. The last column of each table above names the file
that holds the function now.

Image A's `findDamageSlot` is compiled as `updateWarningScreen`: both runtime images are linked into the program,
and image B's `findDamageSlot` (worlddam.c) has the same place in its image, so the loader's tables give both one
name ([port/SYNTH.md](port/SYNTH.md)). `port/assets/tests/test_weapons.c`, `test_hudtext.c` and `test_carscr.c` run
some of these functions on synthetic records. The runtime images' 18 stubs (8-byte `jr ra; nop` bodies of inlined
helpers on the N64, which no code calls or takes the address of) had empty functions in `ovl_a/stubs.c` and
`ovl_b/stubs.c` until wave 2 of the PC-only work deleted them with the game image's empty stubs
([port/PCONLY.md](port/PCONLY.md) 4).
