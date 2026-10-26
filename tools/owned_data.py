#!/usr/bin/env python3
"""Owned ROM data slots: split the extracted data container around the slots
that compiled C now owns, and trim logical table extents of ROM TU objects.

    owned_data.py split SOURCE COMPOSED OUTPUT [--objcopy ...]
    owned_data.py object TU OBJECT

Distilled from the project's ROM-composition tooling; the definitions below are
copied verbatim. The registry is rom_owned_data.json.
"""
import argparse
import hashlib
import importlib.util
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]


REGISTRY = 'rom_owned_data.json'


DATA_ROM_START = 0x283D0


SOURCE_BUILT_GAME_SLOT = 0xAE4740


SOURCE_BUILT_GAME_LENGTH = 326180


IDENT = re.compile(r'[A-Za-z_]\w*\Z')


class OwnershipError(ValueError):
    pass


LOGICAL_KINDS = ('table', 'constants')


def _relative(value):
    if not isinstance(value, (str, Path)) or not re.fullmatch(r'[A-Za-z0-9_./-]+', str(value)):
        raise OwnershipError('unsafe ownership path characters')
    p = Path(value)
    if p.is_absolute() or '..' in p.parts or not p.parts:
        raise OwnershipError('ownership paths must be repository-relative')
    return p.as_posix()


def load_registry(repo=REPO):
    p = Path(repo) / REGISTRY
    if not p.exists():
        return {'schema': 1, 'rom_slots': [], 'storage_blocks': []}
    try:
        data = json.loads(p.read_text())
    except (OSError, ValueError) as exc:
        raise OwnershipError('unreadable ownership registry') from exc
    if not isinstance(data, dict):
        raise OwnershipError('ownership registry must be an object')
    if data.get('schema') != 1:
        raise OwnershipError('unsupported ownership registry schema')
    for k in ['rom_slots', 'storage_blocks']:
        if not isinstance(data.get(k, []), list):
            raise OwnershipError(k + ' must be a list')
        data.setdefault(k, [])
    return data


def rom_slots(repo=REPO, *, check_source=True):
    repo = Path(repo)
    result, owners, sections, containers = [], set(), set(), {}
    registry = load_registry(repo)
    bss = []
    for raw in registry['rom_slots']:
        if isinstance(raw, dict) and raw.get('owner_section') == BSS:
            r = _bss_slot(repo, raw)
            section = (r['owner_object'], r['owner_section'])
            if r['owner'] in owners or section in sections:
                raise OwnershipError('duplicate owner or input section')
            owners.add(r['owner']); sections.add(section)
            bss.append(r); result.append(r)
            continue
        required = {'owner', 'tu', 'source', 'offset', 'size', 'container_vram', 'sha256'}
        if isinstance(raw, dict) and raw.get('storage_owner'):
            storage_owners = [v for v in load_registry(repo)['storage_blocks'] if isinstance(v, dict) and v.get('owner') == raw['storage_owner']]
            if len(storage_owners) != 1 or not storage_owners[0].get('active') or storage_owners[0].get('activation_mode') != 'existing_tu' or storage_owners[0].get('data_slot') != raw:
                raise OwnershipError('mutable slot requires its active reviewed complete storage owner')
        else:
            required.add('passthrough_asm')
        if not isinstance(raw, dict) or not required.issubset(raw):
            raise OwnershipError('missing required ROM owner metadata')
        r = dict(raw)
        if not isinstance(r['owner'], str) or not IDENT.fullmatch(r['owner']):
            raise OwnershipError('invalid ROM owner name')
        # Other functions of the same TU whose bodies also emit into the owned
        # section (__fixunssfdi beside __fixunsdfdi). The slot changes hands
        # only when the owner and every co-owner are promoted together.
        co = r.get('co_owners', [])
        if (not isinstance(co, list) or any(not isinstance(n, str) or not IDENT.fullmatch(n) for n in co)
                or len(set(co)) != len(co) or r['owner'] in co):
            raise OwnershipError('co_owners must be distinct function names other than the owner')
        if co and r.get('storage_owner'):
            raise OwnershipError('mutable storage slots have no co-owners')
        r['co_owners'] = list(co)
        for k in ['tu', 'source'] + ([] if r.get('storage_owner') else ['passthrough_asm']):
            r[k] = _relative(r[k])
            if not (repo / r[k]).resolve().is_relative_to(repo.resolve()):
                raise OwnershipError('ownership path resolves outside repository')
        if not r['tu'].startswith('src/rom/') or not r['tu'].endswith('.c'):
            raise OwnershipError('ROM owner must identify a ROM TU')
        if not r['source'].startswith('assets/') or not r['source'].endswith('.bin'):
            raise OwnershipError('ROM container must identify an extracted binary asset')
        if r.get('composed_source') is not None:
            r['composed_source'] = _relative(r['composed_source'])
            if not (repo / r['composed_source']).resolve().is_relative_to(repo.resolve()):
                raise OwnershipError('composed input path resolves outside repository')
        r['owner_object'] = 'build/us/' + str(Path(r['tu']).with_suffix('.o'))
        r['container_object'] = 'build/us/' + str(Path(r['source']).with_suffix('.o'))
        r.setdefault('container_section', '.data')
        r.setdefault('owner_section', '.rodata')
        for k in ['container_section', 'owner_section']:
            if not isinstance(r[k], str) or not re.fullmatch(r'\.(?:data|rodata)(?:\.[A-Za-z_]\w*)*', r[k]):
                raise OwnershipError('only owned data/readonly sections are supported')
        for k in ['offset', 'size', 'container_vram']:
            if isinstance(r.get(k), str):
                try:
                    r[k] = int(r[k], 0)
                except ValueError as exc:
                    raise OwnershipError('invalid ownership integer') from exc
            if type(r.get(k)) is not int or r[k] < 0:
                raise OwnershipError('nonnegative integer required for ' + k)
        alignment = r.get('alignment', 16)
        logical = r.get('logical_extent', False)
        if type(alignment) is not int or alignment not in (4, 16) or type(logical) is not bool:
            raise OwnershipError('owned alignment must be 4 or 16, logical extent must be boolean')
        if alignment != 16 and not logical:
            raise OwnershipError('4-byte ownership requires an explicit logical table extent')
        if logical and (alignment != 4 or r['owner_section'] != '.rodata' or r.get('storage_owner')):
            raise OwnershipError('logical extents support only complete 4-byte readonly tables')
        # A logical extent is either a word-relocated jump table (default) or,
        # with logical_kind "constants", relocation-free literal words (a
        # float constant the TU's other readonly words follow).
        if 'logical_kind' in r and (not logical or r['logical_kind'] not in LOGICAL_KINDS):
            raise OwnershipError('logical_kind must be "table" or "constants" on a logical extent')
        if logical:
            r['logical_kind'] = r.get('logical_kind', 'table')
        if not r['size'] or r['offset'] % alignment or r['size'] % alignment:
            raise OwnershipError('owned slots must occupy complete aligned sections')
        if logical and (r['container_vram'] + r['offset']) % alignment:
            raise OwnershipError('logical table address must satisfy its actual alignment')
        if r['container_vram'] + r['offset'] + r['size'] > 0x100000000:
            raise OwnershipError('owned slot address overflows MIPS image')
        if not isinstance(r['sha256'], str) or not re.fullmatch(r'[0-9a-f]{64}', r['sha256']):
            raise OwnershipError('owned slot requires exact original SHA-256')
        section = (r['owner_object'], r['owner_section'])
        if r['owner'] in owners or section in sections:
            raise OwnershipError('duplicate owner or input section')
        owners.add(r['owner']); sections.add(section)
        group = containers.setdefault(r['source'], [])
        if group and (group[0]['container_section'], group[0]['container_vram']) != (r['container_section'], r['container_vram']):
            raise OwnershipError('inconsistent container ownership')
        # This region is already produced from game C sources by blob_rom;
        # ownership cannot replace any part of that compressed source image.
        if r['source'] == 'assets/us/data.bin':
            if r['offset'] < SOURCE_BUILT_GAME_SLOT + SOURCE_BUILT_GAME_LENGTH and SOURCE_BUILT_GAME_SLOT < r['offset'] + r['size']:
                raise OwnershipError('owned slot overlaps source-built compressed game blob')
        group.append(r); result.append(r)
    for source, rows in containers.items():
        rows.sort(key=lambda r: r['offset'])
        for a, b in zip(rows, rows[1:]):
            if a['offset'] + a['size'] > b['offset']:
                raise OwnershipError('owned slots overlap')
        if check_source:
            p = repo / source
            if not p.resolve().is_relative_to(repo.resolve()):
                raise OwnershipError('source resolves outside repository')
            try:
                data = p.read_bytes()
            except OSError as exc:
                raise OwnershipError('owned source container is missing or unreadable') from exc
            for r in rows:
                if r['offset'] + r['size'] > len(data):
                    raise OwnershipError('owned slot exceeds source bounds')
                if hashlib.sha256(data[r['offset']:r['offset'] + r['size']]).hexdigest() != r['sha256']:
                    raise OwnershipError('owned slot original-byte identity changed')
    _bss_overlaps(bss, registry['storage_blocks'])
    return sorted(result, key=lambda r: (r['source'] or '', r['container_vram'] + r['offset']))


BSS = '.bss'


BSS_FIELDS = {'owner', 'co_owners', 'tu', 'owner_section', 'container_section', 'container_vram',
              'offset', 'size', 'alignment', 'passthrough_asm', 'comment'}


def _bss_slot(repo, raw):
    if not {'owner', 'tu', 'container_vram', 'offset', 'size', 'passthrough_asm'}.issubset(raw):
        raise OwnershipError('missing required .bss slot metadata')
    extra = set(raw) - BSS_FIELDS
    if extra:
        raise OwnershipError('.bss slot has no ROM container fields: ' + ', '.join(sorted(extra)))
    r = dict(raw)
    if not isinstance(r['owner'], str) or not IDENT.fullmatch(r['owner']):
        raise OwnershipError('invalid ROM owner name')
    co = r.get('co_owners', [])
    if (not isinstance(co, list) or any(not isinstance(n, str) or not IDENT.fullmatch(n) for n in co)
            or len(set(co)) != len(co) or r['owner'] in co):
        raise OwnershipError('co_owners must be distinct function names other than the owner')
    r['co_owners'] = list(co)
    for k in ('tu', 'passthrough_asm'):
        r[k] = _relative(r[k])
        if not (Path(repo) / r[k]).resolve().is_relative_to(Path(repo).resolve()):
            raise OwnershipError('ownership path resolves outside repository')
    if not r['tu'].startswith('src/rom/') or not r['tu'].endswith('.c'):
        raise OwnershipError('ROM owner must identify a ROM TU')
    if r.setdefault('container_section', BSS) != BSS:
        raise OwnershipError('a .bss slot is its own container')
    for k in ['offset', 'size', 'container_vram']:
        if isinstance(r.get(k), str):
            try:
                r[k] = int(r[k], 0)
            except ValueError as exc:
                raise OwnershipError('invalid ownership integer') from exc
        if type(r.get(k)) is not int or r[k] < 0:
            raise OwnershipError('nonnegative integer required for ' + k)
    if r.setdefault('alignment', 16) != 16:
        raise OwnershipError('.bss slots own complete 16-byte aligned sections')
    if r['offset'] != 0:
        raise OwnershipError('a .bss slot starts at its own address (offset 0)')
    if not r['size'] or r['size'] % 16 or r['container_vram'] % 16:
        raise OwnershipError('owned slots must occupy complete aligned sections')
    if not 0x80000000 <= r['container_vram'] or r['container_vram'] + r['size'] > 0x100000000:
        raise OwnershipError('.bss slot must lie in KSEG0/KSEG1 RAM')
    r['source'] = None
    r['owner_object'] = 'build/us/' + str(Path(r['tu']).with_suffix('.o'))
    r['container_object'] = None
    return r


def _bss_overlaps(bss, storage_blocks):
    ranges = [(r['container_vram'], r['container_vram'] + r['size'], r['owner']) for r in bss]
    for row in storage_blocks:
        if isinstance(row, dict) and 'vram_start' in row and 'size' in row:
            start = int(row['vram_start'], 0) if isinstance(row['vram_start'], str) else row['vram_start']
            size = int(row['size'], 0) if isinstance(row['size'], str) else row['size']
            ranges.append((start, start + size, row.get('owner')))
    ranges.sort()
    for a, b in zip(ranges, ranges[1:]):
        if a[1] > b[0]:
            raise OwnershipError('owned .bss slot overlaps another storage block: %s, %s' % (a[2], b[2]))
    tus = [r['tu'] for r in bss]
    if len(set(tus)) != len(tus) or any(isinstance(s, dict) and s.get('tu') in tus for s in storage_blocks):
        raise OwnershipError('a TU has one owned .bss block')


def is_bss(row):
    return row.get('owner_section') == BSS


def _groups(repo):
    groups = {}
    for r in rom_slots(repo):
        if is_bss(r):
            continue
        groups.setdefault(r['source'], []).append(r)
    return groups


def _elf_processor(repo):
    """Use the existing project MIPS ELF reader/writer, including debug fixups."""
    path = Path(repo) / 'tools/asm-processor/asm_processor.py'
    spec = importlib.util.spec_from_file_location('_owned_data_elf', path)
    if spec is None or spec.loader is None:
        raise OwnershipError('missing project ELF processor')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _relocation_symbol(obj, relocation):
    if not 0 <= relocation.sym_index < len(obj.symtab.symbol_entries):
        raise OwnershipError('relocation symbol index is outside the symbol table')
    return obj.symtab.symbol_entries[relocation.sym_index]


def _logical_table(obj, section, size, elf, kind='table'):
    """Validate a complete word-relocated readonly table before trimming its tail.

    kind "table" is the proven 4-byte jump-table case: every retained word is a
    relocated pointer into source-built text. kind "constants" is a
    readonly section of literal words with no relocations at all (IDO pads a
    lone float literal to 16 bytes; the TU's other readonly words follow it in
    retail). It changes no instruction, table word, relocation, meaningful
    symbol, section index or symbol index.
    """
    alignment = section.sh_addralign
    if (section.name != '.rodata' or section.sh_type != elf.SHT_PROGBITS or
            section.sh_flags != elf.SHF_ALLOC or section.sh_addr != 0 or
            alignment not in (4, 16) or size <= 0 or size % 4 or
            len(section.data) != section.sh_size or
            section.sh_size != (size + alignment - 1) // alignment * alignment):
        raise OwnershipError('malformed logical table geometry or section kind')
    if any(section.data[size:]):
        raise OwnershipError('excluded section tail contains nonzero bytes')
    table_relocations = []
    if kind == 'constants' and any(reltab.relocations for reltab in section.relocated_by):
        raise OwnershipError('logical constants extent must carry no relocations')
    for reltab in section.relocated_by:
        for rel in reltab.relocations:
            if rel.r_offset % 4 or rel.r_offset < 0 or rel.r_offset + 4 > size:
                raise OwnershipError('relocation falls in excluded table extent')
            if reltab.sh_type != elf.SHT_REL or rel.rel_type != elf.R_MIPS_32:
                raise OwnershipError('logical table requires actual 32-bit word relocations')
            symbol = _relocation_symbol(obj, rel)
            if not 0 < symbol.st_shndx < len(obj.sections):
                raise OwnershipError('logical table entry has no source-built text target')
            target = obj.sections[symbol.st_shndx]
            addend = obj.fmt.unpack('I', section.data[rel.r_offset:rel.r_offset + 4])[0]
            address = symbol.st_value + addend
            if target.name != '.text' or address % 4 or not 0 <= address < len(target.data):
                raise OwnershipError('logical table entry points outside source-built text')
            table_relocations.append(rel.r_offset)
    if kind == 'table' and sorted(table_relocations) != list(range(0, size, 4)):
        raise OwnershipError('logical table must retain every complete relocated word once')
    descriptive = []
    for symbol in obj.symtab.symbol_entries:
        if symbol.st_shndx != section.index:
            continue
        if symbol.type == elf.STT_SECTION:
            if (symbol.bind != elf.STB_LOCAL or symbol.st_value != 0 or
                    symbol.st_size not in (0, section.sh_size, size)):
                raise OwnershipError('malformed descriptive section symbol')
            descriptive.append(symbol)
        elif symbol.st_value >= size or symbol.st_value + symbol.st_size > size:
            raise OwnershipError('meaningful symbol falls in excluded table extent')

    # An address relocation elsewhere can refer to the section plus an addend.
    # Retain all such records, but reject a pointer to an excluded table byte.
    for reltab in obj.sections:
        if not reltab.is_rel():
            continue
        pending_hi = {}
        for rel in reltab.relocations:
            symbol = _relocation_symbol(obj, rel)
            if symbol.st_shndx != section.index or rel.rel_type == 0:
                continue
            def check(addend):
                if not 0 <= symbol.st_value + addend < size:
                    raise OwnershipError('address relocation refers to excluded table extent')
            if reltab.sh_type == elf.SHT_RELA:
                check(rel.r_addend)
                continue
            payload = reltab.rel_target.data
            if rel.r_offset % 4 or rel.r_offset + 4 > len(payload):
                raise OwnershipError('malformed table address relocation')
            word = obj.fmt.unpack('I', payload[rel.r_offset:rel.r_offset + 4])[0]
            if rel.rel_type == elf.R_MIPS_32:
                check(word)
            elif rel.rel_type == elf.R_MIPS_HI16:
                pending_hi.setdefault(rel.sym_index, []).append(word & 0xffff)
            elif rel.rel_type == elf.R_MIPS_LO16:
                low = (word & 0xffff) - (0x10000 if word & 0x8000 else 0)
                for high in pending_hi.pop(rel.sym_index, [0]):
                    check((high << 16) + low)
            else:
                raise OwnershipError('unsupported address relocation to logical table')
        if pending_hi:
            raise OwnershipError('unpaired high relocation to logical table')
    return descriptive


_MDEBUG_OFFSET_FIELDS = (4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24)


def _mdebug_relocator(section, fmt):
    """asm-processor shifts every HDRR offset when .mdebug moves, including the
    zero offsets of absent tables (ioptMax/crfd = 0). Trimming a logical extent
    moves a later .mdebug BACKWARDS (lib_1cf90: .rodata 16 -> 4, .options
    re-aligns, .mdebug -8), so 0 - 8 overflows the unsigned pack. Shift only the
    present (nonzero) offsets; an absent table stays 0."""
    def relocate(original_offset):
        shift = section.sh_offset - original_offset
        header = list(fmt.unpack('HHIIIIIIIIIIIIIIIIIIIIIII', section.data[0:0x60]))
        if header[0] != 0x7009:
            raise OwnershipError('invalid .mdebug symbolic header')
        for i in _MDEBUG_OFFSET_FIELDS:
            if header[i]:
                header[i] += shift
                if not 0 < header[i] < 1 << 32:
                    raise OwnershipError('.mdebug offset relocation out of range')
        section.data = fmt.pack('HHIIIIIIIIIIIIIIIIIIIIIII', *header) + section.data[0x60:]
    return relocate


def normalize_object_sections(repo, tu, object_path):
    """Apply explicit logical table extents after normal asm post-processing.

    Ordinary existing 16-byte ownership rows are byte-for-byte no-ops. Extent
    validation precedes any write; serialization uses the existing processor
    and an atomic replace. This is a build operation, never a scoring mask.
    """
    repo = Path(repo)
    tu = _relative(tu)
    rows = [r for r in rom_slots(repo, check_source=False)
            if r['tu'] == tu and r.get('logical_extent')]
    if not rows:
        return []
    path = Path(object_path)
    if not path.is_absolute():
        path = repo / path
    if not path.resolve().is_relative_to(repo.resolve()):
        raise OwnershipError('owned object path resolves outside repository')
    if path.is_symlink() or any(path.resolve() != (repo / r['owner_object']).resolve() for r in rows):
        raise OwnershipError('object does not correspond to the registry-derived owner object')
    # Ordinary ROM TUs do not read large ignored containers. A matching logical
    # owner still receives the full original-byte/overlap/source-built-blob guards.
    rows = [r for r in rom_slots(repo) if r['tu'] == tu and r.get('logical_extent')]
    elf = _elf_processor(repo)
    try:
        obj = elf.ElfFile(path.read_bytes())
    except (AssertionError, OSError, IndexError, ValueError) as exc:
        raise OwnershipError('invalid owned MIPS relocatable object') from exc
    changes = []
    # Finish every validation before changing even the in-memory ELF model.
    for row in rows:
        sections = [s for s in obj.sections if s.name == row['owner_section']]
        if len(sections) != 1:
            raise OwnershipError('expected exactly one owned logical input section')
        section = sections[0]
        descriptive = _logical_table(obj, section, row['size'], elf, row['logical_kind'])
        changes.append((row, section, descriptive))
    if all(s.sh_size == r['size'] and s.sh_addralign == r['alignment']
           for r, s, _ in changes):
        return []
    relocations = {s.index: s.data for s in obj.sections if s.is_rel()}
    for row, section, descriptive in changes:
        for symbol in descriptive:
            if symbol.st_size == section.sh_size:
                symbol.st_size = row['size']
        section.data = section.data[:row['size']]
        section.sh_size = row['size']
        section.sh_addralign = row['alignment']
    obj.symtab.data = b''.join(symbol.to_bin() for symbol in obj.symtab.symbol_entries)
    for section in obj.sections:
        if section.sh_type == elf.SHT_MIPS_DEBUG:
            section.relocate_mdebug = _mdebug_relocator(section, obj.fmt)
    fd, temporary = tempfile.mkstemp(prefix=path.name + '.extent-', dir=path.parent)
    os.close(fd)
    try:
        obj.write(temporary)
        replay = elf.ElfFile(Path(temporary).read_bytes())
        if any(replay.sections[i].data != data for i, data in relocations.items()):
            raise OwnershipError('ELF serialization changed a retained relocation')
        if [s.to_bin() for s in replay.symtab.symbol_entries] != [s.to_bin() for s in obj.symtab.symbol_entries]:
            raise OwnershipError('ELF serialization changed a retained symbol')
        os.chmod(temporary, path.stat().st_mode & 0o777)
        os.replace(temporary, path)
    finally:
        if Path(temporary).exists():
            Path(temporary).unlink()
    return [{'owner': r['owner'], 'section': s.name, 'size': r['size'],
             'alignment': r['alignment']} for r, s, _ in changes]


def _slices(rows, length):
    cursor = 0
    for i, r in enumerate(rows):
        yield i, cursor, r['offset']
        cursor = r['offset'] + r['size']
    yield len(rows), cursor, length


def section_name(rows, i):
    return rows[0]['container_section'] + '.owned' + str(i)


def split_container(repo, source, composed, output, *, objcopy='mips-linux-gnu-objcopy'):
    """Remove owned bytes, retaining only actual prefix/inter-slot/suffix inputs."""
    source = _relative(source); rows = _groups(repo).get(source, [])
    data = Path(composed).read_bytes(); original = (Path(repo) / source).read_bytes()
    if len(data) != len(original):
        raise OwnershipError('composed container size changed')
    if not rows:
        subprocess.run([objcopy, '-I', 'binary', '-O', 'elf32-big', str(composed), str(output)], check=True)
        return []
    # Also check the composed range, so an unrelated overlay cannot hide an owner.
    for r in rows:
        if hashlib.sha256(data[r['offset']:r['offset'] + r['size']]).hexdigest() != r['sha256']:
            raise OwnershipError('composed input altered an owned slot')
    output = Path(output); output.parent.mkdir(parents=True, exist_ok=True)
    chunks = []
    for i, start, end in _slices(rows, len(data)):
        chunk = output.with_name(output.name + '.owned' + str(i) + '.bin')
        chunk.write_bytes(data[start:end]); chunks.append((section_name(rows, i), chunk))
    nonempty = [(name, chunk) for name, chunk in chunks if chunk.stat().st_size]
    if nonempty:
        first_name, first = nonempty[0]
        subprocess.run([objcopy, '-I', 'binary', '-O', 'elf32-big', '--rename-section', '.data=' + first_name, str(first), str(output)], check=True)
    else:
        # A completely owned container still needs a valid empty ELF input.
        # The bootstrap byte is removed before linking; no fake payload remains.
        empty = output.with_name(output.name + '.empty.bin'); empty.write_bytes(b'\0')
        subprocess.run([objcopy, '-I', 'binary', '-O', 'elf32-big', str(empty), str(output)], check=True)
        subprocess.run([objcopy, '-I', 'elf32-big', '-O', 'elf32-big', '--remove-section', '.data', str(output)], check=True)
    for name, chunk in nonempty[1:]:
        subprocess.run([objcopy, '-I', 'elf32-big', '-O', 'elf32-big', '--add-section', name + '=' + str(chunk), '--set-section-flags', name + '=alloc,load,data,contents', str(output)], check=True)
    return [str(p) for _, p in chunks]


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--repo', type=Path, default=REPO)
    sub = p.add_subparsers(dest='command', required=True)
    split = sub.add_parser('split'); split.add_argument('source'); split.add_argument('composed'); split.add_argument('output'); split.add_argument('--objcopy', default='mips-linux-gnu-objcopy')
    obj = sub.add_parser('object'); obj.add_argument('tu'); obj.add_argument('path')
    a = p.parse_args(argv)
    if a.command == 'split':
        split_container(a.repo, a.source, a.composed, a.output, objcopy=a.objcopy)
    else:
        normalize_object_sections(a.repo, a.tu, a.path)


if __name__ == '__main__':
    main()
