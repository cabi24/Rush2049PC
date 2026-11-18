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

(Agent B3: the blits and the one colour type.)

## 4. Players, saves, menus and sound

### 4.1 Players and their input

**The input record, `tPadState`** ([input.h](../../include/game/input.h), MAT3's name and fields): one per player,
`gPlayers[4]`, declared beside it (InitOptions.c defines it). The car the player drives (`slot`), the controller
port (`controller`, `CONTROLLER_NONE` for none yet), that controller's buttons this frame (`edges`, `levels`,
`autoEdges`) and stick (`analog`), the player's control layout (`controls[10]`: each control's buttons,
`gControlTable[gNewControls[controller][c]]`, or a `CONTROL_VALUE_` for a stick or a trigger), the car's crashes this
race (`numCrashes`), the transmission chosen on the car screen (`trans`), the sound listener's number in
`gListeners` (`listener`) and the profile's handle (`save`, a `tProfile **`). A human car's `input` (pc_cars) points
here. 80 bytes: `save`, the one pointer, is last, so every other field is at its N64 offset. With it in input.h:
`ControllerState` (gControllerData, MAT3's tPadInfo) and the control indices, `CONTROL_VIEW` (the camera) and
`CONTROL_HORN` named from their readers (CheckCameraView, UpdateCarSounds).

**Its other tags.** Every file that read gPlayers declared it: 58 with a struct of their own under 15 names, each
with the fields it used and byte arrays for the rest (`Player` 15 files, `PlayerPad` 13, `Player76` 9,
`InputRecord` 5, `PlayerRec` 3, `PadState` 3, `PlayerRow` 2, and `Pad`, `PadRecord`, `PadTrans`, `PlayerStatus`,
`InRec`, `InputRec`, `CarRec`, `Rec76`), 7 through types.h's `tPadState`, and fake_init.c and CheckCPs.c read its
first byte as an `s8`/`u8 gPlayers`. Two of the copies also served a car's input pointer (init_cars.c's `PadTrans`,
UpdateGame.c's `Player76`), and four files read it only so: `Pad` (shots.c), `Pose` (AnimateWing.c), `Q` (the steady
and rear cameras). The fields' names became MAT3's: `index`, `car`, `node`,
`vehicle`, `model`, `b0` are `slot`; `selector`, `port`, `control`, `pak`, `resource_index`, `unk1` are `controller`
(GhostStart's and RecordGhost's `slot`, at +1, too); `pressed` is `edges`, `repeated` `autoEdges`; `hornControl`
is `controls[CONTROL_HORN]`, CheckCameraView's `mask` and `edges` are `edges` and `controls[CONTROL_VIEW]`,
AnimateWing's `angle` is `analog[1]`; `profile`, `owner`, `handle`, `ref`, `name`, `h`, `player` are `save`.
Not this record, though named so: ovl_b's `Player` (battle_damage, missile_lock, powerups, proximity_damage, and
`BattleDamage`'s parameters) is the game car (3.1); PrintScores.c's `Player`, options_set's `Player` and
`PlayerState`, and race.h's `struct PlayerRec` (`gGhostPlayers`) are the profile (4.2).

**Where the copies disagreed**, and how it is settled (each file compiles to the same code, but where the proof
below says otherwise):

| Field | Copies | Now |
|---|---|---|
| `controller` | InitOptions.c's `s8` (written only) | `u8`, as every reader |
| `edges` | `s32` in updatePauseScreen, ControlUnlocked, UpdateCarSounds (bit tests, the menu sounds' argument) | `u32` |
| `numCrashes` | PrePlay's `s16` (cleared only) | `u16` |
| `trans` | types.h's `s8`; init_cars.c kept a `u8` view because its copy into the model compiled otherwise; carscr.c's `(s32)trans < 0` is a `u8`'s | `u8`, MAT3's |
| `controls` | 11 or 12 words in shots.c and model_controls_input (running over the fields after) | `[10]`, the N64's (MAT3 has 11) |
| `save` | `void *`, `Handle *`, `Owner *`, `MenuOwner *`, `Profile **`, `PlayerProfile **`, `PSFile **`, `char **`, `intptr_t *` | `tProfile **` |

A file that reads the profile through its own view of it keeps the view and takes the handle by assignment;
comparing such a handle with the player's casts it (playerscr's `Owner *`, playerscr_draw's `Profile **`), and
GhostStart gives a ghost player `(tProfile **)&gGhostPlayers[i]` (race.h's `struct PlayerRec *`): the profile is 4.2's,
and these casts go when it has one type. Where tProfile reads the same, the profile views reached only through the
record went with it (CheckCameraView, ContinueCircuit, SetCircuitCode, UpdateCircuit, the controls screen and its
text, the results table, the camera files, state.c, pausescr_cleanup, modescr, circuitscr, trackscr,
carscr_animText), and so did the name reads at the N64's offset (`*gPlayers[i].name + PROFILE_OFS(20)`:
animRumbleText, animTitleText, animResults): `(*gPlayers[i].save)->name`.

**gPlayers_save** was a name at gPlayers + 0x48 (`PlayerTail[3]`: the handle, then the next record's 72 bytes),
there because no shared declaration named the field (NATIVE.md 18.1). Its six readers use `gPlayers[k].save`;
port/main.c's pak swap reads the handles through a view of gPlayers, and the tests that stood in for the name stand in
for gPlayers.

**gNewLevels** (controlsys/group.c's `VIEW(volatile s32[4])`, include/game's one `s32`): input.h declares MAT3's
`volatile u32 gNewLevels[4]`, its only reader's type. The data loader gives the symbol 16 bytes (`scissorul` follows;
its object was 16 bytes already, the gap to it).

**`gNewControls[5]`** (NATIVE.md 18.4): no path reads it. SetControls takes `gNewControls[controller]` for each of
the gNumPlayers players; only the car screen and the pause menu's controls page call it to read the layouts
(InitOptions resets them to the default, and nothing enters GS_CONTROLS in the front end). Both come after the
player screen, which gives each of the gNumPlayers players a controller (`playerscr_initScreen`, or its skip:
`FindPlayerNum(i, 1)`), and only the start screen raises gNumPlayers, before the player screen. The title screen's
`CONTROLLER_NONE` for players 2-4 is on the way to the start screen. controls_validControl and animControls run only
for a player with a controller (the controls screen's loop skips the others; animControls hides its blit first).
SetControls says so.

**The proof** (against `~/rush2049pc/w4-ref`, d224beab's builds):

1. objsame. The record (stage 1): Debug every unit `same`; RelWithDebInfo every unit but two, where GCC allocates
   and schedules otherwise: `fake_init.c` (the slot written as a `u8` field where it was a `char` object, so the
   camera's copy of the same byte shares its register: 10 bytes shorter) and `animResults.c` (the profile's name as
   `&profile->name`, a field address, where it was `char *` + 0x20: the same address in another instruction
   order). Then, in both builds: the six readers of `gPlayers_save` (NewCircuit, GetCircuitCode,
   highscore_InitScreen, FindGhosts, pause_TitlePane, animBattlePoints) load `gPlayers` + 0x48 + 80 × k, the same
   address, through a displacement where the label had none (4 bytes longer); `controlsys/group.c` reads
   gNewLevels as the `u32` it holds where the view had `s32` (Debug: one register move less in UpdateControls;
   RelWithDebInfo: ControlIRQ's registers allocated otherwise; the words are only copied, XORed, ANDed and stored);
   GhostStart clears a ghost player's record with `sizeof gPlayers[i]` (80) where it cleared 76: the other four
   bytes are the top of `save`, written on the next line. UpdateModel's rumble setting through
   `(*player->save)->rumble` (it was `PROFILE_OFS(0x2B)`) is `same`.
2. gen_data: gPlayers at the same N64 and host places, the same size and the same pointer; its runs carry the
   fields' kinds where they were bytes (BSS: the loader fills nothing). gNewLevels `volatile u32[4]`, 16 bytes: its
   object was 16 bytes already. No `gPlayers_save` (0 names inside own storage; `n64_audit` `symbols.inside_own` 4 to
   3 in its baseline). The bounds check's four reviewed keys for `gPlayers_save` and controlsys' view of gNewLevels
   went with them.
3. `gPlayers[EXTENT(4)]`: game files see it open-ended, as before; with `[4]` GCC's RelWithDebInfo code changed in
   five more units (the object's size known).
4. view_check `--sym gPlayers`: 68 views, all `tPadState`, none disagree (UpdateCarSounds' 80-byte view differed
   before); layout_check, offset_check `--strict`, alias_check, bounds_check, census and n64_audit as before (layout
   and offset reports the same but for shots.c's line numbers).

**Numbers** (src/ and nonmatching/, counted the same way at 7d065d8e and after): 62 local declarations of the record
went (58 files' copies of gPlayers; 7 more files used types.h's `tPadState` as their view; 4 views through a car's
input pointer) and 29 views of the profile, the pak file and the save data that only the record reached: 91 of the
913 struct and union definitions (822 left). Tags defined in more than one file 92 to 80: `Player76`, `PlayerPad`,
`InputRecord`, `PlayerRec`, `PadState`, `PlayerRow`, `Q`, `HN` and four of the profile's (`Handle`, `Item`,
`Resource`, `PlayerProfile`); `Player` is left in 6 files, the game car's 4 and two profile views. VIEW lines 1
(gNewLevels) to 0. Byte arrays (`pad`, `unk`, `opaque` ... members) in the files changed 236 to 148.

**Left, for the others' records.** The game car's copies that name the input pointer (shots.c's, init_cars.c's, the
steady and rear cameras', AnimateWing.c's) have it as `tPadState *` now; pc_cars' `input` is still `void *`
(`PC_CAR_INPUT_AS(tPadState, i)`), 3.1's. The profile's casts above and options_set's views of the profile and its
save data (`Player`, `PlayerState`, `Profile`, `Stats`) are 4.2's, and so are race.h's `gGhostPlayers` (`struct
PlayerRec *`) and `gRealGhostPlayers` (`GhostPlayer`, the profile's layout): the ghosts' profiles. RecordStunt.c
reads `gStuntPlayer` (types.h's `tStuntPlayer`, declared in physics.h) through a view of its own whose
`maxStuntPoints` is unsigned where the header's is signed, and compares it: a stunt record, whose owner the briefs
don't name.

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
