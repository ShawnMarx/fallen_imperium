#!/bin/sh
# Downloads a prebuilt ARM cross-compiler (GCC 10.3, glibc 2.28, hard-float)
# into ~/.ti4-toolchain. It only runs on Linux x86-64 (including WSL on
# Windows and GitHub's build machines).
#
# The compiler comes from the public repo rvagg/rpi-newer-crosstools, which
# stores it directly in git; only the one folder we need is checked out.
set -e
DEST="${TI4_TOOLCHAIN_DIR:-$HOME/.ti4-toolchain}"
NAME=x64-gcc-10.3.0-glibc-2.28
if [ -x "$DEST/$NAME/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-g++" ]; then
  echo "Toolchain already present in $DEST"
  exit 0
fi
rm -rf "$DEST"
git clone --depth 1 --filter=blob:none --sparse https://github.com/rvagg/rpi-newer-crosstools.git "$DEST"
cd "$DEST"
git sparse-checkout set "$NAME"
"$DEST/$NAME/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-g++" --version | head -1
echo "Toolchain ready in $DEST"
