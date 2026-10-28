# tools/texpack: making a texture pack

`texpack.py` upscales a texture dump into a texture pack with an upscaler you install yourself. It needs Python 3
and nothing else; it downloads nothing and bundles no upscaler or model.

1. **Dump** the textures from your own ROM: run the game with `--set video.texture_dump=dump` (or put
   `texture_dump = dump` under `[video]` in `rush2049pc.ini`) and play the tracks, cars and menus you want. Every
   texture drawn is written once into `dump/` in the data folder (next to the program for the release archives),
   as `<format>_<w>x<h>_<hash>.png`.
2. **Install an upscaler**: any command-line program that takes `-i IN -o OUT -s SCALE`, such as
   realesrgan-ncnn-vulkan (from its own project's releases, with the models it ships). Note where its executable is.
3. **Upscale** the dump into the pack folder:

   ```
   python3 tools/texpack/texpack.py DATA/dump DATA/textures --upscaler /path/to/realesrgan-ncnn-vulkan --scale 4
   python3 tools/texpack/texpack.py DATA/dump DATA/textures --upscaler realesrgan-ncnn-vulkan --model realesrgan-x4plus
   python3 tools/texpack/texpack.py DATA/dump DATA/textures --upscaler builtin-nearest --scale 2   # no upscaler: a test
   ```

   Each texture is padded by wrapping (`--pad`), upscaled, and cut back to exactly SCALE times its size, so tiling
   textures get no seams; cut-outs get binary alpha back. Textures under 4 texels a side are skipped (`--min-size`),
   and so are files the pack already has: a run can be stopped and started again, and you can edit or delete any
   file in the pack afterwards (a missing file means the original is drawn). `--only ci8_` limits a run to names
   with that start. Options after `--` go to the upscaler (`-- -g 1` for its second GPU).
4. **Play** with `video.textures = pack` (Textures: pack on the F1 overlay's Video page). The log says how many files
   the pack has and how many textures it replaced; a file of the wrong size is named there.

A pack holds data from your ROM: keep it on your machine. `texpack.py --self-test` checks the script on a synthetic
dump. [docs/port/TEXTURES.md](../../docs/port/TEXTURES.md) explains names, identities and how the game uses the files.
