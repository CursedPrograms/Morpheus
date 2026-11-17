#!/usr/bin/env bash
# Installs Morpheus to ~/Applications/Morpheus (Linux/macOS).
set -e
SRC="$(dirname "$0")/build"
if [ -f "$(dirname "$0")/Morpheus" ]; then SRC="$(dirname "$0")"; fi
if [ ! -f "$SRC/Morpheus" ]; then echo "Build it first: ./build.sh"; exit 1; fi
DEST="$HOME/Applications/Morpheus"
mkdir -p "$DEST"
cp -r "$SRC/"* "$DEST/"
echo "Installed to $DEST"
