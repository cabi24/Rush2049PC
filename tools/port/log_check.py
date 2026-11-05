#!/usr/bin/env python3
"""log_check.py LOG TEXT... [--not TEXT...]: a synthetic path's check (synth_paths.py `# check:`) that the program's
log has each TEXT, in that order, and none of the --not ones. Stdlib only."""
import sys


def main(argv):
    if len(argv) < 3:
        print(__doc__.strip())
        return 2
    log = open(argv[1], encoding="utf-8", errors="replace").read()
    want, never, at, bad = [], [], 0, 0
    lst = want
    for a in argv[2:]:
        if a == "--not":
            lst = never
        else:
            lst.append(a)
    for t in want:
        k = log.find(t, at)
        if k < 0:
            print(f"log_check: {argv[1]} lacks '{t}'" + (" after the ones before it" if t in log else ""))
            bad += 1
        else:
            at = k + len(t)
    for t in never:
        if t in log:
            print(f"log_check: {argv[1]} has '{t}'")
            bad += 1
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
