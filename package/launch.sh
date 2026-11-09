#!/bin/sh
# OnionOS launcher for the Twilight Imperium homebrew.
cd "$(dirname "$0")"
export HOME=/mnt/SDCARD
# Library folders used by OnionOS and the stock firmware (only needed if the
# game falls back to SDL for drawing).
export LD_LIBRARY_PATH="$(pwd)/lib:/lib:/config/lib:/customer/lib:/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/.tmp_update/lib:/mnt/SDCARD/.tmp_update/lib/parasyte:$LD_LIBRARY_PATH"
chmod +x ./ti4 2>/dev/null
# Sound (0.27.0): the firmware's sound helper turns the game's /dev/dsp output
# into the system's audio. Put an empty file called "nosound" in this folder to
# start without it.
if [ ! -f ./nosound ]; then
    for p in /mnt/SDCARD/miyoo/lib/libpadsp.so /customer/lib/libpadsp.so /config/lib/libpadsp.so; do
        if [ -f "$p" ]; then
            export LD_PRELOAD="$p"
            break
        fi
    done
fi
echo "sound helper: ${LD_PRELOAD:-none}" > ./launch_log.txt
start=$(date +%s)
./ti4 >> ./launch_log.txt 2>&1
rc=$?
# if the game stopped at once with the helper, start it again without (and remember)
if [ $rc -ne 0 ] && [ -n "$LD_PRELOAD" ] && [ $(( $(date +%s) - start )) -lt 10 ]; then
    echo "stopped with code $rc right away - starting again without the sound helper" >> ./launch_log.txt
    unset LD_PRELOAD
    touch ./nosound
    ./ti4 >> ./launch_log.txt 2>&1
fi
