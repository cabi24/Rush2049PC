#!/bin/bash
# The N64 check without the ROM: build every N64 object (the game rules from manifest/*.json and the src/rom
# TUs) with IDO at a base commit and at HEAD, and compare what reaches the ROM: code, data, relocations,
# symbols and section sizes. Debug sections (.mdebug keeps line numbers) are ignored. No image is linked, so
# no ROM is read; the three TUs whose last step fills ROM-owned data stop before it and are compared as built.
#
#   IDO53_DIR=... IDO71_DIR=... MIPS_BIN=... tools/port/ido_check.sh [base] [workdir]
#
# IDO53_DIR / IDO71_DIR: decompals ido-static-recomp 5.3 and 7.1 (their build/out directories); MIPS_BIN: the
# directory with mips-linux-gnu-as/ld/objcopy/objdump. base defaults to the decompilation commit 09da2f0. HEAD
# is the committed tree: uncommitted changes are not checked. Exits 1 if any object differs.
set -uo pipefail
base=${1:-09da2f0}
work=${2:-$(mktemp -d)}
: "${IDO53_DIR:?set IDO53_DIR to IDO 5.3 (ido-static-recomp build/out)}"
: "${IDO71_DIR:?set IDO71_DIR to IDO 7.1}"
[ -n "${MIPS_BIN:-}" ] && export PATH=$MIPS_BIN:$PATH
command -v mips-linux-gnu-objdump >/dev/null || { echo "ido_check: no mips-linux-gnu binutils (set MIPS_BIN)" >&2; exit 2; }
top=$(git rev-parse --show-toplevel)
jobs=$(nproc)
for w in base head; do
    rev=$base; [ $w = head ] && rev=HEAD
    git -C "$top" worktree remove --force "$work/$w" 2>/dev/null
    git -C "$top" worktree add -q --detach "$work/$w" "$rev" || exit 2
    (
        cd "$work/$w"
        mkdir -p tools/ido
        ln -sfn "$IDO53_DIR" tools/ido/5.3
        ln -sfn "$IDO71_DIR" tools/ido/7.1
        (python3 tools/gen_rules.py | grep -oE '^build/[^:]+\.o'; ls src/rom/*.c | sed 's|^src/|build/us/src/|; s|\.c$|.o|') |
            sort -u > ../targets.$w
        make -k -j"$jobs" $(cat ../targets.$w) > ../make.$w.log 2>&1
        echo "ido_check: $w ($(git log --oneline -1 | cut -c1-60)): $(wc -l < ../targets.$w) objects"
    )
done

# What a linked ROM would see of an object.
contents() {
    mips-linux-gnu-objdump -d -r -z "$1" | tail -n +4
    mips-linux-gnu-objdump -s -j .data -j .rodata -j .sdata -j .rdata "$1" 2>/dev/null | tail -n +4
    mips-linux-gnu-readelf -S -W "$1" | grep -E '^\s+\[' | sed 's/^\s*\[ *[0-9]*\]//' |
        grep -v -E '\.mdebug|\.comment|\.pdr|\.gptab|\.options|\.reginfo|\.MIPS'
    mips-linux-gnu-nm "$1" | sort
}
cd "$work"
same=0; debug=0; differ=0; missing=0
: > report.txt
for t in $(sort -u targets.base targets.head); do
    if [ ! -f base/$t ] || [ ! -f head/$t ]; then
        missing=$((missing + 1)); echo "MISSING $t" >> report.txt; continue
    fi
    if cmp -s base/$t head/$t; then same=$((same + 1)); continue; fi
    if diff <(contents base/$t) <(contents head/$t) > /dev/null; then
        debug=$((debug + 1)); echo "DEBUG-ONLY $t" >> report.txt
    else
        differ=$((differ + 1)); echo "DIFF $t" >> report.txt
        diff <(contents base/$t) <(contents head/$t) | head -20 | sed 's/^/    /' >> report.txt
    fi
done
echo "ido_check: $same identical, $debug differ only in debug sections, $differ differ, $missing missing ($work/report.txt)"
grep -E '^(DIFF|MISSING)' report.txt | head -20
[ $differ -eq 0 ] && [ $missing -eq 0 ]
