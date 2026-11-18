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

(Agent C1: the players, their pads and input records.)

### 4.2 Profiles, records and ghosts

(Agent C2: profiles, records, ghosts, the pak's files.)

### 4.3 Menus, battles and circuits

| Record | Declared in | What it holds | Its other tags |
|---|---|---|---|
| `MenuButton` (MAT3 `tMenuButton`) | frontend.h | One 3D button of a menu screen: the model it shows (`type`), its scene object (`handle`), how far it has turned (`angle`), the matrix and position its object draws by, and its text's opacity (`alpha`). Every screen's `buttonInfo` table but the start screen's: options, audio, video, controls, mode, track, car, circuit, player and record | include/game's `ScreenButton` (the audio and video screens' tables) and `ModeScreenButton` (the mode screen's); the files' `ButtonInfo` (audio.c, videoscr.c, options_anim.c), `Button` (audio_initScreen.c, updateAudioScreen.c, op_initButtons.c, optionscr/buttons.c, videoscr_update/group.c, video_init.c), `UButton` (audio_uninitScreen.c), `Item` (controls_updateButtons.c), `Slot64` (modescr.c, modescr_init.c), `Slot` (modescr_animText.c), `tMenuButton` (trackscr.c) |
| `StartScreenButton` (MAT3 startscr.c's `buttonInfo`, `anon_buttonType_objIdx_3c`) | frontend.h | The start menu's buttons: a `MenuButton` without the alpha, 4 bytes shorter | `ButtonInfo` (start_screen/group.c) |
| `Battle` (MAT3 `tCarData.battle`, `anon_weapon_health_30`) | types.h, the race section | A car's state in a battle: its weapon and ammunition, health, defence, gun's aim, and its fade while invisible. The game car embeds it by value, so it is declared before the cars' header | the copies in weapons.c, shots.c and StartWeaponIcon.c |
| `Defense` (MAT3 `anon_active_invsTime_10`) | types.h, the race section | The battle record's defence: the invisibility and the shield the power-ups give, and their countdowns | the same three files |
| `tCircuit` (MAT3 `tCircuit`) | types.h, in the profile's file (`tSaveData.circuit`) | A profile's circuit in progress: its type, races driven, seed, difficulty, time, and each car's place in each race. A file format: its comments keep the file's offsets | include/game's `tCircuitRecord`; the copies in UpdateCircuit.c, SetCircuitCode.c, ContinueCircuit.c and circuitscr.c |
| `tCircuitTrack` (MAT3 `tCircuitTrack`) | types.h | One race of a circuit, `gCircuitTracks[]`: the track, its direction, mirror, fog and wind | include/game's `CircuitTrackSlot` |
| `TextBox` | text.h | A box of text on the screen: `gScreenRect`, where the record screen draws its views (`TB_DrawTextScrollBox`) | zlib's `config` (gScreenRect's declaration), recordscr_draw.c's `TextBox` |

The pause screen's three text animators (`animPauseText`, `animSureText`, `animTitleText`) took a `MenuItem`, a copy of
a blit's first fields; they take the blit (`struct Blit *`) as every blit animator does, so `MenuItem` is gone. The
fields have MAT3's names, but where the screens' files already agreed on a clearer one: `MenuButton`'s `type` and
`handle` (MAT3 `buttonType`, `objIdx`), which some 300 reads in two dozen files use. Its matrix and position stay two
fields, marked `/* wave 4: tUVMatrix */`: they are MAT3's `mat` (`uvs`, `pos`), and the files read them as `matrix`
and `position` in some 900 places, so swapping in the matrix record means renaming those reads too.

Fields the copies disagreed on, and how they were settled:

- **The button's last word.** The mode screen's table had a word at +0x3C (`ModeScreenButton.unknown3C`), the audio and
  video screens' four bytes (`ScreenButton.pad3C`), the others the alpha byte. It is the alpha, as MAT3 has it; no code
  reads the mode, audio or video screens' alpha. So the data loader copies the mode screen's 13 alpha words as bytes
  now, where it swapped them as words (data.c's only change but type names); the audio and video tables' program is the
  same.
- **The start screen's matrix** was `f32 uvs[4][3]`, the position its fourth row: `matrix` and `position` now (`uvs[3]`
  is `position`, `uvs[0]` and `uvs` are `matrix`). Its first set-up still copies the identity matrix and the origin in
  one 48-byte copy from `unitmatrix`.
- **The battle record**: a pad before the defence (alignment, gone), and the defence's fifth word, which MAT3 doesn't
  have, `unknown10`: the shield's spin (`shldAngle`; UpdateDefense pitches the shield by it and turns it each frame).
- **The circuit's ranks**: `u8 ranks[54]` in two copies, `[6][9]` in the others and MAT3. `[6][9]`: UpdateCircuit
  indexes `ranks[car][bit >> 3]`. UpdateCircuit and SetCircuitCode reach the circuit as the save data's `circuit`,
  not at its byte 1780.
- **The record screen's box** `gScreenRect` was declared with zlib's `config` (four u16s). MAT3's has six s16s, the
  box and its centre, and the image has room for them; the data loader held four, the four the game reads, and still
  does: `TextBox` is those four (the field program is the same four 16-bit words).

**The VIEW lines** (16): `audio_buttonInfo` (4), `video_buttonInfo` (3), `options_buttonInfo` (3),
`controls_buttonInfo`, `startscr_buttonInfo`, `trackscr_buttonInfo`, `modescr_buttonInfo` (2) and `gScreenRect`. Each
file reads the datum by include/game's declaration. `audio_buttonInfo` and `video_buttonInfo` are declared in
frontend.h with the other tables (audio.h and system.h had them). Four files had a view with the table's extent, which
include/game leaves open (`EXTENT`): at -O2 GCC schedules their stores into the table against the loads of the
screen's other globals by it. So the controls, mode, start and track screens' own files complete the table's type
(`extern MenuButton trackscr_buttonInfo[TRACK_BUTTONS];` ...), and they compile as before; without those four lines
each unit differs only in its instructions' order.

**Not same** (objsame against the reference, Debug and RelWithDebInfo): one unit, UpdateCircuit.c. It indexes the
ranks as MAT3 does, `ranks[i][bit >> 3]`, where its copy had `ranks[i * 9 + (bit >> 3)]` over a flat `u8[54]`: the
same byte, whose address GCC now forms as the row (`i * 9`, 64-bit) plus the column instead of one 32-bit index
sign-extended (i under 6, bit under 72); at -O2 it also counts the row by `i` rather than in 9s and loads `gPlayers`'
address again after a call. The same loads and stores and the one float addition, in the same order. Every other unit
is `same`.

**Numbers** (src/ and nonmatching/): local declarations of these records 30 to 0 (their pad arrays 24 to 0), the
VIEW lines of these data 16 to 0; tags declared in more than one file 92 to 84 (`ButtonInfo`, `MenuItem`, `Slot64`,
`Battle`, `Defense`, `tCircuit`; `Item` and `Slot` are now one file's each, below).

**Left, and whose:**

- `Owner` (pakscr_draw.c, modescr.c, playerscr.c) is no battle record: a pak file record (MAT3 `tPS_File`), a profile
  and a profile handle. They are the save records' (4.2), as are circuitscr.c's `Item` and ghost_files.c's `Slot` (pak
  file records under those tags), and the views of a profile's save data that reach the circuit's fields: SetOptions'
  `Stats` (`option_a`, `option_b`: the circuit's `death` and `difficulty`), trackscr.c's `CIRCUIT_DEATH_MODE_BYTE`
  (the save data's byte 1785) and CheckAndRepairPlayer's checksum of the circuit (bytes 1780 and 1784). Each becomes
  `->circuit.death` and so on once the profile's views go.
- The car record's copies keep their own battle fields, flattened (`InitBattlePlayer.c`, `powerups.c`,
  `battle_damage/group.c`, `animate_arrow/group.c`, `update_model/group.c`, `rocket.c`, the car record's PC
  declaration `struct pc_car`): the game car's (3.1), which embeds `Battle`.

### 4.4 Sound

(Agent C4: voices, channels, speech, sequences, sound resources.)

## 5. The OS records

(Agent D: the message queues, the pak file system, the scheduler's clients, zlib's state.)

## 6. The proof

(The lead's, after the merge.)
