#!/usr/bin/env python3
"""Check that a run's race held the cars it should (docs/port/MORECARS.md 10.3), from the line --log-cars prints at
the end ("cars at the end: N in the race, count C; 0 active place P lap L distance D moved M at X Y Z; ...").

    python3 tools/port/cars_check.py LOG CARS [--count C] [--moved FEET] [--still SLOTS] [--shot PNG VIEWS]

It passes if the race has CARS slots, each active, their places are 0 to CARS - 1 once each, and each car is at
least FEET (default 20) from where it stood as the race began, or has gone FEET along the track (every car starts
at a distance of 0 or less, and one that drove most of a lap can be back beside its start), but the slots --still
names (comma-separated: a player's car the script doesn't drive); and with --shot, if tools/port/synth_shot.py passes on the screenshot (a
path has one check line); with --count, if the race's latched count (pc_race_cars()) is C. Stdlib only."""
import argparse
import re
import subprocess
import sys
from pathlib import Path

CAR = re.compile(r"(\d+) (active|inactive) place (-?\d+) lap (-?\d+) distance (-?[\d.]+|-?nan) moved (-?[\d.]+|-?nan)"
                 r"( at \S+ \S+ \S+)?$")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("log")
    ap.add_argument("cars", type=int)
    ap.add_argument("--count", type=int, help="the race's latched count, pc_race_cars()")
    ap.add_argument("--moved", type=float, default=20.0, help="the least distance a car must be from its start")
    ap.add_argument("--still", default="", help="slots not expected to move (comma-separated)")
    ap.add_argument("--shot", nargs=2, metavar=("PNG", "VIEWS"), help="also synth_shot.py's check of a screenshot")
    a = ap.parse_args()
    still = {int(x) for x in a.still.split(",") if x}
    lines = [l for l in Path(a.log).read_text(errors="replace").splitlines() if "cars at the end:" in l]
    if not lines:
        print(f"cars_check: no 'cars at the end' line in {a.log} (run with --log-cars)")
        return 1
    head, *cars = lines[-1].split("cars at the end: ", 1)[1].split("; ")
    why = []
    m = re.match(r"(\d+) in the race, count (\d+)(.*)$", head)
    if not m or m.group(3):
        why.append(f"the race: '{head}'")
    else:
        if int(m.group(1)) != a.cars:
            why.append(f"{m.group(1)} cars in the race, not {a.cars}")
        if a.count is not None and int(m.group(2)) != a.count:
            why.append(f"the race's count is {m.group(2)}, not {a.count}")
    places = []
    for c in cars:
        f = CAR.match(c)
        if not f:
            why.append(f"unreadable: '{c}'")
            continue
        slot, state, place, lap, dist, moved = f.groups()[:6]
        slot, place = int(slot), int(place)
        places.append(place)
        if state != "active":
            why.append(f"car {slot} is not in the race")
        if "nan" in moved or "nan" in dist:
            why.append(f"car {slot} is at NaN")
        elif slot not in still and float(moved) < a.moved and int(lap) < 1 and float(dist) < a.moved:
            why.append(f"car {slot} moved {moved} ft, less than {a.moved:.0f}")
    if sorted(places) != list(range(a.cars)):
        why.append(f"the places are {places}, not 0 to {a.cars - 1} once each")
    if a.shot:
        r = subprocess.run([sys.executable, str(Path(__file__).resolve().parent / "synth_shot.py")] + a.shot,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print(r.stdout.decode(errors="replace").rstrip())
        if r.returncode:
            why.append(f"synth_shot.py {' '.join(a.shot)} failed")
    if why:
        print("cars_check: " + "; ".join(why))
        return 1
    print(f"cars_check: {a.cars} cars, places 0-{a.cars - 1} once each, every car moved"
          f"{' (but ' + ', '.join(map(str, sorted(still))) + ')' if still else ''}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
