#!/bin/sh
# Joins a friend's online race (README.txt, "Playing online"): asks for the host's
# address, which their waiting screen shows (a Tailscale name or 100.x address).
# Options given to this script are passed on, e.g. a join code:
#     ./join-online.sh --net-code SECRET
printf "The host's address (their Tailscale name or 100.x address): "
read -r host && [ -n "$host" ] || exit 1
exec "$(dirname "$0")/rush2049pc" --net-join "$host" "$@"
