# Saves, text, blits and the HUD

This page is for a programmer new to four connected parts of the game: the 2D blitter and its blits and panes, the
text system, the HUD, and the Controller Pak saves (profiles, records, ghosts, options, passwords, the pak error
screens). It uses the tree's current names (wave 2b, 2026-11-08). The declarations are in `include/game/blit.h`,
`text.h`, `hud.h` and `save.h`, and their structures in those sections of `include/game/types.h`.

MAT3 (the user's GameCube decomp of the same game) has the original source of each part: `blit.c`, `blit_pc.c`,
`zoidblit.c` and `pane.c` for the blits, `HAL_text.c`, `TB_text.c` and `text.c` for the text, `hud.c` and `hudextra.c`
for the HUD, `paksysdc.c`, `save.c`, `gamerecords.c`, `ghost.c`, `password.c` and `pakerr.c` for the saves. Where the
N64 code is the same function, its name and its fields' names are MAT3's.

## 1. Blits and the blitter

A **blit** is one 2D sprite: a texture (or a plain colour, or nothing for a text blit), a position, a crop, an
alpha, a depth, and an **animation function** that runs once a frame. The HUD and every menu are made of blits.

| Piece | Files | What |
|---|---|---|
| The blits (MAT3 `blit.c`) | `InitBlits.c`, `NewBlit.c`, `InitBlit.c`, `RenameBlit.c`, `SelectBlit.c`, `NewMultiBlit.c`, `RemoveBlit.c`, `DrawBlits.c`, `groups/update_blit/group.c`, `move_blit_chain.c` | `tBlit`: the pool of 200 (`BlitPtrs`, the first `NumBlits` in use), made one at a time or as a chain from a table of `tBlitDef` (a *multiblit*); `DrawBlits` runs each one's `animFunc` (0 back removes it) |
| The zoid blit list (MAT3 `zoidblit.c`) | `ZOID_AddBlit.c`, `ZOID_UpdateBlit.c`, `ZOID_CropBlit.c`, `ZOID_SetBlitData.c`, `ZOID_FlipBlit.c`, `ZOID_TextBlit.c`, `ZOID_RemoveBlit.c` | `tBlitInstance` (`gBlitInstanceList`, 200): what the renderer draws; each blit owns one entry (`tBlit.blidx`) and `UpdateBlit` copies the blit into it |
| Panes (MAT3 `pane.c`) | `InitPane.c`, `ChangePaneSize.c`, `ChangePaneColor.c`, `RemovePane.c`, `RemovePanes.c` | `tBlitSet` (`gPaneBlits`, 16): a bordered rectangle of five blits (four one-pixel edges and the inside) |
| The blitter (MAT3 `blit_pc.c`, the N64 side) | `groups/blit/*.c`, `BlitDraw.c`, `blit_load_texture.c`, `blit_mask_bits.c`, `BlitScissor.c`, `BlitXLU.c`, `BlitZDist.c`, `BlitClearColors.c`, `BlitFinish.c` | turns rectangles into RDP commands in its own display list (`blit_dl`, two lists `pc_text_dl` used in turn) |

The blitter's state is `blitAttribute` (the `BLIT_ATTR_*` bits: alpha threshold, flips, depth from `BlitZDist`,
translucency from `BlitXLU`, fill, and *tall*, a screen of more than 220 lines where textures are drawn at double
height), the scissor box (`blitXmin` ...) and the RDP mode (`blitMode`, `BLIT_MODE_*`: copy mode for plain textures,
one-cycle for translucent or depth-tested ones, the intensity modes for fonts, a two-cycle mode). A frame's blitting
goes:

    StartGraphics -> BlitInit (the frame's list, the RDP set-up) and HAL_StartText
    ... the renderer walks gBlitInstanceList: blit_load_texture, BlitColors, BlitDraw per blit;
        a text blit's (BLITINST_TEXT) function draws text instead
    EndGraphics -> HAL_FinishText (the buffered text drawn), BlitFinish (G_ENDDL), the list called from the frame's

`BLIT_NO_TEXTURE` (`(const char *)-1`) as a blit's name makes a text blit: its `texture` field holds a draw function
(the HUD's `animLapCount`, `animPlace` ...) that the renderer calls with the blit.

## 2. Text

| Piece | Files | What |
|---|---|---|
| HAL_text (MAT3 `HAL_text.c`) | `HAL_*.c`, `groups/hal_text/*.c`, `groups/state/halfont.c`, `groups/hud_text_anims/gr3_b.c` (HAL_SetFontInternal), `gr3_w.c` (HAL_StartText), `groups/sound_channel_extra/*.c`, `groups/animgear/ctx_gr3.c` | the fonts and the drawing of strings |
| TB_text (MAT3 `TB_text.c`) | `TB_*.c`, `groups/options_anim/options_anim.c` (TB_FlashTextColors_AEAC) | what the screens call: justification, colours, flashing text, word wrapping, string helpers |
| The strings (MAT3 `text.c`) | `InitTextWave.c`, `LoadTextWave.c` | `gText` (`TextTables`): the current language's strings, from its wave: `single[n]`, and `taba[tab[n] + i]` (MAT3's names) |

A font is two asset waves (`FONT_DATA_WAVE(n)`, `FONT_TEXTURE_WAVE(n)`): a header (`FontHeader`), its letters
(`FontLetter`: where each is in which texture page) and a kerning table. `HAL_DrawText` doesn't draw: between
`HAL_StartText` and `HAL_FinishText` it appends commands to `gBuffer` (`TEXT_CMD_*`: font, leading, spacing,
position, translucency, colour, the string), only the state that changed, and `HAL_FinishText` runs them through
`DrawText` (a `BlitDraw` per glyph) at the frame's end. Text with a depth (`HAL_SetTextDepth`) is drawn at once. The
text lock (`text_lock`, `text_unlock`, a one-message queue) keeps the game thread and the frame's end apart.

Strings are single-byte or double-byte (`TEXT_DOUBLE_BYTE`, 0xFF first, then two bytes a character, ended by two
zeros); the TB helpers (`TB_Strcpy`, `TB_Strcat`, `TB_Strlen`) handle both. `TB_SetJust` takes MAT3's `kTB_Just*` (into `gJust`, a `TextJust` {h, v}),
`TB_SetTextColor` the colour table's `kTB_*Text` (`kTB_FlashText` moves between `gFlashColor2` and `gFlashColor1`,
`TB_TextOncePerFrame` driving `gColorCycle`).

## 3. The HUD

The race's HUD (MAT3 `hud.c`) is one multiblit made by `ShowHUD` (`groups/showhud/showhud.c`; `RaceMultiBlit` and
the split screens' tables) and removed by `RemoveHUD`. Each part is a blit with an animation function that places
it by the number of windows and the player (`animID`), hides it when the option is off or the car finished
(`Hidden`, `animPause.c`), and updates it:

| Animation | File | Shows |
|---|---|---|
| `animSpeed`, `animTach`, `animGear`, `animOdometer`, `animBones` | `groups/hud_speed/animSpeed.c`, `animTach.c`, `groups/animgear/animgear.c`, `animOdometer.c`, `animBones.c` | speedometer box, tachometer, gear, odometer digits, deaths mode's crossbones |
| `animElapsedTimer(Text)`, `animLapTime(Text)`, `animCountdownTimer` | `groups/hud_text_anims/*.c`, `groups/countdown/animCountdownTimer.c` | the race clock, a checkpoint's split time (`ShowCheckpointTime`), the time left |
| `animPlace`, `animLapCount`, `animCreditText` | `groups/hud_text_anims/*.c` | place, laps, the demo's credits |
| `hud_animTrackMap`, `animDot`, `animRadar`, `animRadarBlip` | `hud_animTrackMap.c`, `animDot.c`, `animRadar.c`, `animRadarBlip.c` | the map (drawn into its texture from the track's path), the cars' dots, the radar and its blips |
| `animCoins` | `groups/hud_coins/coins.c` | the 3D coins and the coins found (`NumCoinsFound`) |
| `hud_updateFrame` | `hud_updateFrame.c` | each frame's times as digits (`TrackTimeStr`) and the dots' drawing order (`dot_order_list`) |

`UpdateHUDPostCamera` turns the HUD's 3D objects after the camera moves. Battle and stunt races add **the extras**
(MAT3 `hudextra.c`, runtime image B, `src/ovl_b`): `ShowHUDExtra`, `RemoveHUDExtra`, `animHealth`,
`animAmmoCounts`, `animWeaponIcons`, `animPowerupIcons`, `animStuntPoints`, `animBattlePoints` (points flying from
player to player, `AddBattlePoint`).

## 4. Saves

### The pak system (MAT3 `paksysdc.c`)

`gPaks` (`tPak`, one a controller) holds each pak's state flags, its libultra handle (`pfs`), its free space, the
rumble motor's state and its 16 directory entries (`tPakFile`). The files the game has open are `PakFileRecord`s
(`pak_file_records`, 64, reached through handles, `tPakHandle`), listed per pak in `gPS_Files`.

- `PS_PakServer` (`groups/paksysdc/group.c`), called by `SaveServer` each frame, notices paks put in and taken out:
  a Rumble Pak is noted, an unreadable pak goes to the error screen (repair it, give up, read it again), a pak is
  told from another by its id, a pak moved to another port takes its files along, and a new pak's directory is read.
- A file is read whole into a heap block (`PS_ReadPartialFile`), followed by its **dirty map**, one bit for each 32
  bytes. `PS_WriteFile` marks bytes dirty; `PS_FlushFile` and `PS_FlushPak` (`groups/paksysdc_flush/group.c`) write
  only the dirty runs.
- Errors (`kPS_Err*`, from libultra's by `pak_error_from_pfs`) go to `gErrorFunc`, which is `PS_ErrorFunc`: it dims
  the screen and runs the error dialog (`SetPakError`, the pak error screens of `groups/pakerr`) until the player
  answers.
- The serial bus lock (`si_lock`, `si_unlock`) guards the controllers and paks; the rumble thread
  (`groups/rumble/group.c`) runs the motors once a retrace from each pak's `request`.

### Profiles, records, ghosts (MAT3 `save.c`, `gamerecords.c`, `ghost.c`)

A **profile** in memory is a `tProfile` (`player_profile_records`, through handles); its file on the pak is a
`tSaveData` (2064 bytes, a file format: the layout is the file's): name and id, the 21 game options, the controls,
each race type's settings, each track's race stats (12: six tracks forwards and backwards, `tRaceStats` with the
best laps, averages and ghost times), the stunt, battle, obstacle and circuit stats, the circuit in progress and 13
cars' settings, each part with its own checksum (`CalcChecksumInternal`).

| Piece | Files |
|---|---|
| Profiles | `groups/save/group.c` (InitPlayer, CheckAndRepairPlayer, ReadPlayerData, CreatePlayer, ReadPlayer, SaveServer), `ClearPlayerRecords.c`, `PurgePlayerData.c`, `MovePlayerFile.c`, `RemovePlayerFile.c` |
| Settings and options | `GetPlayer*.c`, `groups/save_*/*.c`, `groups/save_option/save_option.c`, `groups/save_get_option/reverb.c`, `GetDefaultOption.c`, `SaveSelectPlayer.c`, `groups/game_sounds/InitOptions.c`, `groups/options_set/group.c` (`gOptions`, MAT3's `s8[41]`, indexed by `kOpt*`) |
| Records | `Record*.c` (after a race, per race type), `AddPlayerScores.c`, `AddPlayerStats.c`, `RemovePlayerScores.c`, `FindWinner.c`, `FindTempNameIdx.c`, `InitRecords.c`, `ClearTotalStats.c` |
| Unlocks and coins | `CalcUnlockedStuff.c`, `Is*Unlocked.c`, `FoundCoin.c`, `IsCoinFound.c`, `NumCoinsFound.c`, `CountBits.c` |
| Ghosts | `groups/save/ctx_slot.c` (SaveGhost ...), `groups/save_limbo/SaveGhostInLimbo.c`: `tGhostFile`, `tGhostHeader` |
| Passwords (MAT3 `password.c`) | `NewPassword.c`, `StorePasswordElement.c`, `GetPasswordElement.c`, `groups/password/group.c` |

After a race `RecordGameOver` adds each player's results to its profile and to the totals over all profiles
(`gTotalRaceStats` ...), and the high score tables (`gTotalNames`) take the best times; `CalcUnlockedStuff` then
works out what each player has unlocked (cars by coins, parts by miles, arenas by stunt points and kills, circuits by
the circuit before). A player without a profile plays on a "just play" profile (`gJustPlayers`, in memory only).

## 5. What is still N64-shaped (for wave 3)

- **Layouts by N64 offsets.** `PROFILE_OFS`, `PSF_OFS` and `GHOST_OFS` (pc_prelude.h) map the N64's offsets of the
  profile, pak file and ghost records to the PC's: `groups/save/group.c` (CheckAndRepairPlayer, CreatePlayer,
  ReadPlayer ...) and `groups/save/ctx_slot.c` still read them so; `tProfile`, `PakFileRecord` and `tGhostFile` are
  the structures to use instead. Several files still read a record through a view of their own with N64-sized
  padding (`groups/save/ctx_slot.c`'s `Slot`, `Obj`, `MenuData`; the HUD's input records `gPlayers`, 0x4C bytes on
  the N64).
- **Window addresses in data.** A font letter's kerning row (`FontLetter.kerns`), a font texture's and the track
  map's texels (`TXHD` records keep the N64 layout), `tGhostHeader.start`, the heap's links and the handle slots are
  32-bit N64 addresses read through `PC_N64P`/`PC_N64W`/`PC_N64A`. The heap functions here (`ClearMemoryHeap`,
  `allocate_memory_at`, `heap_move_block_low`) and the overlay loader (`load_overlay`, the runtime images' region
  `RUNTIME_IMAGE_START`) work in N64 addresses.
- **The game's own data in the window**: the pools (`BlitPtrs`, `gBlitInstanceList`, `gPaneBlits`, `gPaks`,
  `pak_file_records`) are own storage at their N64 addresses; the save data's totals are in the arena.
- **The RDP.** The blitter writes N64 display list commands (`G_TEXRECT`, `G_LOADTLUT` ...) that port/gfx renders;
  `BLIT_ATTR_TALL` and the copy mode are the N64's ways of drawing.
- **Types to finish**: `HudBlitDef` is an alias of `tBlitDef` (frontend.h uses it); the prototypes of `NewBlit`, `NewMultiBlit`,
  `InitPane`, `RemoveBlit` and `PS_WriteFile` are still each caller's own, as the callers pass blits and addresses as
  pointers or integers.
