#!/usr/bin/env python3
"""Link a compressed code image (game code, runtime image A or B) from its
compiled C and its assembly targets, gate it on byte identity, and compress it
exactly as the cartridge's stream was compressed.

    python3 tools/build_image.py game|ovl_a|ovl_b --deflate CLI --out STREAM

Inputs (all in the tree or made by the Makefile):
    layout/<image>.json      every entry of the image in order: functions with
                             their extents, the .data/.rodata tail, alignment runs
    manifest/<image>.json    which functions are compiled from C: singles
                             (src/.../<fn>.c) and whole-program groups
    build/<image>/obj/*.o    the compiled singles (tools/compile_single.sh)
    build/<image>/groups/*.o the compiled groups (tools/compile_group.py)
    asm/us/<dir>/*.s         assembly targets: the words of every function that
                             is not compiled from C
    asm/us/<dir>/symbols.json  addresses the compiled code links against
    the retail image         (tools/extract_rom.py), read for its DATA only: the
                             .data/.rodata tail and the alignment runs (.incbin)

Steps (tools/romkit holds the project's splice code, copied verbatim):
  1. each single is linked alone at its image address (romkit.blob_splice.
     link_function): relocations resolve against symbols.json, its own
     .rodata/.data/.bss go to the addresses frozen in manifest/<image>.json
     (where retail keeps them), and the body is cut to the extent (only zero
     padding may be cut);
  2. each group object is sliced per member and relocated
     (romkit.blob_group.relocate), its own data placed as frozen;
  3. one assembly file per layout region is generated (compiled bodies as
     .word, other functions from the assembly targets, data .incbin'd from the
     retail image), assembled and linked at the image base;
  4. the linked image must equal the reference image byte for byte (the gate):
     the assembly targets' words plus the retail data, whose sha256 is the
     retail image's;
  5. it is compressed with zlib 1.0.4 (level 9, raw, memLevel 8, one
     Z_FINISH: tools/deflate104), and the stream must fill its ROM slot exactly.
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from romkit import blob_build, blob_group, blob_splice, blob_tu  # noqa: E402

IMAGES = {
    # name: (asm target dir, source dir for comments, ROM stream length)
    "game": ("asm/us/blob", "src/blob", 326180),
    "ovl_a": ("asm/us/ovl_a", "src/ovl_a", 80272),
    "ovl_b": ("asm/us/ovl_b", "src/ovl_b", 23639),
}


def asm_words(asm_dir):
    """{function: [words]} from the assembly targets' `.section .text.<fn>` blocks."""
    words, current = {}, None
    for path in sorted(Path(asm_dir).glob("*.s")):
        for line in path.read_text().splitlines():
            m = re.match(r"\.section \.text\.(\S+?),", line)
            if m:
                current = words.setdefault(m.group(1), [])
                continue
            if line.startswith(".section"):
                current = None
                continue
            m = re.match(r"\s*\.word 0x([0-9A-Fa-f]{8})", line)
            if m and current is not None:
                current.append(int(m.group(1), 16))
    return words


def own_record(own):
    """(linker lines, .bss references) -> the manifest's JSON form, and back."""
    lines, bss = own
    return {"ld": lines, "bss": [[sec, off, list(his), lo, addr] for sec, off, his, lo, addr in bss]}


def own_tuple(record):
    return record["ld"], [(sec, off, tuple(his), lo, addr) for sec, off, his, lo, addr in record["bss"]]


def bodies(name, document, manifest, symbols, image, work, freeze=False):
    """{function: bytes} of every compiled function at its image address.

    Normal builds use the own-data placements frozen in the manifest
    ("own_data"): each compiled function's literals, jump tables and statics
    go where retail keeps them, and no retail code is read. `freeze` derives
    them instead, verifying every such section's bytes against the retail
    image at the address the retail instruction words encode, and records
    them in the manifest (a maintenance step: tools/build_image.py
    --freeze-placements)."""
    extents = {e["target_id"]: e for r in document["regions"] for e in r["entries"]
               if e["kind"] == "function"}
    out, problems = {}, {}
    for fn in sorted(manifest["singles"]):
        rec = manifest["singles"][fn]
        obj = ROOT / "build" / name / "obj" / f"{fn}.o"
        e = extents.get(fn)
        try:
            if e is None:
                raise blob_build.BuildError("not in the layout")
            if freeze:
                own = blob_splice._own_data(obj, fn, e["vaddr"], e["size"], symbols, image)
                rec["own_data"] = own_record(own)
            elif "own_data" not in rec:
                raise blob_build.BuildError("no frozen own-data placement in the manifest "
                                            "(tools/build_image.py --freeze-placements)")
            out[fn] = blob_splice.link_function(obj, fn, e["vaddr"], e["size"], symbols,
                                                work=work / "fn" / fn,
                                                own=own_tuple(rec["own_data"]))
        except (blob_build.BuildError, ValueError, OSError) as exc:
            problems[fn] = " ".join(str(exc).split())[:300]
    runs = blob_group.opaque_runs(document)
    for group, rec in sorted(manifest["groups"].items()):
        obj = ROOT / "build" / name / "groups" / f"{group}.o"
        spec = json.loads((ROOT / rec["dir"] / "group.json").read_text())
        try:
            slices, text_ndx = blob_group.member_slices(
                obj, spec["members"] + spec.get("context", []), extents)
            if freeze:
                captured = {}
                blob_group.relocate(obj, slices, text_ndx, symbols, members=spec["members"],
                                    image=image, data_runs=runs, capture=captured)
                rec["own_data"] = captured
            elif "own_data" not in rec:
                raise blob_group.GroupError("no frozen own-data placement in the manifest")
            built = blob_group.relocate(obj, slices, text_ndx, symbols, members=spec["members"],
                                        frozen=rec["own_data"])
        except (blob_group.GroupError, blob_build.BuildError, ValueError, OSError,
                subprocess.CalledProcessError) as exc:
            problems.update({m: f"group {group}: {' '.join(str(exc).split())[:300]}"
                             for m in rec["members"]})
            continue
        for member in rec["members"]:
            if member in built:
                out[member] = built[member]
            else:
                problems[member] = f"group {group} built no body for it"
    if problems:
        lines = [f"  {k}: {v}" for k, v in sorted(problems.items())]
        sys.exit(f"build_image {name}: {len(problems)} compiled bodies cannot be placed:\n"
                 + "\n".join(lines[:40]))
    return out


def reference_image(document, retail, targets):
    """The image as the assembly targets and the retail DATA say it is: every
    function from asm/us, every other entry from the extracted image. Its
    sha256 must be the layout's (the retail image's)."""
    base = int(document["image"]["base"], 16)
    ref = bytearray(len(retail))
    for region in document["regions"]:
        for e in region["entries"]:
            lo = e["vaddr"] - base
            if e["kind"] == "function":
                ref[lo:lo + e["size"]] = b"".join(w.to_bytes(4, "big") for w in targets[e["target_id"]])
            else:
                ref[lo:lo + e["size"]] = retail[lo:lo + e["size"]]
    return bytes(ref)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image", choices=sorted(IMAGES))
    ap.add_argument("--deflate", required=True, help="the deflate104 executable")
    ap.add_argument("--out", required=True, help="the compressed stream to write")
    ap.add_argument("--non-matching", action="store_true",
                    help="write the stream even when the image differs from retail")
    ap.add_argument("--freeze-placements", action="store_true",
                    help="maintenance: derive every compiled function's own-data placement from "
                         "the retail image (reads retail code words) and record it in "
                         "manifest/<image>.json")
    args = ap.parse_args(argv)
    name = args.image
    asm_dir, src_dir, stream_len = IMAGES[name]
    document = json.loads((ROOT / "layout" / f"{name}.json").read_text())
    manifest = json.loads((ROOT / "manifest" / f"{name}.json").read_text())
    retail_path = ROOT / document["image"]["path"]
    retail = retail_path.read_bytes()
    if len(retail) != document["image"]["size"]:
        sys.exit(f"build_image {name}: {retail_path} is not the retail image (run make extract)")
    base = int(document["image"]["base"], 16)
    symbols = {k: int(v, 16) for k, v in
               json.loads((ROOT / asm_dir / "symbols.json").read_text())["symbols"].items()}
    work = ROOT / "build" / name / "link"
    if work.exists():
        shutil.rmtree(work)
    (work / "asm").mkdir(parents=True)

    compiled = bodies(name, document, manifest, symbols, (retail, base), work,
                      freeze=args.freeze_placements)
    if args.freeze_placements:
        manifest["placements"] = "frozen own-data placements (tools/build_image.py --freeze-placements)"
        (ROOT / "manifest" / f"{name}.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        print(f"build_image {name}: own-data placements frozen into manifest/{name}.json")
    targets = asm_words(ROOT / asm_dir)
    reference = reference_image(document, retail, targets)
    if hashlib.sha256(reference).hexdigest() != document["image"]["sha256"]:
        sys.exit(f"build_image {name}: the assembly targets plus the data of {retail_path} are not "
                 "the retail image (run make extract)")
    rel = retail_path.relative_to(ROOT)
    objects = []
    for region in document["regions"]:
        src = work / "asm" / f"{region['name']}.s"
        try:
            src.write_text(blob_tu.render_region(region, retail, rel, compiled, base=base,
                                                 source_dir=src_dir, asm_words=targets))
        except (KeyError, ValueError) as exc:
            sys.exit(f"build_image {name}: region {region['name']}: {exc}")
        obj = work / f"{region['name']}.o"
        blob_build.assemble(src, obj)
        objects.append(obj)
    script = work / f"{name}.ld"
    script.write_text(blob_tu.render_linker_script(document, symbols={}))
    elf = work / f"{name}.elf"
    blob_build.link(objects, script, elf)
    built = blob_build.extract_image(elf, work / "image.bin")
    sha = hashlib.sha256(built).hexdigest()
    functions = sum(1 for r in document["regions"] for e in r["entries"] if e["kind"] == "function")
    if built != reference:
        message = blob_build.describe_difference(document, reference, built)
        if not args.non_matching:
            sys.exit(f"build_image {name}: the linked image is NOT byte-identical to retail\n{message}")
        print(f"build_image {name}: NON-MATCHING image\n{message}")
    print(f"build_image {name}: {len(compiled)}/{functions} functions from C, image "
          f"{len(built)} B sha256 {sha[:16]}... {'matches retail' if built == reference else ''}")

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([args.deflate, str(work / "image.bin"), str(out)], check=True)
    stream = out.read_bytes()
    if len(stream) != stream_len:
        out.unlink()
        sys.exit(f"build_image {name}: compressed stream is {len(stream)} B, its ROM slot "
                 f"holds exactly {stream_len}")
    print(f"build_image {name}: stream {len(stream)} B -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
