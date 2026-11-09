#!/bin/bash
# The PC-only gate (docs/port/PCONLY.md 2) as one command: build the tree, run the whole suite (the realtime-labelled
# tests under the realtime lock) and compare the game with the base build along every synthetic path, then print one
# summary. On Rocky, from a checkout there:
#
#   tools/port/pconly_gate.sh [TREE] [--base DIR] [--build DIR] [--paths 'NAME ...'] [--skip build|ctest|realtime|compare]...
#       [--data-moved]
#   RUSH_REMOTE=rocky tools/port/remote.sh -n NAME 'tools/port/pconly_gate.sh'
#
# TREE is the checkout to test (default: the current folder); DIR the base, a build of the commit the wave started
# from (default ~/rush2049pc/pconly-base): its build/pc/rush2049pc and its synthetic cartridge
# build/pc/synth/rush2049_synth.z64, which both programs run. The parts:
#
#   build     cmake --build TREE/build/pc -j8 (configured with Ninja first if it never was)
#   ctest     ctest -j4 -LE realtime
#   realtime  ctest -L realtime, under flock ~/rush2049pc/.realtime.lock
#   compare   synth_compare.py --exe BASE --exe NEW --rom BASE_SYNTH --same --shots --audio -j 8: every path must be
#             `same`. A path that isn't is run again alone (twice at most), as PCONLY.md 2 says for --audio's
#             `varies` on a loaded machine; it passes if a run alone is `same`.
#
# --paths runs only those synthetic paths (a quick look; the gate is all of them). --data-moved is synth_compare.py's:
# the loader's summary compared without its pointer counts by class, for a change that moves where the ROM's pointers
# resolve (a datum given pointers or own storage), whose data.c diff shows where. Logs go to TREE/build/pconly-gate.
# Exit status 0 when every part passes. Every process the gate starts is stopped when it ends, however it ends.
set -uo pipefail

tree=.
base=${PCONLY_BASE:-$HOME/rush2049pc/pconly-base}
build=build/pc
paths=""
data_moved=""
skip=" "
jobs_build=8
jobs_ctest=4
jobs_compare=8
lock=$HOME/rush2049pc/.realtime.lock
while [ $# -gt 0 ]; do
    case $1 in
        --base) base=$2; shift 2 ;;
        --build) build=$2; shift 2 ;;
        --paths) paths=$2; shift 2 ;;
        --data-moved) data_moved=--data-moved; shift ;;
        --skip) skip="$skip$2 "; shift 2 ;;
        -h|--help) sed -n '2,/^set -uo/p' "$0" | sed 's/^# \{0,1\}//; /^set -uo/d'; exit 0 ;;
        -*) echo "pconly_gate: unknown option $1" >&2; exit 2 ;;
        *) tree=$1; shift ;;
    esac
done
cd "$tree" || { echo "pconly_gate: no tree $tree" >&2; exit 2; }
tree=$(pwd)
logs=$tree/build/pconly-gate
mkdir -p "$logs"

# Stop everything this script started (ctest's tests, synth_compare's games) when it ends, however it ends. Each step
# runs in the background, in a process group of its own (set -m), and the script waits for it: bash runs a trap only
# once the foreground command returns, but a signal interrupts `wait` at once. On the way out the step's group goes,
# then any process left under the script, children before their parents, TERM and then KILL.
set -m
exec 3>&2   # the gate's own stderr, whatever a step's redirection is when a trap runs
step=""
run() {
    "$@" &
    step=$!
    wait "$step"
    local rc=$?
    step=""
    return $rc
}
kill_tree() {
    local sig=$1 pid=$2 child
    for child in $(ps -o pid= --ppid "$pid" 2>/dev/null); do
        kill_tree "$sig" "$child"
    done
    kill -"$sig" "$pid" 2>/dev/null
}
cleanup() {
    trap - EXIT INT TERM HUP PIPE
    local sig child
    for sig in TERM KILL; do
        [ -n "$step" ] && kill -"$sig" -- -"$step" 2>/dev/null
        for child in $(ps -o pid= --ppid $$ 2>/dev/null); do kill_tree "$sig" "$child"; done
        [ $sig = TERM ] && sleep 1
    done
    return 0
}
trap cleanup EXIT
trap 'echo "pconly_gate: interrupted" >&3; exit 130' INT TERM HUP PIPE

declare -A result detail
order=(build ctest realtime compare)
for p in "${order[@]}"; do result[$p]=skipped; detail[$p]=""; done
skipped() { [[ "$skip" == *" $1 "* ]]; }
say() { echo "pconly_gate: $*"; }

base_exe=$base/build/pc/rush2049pc
base_rom=$base/build/pc/synth/rush2049_synth.z64
if ! skipped compare; then
    for f in "$base_exe" "$base_rom"; do
        [ -f "$f" ] || { say "the base has no $f (--base DIR)"; exit 2; }
    done
fi

# ---------------------------------------------------------------------------------------------------------- build
if ! skipped build; then
    say "build: $tree/$build"
    build_step() {
        if [ ! -f "$build/CMakeCache.txt" ]; then
            cmake -S . -B "$build" $(command -v ninja >/dev/null && echo -G Ninja) || return 1
        fi
        cmake --build "$build" -j"$jobs_build"
    }
    if run build_step > "$logs/build.log" 2>&1; then
        result[build]=PASS
    else
        result[build]=FAIL
        detail[build]="see $logs/build.log"
        tail -n 30 "$logs/build.log"
    fi
fi
built=yes
[ -x "$build/rush2049pc" ] || [ -x "$build/rush2049pc.exe" ] || built=no
[ "${result[build]}" = FAIL ] && built=no

# ctest's summary line: "100% tests passed, 0 tests failed out of 612"
ctest_totals() {
    local log=$1 line
    line=$(grep -E 'tests passed, [0-9]+ tests? failed out of [0-9]+' "$log" | tail -n 1)
    if [ -n "$line" ]; then
        local failed total
        failed=$(sed -E 's/.* ([0-9]+) tests? failed out of ([0-9]+).*/\1/' <<<"$line")
        total=$(sed -E 's/.* failed out of ([0-9]+).*/\1/' <<<"$line")
        echo "$((total - failed))/$total passed"
    elif grep -q 'No tests were found' "$log"; then
        echo "0/0 (no tests)"
    else
        echo "no summary"
    fi
}

# ---------------------------------------------------------------------------------------------------------- ctest
if ! skipped ctest; then
    if [ $built = no ]; then
        result[ctest]=FAIL; detail[ctest]="not built"
    else
        say "ctest: -j$jobs_ctest -LE realtime"
        run ctest --test-dir "$build" -j"$jobs_ctest" -LE realtime --output-on-failure > "$logs/ctest.log" 2>&1
        rc=$?
        detail[ctest]=$(ctest_totals "$logs/ctest.log")
        if [ $rc -eq 0 ]; then result[ctest]=PASS; else
            result[ctest]=FAIL
            detail[ctest]="${detail[ctest]}; failed: $(grep -E '^\s+[0-9]+ - ' "$logs/ctest.log" | sed -E 's/^\s+[0-9]+ - //; s/ \(.*//' | tr '\n' ' ')"
        fi
    fi
fi

# ------------------------------------------------------------------------------------------------------- realtime
if ! skipped realtime; then
    if [ $built = no ]; then
        result[realtime]=FAIL; detail[realtime]="not built"
    else
        say "realtime: ctest -L realtime under $lock"
        mkdir -p "$(dirname "$lock")"
        run flock "$lock" ctest --test-dir "$build" -L realtime --output-on-failure > "$logs/realtime.log" 2>&1
        rc=$?
        detail[realtime]=$(ctest_totals "$logs/realtime.log")
        if [ $rc -eq 0 ]; then result[realtime]=PASS; else
            result[realtime]=FAIL
            detail[realtime]="${detail[realtime]}; failed: $(grep -E '^\s+[0-9]+ - ' "$logs/realtime.log" | sed -E 's/^\s+[0-9]+ - //; s/ \(.*//' | tr '\n' ' ')"
        fi
    fi
fi

# -------------------------------------------------------------------------------------------------------- compare
# synth_compare.py's verdict lines: "synth_compare: NAME       same     ..." (same, varies, DIFFERS, MISSES)
verdicts() { sed -nE 's/^synth_compare: ([A-Za-z0-9_]+) +(same|varies|DIFFERS|MISSES) .*/\1 \2/p' "$1"; }
if ! skipped compare; then
    if [ $built = no ]; then
        result[compare]=FAIL; detail[compare]="not built"
    else
        say "compare: $base_exe against $build/rush2049pc on $base_rom${paths:+ ($paths)}"
        cmp=(python3 tools/port/synth_compare.py --exe "$base_exe" --exe "$build/rush2049pc" --rom "$base_rom"
             --same --shots --audio $data_moved)
        # shellcheck disable=SC2086
        run "${cmp[@]}" -j "$jobs_compare" --logs build/pconly-compare $paths > "$logs/compare.log" 2>&1
        declare -A verdict
        while read -r name v; do verdict[$name]=$v; done < <(verdicts "$logs/compare.log")
        total=${#verdict[@]}
        rerun=()
        for name in "${!verdict[@]}"; do [ "${verdict[$name]}" = same ] || rerun+=("$name"); done
        again=""
        for name in $(printf '%s\n' "${rerun[@]}" | sort); do
            for try in 1 2; do
                say "compare: $name was ${verdict[$name]}; alone, run $try"
                run "${cmp[@]}" -j 2 --logs "build/pconly-compare/again-$try" "$name" > "$logs/compare-$name-$try.log" 2>&1
                v=$(verdicts "$logs/compare-$name-$try.log" | awk '{print $2}')
                again="$again $name:${v:-none}"
                if [ "$v" = same ]; then verdict[$name]=same; break; fi
            done
        done
        not_same=()
        for name in "${!verdict[@]}"; do [ "${verdict[$name]}" = same ] || not_same+=("$name:${verdict[$name]}"); done
        same=$((total - ${#not_same[@]}))
        detail[compare]="$same/$total paths same${again:+ (alone:$again)}"
        if [ "$total" -gt 0 ] && [ ${#not_same[@]} -eq 0 ]; then
            result[compare]=PASS
        else
            result[compare]=FAIL
            [ "$total" -eq 0 ] && detail[compare]="no verdicts: see $logs/compare.log"
            [ ${#not_same[@]} -gt 0 ] && detail[compare]="${detail[compare]}; not same: ${not_same[*]}"
        fi
        grep -aE '^synth_compare: [0-9]+ of [0-9]+ paths agree' "$logs/compare.log" | sed 's/^/  /'
    fi
fi

# -------------------------------------------------------------------------------------------------------- summary
echo
status=0
for p in "${order[@]}"; do
    printf 'pconly_gate: %-9s %-7s %s\n' "$p" "${result[$p]}" "${detail[$p]}"
    [ "${result[$p]}" = FAIL ] && status=1
done
if [ $status -eq 0 ]; then say "PASS (logs: $logs)"; else say "FAIL (logs: $logs)"; fi
exit $status
