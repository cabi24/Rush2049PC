# The functions the decompilation never matched

The N64 decompilation rebuilt the ROM byte for byte, except for these functions (its "matching ceiling",
2026-10-26), which the ROM took from their retail assembly. Their C was kept as drafts. The PC build compiles the
drafts in this folder, the ones whose functions no file under `src/` defines; the others' C is in the `src/` file the
table names. Since 2026-11-08 there is no N64 build ([docs/port/PCONLY.md](../docs/port/PCONLY.md)), and the drafts
the PC never compiled are gone (they are in the history). The identities, from the game's other versions, are
evidence for names; [`docs/game-asm-ceiling.md`](../docs/game-asm-ceiling.md) has more on each.

| Image | Function | Bytes | Identity | C |
|---|---|---:|---|---|
| game | video_animText | 2,444 | animText@videoscr.c | `src/blob/groups/videoscr/videoscr.c` |
| game | display_list_flush | 2,356 | PrePlay@game.c | `game/display_list_flush.c` |
| game | camera_transform | 2,228 | AudioSfxServer@audiosfx.c | `src/blob/groups/MP_TargetSteerPos/group.c` |
| game | RaceStateMachine_Update | 2,148 | updatePauseScreen@pausescr.c | `game/RaceStateMachine_Update.c` |
| game | highscore_animText | 1,780 | animText@highscorescr.c | `src/blob/groups/highscore_anim/highscore.c` |
| game | animSpeedText | 1,460 | animSpeedText@hud.c | `game/animSpeedText.c` |
| game | physics_float_calc | 1,176 | WorldCullObjects@world.c | `game/physics_float_calc.c` |
| game | CreatePlayer | 1,088 | CreatePlayer@save.c (N64 head) | `src/blob/groups/menu_save_options/group.c` |
| game | drone_set_catchup | 852 | PS_ReadPartialFile@paksysdc.c | `game/drone_set_catchup.c` |
| game | createBG | 732 | createBG@front.c | `game/createBG.c` |
| game | StartWeaponIcon | 704 | _StartWeaponIcon@targets.c | `game/StartWeaponIcon.c` |
| game | TB_EllipseText | 484 | TB_EllipseText@TB_text.c | `game/TB_EllipseText.c` |
| game | func_8010D680 | 476 | _AnimateWeaponIcon@targets.c | `game/func_8010D680.c` |
| game | ghost_race_setup | 440 | GhostEnd@ghost.c | `game/ghost_race_setup.c` |
| game | state_utility | 396 | TB_DrawText@TB_text.c | `game/state_utility.c` |
| game | func_8010E8B4 | 340 | animPanes@videoscr.c | `game/func_8010E8B4.c` |
| game | display_enable | 316 | LoadFrontEndOverlay@state.c | `src/blob/groups/state/state.c` |
| game | object_bytes_sum_global | 84 | TB_GetFontLineHeight | `src/blob/groups/animgear/ctx_gr3.c` |
| game | object_bytes23_sum | 72 | TB_GetFontHeight | `src/blob/groups/animgear/ctx_gr3.c` |
| ovl_a | func_8038CD14 | 4,824 | track loader | `src/ovl_a/groups/runtime_a_c93c_closure/group.c` |
| ovl_a | func_803A9088 | 3,668 | drawDeletePlayer@playerscr.c | `src/ovl_a/groups/runtime_a_profile_menu/group.c` |
| ovl_a | func_803A44FC | 3,668 | – | `ovl_a/func_803A44FC.c` |
| ovl_a | func_803AD57C | 2,092 | – | `src/ovl_a/groups/runtime_a_stats_menu/group.c` |
| ovl_a | func_803A9EEC | 1,260 | – | `src/ovl_a/groups/runtime_a_profile_menu/group.c` |
| ovl_a | func_8038A820 | 1,168 | animTrackMaps@trackscr.c | `src/ovl_a/groups/runtime_a_c93c_closure/group.c` |
| ovl_a | func_8038A400 | 556 | updateWarningScreen@warningscr.c | `ovl_a/func_8038A400.c` (compiled as updateWarningScreen) |
| ovl_b | func_80391B00 | 2,380 | – | `src/ovl_b/groups/runtime_b_status_scene/group.c` |

Each draft's header says who reaches it; `port/assets/tests/test_weapons.c`, `test_hudtext.c` and `test_carscr.c` run
some of them on synthetic records. `ovl_a/stubs.c` and `ovl_b/stubs.c` are the runtime images' 18 stubs (8-byte
`jr ra; nop` bodies of inlined helpers on the N64), as empty functions.
