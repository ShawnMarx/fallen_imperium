#!/bin/sh
# Downloads llvm-mingw (clang for Windows, runs on Linux x86-64) into
# ~/.ti4-toolchain for "make windows". About 90 MB.
set -e
VER=20240619
NAME=llvm-mingw-$VER-ucrt-ubuntu-20.04-x86_64
DEST="$HOME/.ti4-toolchain"
mkdir -p "$DEST"
if [ -x "$DEST/$NAME/bin/x86_64-w64-mingw32-g++" ]; then
  echo "already there: $DEST/$NAME"
  exit 0
fi
URL=https://github.com/mstorsjo/llvm-mingw/releases/download/$VER/$NAME.tar.xz
echo "downloading $URL"
curl -L --fail -o "$DEST/$NAME.tar.xz" "$URL"
tar -xJf "$DEST/$NAME.tar.xz" -C "$DEST"
rm -f "$DEST/$NAME.tar.xz"
echo "installed $DEST/$NAME"
