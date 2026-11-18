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

(Agent C3: the menus' buttons and items, battles, circuits.)

### 4.4 Sound

(Agent C4: voices, channels, speech, sequences, sound resources.)

## 5. The OS records

(Agent D: the message queues, the pak file system, the scheduler's clients, zlib's state.)

## 6. The proof

(The lead's, after the merge.)
