#!/bin/sh
# Twilight Imperium - Linux / Steam Deck launcher: runs the game from its own
# folder (settings, saves and the log are kept there).
cd "$(dirname "$(readlink -f "$0")")" || exit 1
exec ./ti4 "$@"
