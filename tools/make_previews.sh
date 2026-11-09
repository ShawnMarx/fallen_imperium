#!/bin/sh
# Turns the frames recorded by "ti4 --preview DIR" into videos:
#   DIR/<clip>.mp4 (1280x960, crisp pixels) and DIR/<clip>.gif (640x480)
# Usage: sh tools/make_previews.sh build/preview
set -e
dir=${1:-build/preview}
for clip in map exploration negotiations faction betrayal navigate tile; do
  [ -d "$dir/$clip" ] || continue
  ffmpeg -loglevel error -y -framerate 10 -i "$dir/$clip/f%04d.ppm" \
    -vf "scale=1280:960:flags=neighbor" -c:v libx264 -pix_fmt yuv420p -crf 20 -movflags +faststart "$dir/$clip.mp4"
  ffmpeg -loglevel error -y -framerate 10 -i "$dir/$clip/f%04d.ppm" \
    -vf "split[a][b];[a]palettegen=max_colors=200:stats_mode=full[p];[b][p]paletteuse=dither=bayer:bayer_scale=4" "$dir/$clip.gif"
  echo "$clip: $(du -h "$dir/$clip.mp4" | cut -f1) mp4, $(du -h "$dir/$clip.gif" | cut -f1) gif"
done
