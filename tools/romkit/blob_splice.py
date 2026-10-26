"""Link one compiled function at its image address (distilled from the
project's image-splice tooling; definitions copied verbatim)."""
import re
import subprocess
import tempfile
from pathlib import Path

from . import blob_build, owndata


def _own_data(object_path, target_id, vaddr, size, provides, image=None):
    """(linker-script lines, verified .bss references) for the object.

    The .bss references are owndata `bss_sites` entries:
    (section, object offset, hi sites, lo site, image address)."""
    try:
        if not owndata.own_references(object_path, target_id, limit=size):
            return "", []
    except (ValueError, IndexError) as exc:
        raise blob_build.BuildError(f"{target_id}: {exc}") from exc
    if image is None:
        raise ValueError("own data needs the retail image")
    data, base = image
    if not base <= vaddr or vaddr + size > base + len(data):
        raise blob_build.BuildError(f"{target_id}: extent is outside the retail image")
    body = data[vaddr - base:vaddr - base + size]
    want = [int.from_bytes(body[i:i + 4], "big") for i in range(0, size, 4)]

    def named(name):
        return provides[name] if name in provides else address_named(name)

    result = owndata.verify(object_path, target_id, want, address=vaddr,
                            image=owndata.ImageData.from_image(data, base),
                            addresses=named)
    if result.failures:
        raise blob_build.BuildError(f"{target_id}: " + "; ".join(result.failures))
    try:
        bases = result.bases()
    except ValueError as exc:
        raise blob_build.BuildError(
            f"{target_id}: {exc}; the single-function link cannot place them") from exc
    # SUBALIGN(1): IDO aligns its data sections to 16, retail's literals sit at
    # any word; without it ld would round the section up past the address.
    lines = [f"    .own{index} 0x{address:08X} : SUBALIGN(1) {{ *({name}) }}\n"
             for index, (name, address) in enumerate(sorted(bases.items()))]
    # Zero-initialised objects: NOLOAD, so nothing is emitted, at the address
    # of the section's first verified object. One input section cannot be
    # split, so objects retail keeps at other distances are relocated per
    # reference after the link (`_place_bss`).
    for index, (name, objects) in enumerate(sorted(result.bss().items())):
        first = min(objects)
        lines.append(f"    .ownbss{index} 0x{(objects[first] - first) & 0xFFFFFFFF:08X} (NOLOAD)"
                     f" : SUBALIGN(1) {{ *({name}) }}\n")
    return "".join(lines), list(result.bss_sites)


def _place_bss(data, start, size, references, target_id):
    """Apply verified .bss references to the linked body `data` (from the
    function's start): each HI16/LO16 immediate is set for the object's
    verified image address, exactly as the linker would for a symbol there.
    Only the 16-bit immediates of the listed sites change; a HI16 shared by
    references whose addresses need different upper halves is refused."""
    data = bytearray(data)
    his = {}
    for _section, _offset, hi_sites, lo_site, address in references:
        for site, value in [(h, ((address + 0x8000) >> 16) & 0xFFFF) for h in hi_sites] + \
                [(lo_site, address & 0xFFFF)]:
            at = site - start
            if not 0 <= at <= size - 4:
                raise blob_build.BuildError(f"{target_id}: .bss reference site "
                                            f"+0x{at:x} is outside the function")
            if site in hi_sites:
                if his.setdefault(site, value) != value:
                    raise blob_build.BuildError(
                        f"{target_id}: HI16 at +0x{at:x} serves .bss objects whose "
                        "addresses need different upper halves")
            word = int.from_bytes(data[at:at + 4], "big")
            data[at:at + 4] = ((word & 0xFFFF0000) | value).to_bytes(4, "big")
    return bytes(data)


def link_function(object_path, target_id, vaddr, size, provides=None, work=None,
                  image=None, own=None):
    """Bytes a compiled function contributes at its image address.

    Linked alone so its relocations resolve against the image symbol table (data
    globals inside opaque runs, and libultra/libc out in the cartridge), then
    cut to the extent only after verifying excess words are zero padding.
    IDO pads .text to 16 bytes; a following function bounds that padding.
    `provides` is {name: address}: several names may
    share an address (a data-symbol label and the function's own target_id),
    and every one of them must resolve. The object's own data sections
    (literals, jump tables, local statics) are verified against `image`
    (bytes, base address; default: the layout's retail image) and placed at
    the addresses retail uses; see `own_data_placements`. Zero-initialised
    objects (.bss) are verified by address only, never emitted, and placed
    per object after the link (`_place_bss`)."""
    if provides is None:
        raise ValueError("link_function needs the image symbol table")
    provides = dict(provides)
    defined_data = defined_data_symbols(object_path)
    for name in defined_data:
        addr = address_named(name)
        if name not in provides and addr is not None:
            provides[name] = addr
    for name in undefined_symbols(object_path):
        addr = address_named(name)
        if name not in provides and addr is not None:
            provides[name] = addr
    work = Path(work or tempfile.mkdtemp(prefix="blobfn-"))
    work.mkdir(parents=True, exist_ok=True)
    script = work / f"{target_id}.ld"
    # Definitions can affect as1 scheduling, but their storage already lives
    # in the image. PROVIDE does not override an object's definition: without
    # an absolute assignment GNU ld places these globals after the function.
    # Only externally visible data objects are rebound; function definitions
    # and local statics retain their normal link semantics.
    # own: frozen (linker lines, .bss references) from the manifest; else derived
    own_data, bss = (own if own is not None
                     else _own_data(object_path, target_id, vaddr, size, provides, image))
    provides = "\n".join((f"    {name} = 0x{addr:08X};" if name in defined_data
                           else f"    PROVIDE({name} = 0x{addr:08X});")
                          for name, addr in sorted(provides.items())
                          if name != target_id)
    # A file may define helpers ahead of the target (IDO -O3 emits an inlined
    # callee first). The section is placed so the TARGET lands on vaddr; the
    # helpers' bytes are linked below it and discarded.
    functions = blob_build.function_symbols(object_path)
    if target_id not in functions:
        raise blob_build.BuildError(f"{target_id}: no defined function symbol")
    start, section = functions[target_id]
    if start % 4:
        raise blob_build.BuildError(f"{target_id}: function is not word aligned in its section")
    script.write_text(
        "SECTIONS\n{\n"
        f"    . = 0x{vaddr - start:08X};\n"
        # Explicit address + SUBALIGN: an input section aligned to 16 would move .out above
        # vaddr, and a self-call (recursion) would then resolve 4-12 bytes late.
        f"    .out 0x{vaddr - start:08X} : SUBALIGN(4) {{ *(.text.{target_id}) }}\n"
        f"{provides}\n"
        f"{own_data}"
        "    /DISCARD/ : { *(.pdr) *(.mdebug*) *(.comment) *(.note*)"
        " *(.reginfo) *(.options) *(.MIPS.abiflags) }\n}\n")
    elf = work / f"{target_id}.elf"
    proc = subprocess.run(
        [blob_build.LD, "-T", str(script), "--no-warn-rwx-segments",
         "-o", str(elf), str(object_path)], capture_output=True, text=True)
    if proc.returncode != 0:
        raise blob_build.BuildError(
            "link failed: " + " ".join(proc.stderr.split())[:220])
    binary = work / f"{target_id}.bin"
    dump = subprocess.run(
        [blob_build.OBJCOPY, "-O", "binary", "--only-section=.out",
         str(elf), str(binary)], capture_output=True, text=True)
    if dump.returncode != 0:
        raise blob_build.BuildError("objcopy failed: " + dump.stderr.strip()[:200])
    data = binary.read_bytes()
    # Use input-section offsets: the linker can align .out above vaddr, while
    # objcopy emits only its contents (without that leading address gap).
    data = data[start:]
    end = min((value - start for value, ndx in functions.values()
               if ndx == section and value > start), default=len(data))
    end = min(end, len(data))
    if end < size:
        raise blob_build.BuildError(
            f"compiled body is {end} bytes, shorter than the "
            f"{size}-byte extent")
    if size % 4 or end % 4:
        raise blob_build.BuildError(f"{target_id}: function extent is not word aligned")
    extra = sum(any(data[offset:offset + 4]) for offset in range(size, end, 4))
    if extra:
        raise blob_build.BuildError(f"{target_id}: {extra} extra words "
                                    "(nonzero beyond target length)")
    if bss:
        data = _place_bss(data, start, size, bss, target_id)
    return data[:size]


_ADDRESS_NAME = re.compile(r"^(?:func|D)_([0-9A-Fa-f]{8})$")


def address_named(name):
    """The address an m2c-style `func_XXXXXXXX` / `D_XXXXXXXX` name encodes.

    m2c names an unknown callee or global by its address, so the name IS the
    address — including callees outside the image (static ROM, or the
    0x8038xxxx overlay range) that no table knows."""
    match = _ADDRESS_NAME.match(name)
    return int(match.group(1), 16) if match else None


def undefined_symbols(object_path):
    """Names the object references but does not define."""
    proc = subprocess.run([blob_build.NM, "-u", str(object_path)],
                          capture_output=True, text=True)
    if proc.returncode != 0:
        raise blob_build.BuildError("nm failed: " + proc.stderr.strip()[:200])
    return [line.split()[-1] for line in proc.stdout.splitlines()
            if line.strip()]


def defined_data_symbols(object_path):
    """Externally visible ELF objects, including common definitions."""
    proc = subprocess.run([blob_build.READELF, "-sW", str(object_path)],
                          capture_output=True, text=True)
    if proc.returncode:
        raise blob_build.BuildError("readelf failed: " + proc.stderr.strip()[:200])
    result = set()
    for line in proc.stdout.splitlines():
        fields = line.split()
        if (len(fields) >= 8 and fields[0].rstrip(":").isdigit()
                and fields[3] == "OBJECT" and fields[4] in ("GLOBAL", "WEAK")
                and (fields[6].isdigit() or fields[6] == "COM")):
            result.add(fields[7])
    return result
