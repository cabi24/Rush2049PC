#!/bin/bash
# Run a command in a copy of the current git worktree on a faster build host (RUSH_REMOTE, an ssh host name).
#   RUSH_REMOTE=host tools/port/remote.sh [-n name] 'command'      e.g. remote.sh 'cmake -S . -B build/pc -G Ninja && cmake --build build/pc -j16'
# Only files git tracks or would track (git ls-files -co --exclude-standard) are sent, so ignored files
# (ROMs, build/, .claude/) never leave this machine. The copy lives in $RUSH_REMOTE:~/rush2049pc/<name>
# (name defaults to the worktree's directory name); its build/ is kept between runs.
set -euo pipefail
host=${RUSH_REMOTE:?set RUSH_REMOTE to the ssh host to build on}
top=$(git rev-parse --show-toplevel)
name=$(basename "$top")
if [ "${1:-}" = "-n" ]; then name=$2; shift 2; fi
[ $# -ge 1 ] || { echo "usage: remote.sh [-n name] 'command'" >&2; exit 2; }
dir="rush2049pc/$name"
list=$(mktemp)
trap 'rm -f "$list"' EXIT
(cd "$top" && git ls-files -co --exclude-standard -z | while IFS= read -r -d '' f; do [ -e "$f" ] && printf '%s\0' "$f"; done) > "$list"
ssh -o BatchMode=yes "$host" "mkdir -p ~/$dir"
rsync -a --from0 --files-from="$list" "$top/" "$host:$dir/"
# Drop files that are gone here (outside build/), so CMake globs see the same tree.
tr '\0' '\n' < "$list" | LC_ALL=C sort | ssh -o BatchMode=yes "$host" "cd ~/$dir && keep=\$(mktemp) && cat > \$keep && find . -path ./build -prune -o -type f -print | sed 's|^\./||' | LC_ALL=C sort | LC_ALL=C comm -23 - \$keep | while IFS= read -r f; do rm -f -- \"\$f\"; done; rm -f \$keep"
ssh -o BatchMode=yes "$host" "cd ~/$dir && bash -lc $(printf '%q' "$*")"
