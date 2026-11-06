#!/bin/sh
# Hosts an online race (README.txt, "Playing online"). The window lists this PC's
# addresses; your friend runs join-online.sh and types one of them in.
# Options given to this script are passed on, e.g. a join code:
#     ./host-online.sh --net-code SECRET
exec "$(dirname "$0")/rush2049pc" --net-host "$@"
