"""Relocate the members of an IDO -O3 whole-program group object to their
image addresses (distilled from the project's image-splice
tooling; definitions copied verbatim)."""
import re
import struct
import subprocess
from pathlib import Path

from . import blob_build, blob_splice, owndata

READELF = "mips-linux-gnu-readelf"
OBJCOPY = blob_build.OBJCOPY


class GroupError(Exception):
    pass


def _freeze(placement):
    """One own-data section's placement as JSON (see relocate: frozen/capture)."""
    if isinstance(placement, int):
        return {"base": placement}
    if isinstance(placement, _TableWindows):
        return {"windows": [list(w) for w in placement.windows]}
    if isinstance(placement, _ReferenceWindows):
        return {"references": {str(k): v for k, v in sorted(placement.addresses.items())}}
    raise GroupError(f"cannot freeze placement {placement!r}")


def _thaw(section, record):
    if "base" in record:
        return record["base"]
    if "windows" in record:
        return _TableWindows(tuple(tuple(w) for w in record["windows"]))
    return _ReferenceWindows(section, {int(k): v for k, v in record["references"].items()})


def _text(obj):
    return bytearray(subprocess.run(
        [OBJCOPY, "-O", "binary", "-j", ".text", str(obj), "/dev/stdout"],
        capture_output=True, check=True).stdout)


def _sections(obj):
    """{name: (index, type, size)} from the section headers."""
    out = subprocess.run([READELF, "-SW", str(obj)], capture_output=True,
                         text=True, check=True).stdout
    secs = {}
    for m in re.finditer(r"\[\s*(\d+)\]\s+(\S+)\s+(\S+)\s+[0-9a-f]+\s+[0-9a-f]+\s+([0-9a-f]+)",
                         out):
        secs[m.group(2)] = (m.group(1), m.group(3), int(m.group(4), 16))
    return secs


def _section_bytes(obj, name):
    return subprocess.run([OBJCOPY, "-O", "binary", "-j", name, str(obj), "/dev/stdout"],
                          capture_output=True, check=True).stdout


LOCAL_DATA = (".data", ".sdata", ".rodata", ".rdata", ".lit4", ".lit8")


LOCAL_BSS = (".bss", ".sbss")       # no bytes: verified by address, per reference


def _sext16(v):
    return v - 0x10000 if v & 0x8000 else v


class _TableWindows:
    """Verified disjoint object ranges placed at independent image addresses."""

    def __init__(self, windows):
        self.windows = tuple(windows)

    def address(self, addend):
        if addend % 4:
            raise GroupError(f"unaligned jump table addend {addend:#x}")
        for lo, hi, address in self.windows:
            if lo <= addend < hi:
                return address + addend - lo
        raise GroupError(f"jump table addend {addend:#x} outside verified windows")


class _ReferenceWindows:
    """Per-reference placements of one own section: {object offset: image
    address}, each verified by content for a member that is being output."""

    def __init__(self, section, addresses):
        self.section = section
        self.addresses = dict(addresses)

    def address(self, addend):
        try:
            return self.addresses[addend]
        except KeyError:
            raise GroupError(f"{self.section}+0x{addend:x} is not a verified "
                             "own-data reference of a member") from None


def _local_address(placement, addend):
    return (placement.address(addend)
            if isinstance(placement, (_TableWindows, _ReferenceWindows))
            else placement + addend)


_RELOC_TYPES = {2: "R_MIPS_32", 4: "R_MIPS_26", 5: "R_MIPS_HI16", 6: "R_MIPS_LO16"}


def _same_records(ours, elf_records):
    """Whether the readelf view of a relocation table equals the ELF reader's."""
    theirs = list(elf_records)
    return len(ours) == len(theirs) and all(
        offset == r_offset and name == sym["name"]
        and kind == _RELOC_TYPES.get(r_type, kind)
        for (offset, kind, name), (r_offset, r_type, sym) in zip(ours, theirs))


ONE_BASE_REFUSAL = ": this function's references disagree on the section's image address"


SECTION_PADDING = 16        # IDO aligns (and pads) its data sections to 16 bytes


def _independent_regions(obj, name, want, start, image, failures, data_runs, address=None):
    """{section name: [(object lo, hi, image address, "rodata")]} proving that
    `name`'s own read-only sections whose windows owndata refused for lack of
    one common base sit, window by window, at the retail addresses; else
    GroupError with the reason. `failures` are owndata's failures for the
    function (nothing may be unverified).

    Complete evidence, re-derived here from the object and the retail words:
      * owndata's only failures are that one-base refusal (every window it
        opened in the section verified by content);
      * each window is compared over its WHOLE extent [offset, stop), up to
        the next object any function of the object references, trailing
        zeros included, so no object byte a reference reaches rides along
        unchecked once the windows no longer sit side by side. Only the window
        that ends the section may leave trailing zeros uncompared: fewer than
        SECTION_PADDING bytes of IDO section padding (another owner's data in
        the image, exactly as owndata treats it);
      * every access through a window fits in it (8 bytes for ldc1/sdc1/ld/sd);
      * a window holds no relocated word, with ONE exception, the jump-table
        branch (`_own_jump_table_entries`, needs the function's image
        `address`): every relocated word inside a window is an aligned
        R_MIPS_32 whose target lies in this function's own text, and the
        window is compared AFTER each such word becomes `address + (target -
        function start)`. Any other relocation in the section, or an entry
        leaving the function, refuses; a relocated window that differs from
        retail is refused with its own message. Relocated words outside this
        function's windows (another member's table) are not compared here:
        that member's own proof covers them (func_8010AEAC);
      * each window lies inside one non-function run of the image (`data_runs`,
        the layout's opaque entries), and no two windows share image bytes.
    """
    sections = set()
    for failure in failures:
        head, sep, _rest = failure.partition(ONE_BASE_REFUSAL)
        if not sep or not head.startswith("own ") or head[4:] not in owndata.READ_ONLY:
            raise GroupError(f"not only the one-base refusal: {failure}")
        sections.add(head[4:])
    try:
        elf = owndata._Object(obj)
        text, lo, hi = owndata._locate(elf, name, start, len(want) * 4)
        refs, stray = owndata._references(elf, text)
    except (ValueError, struct.error, IndexError) as exc:
        raise GroupError(f"own data: {exc}") from exc
    if any(lo <= site < hi for site, _sym, _why in stray):
        raise GroupError("a stray own-section relocation in the function")
    runs = sorted(data_runs)
    out = {}
    for section_name in sorted(sections):
        index = [i for i, sec in enumerate(elf.sections) if sec["name"] == section_name]
        if len(index) != 1:
            raise GroupError(f"{section_name}: not exactly one such section")
        index = index[0]
        raw = elf.raw(index)
        if raw is None:
            raise GroupError(f"{section_name}: no bytes")
        entries = {}
        if elf.relocations(index):
            if address is None:
                raise GroupError(f"{section_name}: relocated words (a jump table) need the "
                                 "one-base or jump-table proof")
            entries = _own_jump_table_entries(elf, index, raw, text, section_name)
        starts = sorted({ref.offset for _sym, ref in refs if ref.section == index})
        windows = {}
        for _sym, ref in refs:
            if ref.section != index or not lo <= ref.lo_site < hi:
                continue
            if not all(lo <= site < hi for site in ref.hi_sites):
                raise GroupError(f"{section_name}+0x{ref.offset:x}: HI16 outside the function")
            retail = {((want[(site - lo) // 4] & 0xFFFF) << 16)
                      + owndata._sext16(want[(ref.lo_site - lo) // 4] & 0xFFFF)
                      for site in ref.hi_sites}
            if len(retail) != 1:
                raise GroupError(f"{section_name}+0x{ref.offset:x}: retail HI16 words disagree")
            read_at = retail.pop() & 0xFFFFFFFF
            seen = windows.setdefault(ref.offset, [read_at, 4])
            if seen[0] != read_at:
                raise GroupError(f"{section_name}+0x{ref.offset:x}: read at two retail "
                                 "addresses")
            if ref.opcode in owndata.DOUBLEWORD_OPCODES:
                seen[1] = 8
        placed = []
        for offset, (retail_at, widest) in sorted(windows.items()):
            if not 0 <= offset < len(raw):
                raise GroupError(f"{section_name}+0x{offset:x}: outside the section")
            stop = min([s for s in starts if s > offset] + [len(raw)])
            data, relocated = raw[offset:stop], 0
            if entries:
                data, relocated = _relocate_own_jump_table(
                    data, offset, entries, lo, hi, address, name, section_name)
            if len(data) < widest:
                raise GroupError(f"{section_name}+0x{offset:x}: {widest}-byte access, but the "
                                 f"next referenced object starts {len(data)} bytes later")
            if stop == len(raw):
                length = max(widest, (len(data.rstrip(b"\0")) + 3) & ~3)
                if len(data) - length >= SECTION_PADDING:
                    raise GroupError(f"{section_name}+0x{offset:x}: {len(data) - length} "
                                     "uncompared bytes at the section end are more than "
                                     "its padding")
                data = data[:min(length, len(data))]
            expected = image.read(retail_at, len(data))
            if expected is None:
                raise GroupError(f"{section_name}+0x{offset:x}: retail bytes "
                                 f"0x{retail_at:08X}+{len(data)} are not available")
            if expected != data:
                first = next(i for i in range(len(data)) if expected[i] != data[i])
                if relocated:
                    first &= ~3             # name the whole entry word
                    raise GroupError(
                        f"{section_name}+0x{offset:x}..+0x{offset + len(data):x} (jump-table "
                        f"window, {relocated} entries relocated) differs from retail "
                        f"0x{retail_at:08X} at +0x{first:x} after relocation (retail "
                        f"{expected[first:first + 4].hex()}, got {data[first:first + 4].hex()})")
                raise GroupError(
                    f"{section_name}+0x{offset:x}..+0x{offset + len(data):x} differs from "
                    f"retail 0x{retail_at:08X} at +0x{first:x} (retail "
                    f"{expected[first:first + 4].hex()}, got {data[first:first + 4].hex()}; "
                    "whole extent compared)")
            if not any(r_lo <= retail_at and retail_at + len(data) <= r_hi for r_lo, r_hi in runs):
                raise GroupError(f"{section_name}+0x{offset:x} at 0x{retail_at:08X}+{len(data)}: "
                                 "not inside one non-function run of the image")
            placed.append((offset, offset + len(data), retail_at, "rodata"))
        ordered = sorted(placed, key=lambda p: p[2])
        for a, b in zip(ordered, ordered[1:]):
            if b[2] < a[2] + a[1] - a[0]:
                raise GroupError(f"{section_name}+0x{a[0]:x} and {section_name}+0x{b[0]:x} "
                                 f"occupy the same image bytes at 0x{b[2]:08X}")
        out[section_name] = placed
    return out


def _own_jump_table_entries(elf, index, raw, text, section_name):
    """{section offset: object .text offset} of every relocated word in the
    read-only section `index`: each must be a word-aligned R_MIPS_32 against
    the function's text section (its section symbol or a function in it).
    Any other relocation refuses the independent-regions proof as before."""
    entries = {}
    for roff, rtype, rsym in elf.relocations(index):
        if rtype != owndata.R_MIPS_32 or roff % 4 or roff + 4 > len(raw):
            raise GroupError(f"{section_name}+0x{roff:x}: relocation type {rtype} is not a "
                             "jump-table entry (only aligned R_MIPS_32 words into the "
                             "function's own text are accepted)")
        if rsym["section"] != text:
            raise GroupError(f"{section_name}+0x{roff:x}: relocated word against "
                             f"{rsym['name'] or rsym['section']} is not a jump-table entry "
                             "into the function's text")
        addend, = struct.unpack_from(">I", raw, roff)
        entries[roff] = (rsym["value"] + addend) & 0xFFFFFFFF
    return entries


def _relocate_own_jump_table(data, offset, entries, lo, hi, address, name, section_name):
    """(window bytes with each jump-table entry relocated, count): an entry
    at window offset k becomes `address + (target - lo)`, exactly as the
    linker would write it once the function sits at its retail `address`.
    An entry whose target leaves [lo, hi) — this function's text — refuses."""
    data = bytearray(data)
    count = 0
    for roff, target in sorted(entries.items()):
        if not offset <= roff < offset + len(data):
            continue
        if roff + 4 > offset + len(data):
            raise GroupError(f"{section_name}+0x{roff:x}: jump-table entry straddles the "
                             "window end")
        if not lo <= target < hi:
            raise GroupError(f"{section_name}+0x{roff:x}: jump-table entry targets .text+0x"
                             f"{target:x}, outside {name} (.text+0x{lo:x}..+0x{hi:x})")
        struct.pack_into(">I", data, roff - offset, (address + target - lo) & 0xFFFFFFFF)
        count += 1
    return bytes(data), count


def verify_own_data(obj, name, want, address, image, start=None, addresses=None,
                    data_runs=None):
    """owndata.verify for a group member, then, with `data_runs`, the
    `_independent_regions` fallback for its one-base refusal. A fallback that
    is refused adds its reason to the failures; the result is never better
    than owndata's except for exactly those sections."""
    result = owndata.verify(obj, name, want, address=address, image=image, start=start,
                            addresses=addresses)
    if data_runs is None or not result.failures or result.unverified:
        return result
    try:
        placed = _independent_regions(obj, name, want, start, image, result.failures,
                                      data_runs, address=address)
    except GroupError as exc:
        result.failures.append(f"independent regions refused: {exc}")
        return result
    result.failures = []
    result.placements.update(placed)
    try:
        elf = owndata._Object(obj)
        text, lo, hi = owndata._locate(elf, name, start, len(want) * 4)
        refs, _stray = owndata._references(elf, text)
    except (ValueError, struct.error, IndexError) as exc:
        raise GroupError(f"own data: {exc}") from exc
    indices = {i for i, sec in enumerate(elf.sections) if sec["name"] in placed}
    for _sym, ref in refs:
        if ref.section in indices and lo <= ref.lo_site < hi:
            result.sites.update(ref.hi_sites + (ref.lo_site,))
    result.notes = owndata._notes(result.placements, result.zero)
    return result


def opaque_runs(document):
    """[(start, end)] of the layout's non-function runs (opaque or data entries).

    Adjacent entries (the image's .data then .rodata) form one run, as they
    did when the layout held the whole tail as a single opaque entry."""
    out = []
    for region in document["regions"]:
        for entry in region["entries"]:
            if entry["kind"] != "function":
                vaddr, size = entry["vaddr"], entry["size"]
                vaddr = int(vaddr, 16) if isinstance(vaddr, str) else vaddr
                size = int(size, 16) if isinstance(size, str) else size
                if out and out[-1][1] == vaddr:
                    out[-1] = (out[-1][0], vaddr + size)
                else:
                    out.append((vaddr, vaddr + size))
    return out


def _member_own_data(obj, members, slices, text, rels, image, addresses, data_runs=None):
    """{member: owndata.Result}: every own-data reference of every output
    member, checked by content against the image.

    Any reference that fails or cannot be checked refuses the group; the
    message names the member, the site, the retail address and both values.
    Functions that are not output (context, stand-ins) are not verified: their
    references only bound the members' windows.

    data_runs  [(start, end)] non-function image runs, or None. With them, a
               member whose own .rodata windows sit in more than one retail
               region is accepted when each whole window is proven there
               (`_independent_regions`); without them that is refused.
    """
    if image is None:
        raise GroupError("relocations against the unit's own data need the image")
    try:
        elf = owndata._Object(obj)
        text_index = [i for i, sec in enumerate(elf.sections) if sec["name"] == ".text"]
        # owndata reads the object itself. It must be looking at the very text
        # and relocation records this module relocates.
        if (len(text_index) != 1 or elf.raw(text_index[0]) != bytes(text)
                or not _same_records(rels, elf.relocations(text_index[0]))):
            raise GroupError("own data: the object's .text or its relocation records "
                             "differ between the two ELF readers")
    except (ValueError, struct.error, IndexError) as exc:
        raise GroupError(f"own data: {exc}") from exc
    data, base = image
    retail = owndata.ImageData.from_image(data, base)
    results = {}
    for member in members:
        off, vaddr, size = slices[member]
        if not base <= vaddr or vaddr + size > base + len(data):
            raise GroupError(f"{member}: extent is outside the image")
        body = data[vaddr - base:vaddr - base + size]
        want = list(struct.unpack(f">{size // 4}I", body))
        result = verify_own_data(obj, member, want, vaddr, retail, start=off,
                                 addresses=addresses, data_runs=data_runs)
        problems = result.failures + result.unverified
        if problems:
            raise GroupError(f"{member}: " + "; ".join(problems))
        results[member] = result
    return results


def _reference_windows(obj, section, wanted, results, section_rels, syms, in_member):
    """A `_ReferenceWindows` for one own section from the members' verified
    references (`results`, see `_member_own_data`).

    wanted        object offsets the members' relocation pairs name; each must
                  be the start of a verified window
    section_rels  the section's own relocations (jump-table entries)

    Refused: one object offset placed at two image addresses, two different
    object windows placed over the same image bytes, a referenced offset
    without a verified window, and a table entry that enters a member's text
    outside every verified window (a table cut short by a foreign reference).
    """
    try:
        elf = owndata._Object(obj)
        index = [i for i, sec in enumerate(elf.sections) if sec["name"] == section]
        if len(index) != 1 or not _same_records(section_rels, elf.relocations(index[0])):
            raise GroupError(f"{section}: its relocation records differ between the "
                             "two ELF readers")
    except (ValueError, struct.error, IndexError) as exc:
        raise GroupError(f"{section}: {exc}") from exc
    placed = {}                     # object offset -> [image address, end, member]
    for member, result in results.items():
        for lo, hi, address, _cls in result.placements.get(section, []):
            seen = placed.get(lo)
            if seen is None:
                placed[lo] = [address, hi, member]
            elif seen[0] != address:
                raise GroupError(
                    f"{section}+0x{lo:x} is placed at different image addresses: "
                    f"0x{seen[0]:08x} ({seen[2]}), 0x{address:08x} ({member})")
            else:
                seen[1] = max(seen[1], hi)
    for offset in sorted(wanted):
        if offset not in placed:
            raise GroupError(f"{section}+0x{offset:x} is referenced by a member but "
                             "was not verified against the image")
    spans = sorted((address, address + hi - lo, lo, member)
                   for lo, (address, hi, member) in placed.items())
    for (_a, end, lo, member), (address, _e, other_lo, other) in zip(spans, spans[1:]):
        if address < end:
            raise GroupError(
                f"{section}+0x{lo:x} ({member}) and {section}+0x{other_lo:x} ({other}) "
                f"are placed over the same image bytes at 0x{address:08x}")
    raw = _section_bytes(obj, section)
    text_ndx = _sections(obj)[".text"][0]
    for offset, _kind, name in section_rels:
        symbol = syms.get(name)
        if name != ".text" and (not symbol or symbol[1] != text_ndx):
            continue
        if offset % 4 or offset < 0 or offset + 4 > len(raw):
            raise GroupError(f"{section}: unaligned or out-of-section table relocation")
        target = struct.unpack(">I", raw[offset:offset + 4])[0]
        target += 0 if name == ".text" else symbol[0]
        if in_member(target) and not any(lo <= offset < hi
                                         for lo, (_a, hi, _m) in placed.items()):
            raise GroupError(f"{section}+0x{offset:x}: a table entry into member text "
                             f"(.text+0x{target:x}) is outside every verified reference")
    return _ReferenceWindows(section, {lo: address for lo, (address, _h, _m) in placed.items()})


def _jump_table_windows(obj, references, rels, syms, data_target, image_bytes):
    """Strict fallback for native .rodata tables linked into separate image windows.

    References are (object table offset, original image table address), derived
    from actual HI16/LO16 pairs. Every used object word must be a relocated
    table entry, and every resulting window must equal its original bytes.
    """
    raw = _section_bytes(obj, ".rodata")
    if not references or not rels or data_target is None:
        raise GroupError(".rodata: multiple placements need complete jump table evidence")
    starts = sorted({offset for offset, _ in references})
    if starts[0] != 0:
        raise GroupError(".rodata: unmapped data before first jump table")
    addresses = {}
    for offset, address in references:
        if offset % 4 or address % 4:
            raise GroupError(".rodata: unaligned jump table reference")
        if offset < 0 or offset + 4 > len(raw):
            raise GroupError(".rodata: jump table reference outside section")
        if offset in addresses and addresses[offset] != address:
            raise GroupError(".rodata: conflicting references to one jump table")
        addresses[offset] = address
    entries = {}
    text_ndx = _sections(obj)[".text"][0]
    for offset, kind, name in rels:
        if offset % 4 or offset < 0 or offset + 4 > len(raw):
            raise GroupError(".rodata: unaligned or out-of-window table relocation")
        if offset in entries:
            raise GroupError(".rodata: overlapping table relocations")
        if kind != "R_MIPS_32":
            raise GroupError(f".rodata: unsupported table relocation {kind}")
        symbol = syms.get(name)
        if name != ".text" and (not symbol or symbol[1] != text_ndx):
            raise GroupError(f".rodata: table entry {name} is not covered text")
        addend = struct.unpack(">I", raw[offset:offset + 4])[0]
        object_target = addend if name == ".text" else symbol[0] + addend
        if object_target % 4:
            raise GroupError(".rodata: unaligned table text target")
        target = data_target(name, addend)
        if target % 4:
            raise GroupError(".rodata: unaligned image table target")
        entries[offset] = struct.pack(">I", target & 0xFFFFFFFF)
    used = max(entries) + 4
    if set(entries) != set(range(0, used, 4)):
        raise GroupError(".rodata: unmapped data between jump table entries")
    if any(raw[used:]):
        raise GroupError(".rodata: nonzero unmapped data after jump tables")
    if starts[-1] >= used:
        raise GroupError(".rodata: jump table reference outside relocated windows")
    windows = []
    for i, lo in enumerate(starts):
        hi = starts[i + 1] if i + 1 < len(starts) else used
        address = addresses[lo]
        expected = b"".join(entries[o] for o in range(lo, hi, 4))
        if address + len(expected) > 0x100000000:
            raise GroupError(".rodata: image table window overflows address space")
        windows.append((lo, hi, address))
    for i, (lo, hi, address) in enumerate(windows):
        for other_lo, other_hi, other_address in windows[:i]:
            if max(address, other_address) < min(address + hi - lo,
                                                other_address + other_hi - other_lo):
                raise GroupError(".rodata: overlapping image jump table windows")
        expected = b"".join(entries[o] for o in range(lo, hi, 4))
        if image_bytes(address, hi - lo) != expected:
            raise GroupError(f".rodata: jump table window at {address:#010x} differs from image")
    return _TableWindows(windows)


def _local_data_bases(obj, rels, syms, in_member, image_word, image_bytes,
                      known=lambda name: None, relocated=None, data_target=None,
                      table_member=None, per_reference=None):
    """Verified placements for the unit's own data sections.

    Ordinary placements are one section base. Only after that existing proof
    refuses a .rodata section can complete native jump-table evidence select
    independent _TableWindows placements. All covered context pairs then count.

    Retail lays .rodata out per function, so members that are not address
    neighbours have their literals and tables at unrelated image addresses and
    no whole-section proof can hold. When both proofs above refuse a section,
    `per_reference(section, offsets, refusal)` (see `relocate`) may place it
    reference by reference instead: `offsets` are the object offsets the
    output members reference. Without it the refusal stands.

    A function-local static lives in the unit's .data, so its relocations name
    a section of this object, not an image symbol. The section's image address
    is derived from the retail HI16/LO16 words at each relocation site; every
    site must agree on one base, and the section's bytes must equal the image
    there. Trailing zero bytes beyond the last referenced word are IDO's
    alignment padding (the linked program placed the next unit's data there)
    and are not compared. .bss/.sbss (no bytes) are always placed per
    reference through `per_reference`, which applies owndata's address-only
    rules (one address per object, inside GAME_BSS, no overlaps); without it
    they are refused, as are other sections.

    A symbol with a known image address (`known(name)`, e.g. D_80149B64
    defined by the unit) resolves by name like any extern, wherever the unit
    defines it; the image comparison of the member bodies checks it.

    A section can carry relocations of its own (a switch's jump table in
    .rodata: R_MIPS_32 against .text). `relocated` maps such a section to its
    entries and `data_target(name, addend)` gives their image addresses; the
    section is compared with the image after they are applied.
    """
    relocated = relocated or {}
    secs = _sections(obj)
    by_index = {ndx: name for name, (ndx, _, _) in secs.items()}

    def section_of(name):
        if name in secs:
            return name, 0
        sym = syms.get(name)
        if sym and sym[1] in by_index and known(name) is None:
            return by_index[sym[1]], sym[0]
        return None, 0

    text = _text(obj)
    word = lambda o: struct.unpack(">I", text[o:o + 4])[0]
    candidates, reach, pending, wanted, shared = {}, {}, [], {}, {}
    for offset, rtype, name in rels:
        if not in_member(offset):
            continue
        section, value = section_of(name)
        if section is None or section == ".text":
            continue
        if not ((section in LOCAL_DATA and secs[section][1] == "PROGBITS")
                or (section in LOCAL_BSS and secs[section][1] == "NOBITS")):
            raise GroupError(f"relocation against {section} ({secs[section][1]}) "
                             "is not supported")
        if rtype == "R_MIPS_HI16":
            pending.append((offset, name))
        elif rtype == "R_MIPS_LO16":
            # IDO loads several words through one lui: a LO16 without a HI16 of
            # its own pairs with its symbol's previous one (the MIPS ABI rule).
            his = [h for h, n in pending if n == name] or shared.get(name, [])
            if not his:
                raise GroupError(f"{section} LO16 at .text+0x{offset:x} has no HI16")
            shared[name] = his
            pending = [(h, n) for h, n in pending if n != name]
            ours = value + ((word(his[0]) & 0xFFFF) << 16) + _sext16(word(offset) & 0xFFFF)
            for h in his:
                retail = ((image_word(h) & 0xFFFF) << 16) + _sext16(image_word(offset) & 0xFFFF)
                candidates.setdefault(section, set()).add((retail - ours) & 0xFFFFFFFF)
            reach[section] = max(reach.get(section, 0), ours + 4)
            wanted.setdefault(section, set()).add(ours)
        else:
            raise GroupError(f"unsupported {rtype} against {section} at .text+0x{offset:x}")
    def whole_section(section, found):
        # Try the original contiguous-section proof first. In particular, a
        # successful old path never examines a context's own partial pairs.
        if len(found) == 1:
            base = next(iter(found))
            data = bytearray(_section_bytes(obj, section))
            for off, rtype, name in relocated.get(section, []):
                if rtype != "R_MIPS_32" or data_target is None:
                    raise GroupError(f"{section}: {rtype} inside the section is not supported")
                addend = struct.unpack(">I", data[off:off + 4])[0]
                data[off:off + 4] = struct.pack(">I", data_target(name, addend) & 0xFFFFFFFF)
            data = bytes(data)
            used = max(reach[section], len(data.rstrip(b"\0")))
            used = min((used + 3) & ~3, len(data))
            if image_bytes(base, used) == data[:used]:
                return base
            refusal = GroupError(f"{section}: {used} bytes at 0x{base:08x} differ from the image")
        else:
            refusal = GroupError(f"{section}: relocation sites disagree on its image address "
                                 f"{sorted(hex(b) for b in found)}")
        if section != ".rodata" or table_member is None:
            raise refusal
        references, table_pending, table_sites = [], [], set()
        for offset, kind, name in rels:
            own, value = section_of(name)
            if own != section:
                continue
            if not table_member(offset):
                raise GroupError(".rodata: reference outside covered text in multi-window group")
            if offset % 4 or offset < 0 or offset + 4 > len(text):
                raise GroupError(".rodata: unaligned or invalid text relocation site")
            if offset in table_sites:
                raise GroupError(".rodata: duplicate text relocation site")
            table_sites.add(offset)
            if kind == "R_MIPS_HI16":
                table_pending.append((offset, name))
            elif kind == "R_MIPS_LO16":
                his = [h for h, n in table_pending if n == name]
                if not his:
                    raise GroupError(".rodata LO16 has no paired HI16")
                table_pending = [(h, n) for h, n in table_pending if n != name]
                for h in his:
                    ours = value + ((word(h) & 0xFFFF) << 16) + _sext16(word(offset) & 0xFFFF)
                    retail = ((image_word(h) & 0xFFFF) << 16) + _sext16(image_word(offset) & 0xFFFF)
                    references.append((ours, retail & 0xFFFFFFFF))
            else:
                raise GroupError(f"unsupported {kind} against .rodata")
        if table_pending:
            raise GroupError(".rodata has unpaired HI16 references")
        if len({(address - offset) & 0xFFFFFFFF
                for offset, address in references}) <= 1:
            raise refusal
        return _jump_table_windows(
            obj, references, relocated.get(section, []), syms,
            data_target, image_bytes)

    bases = {}
    for section, found in candidates.items():
        if section in LOCAL_BSS:
            # Nothing to compare by content, so no whole-section proof exists:
            # every object is placed where retail's words put it, after the
            # address-only checks
            refusal = GroupError(f"{section} is zero-initialised: it can only be placed "
                                 "per reference")
            if per_reference is None:
                raise refusal
            bases[section] = per_reference(section, wanted[section], refusal)
            continue
        try:
            bases[section] = whole_section(section, found)
        except GroupError as refusal:
            if per_reference is None:
                raise
            bases[section] = per_reference(section, wanted[section], refusal)
    return bases


def _symbols(obj):
    """{name: (value, section_index or 'UND')} from the symbol table."""
    out = subprocess.run([READELF, "-sW", str(obj)], capture_output=True,
                         text=True, check=True).stdout
    syms = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 8 and parts[0].rstrip(":").isdigit():
            syms[parts[7]] = (int(parts[1], 16), parts[6])
    return syms


def _text_relocations(obj):
    out = subprocess.run([READELF, "-rW", str(obj)], capture_output=True,
                         text=True, check=True).stdout
    rels, section, others = [], None, {}
    for line in out.splitlines():
        m = re.match(r"Relocation section '(\.rel\S+)'", line)
        if m:
            section = m.group(1)
            continue
        m = re.match(r"\s*([0-9a-f]{8})\s+[0-9a-f]{8}\s+(R_MIPS_\w+)\s+[0-9a-f]{8}\s+(\S+)", line)
        if not m:
            continue
        if section == ".rel.text":
            rels.append((int(m.group(1), 16), m.group(2), m.group(3)))
        else:
            others.setdefault(section, []).append(
                (int(m.group(1), 16), m.group(2), m.group(3)))
    return rels, others


def member_slices(obj, members, extents):
    """{member: (object offset, image vaddr, size)} located by symbol."""
    syms = _symbols(obj)
    text_ndx = None
    slices = {}
    for member in members:
        if member not in syms or syms[member][1] == "UND":
            raise GroupError(f"member {member} is not defined in the group object")
        entry = extents.get(member)
        if entry is None:
            raise GroupError(f"member {member} has no extent in the layout")
        value, ndx = syms[member]
        text_ndx = text_ndx or ndx
        if ndx != text_ndx:
            raise GroupError(f"member {member} is not in .text")
        slices[member] = (value, entry["vaddr"], entry["size"])
    return slices, text_ndx


def relocate(obj, slices, text_ndx, extern, members=None, image=None, data_runs=None,
             frozen=None, capture=None):
    """Bytes of each member slice with its .text relocations applied at the
    member's image address. Supports R_MIPS_26 and REL HI16/LO16 pairs;
    anything else (and any relocation outside .text) is refused. HI16/LO16
    pairs against the unit's own data sections need `image` (bytes, base
    address); see `_local_data_bases`.

    Own data is placed as one section base where that is provable (members
    that are neighbours in the image). Otherwise every own-data reference of
    every output member is verified by content at the address the retail
    words encode at its site and relocated to exactly
    that address. Functions that are not output (context, stand-ins) are not
    verified then; their references cannot refuse the members. One member's
    own .rodata in several retail regions additionally needs `data_runs`
    (the image's non-function runs, see `opaque_runs`); see
    `_member_own_data`.

    Check body lengths for members being spliced. Other slices supply context
    addresses only; those functions need not match their retail bodies.
    """
    text = _text(obj)
    syms = _symbols(obj)
    try:
        functions = blob_build.function_symbols(obj)
    except blob_build.BuildError as exc:
        raise GroupError(str(exc)) from exc
    for member in (slices if members is None else members):
        off, _, size = slices[member]
        if functions.get(member) != (off, text_ndx):
            raise GroupError(f"{member}: no function symbol at its slice start")
        end = min((value for value, ndx in functions.values()
                   if ndx == text_ndx and value > off), default=len(text))
        end = min(end, len(text))
        if off % 4 or size % 4 or end % 4:
            raise GroupError(f"{member}: function extent is not word aligned")
        if end - off < size:
            raise GroupError(f"{member}: compiled body is {end - off} bytes, "
                             f"shorter than the {size}-byte extent")
        extra = sum(any(text[o:o + 4]) for o in range(off + size, end, 4))
        if extra:
            raise GroupError(f"{member}: {extra} extra words (nonzero beyond target length)")
    rels, others = _text_relocations(obj)
    # Context can compile shorter than its retail extent. Its address map
    # must stop at the next compiled function, or a call into that function
    # is incorrectly attributed to the preceding context (or stand-in).
    starts = sorted({value for value, ndx in functions.values() if ndx == text_ndx})
    covered = sorted((off, min(off + size,
                              next((start for start in starts if start > off), len(text))),
                      vaddr) for off, vaddr, size in slices.values())

    def text_addr(offset):
        for lo, hi, vaddr in covered:
            if lo <= offset < hi:
                return vaddr + (offset - lo)
        raise GroupError(f"relocation targets .text+0x{offset:x}, which no member "
                         "slice covers (a call into a stand-in?)")

    applied = [slices[m] for m in (slices if members is None else members)]
    in_member = lambda o: any(lo <= o < lo + size for lo, _, size in applied)

    def image_at(vaddr, n):
        if image is None:
            raise GroupError("relocations against the unit's own data need the image")
        data, base = image
        return data[vaddr - base:vaddr - base + n]

    def image_word(offset):
        return struct.unpack(">I", image_at(text_addr(offset), 4))[0]

    def known(name):
        if name in extern:
            return extern[name]
        return blob_splice.address_named(name)

    def data_target(name, addend):
        sym = syms.get(name)
        if name == ".text":
            return text_addr(addend)
        if sym and sym[1] == text_ndx:
            return text_addr(sym[0] + addend)
        addr = known(name)
        if addr is None:
            raise GroupError(f"jump table entry names {name}, which has no image address")
        return addr + addend

    relocated = {o[len(".rel"):]: v for o, v in others.items()}
    output = list(slices if members is None else members)
    verified = []                   # the members' owndata results, computed once

    def function_address(name):
        return slices[name][1] if name in slices else known(name)

    def per_reference(section, offsets, refusal):
        """Place `section` reference by reference after the whole-section
        proofs refused it. A failure here reports both reasons."""
        try:
            if not verified:
                verified.append(_member_own_data(obj, output, slices, text, rels, image,
                                                 function_address, data_runs))
            return _reference_windows(obj, section, offsets, verified[0],
                                      relocated.get(section, []), syms, in_member)
        except GroupError as exc:
            raise GroupError(f"{exc} [per-reference placement tried because the "
                             f"whole-section placement was refused: {refusal}]") from refusal

    # frozen: own-data placements recorded in the manifest (no retail code is read);
    # capture: where to record them when they are derived from the retail image.
    local = {k: _thaw(k, v) for k, v in frozen.items()} if frozen is not None else \
        _local_data_bases(obj, rels, syms, in_member, image_word, image_at, known,
                              relocated=relocated,
                              data_target=data_target,
                              table_member=lambda o: any(lo <= o < hi for lo, hi, _ in covered),
                              per_reference=per_reference)
    if capture is not None:
        capture.update({k: _freeze(v) for k, v in local.items()})
    sec_index = {ndx: name for name, (ndx, _, _) in _sections(obj).items()
                 if name in local}

    def resolve(name, addend):
        sym = syms.get(name)
        if name == ".text":
            return text_addr(addend)
        if name in local:
            return _local_address(local[name], addend)
        if sym and sym[1] == text_ndx:
            return text_addr(sym[0] + addend)
        addr = known(name)
        if addr is not None:
            return addr + addend
        if sym and sym[1] in sec_index:
            return _local_address(local[sec_index[sym[1]]], sym[0] + addend)
        raise GroupError(f"unresolved symbol {name}")

    def word(o):
        return struct.unpack(">I", text[o:o + 4])[0]

    def put(o, w):
        text[o:o + 4] = struct.pack(">I", w & 0xFFFFFFFF)

    # Relocations are applied only inside the slices being output. Context
    # slices map call targets to image addresses, but their bodies are never
    # spliced and may be longer than their extent (a HI16/LO16 pair can
    # straddle the cut), so their own relocations are not touched.
    pending_hi, shared_hi = [], {}
    for offset, rtype, name in rels:
        if not in_member(offset):
            continue            # a stand-in's own relocation: not image bytes
        insn = word(offset)
        if rtype == "R_MIPS_26":
            target = resolve(name, (insn & 0x03FFFFFF) << 2)
            put(offset, (insn & 0xFC000000) | ((target >> 2) & 0x03FFFFFF))
        elif rtype == "R_MIPS_HI16":
            pending_hi.append((offset, name, insn))
        elif rtype == "R_MIPS_LO16":
            lo = struct.unpack(">h", struct.pack(">H", insn & 0xFFFF))[0]
            his = [h for h in pending_hi if h[1] == name]
            # a LO16 sharing an earlier lui takes that HI16's addend (see _local_data_bases)
            hi_imm = (his[0][2] & 0xFFFF) if his else shared_hi.get(name, 0)
            if his:
                shared_hi[name] = hi_imm
            value = resolve(name, (hi_imm << 16) + lo)
            for h_off, _, h_insn in his:
                put(h_off, (h_insn & 0xFFFF0000) | (((value + 0x8000) >> 16) & 0xFFFF))
            pending_hi = [h for h in pending_hi if h[1] != name]
            put(offset, (insn & 0xFFFF0000) | (value & 0xFFFF))
        else:
            raise GroupError(f"unsupported relocation {rtype} at .text+0x{offset:x}")
    if pending_hi:
        raise GroupError(f"unpaired R_MIPS_HI16 at {[hex(h[0]) for h in pending_hi]}")
    return {member: bytes(text[off:off + size])
            for member, (off, vaddr, size) in slices.items()}
