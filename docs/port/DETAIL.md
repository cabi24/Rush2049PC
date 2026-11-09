# Draw distance and detail (video.draw_distance, video.detail, video.fog, video.texture_lod)

The N64 game draws a few thousand feet of the world, and less of what is on it: a far plane at 2,000 ft, a table of
the world's parts each place on the track lets through, a distance past which each model is left out, simpler
levels further away, a fog in the fog options, and cuts for split screen. This page maps everything in the game C that
limits what is drawn (section 1), the three settings that move those limits on a PC (section 2), the renderer's part
(section 3), what they cost (section 4), what they look like on the retail cartridge (section 5), and how they are
tested (sections 6, 7). Written 2026-11-03 on main 0d7d016.

| Piece | Where |
|---|---|
| The settings the game reads, the scene's counts | `port/config/detail.c`; accessors in `port/include/pc_game_types.h` ("Draw distance and detail"); the levels in `port/include/pc_detail.h` |
| The renderer's depth range and fog | `port/gfx/gfx_detail.c` |
| The settings | `port/config/config.c` (`[video]`), `port/input/menu.c` (F1's Video page), `port/main.c` |
| The game C | `nonmatching/game/WorldCullObjects.c`, `src/blob/groups/zoid_draw/group.c`, `src/blob/build_node_matrix.c`, `build_node_matrix_rows4.c`, `build_far_node_matrix.c`, `src/blob/groups/waves/waves.c`, `src/blob/groups/targets/group.c` |
| Tests | `gfx.detail`, `gfx.all.draw-max`, `gfx.interp.all.draw-max`, `frontend.detail`, the synthetic paths `detail*` (`tools/port/detail_check.py`) |
| Retail runs, logs, frames | `build/scratch/retail/` (local only, never committed) |

Names: a node is a scene node (`ObjInstanceList[]`, MAT3's ObjInstanceList), a part is one of the world's top-level nodes
(the track's pieces), a cell is a part's box as the visibility tables see it, a view is one window's scene walk
(`zoid_draw_window`, once a frame per player). Distances are in feet; the scene's matrices and vertices are in
sixteenths of a foot. `gNumPlayers` is the number of human players (`gNumPlayers`), not the track:
`WorldFogDensity.c`'s header calls it the track.

## 1. What limits what is drawn

### 1.1 The world's visibility tables

`WorldCullObjects` (MAT3's WorldCullObjects, `nonmatching/game/WorldCullObjects.c:223-293`), called at the start
of each view (`zoid_draw/group.c:1271`):

- WorldDistance (`:61-90`) walks the world's top-level list (`WorldBase`, the world WAD's nodes, from
  `src/blob/NewWorld.c:60-64`) and finds the part whose box holds the eye, the nearest by its place if
  several do (the box has no top: an eye above it is still in it).
- That part's row of the track's table (`track1Parts` for track 0 ... `track19Parts` for track 18, `u32[4]` a row: one
  bit a part, up to 128) says which parts are drawn; `trackParts[track]` is the track's parts, `showAllParts` the row
  for an eye in no box. Each part's node gets the hidden flag (0x80000000) or loses it (`:283-287`), so its whole
  subtree is left out (`Traverse`, `zoid_draw/group.c:1036-1039`). Level objects parented to a cell
  (`FindWorldGroup.c`) go with it.
- `gInvisibleTrack` (the invisible track cheat, `cheatscr.c:126`) hides every part; the call's `show` argument and the
  `ptr_no_clip` path are dead (always 0).
- The tables are ROM data. On the retail cartridge a row lets through about a fifth of a race track's parts on
  average, at most about two fifths (the eye's place only: they don't depend on where it looks).

### 1.2 The scene walk

`Traverse` (`zoid_draw/group.c:1022-1192`) walks the node tree for each view, and `zoid_draw_object`
(`:871-1020`) draws a node:

| Test | Where | What it leaves out |
|---|---|---|
| the hidden flag 0x80000000 | `:1036` | the node and its subtree (the visibility tables, the cars' AnimateFrame, `MBOX_HideObject`) |
| the deferred passes 0x40000, 0x800000 | `:1044-1055` | nothing: such nodes are drawn after the poly list (`:1515-1532`) |
| the per-view mask `0x100 << view` (`zoid_node_visible.c`) | `:1056` | a node hidden in this view (the own car in the cockpit, the hood outside the hood view, the sky dome of another window), unless a child is shown |
| the matrices' range | `:1080-1094` | a node 2,048 ft or more from the eye on any axis (1.5) |
| the culling box, G_CULLDL | `:1111-1146` | a node whose box (8 vertices, `cull_vertices`; or the two unit boxes for flags 0x10, 0x20) is wholly off one side of the screen; done by the renderer against the widened frustum |
| the fog's radius | `:1127-1136` | a part with a box, unless flag 0x400000, further than `gViewableDistance` plus its box's reach in x and z; only when `gViewableDistance < 2000` (a fog option). The node's own geometry only: its children are still walked |
| a model's last level of detail | `:919` | a node further than its last level's distance (1.3) |

### 1.3 Levels of detail

A model (OBHD, `tLod`: docs/port/ASSETS.md) has up to four levels, each with a display list and a distance, `lodRad`.
`zoid_draw_object` (`:887-928`):

- The node's distance `node_eye_distance` is measured from the eye when its last level has a distance (or flag 8: the car's
  frame); children of a flag-8 node reuse their parent's (the tires, hood and sheen take the frame's).
- Past the last level's distance times the node's scale it isn't drawn (`:919`); a last level of 0 has no limit.
  Otherwise the nearest level whose distance it is within (`:925-928`).
- Flag 4 forces level `flags & 3` (`:889-895`, `:920-924`): CreateCar (`CreateCar.c:124-139`) forces the tires
  to level 1 with two to four players, and the menus' cars to level 0. Nothing else in the code forces a level; the
  world's and the level objects' flags come from the ROM.
- Flag 0x200000 fades the node out from 24 to 80 ft (`:980-986`): only the car's sheen (`CreateCar.c:130`).
- On the retail cartridge (the levels are ROM data; seen on track 0): nearly every part of the world has one level
  with no limit; a car's body has two levels and a car is left out a few hundred feet away, its wheels and sheen
  within a hundred feet, so a car further out is drawn without its wheels; a few trackside objects have limits of
  their own.
- **The display lists' own markers** (load time). SetupWaveModel (`waves.c:178-187`) gives relocate_model_dl
  (`relocate_model_dl.c:107-129`) a mask: 2 with one player, 4 with two, none with three or four; 0x10 on a reversed
  track (`gBackwards`), else 8. A G_SPNOOP marker of type 1 whose bit isn't in the mask becomes a branch past its
  section, so sections tagged one-player-only or two-player-only are cut out of every model at load. Which sections
  carry which tag is ROM data.
- G_BRANCH_Z (the microcode's own level of detail) is not in the race lists seen (a traced race frame of track 0 has
  none).

### 1.4 The fog's radius and the far plane per fog option

`WorldFogDensity` (`src/blob/WorldFogDensity.c:78-127`), at PrePlay (`PrePlay.c:380`) and every frame of the
countdown and the race (`update_game/UpdateGame.c:331-333`), when the fog option `gFogIntensity` is 1-3:

- the fog's start (`:90`, gSPFogPosition's min, of 1,000: depth, not feet) from `fogMin[players][option]`, its
  end 1,000 (the far plane); the far plane (`:92`) from `fogDistance[players][option]` (feet); the radius `gViewableDistance`
  (`:93`) from `fog_viewable_distance[players][option]`. The three are ROM tables indexed by the number of human players, so
  split screen fogs nearer; the reads with four players run into the next symbol, so the real tables probably start
  a row after each symbol;
- a fog region of the track (`trackFog[track]`, ROM, `{x, z, radius, start, far, radius}`) that holds the eye blends
  the three toward its own values by the distance to its centre (`:94-125`).
- The option comes from the track select's option 14 (`gOptions[29]`, 0-3; `trackscr/trackscr.c`
  case 14), the circuit's per-race byte (`options_set/group.c:168`, rolled by `ContinueCircuit.c:139`); it
  is 0 in the attract demo and in stunt, obstacle and battle modes (`options_set/group.c:104, 185, 207, 224`).

### 1.5 The matrices' range and the far scenery

- A node's matrix (`build_node_matrix`, `build_node_matrix_rows4`: `src/blob/build_node_matrix.c:89-143`, `build_node_matrix_rows4.c:99-153`)
  holds its place against the eye in its s15.16 translation, in sixteenths of a foot: a node 2,048 ft or more away on
  any axis is left out (`return 0`). A child placed against its parent (the walk's `count`) has its own small offset.
- Far scenery, flag 0x400000 (`build_far_node_matrix.c:105-158`): a node further than 1,600 ft is pulled in to 1,600 ft and
  shrunk by as much, the same picture inside the 2,000-ft far plane; it is never left out and never has the fog's
  radius. On the retail tracks about a tenth of the parts are far scenery.
- The poly list (`PolyInstanceList`, at most 200: `ZOID_NewPoly.c:17`) holds the cars' shadows (`StartEnvironment.c:103`),
  the multiplayer markers, the skids, the countdown, scorch decals and weapon trails; zoid_draw_window skips a poly with
  a vertex 2,048 ft or more from the eye on any axis (`zoid_draw/group.c:1411`): its vertices are s16 in
  sixteenths of a foot.

### 1.6 The projection

zoid_draw_window (`zoid_draw/group.c:1260-1533`) loads, for each view, `guPerspective(fov, aspect, near x 16, far x
16)` from the window record `ZoidWinInfo[view]` (or guOrtho for an orthographic window) and multiplies the camera's
lookat into the projection. ZOID_InitializeWindow (`ZOID_InitializeWindow`, `ZOID_InitializeWindow/ZOID_InitializeWindow.c:24-44`) sets the near
plane to 2.5 ft and the far plane to 2,000 ft (the record's fields at +0x38 and +0x3C, which that file calls `zoom`
and `fog`), the fog to 1000/999 and black, and `gViewableDistance` to 2,000; WorldFogDensity changes the far plane with a fog
option (1.4). Nothing else writes them.

The NoN microcode clips no near or far plane: geometry past the far plane is drawn at the far plane's depth, in the
order it is drawn (the renderer clamps the depth to [0, 1] per vertex, GFX.md "Clipping"), and its fog factor is
gSPFogPosition's past the far plane.

### 1.7 The fog and its colour

- ZOID_SetFog (`ZOID_SetFog`, `ZOID_SetFog/ZOID_SetFog.c:27-38`) writes the fog's start and end and colour into the window
  record and the frame's clear colour (`ZOID_SetBGColor`, used at `StartGraphics.c:184`). PrePlay
  (`PrePlay.c:379`) gives each window `ZOID_SetFog(1000, 999, gFogColors[gFogColor], 255)`: start past
  end, no fog (gSPFogPosition's s16 words wrap to fm 3072, fo -3072, 0 inside the far plane), in the track's colour,
  `gFogColors` (`gFogColors`, ROM). `gFogColor` is the track (`PrePlay.c:362`) unless the cheat screen
  set another colour (19-26, `cheatscr.c:185-199`). zoid_draw_window emits the colour and gSPFogPosition at each view
  (`zoid_draw/group.c:1284-1285`).
- With a fog option the fog runs from `fogMin`'s start to the far plane (1.4), in the same colour; the screen is
  cleared to it.
- The sky: SkyInit (`SkyInit.c:3806-3827`) makes a sky dome a window (model `sky_object_name`, ROM), shown in its
  view only and kept at the camera's x and z (`SkyUpdate.c:3812-3822`), except with a fog option or on
  tracks 7-13, 17 and 18.

### 1.8 Split screen, the Expansion Pak, and what isn't distance

Load-time and spawn cuts, not changed by the settings unless the table says so:

| What | Where | Limit | Here |
|---|---|---|---|
| the models' LOD markers | `waves.c:178-187` | one- and two-player sections cut with 3-4 players, one-player ones with 2 | detail high: the one-player mask (2.5) |
| the tires | `CreateCar.c:124, 135` | forced to level 1 with 2-4 players | detail high: level 0 |
| level objects with metadata 0x100 | `targets/group.c:490` | no scene node with 2-4 players | detail high: made, as with one |
| the fog tables | `WorldFogDensity.c:90-93` | nearer fog with more players | as the N64 |
| tire smoke, wing trails | `StartSmoke.c:205-232` | none below 10 mph, more puffs at speed, none with 3-4 players; a frame-time gate (`UpdateVisuals.c:63-70`) | as the N64 |
| skids | `skid_marks/group.c:310`, `:185-221` | none with 3-4 players; with all 100 in use the one farthest from camera 0 is taken | as the N64 |
| explosions' glow, smoke, ring; scorch decals | `blast.c:215, 236, 290, 306`; `AnimateNewBlast.c:180` | fewer than 4 players | as the N64 |
| the sheen, the hood | `rocket.c:400, 402, 419` | the sheen hidden in battle and stunt with 3-4 windows and with a fog option; the hood only in the hood view with one window | as the N64 (hide masks) |
| the Expansion Pak (`expansion_pak_present`, `memsys/group.c:308`) | `targets/group.c:394`; `loadtrack.c:176-200`; `blast.c` | objects without metadata 0x20, extra waves, explosions' glow and smoke | `game.expansion_pak` (on by default) |

Not distance at all, and kept: the per-view mask (the own car in the cockpit, the hood), the hidden flag the cars'
animation sets (the ghost and invisible car cheats, battle's dead cars), coins hidden once collected, weapon icons
while a player holds the weapon, scenes and `_FW`/`_BW` nodes by the track's direction (`targets/group.c:401-407,
551`), the battle arrow by the horizontal field of view (GFX.md "Widescreen"). No show or hide by checkpoint or lap
was found.

## 2. The settings

`[video]` in rush2049pc.ini, `--set video.KEY=VALUE`, and F1's Video page ("Draw distance", "Detail", "Fog", "Far textures"), with
a line of help each ([FRONTEND.md](FRONTEND.md)). Each applies from the game's next frame; detail's load-time parts
(2.5) from the next track. `draw_distance` and `fog` are also `RUSH_GFX_DRAW_DISTANCE` and `RUSH_GFX_FOG`, the
renderer's own (a layer of the settings, as `RUSH_GFX_HUD` is). The defaults are `original`, the N64's game exactly
(section 7). The factors are `PC_DRAW_FAR_FACTOR` (2) and `PC_DRAW_MAX_DEPTH` (32), `port/include/pc_detail.h`.

| Setting | Values | What it changes |
|---|---|---|
| `video.draw_distance` | `original`, `far`, `max` | far: every distance limit of 1.1-1.6 twice as far, and the depth range's far plane twice the game's; max: none of them, every part of the world and every model at any distance, the far plane 32 times the game's |
| `video.detail` | `original`, `high` | high: each model's first (most detailed) level wherever it is drawn, the tires at their first level in split screen, and split screen's models and level objects as one player has them |
| `video.fog` | `original`, `far`, `off` | far: the fog's start and end twice as far (the fog options and the fog regions, as draw_distance far's limits); off: no fog, and with a fog option the sky dome the game leaves out in one (from the next track) |
| `video.texture_lod` | `original`, `sharp`, `anisotropic` | which copy of a texture's mip chain a pixel shows (2.7): sharp, the one the window's own pixels need, not the N64's 320 x 240; anisotropic, that and four samples along a surface seen edge on (the renderer's alone: the game's C is untouched) |

### 2.1 The visibility tables

`nonmatching/game/WorldCullObjects.c:92-221`, the PC's WorldCullObjects: original the N64's (the row of the eye's
cell). Far ORs into it the rows of every cell within `PC_VIS_FAR` = 2,000 ft of the eye (the distance to the cell's
box, without its top, as WorldDistance tests it; `VisReach`), for cells with a box and a row of their own table (each
table runs to the next one's address: the battle arenas' have one row, `VisRows`): what the cells the eye reaches in
2,000 ft let through, for a far plane of 4,000 ft where the rows were made for 2,000. Max lets every part through (as
`show` would); the invisible track cheat still hides them. What that adds on the retail tracks: section 4.

### 2.2 Levels of detail and the fog's radius

`zoid_draw/group.c`, through macros that give the N64's tokens on the N64 (`:849-870`):

- a model's last level's distance, `PC_LOD_REACH` (`:919`): x 2 at far, none at max (`pc_draw_reach`,
  `port/config/detail.c:38-42`); not for a node that fades out by then (flag 0x200000, the sheen), which would spend a
  frame lookat and its list on an invisible node;
- the fog's radius `gViewableDistance` (`:1134`): x 2, none; whether a fog option is on (`gViewableDistance < 2000`, `:1127`) is
  still read from the N64's value;
- detail high: the level chosen as for distance -1 (`PC_LOD_DIST`, `:926`), the first; and a level forced by the
  node's flags, the second time the walk reads it (`PC_LOD_FORCED`, `:921`), the first: the tires in split screen. The
  distance the walk keeps for the node's children and for the cull (`:889-895`) stays the N64's, so the sheen's fade
  is the same.

### 2.3 The matrices' range and the far scenery

`build_node_matrix.c:24-87`, `build_node_matrix_rows4.c:34-97`, `build_far_node_matrix.c:34-103`: PC versions. A node 2,048 ft or more from
the eye is left out at original, kept to 4,096 ft at far, kept at any distance at max: its whole matrix (the rotation,
the place and the w the node gives the vertex) is divided by a power of two h (`pc_draw_range_scale`,
`port/config/detail.c:60-79`), so each vertex comes out h times smaller in x, y, z and w, the same point on the screen
with the same z/w, so the same depth and fog in the renderer, and children multiplied onto it the same. Inside the
N64's range h is 1 and every word is the N64's. A NaN place is never left out, as on the N64. Far scenery
(build_far_node_matrix) is pulled in to 3,200 ft at far and not at all at max (its true place, divided by h past 2,048 ft).

The renderer is unaffected by h: lights and texture generation normalize the modelview's directions, the clipping and
the culling boxes are homogeneous, and the fog and depth read z/w. The high frame rate's pairing blends modelviews
elementwise; a node whose h changes between two frames (crossing 2,048 x 2^k ft) is blended with a weight toward the
nearer h for that one frame between, a slip of a fraction of a frame in its place, far away.

The poly list's 2,048-ft guard (shadows, skids, markers, decals: 1.5) is kept: its vertices are s16 against the eye,
and such polys are smaller than a pixel there.

### 2.4 The far plane and the fog

The renderer's part (section 3): the depth's far plane x 2 at far, x 32 at max; the game's projection, and with it
the fog and everything the game sees of z/w, unchanged. And fog off keeps the sky dome with a fog option: SkyInit
(`SkyInit.c:3809`, `PC_SKY_FOG`) makes none in a fog, whose colour is then the sky; with the fog off that is
a flat colour behind the world (5.3). The game reads `video.fog` for that alone (`pc_fog`), from the next track.

### 2.5 Detail high's load-time parts

- `waves.c:178` (SetupWaveModel): the LOD markers' mask as one player's (`PC_DETAIL_PLAYERS`, detail.c:49-52): the
  one-player sections of every model in split screen, without the two-player ones (one player's mask has no
  two-player bit). From the next track load, as the models are relocated once.
- `targets/group.c:490`: a level object with metadata 0x100 gets its scene node with two to four players.
- Not changed: the smoke, skid, explosion and trail cuts of 1.8 (spawn decisions on shared pools: the poly list's
  200, the effect pools); the fog tables by player count (fog far moves those).

### 2.6 The scene's counts

The scene walk counts, per view (`PC_SCENE`, `pc_scene_count`, `port/config/detail.c:82-136`): nodes drawn, drawn at a
level past their first, the world's parts, those the visibility tables let through, nodes hidden, hidden in that
view, out of the matrices' range, past the fog's radius, past their last level of detail. At a run's end one line
gives each one's mean and most over the views of a world (the race's, not the menus' models), and the renderer's
triangles a graphics task (`pc_gfx_scene_stats`, `port/gfx/gfx_detail.c`):

    rush2049pc: scene: draw_distance max, detail original; 137 views of a world (of 829 drawn), a view's mean and
    most: nodes drawn 108.1, 155; at a level of detail past their first 1.0, 1; world parts 6.0, 6; let through by
    the visibility tables 6.0, 6; hidden 2.0, 2; hidden in that view 67.3, 68; out of the matrices' range 0.0, 0;
    past the fog's radius 0.0, 0; past their last level of detail 0.0, 0; 829 graphics tasks, triangles 73.6 a task,
    at most 910

(the synthetic `detail_max` path).

It is printed at every setting, after the pools line; it is not part of a trace (`synth_compare.py` reads the state
and status lines). `tools/port/detail_check.py` reads it.

### 2.7 Far textures (video.texture_lod)

2026-11-04, from the user's play: at draw distance far and max the road "is still loading in lower resolution in the
mid and longer distance". The world's textures are mip chains (G_TL_LOD: the first level and copies of half the size,
[GFX.md](GFX.md) "Texture LOD"), and the renderer picks a pixel's level as the RDP does, from the step of s and t to
the next pixel, counted in N64 pixels (`uScale`, the internal pixels per N64 pixel, times the step across an internal
pixel). At 1920 x 1080 an N64 pixel is about 4.5 of the window's, so the road a few car lengths ahead already showed
the level two smaller (a quarter of the texels across), as on a 320 x 240 screen.

- **`sharp`**: the step is counted in the window's own pixels (`uLodScale` 1, `port/gfx/gfx_combine.c`), so a pixel
  shows the level its own size needs, log2(k) levels finer at an internal scale k (2.2 at 1080p). A texture is no
  sharper than its first level (the upscaled textures or a pack give it more texels: [TEXTURES.md](TEXTURES.md)).
- **`anisotropic`**: sharp, and for a surface seen edge on (the road ahead: a pixel's footprint long down the road and
  narrow across it), the step is divided by the footprint's ratio of its longer axis to its shorter, at most
  `PC_TEXLOD_TAPS` (4), and the pixel averages that many samples along the longer axis, each from the two levels and
  their fraction as before. The far road keeps its stripes and grain instead of their average. Four taps are eight
  texture reads a pixel of the world's mip-chained triangles, against two.
- **`original`**, the default: `uLodScale` is `uScale`, the expression the same, the N64's levels pixel for pixel (the
  synthetic paths' screenshots with `--shots`: the same, 33 of 33, 2026-11-04).
- From the next triangle (F1 > Video > "Far textures", `RUSH_GFX_TEXTURE_LOD`); it is the renderer's alone and
  changes nothing the game computes.
- Tests: `gfx.lod.sharp.*` and `gfx.lod.anisotropic.*` (GLES and desktop GL at 2, 3.2 and 6.75): every internal pixel
  of the LOD case's quads against the levels and fraction its own pixels give (anisotropic: those levels, and every
  pixel but level 1's, whose one-texel stripes its taps average); `frontend.detail` the setting.

## 3. The renderer: the depth range and the fog

`port/gfx/gfx_detail.c`. The game's projection (guPerspective, the lookat multiplied in) makes z/w a function of the
eye's depth w alone, z/w = a + b/w (-1 at the near plane, 1 at the far one). `pcgfx_detail_projection` (`:94-109`)
takes a and b from each projection loaded or multiplied (`gfx_rsp.c:659, 764`): b is the fourth row of z's column,
a the ratio of z's column to w's in the first three rows. A projection with no w column, or near and far not in front
of the eye in that order, has none (orthographic views, the identity a task starts with).

- **The depth** (draw_distance far or max). With the near plane n = b / (-1 - a) and the far f = b / (1 - a), the depth
  GL stores is that of a projection with the far plane at 2 f or 32 f and the same n: for the same w, z/w' = al z/w +
  be, so the N64 depth d (the screen z over 1,023, or the primitive depth over 32,767) becomes d' = d mul + add for
  the projection and the viewport in effect (`dz_update`, `:112-132`; `gfx_rsp.c:520`). It is linear in d, so exact
  per vertex: GL interpolates z/w linearly across the screen, as the RDP does. The primitive depth of triangles and of
  rectangles drawn in the view is mapped the same (`gfx_rdp.c:1037`). Clears are still the far end. The geometry past
  the game's far plane, which the N64 draws at the far plane's depth in the order drawn, is then in depth order up to
  the new far plane. Under an MP the list sets itself (G_MV_MATRIX, G_MW_MATRIX: not the modelview times the
  projection) there is no map; a G_MTX or a G_POPMTX, which make MP the product again, bring it back (G_MW_MATRIX and
  G_POPMTX didn't mark the map for recomputing until 2026-11-04, so after them it stayed as it was).
- **Its precision.** The near plane sets it, and it doesn't move: with GL's 24-bit depth buffer (`gfx_gl.c:648`) a
  depth step at distance D is about D^2 / (n 2^24), with n = 2.5 ft 0.1 ft at 2,000 ft, 0.6 ft at 5,000 ft and 2.4 ft
  at 10,000 ft, the same at 2 f as at 32 f (b' tends to -2n). The N64's own z buffer is 18 bits; d' is computed in
  floats (24 bits), and GL's clip z is (2 d' - 1) w, divided back by w.
- **The fog** (`pcgfx_fog_alpha`, `:160-179`; G_FOG's shade alpha, clamp(z/w fm + fo)): fog far gives a vertex at
  depth w the fog the game gives one at w / 2 (z/w' = a + 2 (z/w - a), with a from the projection the vertex was
  loaded under); off gives none. With anything but original, z/w is held at 1 for the fog, so what draw_distance
  brings in past the game's far plane gets the far plane's fog: in a fog option the fog's end colour, and with no
  fog option none (the N64's gSPFogPosition(1000, 999) would give 3% of the fog colour far past the far plane).
- **The frame interpolation** draws frames between with the triangles' recorded context (the depth map with it) and
  recomputes fog with the transform's recorded term a (`gfx_interp.c:306, 1042`): the same depth and fog as the
  game's frames.
- **At original** none of it runs: no depth map (`dz_on` 0), and the fog the N64's expression, bit for bit
  (`gfx.all.*` and every synthetic path's screenshots, section 7).

## 4. What it costs

The scene line (2.6) gives what a view drew; `RUSH_GFX_TIMES=2` the time of a graphics task and of a frame drawn
between two, the CPU's with the GPU's (glFinish around each). Runs on the virtual clock (`--fast`), 1280 x 720 (the
native scale), 16:9, a high frame rate of 60.

### 4.1 The retail cartridge (Pi 5, V3D 7.1, Debug)

A run is a press script (`build/scratch/retail/press/`, local) from the menu into a single race with the gas held
for 45 s, or the attract demo; RUSH_GFX_TIMES=2 serializes the CPU and the GPU (glFinish around each part), so a
task's time is the two added, not what a pipelined frame costs. "Parts" is the share of the world's parts the
visibility tables let through, a view's mean (most); nodes and triangles are a view's and a graphics task's mean
(most).

| Run | Level | Nodes a view | At a coarser level | Parts | Triangles a task | Task, ms | Frame between, ms | Frame matrices | Display list | Lookats |
|---|---|---|---|---|---|---|---|---|---|---|
| the attract demo, tracks 0-4, 6 cars | original | 105.0 (228) | 5.1 | 20% (36%) | 1,226 (3,646) | | | 224 | 1,822 | 6 |
| | max | 265.5 (374) | 21.8 | all | 3,481 (10,226) | | | 370 | 2,778 | 6 |
| track 0, one player, 12 cars | original | 137.6 (213) | 3.9 | 27% (37%) | 1,545 (3,455) | 16.8 (160) | 9.2 | 237 | 1,881 | 12 |
| | far | 250.2 (337) | 8.3 | 81% (all) | 2,958 (9,157) | 27.4 (303) | 12.6 | 351 | 2,748 | 12 |
| | max | 324.5 (356) | 55.5 | all | 3,621 (11,183) | 31.8 (303) | 14.2 | 354 | 2,880 | 12 |
| track 0, two players, 12 cars | original | 175.8 (249) | 36.0 | 27% (37%) | 2,909 (6,938) | 27.0 (176) | 12.3 | 511 | 4,093 | 24 |
| | max | 333.0 (359) | 59.5 | all | 6,741 (22,276) | 56.0 (409) | 21.1 | 714 | 5,815 | 24 |
| track 1, two players, 6 cars | original | 156.3 (244) | 15.4 | 18% (24%) | 2,181 (6,856) | 21.1 (175) | 11.0 | 490 | 3,725 | 12 |
| | max | 376.5 (407) | 26.3 | all | 6,336 (19,189) | 49.2 (207) | 19.6 | 810 | 5,763 | 12 |
| | detail high | 156.3 (244) | 0 | 18% (24%) | 2,460 (7,416) | 20.8 (193) | 10.8 | 490 | 3,725 | 12 |
| tracks 1, 2, 3, 4, 5: two players, 12 cars | max | 410, 322, 389, 317, 375 | | all | 6,541, 4,365, 5,184, 5,547, 11,839 | 48.9, 38.6, 47.4, 40.9, 65.8 | 19.8, 16.7, 17.4, 17.8, 25.2 | 870, 694, 832, 680, 802 | 6,347, 5,715, 6,255, 5,643, 6,243 | 24, 24, 24, 24, 18 |
| battle arena (track 6), four players | original, far, max | | | | 1,631, 1,633, 1,633 | 16.1 at each | 8.6 | 204 | 1,732 | 4 |
| stunt arena (track 14), four players | max | | | | 3,359 (9,690) | 24.4 | 11.6 | 444 | 2,912 | 4 |

(The arenas' runs predate the scene line counting their views, 2.6: their node counts aren't given.)

- **Far** draws about twice the triangles of original on a race track (track 0: 1.9x), **max** two to five times (2.3x
  to 5x; track 5, whose far parts are big, the most); a view's nodes 2-2.5x. The battle and stunt arenas are within
  every limit already: the same at each level.
- **Detail high** costs 13% more triangles in split screen (track 1), the same nodes.
- **The Pi 5** at 1,396 x 720 (Debug, CPU and GPU serialized): original 17-27 ms a game frame, max 32 ms with one
  view and 39-66 ms with two, so max on the Pi holds the game's 30 frames a second with one view only, and the frames
  between two at 60 are drawn only when the GPU keeps up (FRAMERATE.md 2: none is drawn when the host is behind, the
  game's own frames are never late). A desktop GPU has the margin; the build host's Intel UHD 770 ran the synthetic
  paths at every level within its spread (4.2).
- **The pools** hold at max with room: two views and 12 cars, the heaviest case, reach 870 frame matrices and 6,347
  display-list commands (track 1) of the N64-sized 1,200 and 8,000 MORECARS.md 8.1 gave them, which are now 1,600
  and 12,000 for headroom (`pc_game_types.h`); the lookats are one a car's sheen in each view that draws it, 24 of 32
  with 12 cars in two views at every level (on the grid, where every car is near); persistent matrices and scene
  nodes don't depend on the setting (the same nodes are made, more are drawn). No pool ran past its end in any run.


### 4.2 The synthetic cartridge (build host, Intel UHD 770, Debug)

Its tracks are a few boxes and a ground, so this measures the overhead of the settings themselves more than their
drawing: within the run-to-run spread of a shared host at every level.

| Path | Level | Task, ms (most) | Frame between, ms | Frame matrices | Display list | Lookats |
|---|---|---|---|---|---|---|
| race12 (one view, 12 cars) | original | 2.62 (15.6) | 3.84 | 201 | 1,207 | 12 |
| | far | 2.69 (13.3) | 4.05 | 202 | 1,221 | 12 |
| | max | 2.56 (13.0) | 3.84 | 203 | 1,229 | 12 |
| race2p12 (two views, 12 cars) | original | 2.76 (11.5) | 4.07 | 402 | 2,393 | 24 |
| | far | 2.69 (17.0) | 3.98 | 404 | 2,421 | 24 |
| | max | 2.75 (10.6) | 4.08 | 406 | 2,437 | 24 |
| race4p (four views) | original | 2.65 (11.7) | 3.25 | 484 | 2,403 | 16 |
| | far | 2.65 (12.1) | 3.25 | 488 | 2,459 | 16 |
| | max | 2.48 (13.1) | 3.00 | 492 | 2,491 | 16 |
| demo12 (12 drones) | original | 3.89 (20.6) | 4.70 | 201 | 1,207 | 12 |
| | far | 3.36 (15.5) | 3.80 | 202 | 1,221 | 12 |
| | max | 4.07 (19.7) | 4.86 | 203 | 1,229 | 12 |

Every synthetic path also ran at max with detail high, fog far, 60 fps and 16:9 (31 of 31 reach their states, no pool
past its end; `build/scripts/modes_max.sh`, local).

## 5. What it looks like on the retail cartridge

Pi 5, the branch's Debug build, `--fast`, 16:9, a high frame rate of 60, the frames as the game draws them
(`RUSH_GFX_DUMP`, internal scale 2: 852 x 480), compared frame by frame across the levels; the runs, logs and frames
are in `build/scratch/retail/` and stay there. Described in words.

### 5.1 The attract demo: tracks 0-4, one view

The demo races six drones on tracks 0 to 4 in turn (the Expansion Pak's fifth), with no fog option. Frames every
five seconds, the same moments at original, far and max (the demo's course is the same at every level: the settings
change no game logic):

- **More of the world.** At far, buildings and the edge of the city appear on the horizon where original shows sky;
  at max, a distant skyline (towers on the far side of the bay, antennas, the track's farther sections) stands at the
  horizon in the open views of tracks 0, 2 and 4, and drones further down the road are drawn where original drops
  them. In the enclosed stretches (walls, buildings both sides, tunnels) the three levels look the same: the z buffer
  hides what the visibility tables let through behind the walls.
- **No holes.** No part drawn at far or max shows a missing face, a gap at its seam with the next, or an open back:
  the parts the tables hid from a place are whole models, seen from the side the track faces them.
- **Against the sky.** The added skyline is drawn in front of the backdrop with its own colours; with no fog option
  there is no fog to match. The sky dome follows the camera and is drawn behind everything at every level.
- **Depth.** No flicker between far parts (the skyline's towers against each other and the ground under them), at
  2,000-6,000 ft, where a depth step is under a foot (section 3).
- **What shouldn't be drawn isn't**: the per-view masks hide the same kinds of node at every level (about 30 a view,
  section 4); the title screen between races is the same.

### 5.2 The camera's views, track 0

A race with the gas held and the camera changed every 6 s (C-up), frames every second, original against max:

- **The own car's hide mask holds.** In the bumper view the player's car is hidden in its view at max as at original
  (the per-view mask is not a distance and no setting touches it); the chase views draw it as before.
- **More skyline.** From the bumper view on the bay side, max adds the city's distant towers above the hills, in front
  of the sky and behind the hills, where original shows sky.

### 5.3 The fog, track 1 with the fog option at 3

The heaviest fog option, one player, frames every 2 s: original; far with fog far; max with fog off.

- **Original**: the game's lavender fog; the world fades into it within a few hundred feet, and the fog's colour is
  the sky (the game makes no sky dome with a fog option, 1.7).
- **Far with fog far**: the fog starts and ends twice as far: hills, trees and buildings that original fogs out stand
  in a thinner haze, fading at twice the distance; the parts and models the radius still leaves out are past the fog's
  end, so nothing pops in front of the fog.
- **Max with fog off**: no fog; the whole track is drawn to its end, trees and buildings sharp at any distance. The
  background was the fog's flat colour (no sky dome), which looked wrong behind a world without fog: fog off now makes
  the sky dome as without a fog option (`SkyInit.c`, `PC_SKY_FOG`, from the next track). Rerun with it: the
  track's blue sky and clouds behind the whole track, as without a fog option, every frame otherwise the same.

### 5.4 Split screen

- **Four players** (battle arena, track 6): the four views look the same at every level, as the arena is within every
  limit (4.1); the HUD, the arrows and the dividers are where they were.
- **Two players, detail high** (track 1, frames every 2 s against original): the cars' tires show their tread, their
  first level, where split screen draws the plainer second; the trackside is otherwise the same at this size (the
  one-player sections add 13% of triangles, 4.1). No part is drawn twice: no z-fighting, no doubled edge.

### 5.5 Track 5 (the Expansion Pak's, at night)

Unlocked as a winner's profile has it (gdb sets the player's two unlock bytes as the track select opens), one player,
frames every 2 s: at max the city's lit towers stand beyond the bridge's end and the hills behind the harbour, against
the night sky, where original shows the sky; the tunnel, the bridge and its cables look the same. No hole, no
flicker. It is the heaviest track at max (4.1).

### 5.6 Frames between two (a high frame rate) at max

Track 0 at max with detail high and 60 frames a second, every frame shown for two seconds (`RUSH_GFX_HFR_DUMP`): the
frames drawn between the game's step evenly between them, the far trees, the hills and the bridge included, with no
part missing from a frame between that both frames have and no jump; 1,551 frames were drawn between two in the run
and none was held back. Divided matrices (2.3) pair and blend as the others do.

### 5.7 What was looked for and not seen

Holes or open backs in parts the tables hid; seams between far parts; the fog's colour against far geometry (with a fog
option, far geometry fades into the fog's colour at fog far and original alike); popping at the new limits (frames
seconds apart can't show a pop: by construction a model at far switches levels where original does and leaves at
twice its distance, and detail high and max have no level change or limit left to pop, save the sheen's fade);
z-fighting from the longer depth range; the own car, the hood or the
cockpit where the view hides them; objects ahead of their time (no show or hide by lap or checkpoint was found in the
code, 1.8, and none was seen).

## 6. Tests

### 6.1 The synthetic cartridge's detail world

The synthetic track had one world part and models of one level with no limit, so no setting changed anything on it.
`tools/port/synth_rom.py` (`DETAIL_PARTS`, `detail_world`) now gives track 0 what the limits read, from synthetic
values only (no ROM): the ground with a box round the loop (the cell the eye is in), and five parts with models of
their own, boxes and the visibility rows (`track1Parts` rows 0-5, `trackParts[0]` 6):

| Part | Where | Made for |
|---|---|---|
| SYN_NEAR | a tower 1,500 ft ahead of the start, outside the loop | the ground's row lets it through |
| SYN_HIDDEN | inside the loop, about 1,800 ft from the start | not in the ground's row; its own cell's is within 2,000 ft: far adds it |
| SYN_FAR | 6,000 ft out | in no row the eye reaches, past 4,096 ft: only max, through a matrix divided by 4 |
| SYN_LOD | two levels (red to 200 ft, blue to 1,200 ft), about 2,000 ft away | past its last level at original; blue at far; red with detail high |
| SYN_SCENERY | far scenery (0x400000), 21,000 ft out | pulled in to 1,600 ft, 3,200 ft at far, its place at max |

and one player's fog tables for options 1-3 (start, far plane 1,500/1,000/600 ft, radius 1,200/800/500 ft). The
existing paths race on track 0 and draw it at original; their traces are the base's (section 7). The six paths
(`tools/port/synth/detail*.press`, ctest `synth_boot_detail*`) race two seconds from the start, and
`tools/port/detail_check.py` reads the scene line's most a view:

| Path | Settings | Checked |
|---|---|---|
| `detail` | original | 4 of 6 parts let through; 1 past its last level (SYN_LOD); none out of range or past the fog's radius |
| `detail_far` | draw_distance far | 5 parts (SYN_HIDDEN); none past its last level; SYN_LOD at its second level |
| `detail_max` | draw_distance max | all 6 parts; none past its last level or out of range (SYN_FAR drawn 6,000 ft out) |
| `detail_high` | far, detail high, fog far | 5 parts; every node at its first level |
| `detail_fog` | the track select's fog option at 1 | 4 parts; 2 past the fog's radius (SYN_NEAR, SYN_LOD) |
| `detail_fogfar` | the fog option, far, fog far | 5 parts; none past the fog's radius or their last level |

So max draws more than original on the same path (6 parts against 4, and the nodes drawn), and each limit is seen
to move.

### 6.2 The renderer and the settings

- `gfx.detail` (port/gfx/tests/gfx_test.c, GFX.md "Tests"): geometry past the far plane in the order drawn at original
  and in depth order at far and max; depth order inside the far plane and primitive depth at every level; fog far
  against original at half the depth, off, and the far plane's fog for what draw_distance brings in; at far, no depth
  map under a forced MP (G_MW_MATRIX) and the map again after a G_POPMTX or a G_MTX that follows one. On GLES, desktop
  GL, llvmpipe, at 3x in 16:9 (rocky).
- `gfx.all.draw-max`, `gfx.interp.all.draw-max`: every renderer case and every frame-interpolation case with
  `RUSH_GFX_DRAW_DISTANCE=max` and `RUSH_GFX_FOG=far`.
- `frontend.detail` (port/input/tests/test_frontend.c): the keys, the file, the environment, and what the game reads
  (FRONTEND.md "Tests").

## 7. Proof: original is the N64's game

"Base" is a build of main 0d7d016, "branch" of this branch at its last code commit (d199c5b; the build host's rows
again at the documents' last). With every setting at `original`:

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2 --shots`, base against the branch, Debug, build host (x86-64, GCC 14), on the branch's cartridge (track 0's detail world): every path whose settings are original (27: all but `detail_far`, `detail_max`, `detail_high`, `detail_fogfar`, which the base doesn't take) | the same trace and the same screenshots (drive, race2p, race4p, race2p12, detail, detail_fog), every run: 27 of 27 |
| the same on the base's own cartridge (no detail world), its 25 paths | 25 of 25 |
| the same 27, Pi 5 (aarch64, GCC 12), under the lock | the same trace and screenshots, every run: 27 of 27 |
| the six retail scripts (`race`, `race2p`, `battle2p`, `stunt`, `battlequit`, `pause`: `synth_compare.py --script`, MORECARS.md 10.4), base against the branch, Pi 5, retail cartridge, `--runs 2 --same` | the same trace, every run, each reaching its state: 6 of 6 |
| Windows cross build (Pi, mingw-w64, fresh configure) | builds; gen_data's check of the executable: OK, 26 decoupled, 1 reserved in the window |
| ctest, build host: Debug `build/pc`, RelWithDebInfo `build/opt` | 265 of 265 each (`assets_rom` skipped: no ROM there) |
| `n64_neutral.py --base 09da2f0` | passes (1,018 files: 92 code-exact, 926 shifted); against 0d7d016 the 7 changed game files under `src/`, shifted |
| the N64 IDO check (`tools/port/ido_check.sh`, build host, at d199c5b) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |
| `offset_check --strict`, build host | no open findings; 59,472 accesses placed |
| `alias_check` (RelWithDebInfo) | none open |
| `layout_check` | no open findings; 48,217 accesses |
| `endian_check` (and `--check-loader`) | the base's 2 open value conflicts in 1 storage and 3,525 puns; 2,558 live functions (the base 2,556: VisRow, VisReach), 48,217 accesses (48,203); check-loader 96 places, 0 wrong |
| `bounds_check` (GCC 14, RelWithDebInfo) | none new, 0 two offsets, 0 clamped |

## 8. What is left

- **The poly list's 2,048 ft** (shadows, skids, markers, decals): kept; its vertices are s16 against the eye.
- **Split screen's particle cuts** (smoke, skids, explosions, trails with three or four players: 1.8) and the fog
  tables by the number of players: kept, as the N64 has them. They are spawn decisions on shared pools (the poly
  list's 200, the effect pools), not models.
- **G_BRANCH_Z** (the microcode's level of detail): not in the race lists traced; detail high doesn't touch it.
- **A frame between two at a node whose divisor h changes** (2.3): a slip of a fraction of a frame in its place for one
  frame between; not seen.
- **Not run on the retail cartridge**: the Windows program (the cross build only), a real display and a desktop GPU's
  timings (the Pi's V3D on retail, the build host's iris on the synthetic paths), whole laps and finishes at far and
  max (each run covered the first 45 s of a race, and the demo five tracks' starts), the stunt and battle arenas other
  than tracks 6 and 14, the obstacle course, three players, a ghost race (the game's replays: ReplayStream), the pause
  screen at far and max (the synthetic `pause` and `pause12` reach their states at max), a fog region's blend at fog
  far (the same arithmetic, per vertex), and whether a retail track has level objects for one player only (2.5).
- **Far's 2,000 ft** (2.1) is a choice: the rows were made for a 2,000-ft far plane, so the cells within as much more
  are the ones a far plane twice as far could see from. On track 0 it lets 81% of the parts through on average.
