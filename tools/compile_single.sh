#!/bin/sh
# Compile one game / runtime-image function with IDO and prepare its object
# for placement at its image address.
#
#   tools/compile_single.sh IDO_CC NAME SOURCE OUTPUT FLAGS...
#
# The object's .text becomes .text.NAME (4-byte aligned: IDO pads .text to 16,
# which is padding, not code), and the sections GNU ld cannot merge into a
# script-driven link (.reginfo, .options, .mdebug, ...) are dropped. Same steps
# as the pipeline's blob_splice.compile_on_builder.
set -e
CC="$1"; NAME="$2"; SRC="$3"; OUT="$4"; shift 4
OBJCOPY="${OBJCOPY:-mips-linux-gnu-objcopy}"
mkdir -p "$(dirname "$OUT")"
"$CC" -c "$@" "$SRC" -o "$OUT.raw.o"
"$OBJCOPY" --rename-section=.text=.text."$NAME" \
    --set-section-alignment=.text."$NAME"=4 \
    -R .options -R .reginfo -R .mdebug -R .comment -R .pdr -R .MIPS.abiflags \
    "$OUT.raw.o" "$OUT"
rm -f "$OUT.raw.o"
