# Non-matching drafts: the matching ceiling

These are the best C drafts for the functions that are NOT byte-matched (the matching ceiling,
2026-10-26). The ROM is built from the retail bytes for every one of them; these files are not compiled
into the build and earn no coverage. Each file's header states its identity (MAT3/Dreamcast source), the
distance from retail (differing words or rows after alignment), the compiler mechanism that stops it, and
any disclosed shaping. The full table, with the mechanism and any runtime verdict for each function, is
[`docs/game-asm-ceiling.md`](../docs/game-asm-ceiling.md).

| Population | Function | Bytes | Identity | Distance | File |
|---|---|---:|---|---|---|
| game | video_animText | 2,444 | animText@videoscr.c | 423/611 words | `game/video_animText.c` |
| game | display_list_flush | 2,356 | PrePlay@game.c | 58 rows, frame exact | `game/display_list_flush.c` |
| game | camera_transform | 2,228 | AudioSfxServer@audiosfx.c | 189 rows, frame exact | `game/camera_transform.c` |
| game | RaceStateMachine_Update | 2,148 | updatePauseScreen@pausescr.c | 47 rows | `game/RaceStateMachine_Update.c` |
| game | highscore_animText | 1,780 | animText@highscorescr.c | 2 of 445 words | `game/highscore_animText.c` |
| game | animSpeedText | 1,460 | animSpeedText@hud.c | 31 words, length exact | `game/animSpeedText.c` |
| game | physics_float_calc | 1,176 | WorldCullObjects@world.c | 32 words | `game/physics_float_calc.c` |
| game | CreatePlayer | 1,088 | CreatePlayer@save.c (N64 head) | 120 rows | `game/CreatePlayer.c` |
| game | drone_set_catchup | 852 | PS_ReadPartialFile@paksysdc.c | 21 rows | `game/drone_set_catchup.c` |
| game | createBG | 732 | createBG@front.c | 51 words, ops 0 | `game/createBG.c` |
| game | StartWeaponIcon | 704 | _StartWeaponIcon@targets.c | 23 rows | `game/StartWeaponIcon.c` |
| game | TB_EllipseText | 484 | TB_EllipseText@TB_text.c | 60 rows | `game/TB_EllipseText.c` |
| game | func_8010D680 | 476 | _AnimateWeaponIcon@targets.c | 22 rows | `game/func_8010D680.c` |
| game | ghost_race_setup | 440 | GhostEnd@ghost.c | 3 of 110 words | `game/ghost_race_setup.c` |
| game | state_utility | 396 | TB_DrawText@TB_text.c | 10 rows | `game/state_utility.c` |
| game | func_8010E8B4 | 340 | animPanes@videoscr.c | 3 of 85 words | `game/func_8010E8B4.c` |
| game | display_enable | 316 | LoadFrontEndOverlay@state.c | 55 of 79 words | in `src/blob/groups/state/state.c` (context) |
| game | object_bytes_sum_global | 84 | TB_GetFontLineHeight | 4 of 21 words | `game/object_bytes_sum_global.c` |
| game | object_bytes23_sum | 72 | TB_GetFontHeight | 4 of 18 words | `game/object_bytes23_sum.c` |
| ovl_a | func_8038CD14 | 4,824 | track loader | 354/1206 words | `ovl_a/func_8038CD14.c` |
| ovl_a | func_803A9088 | 3,668 | drawDeletePlayer@playerscr.c | 10/917 words | `ovl_a/func_803A9088.c` |
| ovl_a | func_803A44FC | 3,668 | – | 2/917 words | `ovl_a/func_803A44FC.c` |
| ovl_a | func_803AD57C | 2,092 | – | 497/523 | in `src/ovl_a/groups/runtime_a_stats_menu/group.c` (context) |
| ovl_a | func_803A9EEC | 1,260 | – | 102/315 | `ovl_a/func_803A9EEC.c` |
| ovl_a | func_8038A820 | 1,168 | animTrackMaps@trackscr.c | 292/292 with .data at 8 mod 16 | `ovl_a/func_8038A820.c` |
| ovl_a | func_8038A400 | 556 | updateWarningScreen@warningscr.c | 118/139 | `ovl_a/func_8038A400.c` |
| ovl_b | func_80391B00 | 2,380 | – | 83/595 with a holder; 407/595 clean | `ovl_b/func_80391B00.c` |

Stubs (8-byte `jr ra; nop` bodies of inlined helpers) land only with their callers; see the ceiling page.

