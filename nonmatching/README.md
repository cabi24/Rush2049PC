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
| game | PrePlay | 2,356 | PrePlay@game.c | `game/PrePlay.c` |
| game | AudioSfxServer | 2,228 | AudioSfxServer@audiosfx.c | `src/blob/groups/audiosfx_play/group.c` |
| game | updatePauseScreen | 2,148 | updatePauseScreen@pausescr.c | `game/updatePauseScreen.c` |
| game | highscore_animText | 1,780 | animText@highscorescr.c | `src/blob/groups/highscorescr_draw/highscore.c` |
| game | animSpeedText | 1,460 | animSpeedText@hud.c | `game/animSpeedText.c` |
| game | WorldCullObjects | 1,176 | WorldCullObjects@world.c | `game/WorldCullObjects.c` |
| game | CreatePlayer | 1,088 | CreatePlayer@save.c (N64 head) | `src/blob/groups/save/group.c` |
| game | PS_ReadPartialFile | 852 | PS_ReadPartialFile@paksysdc.c | `game/PS_ReadPartialFile.c` |
| game | createBG | 732 | createBG@front.c | `game/createBG.c` |
| game | StartWeaponIcon | 704 | _StartWeaponIcon@targets.c | `game/StartWeaponIcon.c` |
| game | TB_EllipseText | 484 | TB_EllipseText@TB_text.c | `game/TB_EllipseText.c` |
| game | AnimateWeaponIcon | 476 | _AnimateWeaponIcon@targets.c | `game/AnimateWeaponIcon.c` |
| game | GhostEnd | 440 | GhostEnd@ghost.c | `game/GhostEnd.c` |
| game | TB_DrawText | 396 | TB_DrawText@TB_text.c | `game/TB_DrawText.c` |
| game | video_animPanes | 340 | animPanes@videoscr.c | `game/video_animPanes.c` |
| game | LoadFrontEndOverlay | 316 | LoadFrontEndOverlay@state.c | `src/blob/groups/state/state.c` |
| game | TB_GetFontLineHeight | 84 | TB_GetFontLineHeight | `src/blob/groups/animgear/ctx_gr3.c` |
| game | TB_GetFontHeight | 72 | TB_GetFontHeight | `src/blob/groups/animgear/ctx_gr3.c` |
| ovl_a | buildTrackMaps | 4,824 | track loader | `src/ovl_a/groups/trackscr/trackscr.c` |
| ovl_a | drawDeletePlayer | 3,668 | drawDeletePlayer@playerscr.c | `src/ovl_a/groups/playerscr_draw/playerscr_draw.c` |
| ovl_a | carscr_animText | 3,668 | – | `ovl_a/carscr_animText.c` |
| ovl_a | recordscr_animText | 2,092 | – | `src/ovl_a/groups/recordscr_draw/recordscr_draw.c` |
| ovl_a | playerscr_animText | 1,260 | – | `src/ovl_a/groups/playerscr_draw/playerscr_draw.c` |
| ovl_a | animTrackMaps | 1,168 | animTrackMaps@trackscr.c | `src/ovl_a/groups/trackscr/trackscr.c` |
| ovl_a | findDamageSlot | 556 | updateWarningScreen@warningscr.c | `ovl_a/updateWarningScreen.c` (compiled as updateWarningScreen) |
| ovl_b | UpdateHUDExtraPostCamera | 2,380 | – | `src/ovl_b/groups/hud_icons/hudextra_icons.c` |

Each draft's header says who reaches it; `port/assets/tests/test_weapons.c`, `test_hudtext.c` and `test_carscr.c` run
some of them on synthetic records. `ovl_a/stubs.c` and `ovl_b/stubs.c` are the runtime images' 18 stubs (8-byte
`jr ra; nop` bodies of inlined helpers on the N64), as empty functions.
