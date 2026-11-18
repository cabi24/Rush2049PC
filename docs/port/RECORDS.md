# One type per record (wave 4)

Every record the game shares between files has one declaration, in `include/game`, with every field the game uses
named and typed. A file includes `game/game.h` and uses that declaration. It doesn't declare its own copy that names
only the fields it touches and skips the rest with byte arrays.

The headers before this wave declared each datum once (PCONLY.md 2). Many records still had a different struct in each
file that reads them: the physics model, for example, had 73 declarations across the files that use it. A file that
read a datum with a different type said so with a `VIEW()` line ([view.h](../../include/game/view.h)). Those copies
had to agree on the layout, and `view_check.py`, `view_audit.py` and `layout_check.py`
([LAYOUT.md](LAYOUT.md)) checked that they did. With one declaration there is nothing to disagree.

## 1. The rules

- A record shared by two or more files is declared once, in the header of its subsystem. A record that only one file
  uses may stay in that file, declared whole.
- MAT3's names are preferred for records and fields. A field that no code reads keeps its place under a name that
  says so.
- Comments describe the record as the PC has it. Offsets are given only where they belong to a file format: a save,
  a ROM file the loader converts, or a network message.
- A file that reads a field as another type does so in its code, through a union member or a commented conversion,
  not through a second struct.

## 2. The car's physics

### 2.1 The model record

(Agent A1: the model record, MAT3's MODELDAT.)

### 2.2 The car's descriptor, its tune, its tires and the paths

(Agent A2: the physics descriptor, the tune, the tires, the reckoning, resurrection, the drones' paths.)

## 3. The game car, the scene and drawing

### 3.1 The game car

(Agent B1: the game car, MAT3's CAR_DATA.)

### 3.2 The scene

(Agent B2: visuals, matrices and vectors, windows, scene objects, polygons, skids, blasts, smoke, texture swaps.)

### 3.3 Blits and colours

The 2D sprites of the HUD and the menus and the game's colours (agent B3). Each record below is declared once, in
`include/game/types.h`'s blit part (the blitter's own data in [blit.h](../../include/game/blit.h)), and every file
uses that declaration ([docs/code/ui.md](../code/ui.md)).

| Record | What it holds | Its other tags before |
|---|---|---|
| `tBlit` (MAT3 `_tBlit`) | one blit: texture name, palette or flat colour, texture, place, size, crop, alpha, flip, hide, the animation function, its argument (`animID`) and data (`animDTA`), the zoid entry it owns (`blidx`), the next blit of its multiblit (`child`) | `Blit` (13 files), `HostBlit` (3), animBones' `BonesBlit`, the pause pages' `MenuItem` (3); the headers' `struct Blit *`, `struct Voice *` (`HudBlit1`, `HudBlit2`, `gameover_blit`), `struct SoundState *` (`backBlit`) and `void *` (`multiPlayerBlit`, `hudextra_blit`) |
| `tBlitDef` (MAT3 `tBlitDef`) | one entry of a multiblit table: texture name, offset, size, crop, depth, alpha, animation function and its argument | `MultiBlit` (12 files: 11 never used it, ShowHUD's PC copy of the race set did), frontend.h's `HudBlitDef`, pc_prelude.h's `struct pc_multiblit` (13 tables in audio.h, frontend.h, race.h and save.h; now test_carshud.c's own), `SoundClearRecord` (`frontBack`, `trackBack`) |
| `tBlitSet` (MAT3 `tBlitSet`) | a pane: in use, permanent, the inside's and the border's colours, its five blits | `FiveParts` (6 files, frontend.h's `carscr_pane`, `playerscr_pane`, `circuitscr_pane`), `void *` (`recordscr_viewPane`, `video_gTitlePane`, `gTestPane`, `gConfirmPane`) |
| `tRGBA` (MAT3 `tRGBA`) | a colour: `c[0]` to `c[3]` red, green, blue, alpha in memory; `rgba` moves the four bytes at once | `union Color` (11 files, and cars.h's `GetCarsIRColor`, `SetSheenColor`), `Color4` (4), `RGBA` (3), `MapColor`, anim_car_color_marker's `Color`, TB_SetTextColor's `Color8`; include/game's `BlitColor`, `ArrowColor`, `HudColor`, `ButtonColor`, `ColorWord`, `Color8` (`gFontColors` is `tRGBA[22][2]`, MAT3's `gFontColors[22][2][4]`), and `PcText4` for `modescr_button_color` |
| `PaneColors` | a pane's two colours as `ChangePaneColor` takes them, the inside's and the border's (`hud_playerPaneColors`) | its fields were `u8 x[4], y[4]` |
| `BlitXluColors` (blit.h) | `blitPrimColor`: the primitive and the environment colour `BlitXLU` loads | two `BlitColor` |

**Fields that disagreed.** Every local view of the blit read `hide` as `s8`; `tBlit` and its users (UpdateBlit, the
radar, the tachometer, the credits) read it as `u8`, MAT3's `char` (unsigned on the original compiler). It stays
`u8`: where the code read it signed (`Hidden`, `animOdometer`, `animBones`) it reads `(s8)blt->hide`, with a comment,
so the code is the same; the flag is 0 or 1. Six views had `texIndex` as `s16` and five `init` as `s8`; no code
reads either through them. The views named fields by place: `data` (animControls) is `animID`, `half18` and `State`
are `z`, `Texture` (animArrows) and `resource` (trackscr) are `blidx`, `Image` is `palette`, `state` and `AnimDTA`
over the function's slot are `animFunc`. The callbacks that take their blit as `void *` (animBones,
animControllerDim, animate_pause_dim, anim_show_on_high_score) keep that parameter with a typed local, as their
code was.

The colour unions were the same four bytes under other names (`b`/`w`, `channel`/`word`, `c.r`..`c.a`,
`red`..`alpha`): each read is `c[i]`, each word copy `rgba`. Three locals the menus pass to `ZOID_SetColor` needed
four-byte alignment (`PC_WORD_ALIGNED`, [SANITIZE.md](SANITIZE.md) 3.2); a `tRGBA` has it, and the macro is gone.
playerscr's two white locals, `u8[4]` arrays, are `tRGBA` initialized by their word, `{.rgba = 0xFFFFFFFF}` (every
byte 255, the same in any byte order), which compiles as the array's initializer did.

**Data and views.** The 18 views of B3's data are gone, each file reading the datum by its declaration:
`gFlashColor1`, `gFlashColor2` (TB_SetTextColor, through `Color4`), `modescr_button_color`, `carscr_solidColor`,
`carscr_normalColor`, `circuitscr_button_color`, `circuitscr_init_button_color`, `loadTrackScreen_backColor`,
`race_map_vertex_color`, `trackscr_init_button_color`, `trackscr_update_button_color` (through the files' colour
unions), `recordscr_viewPane` (`FiveParts *`), and the six of `flashColors_at_1`, which no code read any more (the
screens read `flashColors[1]`): frontend.h's declaration of the label went with them. The colour data are `tRGBA`:
`hud_playerColor`, `hudextra_playerColor`, `stunt_coin_color`, `weapon_icon_color`, `arrow_base_color`,
`paint_colors_rgb`, `controller_dim_color`, `pause_dim_color`, `carscr_marker_colors`, the circuit and track
screens' button colours, `modescr_button_color`, `gFontColors`. gen_data lays them out as before: a struct of four
bytes and a union with a byte member that covers it are both bytes ([DATA.md](DATA.md) 2); data.c and the symbol list
are the reference's but for type names, and data_layout.txt lists 42 data under "Unions copied as bytes" where it
listed 33 (the nine that were byte structs). The blits' data have their records too: every multiblit table is a
`tBlitDef`, every blit pointer a `tBlit *` and every pane pointer a `tBlitSet *` (but the three below).

**Leftover bytes, kept.** Two environment colours nothing sets are read past their colour, as on the N64: BlitXLU's
(`blitPrimColor.env`, the first word of the Controller Pak names' table after it on the N64, loaded as before) and the
IA blits' (`blit_ia_palette`'s last four bytes, the first two of `blit_tile_lines`, one storage). blit.h and maths.h
say so where they are declared; [NATIVE.md](NATIVE.md) 18.4.

**Left.** weapons.c and shots.c keep their `union RGBA` for one member, the car view's `IRclr` (the typedef says
`wave 4: tRGBA`): the car record is B1's (3.1), whose header gives it its type. The blit animators of hud.h and
frontend.h whose parameter is an `s32` or a `void *` (`animPlace`, `hud_updateFrame`, `animLapCount` ...) keep it:
as a `tBlit *` an unused parameter spills eight bytes instead of four at -O0, which changes the Debug build's code.
Three blit and pane pointers stay `intptr_t` (`highscore_blit`, `audio_gTitlePane`, `options_gTitlePane`): gen_data
lays an `intptr_t` out as the N64's long and a pointer as a pointer, so their type would change their field program.
`trophySheenColor` (`s32[2]`, MAT3's `tRGBA`) and `animPause_color` (`u32`) are colours the game passes to
`ZOID_SetColor` and as a blit's flat colour, read as bytes, but declared as words: the loader swaps them as words,
so on the PC their bytes come out in the other order. Declaring them `tRGBA` would change what the loader does, which
this wave doesn't: the lead's, with the retail cartridge (the circuit screen's trophy sheen, the pause screen's
dimming). The track map's `animTrackMaps` builds a vertex colour from a word constant (`col.rgba = 255`), whose bytes
are the host's order.

**objsame** against the reference (d224beab): every unit `same`, Debug and RelWithDebInfo, after each stage.

| Count (src/, nonmatching/) | Before | After |
|---|---:|---:|
| local declarations of the blit and colour records | 54 | 2 |
| tags declared in more than one file (their declarations) | 7 (50) | 1 (2: `RGBA`, above) |
| VIEW lines on B3's data | 18 | 0 |
| pad arrays in those declarations (and members named pad, reserved or unknown) | 1 (19) | 0 |
| colour types in include/game | 8 (`tRGBA`, `BlitColor`, `ArrowColor`, `HudColor`, `ButtonColor`, `ColorWord`, `Color8`, `PaneColors`'s bytes) | 1 (`tRGBA`; `PaneColors` and `BlitXluColors` are pairs of it) |

The layout tools: layout_check's reference for the blits is `tBlit` (it named NewMultiBlit.c's `Blit`, which that
file no longer had), so 41 more storages have a reference layout, with no new finding; offset_check, alias_check,
bounds_check, census and n64_audit as before.

## 4. Players, saves, menus and sound

### 4.1 Players and their input

(Agent C1: the players, their pads and input records.)

### 4.2 Profiles, records and ghosts

(Agent C2: profiles, records, ghosts, the pak's files.)

### 4.3 Menus, battles and circuits

(Agent C3: the menus' buttons and items, battles, circuits.)

### 4.4 Sound

(Agent C4: voices, channels, speech, sequences, sound resources.)

## 5. The OS records

(Agent D: the message queues, the pak file system, the scheduler's clients, zlib's state.)

## 6. The proof

(The lead's, after the merge.)
